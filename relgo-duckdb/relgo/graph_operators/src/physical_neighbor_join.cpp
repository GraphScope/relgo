#include "../includes/physical_neighbor_join.hpp"

#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/function/aggregate/distributive_functions.hpp"
#include "duckdb/function/function_binder.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/query_profiler.hpp"
#include "duckdb/parallel/base_pipeline_event.hpp"
#include "duckdb/parallel/pipeline.hpp"
#include "duckdb/parallel/thread_context.hpp"
#include "duckdb/planner/expression/bound_aggregate_expression.hpp"
#include "duckdb/planner/expression/bound_reference_expression.hpp"
#include "duckdb/storage/buffer_manager.hpp"
#include "duckdb/storage/storage_manager.hpp"

namespace duckdb {

PhysicalNeighborJoin::PhysicalNeighborJoin(
    LogicalOperator &op, unique_ptr<PhysicalOperator> left,
    unique_ptr<PhysicalOperator> right, vector<IndexedJoinCondition> cond_p,
    JoinType join_type, const vector<idx_t> &left_projection_map,
    const vector<idx_t> &right_projection_map_p,
    const vector<idx_t> &merge_projection_map, vector<LogicalType> delim_types,
    idx_t estimated_cardinality)
    : PhysicalGraphIndexJoin(cond_p),
      PhysicalJoin(op, PhysicalOperatorType::INDEX_JOIN, join_type,
                   estimated_cardinality),
      right_projection_map(right_projection_map_p),
      delim_types(std::move(delim_types)) {

  children.push_back(std::move(left));
  children.push_back(std::move(right));

  D_ASSERT(left_projection_map.empty());
  for (auto &condition : conditions) {
    condition_types.push_back(condition.left->return_type);
  }

  // for ANTI, SEMI and MARK join, we only need to store the keys, so for these
  // the build types are empty
  if (join_type != JoinType::ANTI && join_type != JoinType::SEMI &&
      join_type != JoinType::MARK) {
    build_types = LogicalOperator::MapTypes(children[1]->GetTypes(),
                                            right_projection_map);
  }
}

//===--------------------------------------------------------------------===//
// Sink
//===--------------------------------------------------------------------===//
class MergeSIPJoinGlobalSinkState : public GlobalSinkState {
public:
  MergeSIPJoinGlobalSinkState(const PhysicalNeighborJoin &op,
                              ClientContext &context_p)
      : context(context_p), finalized(false), scanned_data(false), op(op) {
    hash_table = op.InitializeHashTable(context);

    // for perfect hash join
    // perfect_join_executor = make_uniq<PerfectHashJoinExecutor>(op,
    // *hash_table, op.perfect_join_statistics); for external hash join
    external = ClientConfig::GetConfig(context).force_external;
    // Set probe types
    const auto &payload_types = op.children[0]->types;
    probe_types.insert(probe_types.end(), op.condition_types.begin(),
                       op.condition_types.end());
    probe_types.insert(probe_types.end(), payload_types.begin(),
                       payload_types.end());
    probe_types.emplace_back(LogicalType::HASH);
  }

  void ScheduleFinalize(Pipeline &pipeline, Event &event);
  void InitializeProbeSpill();

public:
  ClientContext &context;
  //! Global HT used by the join
  unique_ptr<SIPHashTable> hash_table;
  //! The perfect hash join executor (if any)
  // unique_ptr<PerfectHashJoinExecutor> perfect_join_executor;
  //! Whether or not the hash table has been finalized
  bool finalized = false;

  //! Whether we are doing an external join
  bool external;

  //! Hash tables built by each thread
  mutex lock;
  vector<unique_ptr<SIPHashTable>> local_hash_tables;

  //! Excess probe data gathered during Sink
  vector<LogicalType> probe_types;
  unique_ptr<SIPHashTable::SIPProbeSpill> probe_spill;

  //! Whether or not we have started scanning data using GetData
  atomic<bool> scanned_data;

  const PhysicalNeighborJoin &op;
};

class MergeSIPJoinLocalSinkState : public LocalSinkState {
public:
  MergeSIPJoinLocalSinkState(const PhysicalNeighborJoin &op,
                             ClientContext &context)
      : build_executor(context) {
    auto &allocator = BufferAllocator::Get(context);
    // if (!op.right_projection_map.empty()) {
    build_chunk.Initialize(allocator, op.build_types);
    // }
    for (auto &cond : op.conditions) {
      build_executor.AddExpression(*cond.right);
    }
    join_keys.Initialize(allocator, op.condition_types);
    right_condition_chunk.Initialize(allocator, op.condition_types);

    hash_table = op.InitializeHashTable(context);

    hash_table->GetSinkCollection().InitializeAppendState(append_state);
  }

public:
  PartitionedTupleDataAppendState append_state;

  DataChunk build_chunk;
  DataChunk join_keys;
  ExpressionExecutor build_executor;

  //! Thread-local HT
  unique_ptr<SIPHashTable> hash_table;
  // state for NLAJoin
  DataChunk right_condition_chunk;
};

unique_ptr<SIPHashTable>
PhysicalNeighborJoin::InitializeHashTable(ClientContext &context) const {
  auto result =
      make_uniq<SIPHashTable>(BufferManager::GetBufferManager(context),
                              conditions, build_types, join_type);
  result->max_ht_size =
      double(0.6) * BufferManager::GetBufferManager(context).GetMaxMemory();
  if (!delim_types.empty() && join_type == JoinType::MARK) {
    // correlated MARK join
    if (delim_types.size() + 1 == conditions.size()) {
      // the correlated MARK join has one more condition than the amount of
      // correlated columns this is the case in a correlated ANY() expression in
      // this case we need to keep track of additional entries, namely:
      // - (1) the total amount of elements per group
      // - (2) the amount of non-null elements per group
      // we need these to correctly deal with the cases of either:
      // - (1) the group being empty [in which case the result is always false,
      // even if the comparison is NULL]
      // - (2) the group containing a NULL value [in which case FALSE becomes
      // NULL]
      auto &info = result->correlated_mark_join_info;

      vector<LogicalType> payload_types;
      vector<BoundAggregateExpression *> correlated_aggregates;
      unique_ptr<BoundAggregateExpression> aggr;

      // jury-rigging the GroupedAggregateHashTable
      // we need a count_star and a count to get counts with and without NULLs

      FunctionBinder function_binder(context);
      aggr = function_binder.BindAggregateFunction(CountStarFun::GetFunction(),
                                                   {}, nullptr,
                                                   AggregateType::NON_DISTINCT);
      correlated_aggregates.push_back(&*aggr);
      payload_types.push_back(aggr->return_type);
      info.correlated_aggregates.push_back(std::move(aggr));

      auto count_fun = CountFun::GetFunction();
      vector<unique_ptr<Expression>> children;
      // this is a dummy but we need it to make the hash table understand whats
      // going on
      children.push_back(make_uniq_base<Expression, BoundReferenceExpression>(
          count_fun.return_type, 0));
      aggr = function_binder.BindAggregateFunction(
          count_fun, std::move(children), nullptr, AggregateType::NON_DISTINCT);
      correlated_aggregates.push_back(&*aggr);
      payload_types.push_back(aggr->return_type);
      info.correlated_aggregates.push_back(std::move(aggr));

      auto &allocator = BufferAllocator::Get(context);
      info.correlated_counts = make_uniq<GroupedAggregateHashTable>(
          context, allocator, delim_types, payload_types,
          correlated_aggregates);
      info.correlated_types = delim_types;
      info.group_chunk.Initialize(allocator, delim_types);
      info.result_chunk.Initialize(allocator, payload_types);
    }
  }
  return result;
}

unique_ptr<GlobalSinkState>
PhysicalNeighborJoin::GetGlobalSinkState(ClientContext &context) const {
  return make_uniq<MergeSIPJoinGlobalSinkState>(*this, context);
}

unique_ptr<LocalSinkState>
PhysicalNeighborJoin::GetLocalSinkState(ExecutionContext &context) const {
  return make_uniq<MergeSIPJoinLocalSinkState>(*this, context.client);
}

SinkResultType PhysicalNeighborJoin::Sink(ExecutionContext &context,
                                          DataChunk &chunk,
                                          OperatorSinkInput &input) const {
  auto &lstate = input.local_state.Cast<MergeSIPJoinLocalSinkState>();

  // resolve the join keys for the right chunk

  idx_t build_side_size = 0;
  DataChunk im_chunk;
  auto im_types = chunk.GetTypes();
  im_types.push_back(LogicalType::BIGINT);
  im_chunk.Initialize(context.client, im_types);
  // lstate.build_chunk.InitializeEmpty(lstate.hash_table->build_types);
  lstate.join_keys.Reset();
  lstate.build_executor.Execute(chunk, lstate.right_condition_chunk);

  auto &rai_info = conditions[0].rais[0];
  idx_t right_tuple = 0;
  idx_t left_tuple = 0;

  if (chunk.size() != 0) {
    build_side_size += chunk.size();
    do {
      GetVertexes(*rai_info->rai, chunk, lstate.right_condition_chunk, im_chunk,
                  left_tuple, right_tuple, rai_info->forward);
      AppendHTBlocks(lstate, im_chunk, lstate.build_chunk);
    } while (right_tuple < chunk.size());
  }

  return SinkResultType::NEED_MORE_INPUT;
}

void PhysicalNeighborJoin::GetVertexes(
    relgo::GraphIndex<int64_t *, SelectionVector> &rai, DataChunk &right_chunk,
    DataChunk &rid_chunk, DataChunk &new_chunk, idx_t &left_tuple,
    idx_t &right_tuple, bool forward) const {
  assert(new_chunk.ColumnCount() == right_chunk.ColumnCount() + 1);
  SelectionVector rvector(STANDARD_VECTOR_SIZE);

  UnifiedVectorFormat right_data;
  rid_chunk.data[0].ToUnifiedFormat(rid_chunk.size(), right_data);
  SelectionVector orvector(*right_data.sel);
  auto matched_count = rai.alist->FetchVertexes(
      left_tuple, right_tuple, rid_chunk, rid_chunk.size(), orvector, rvector,
      new_chunk, right_chunk.ColumnCount(), forward);
  // slice and construct new_chunk
  new_chunk.Slice(right_chunk, rvector, matched_count);
  new_chunk.SetCardinality(matched_count);
}

SinkCombineResultType
PhysicalNeighborJoin::Combine(ExecutionContext &context,
                              OperatorSinkCombineInput &input) const {
  auto &gstate = input.global_state.Cast<MergeSIPJoinGlobalSinkState>();
  auto &lstate = input.local_state.Cast<MergeSIPJoinLocalSinkState>();
  if (lstate.hash_table) {
    lstate.hash_table->GetSinkCollection().FlushAppendState(
        lstate.append_state);
    lock_guard<mutex> local_ht_lock(gstate.lock);
    gstate.local_hash_tables.push_back(std::move(lstate.hash_table));
  }
  auto &client_profiler = QueryProfiler::Get(context.client);
  context.thread.profiler.Flush(*this, lstate.build_executor, "build_executor",
                                1);
  client_profiler.Flush(context.thread.profiler);

  return SinkCombineResultType::FINISHED;
}

void PhysicalNeighborJoin::InitializeAList() {
  auto &rai_info = conditions[0].rais[0];
  // determine the alist for usage
  switch (rai_info->rai_type) {
  case relgo::GraphIndexType::TARGET_EDGE: {
    rai_info->forward = true;
    rai_info->compact_list = &rai_info->rai->alist->compact_forward_list;
    break;
  }
  case relgo::GraphIndexType::SOURCE_EDGE: {
    rai_info->forward = false;
    rai_info->compact_list = &rai_info->rai->alist->compact_backward_list;
    break;
  }
  default:
    break;
  }
}

void PhysicalNeighborJoin::PassZoneFilter(
    relgo::PushDownStatistics &rai_stat) const {
  // actually do the pushdown
  auto &rai_info = conditions[0].rais[0];
  PushdownZoneFilter(children[0].get(), rai_info->passing_tables[0],
                     rai_stat.row_bitmask, rai_stat.zone_bitmask);
  if (rai_info->passing_tables[1] != 0) {
    PushdownZoneFilter(children[0].get(), rai_info->passing_tables[1],
                       rai_stat.extra_row_bitmask, rai_stat.extra_zone_bitmask);
  }
}

class MergeSIPJoinOperatorState;

void PhysicalNeighborJoin::AppendHTBlocks(LocalSinkState &input,
                                          DataChunk &chunk,
                                          DataChunk &build_chunk) const {
  auto &lstate = input.Cast<MergeSIPJoinLocalSinkState>();

  lstate.join_keys.SetCardinality(chunk);
  lstate.join_keys.data[0].Reference(chunk.data[chunk.ColumnCount() - 1]);
  //	state->rhs_executor.Execute(chunk, state->join_keys);
  if (right_projection_map.size() > 0) {
    // build_chunk.Reset();
    build_chunk.SetCardinality(chunk);
    for (idx_t i = 0; i < right_projection_map.size(); i++) {
      build_chunk.data[i].Reference(chunk.data[right_projection_map[i]]);
    }
    lstate.hash_table->Build(lstate.append_state, lstate.join_keys,
                             build_chunk);
  } else {
    //		build_chunk.Reset();
    build_chunk.SetCardinality(chunk);
    for (idx_t i = 0; i < build_chunk.ColumnCount(); i++) {
      build_chunk.data[i].Reference(chunk.data[i]);
    }
    lstate.hash_table->Build(lstate.append_state, lstate.join_keys,
                             build_chunk);
  }
}

//===--------------------------------------------------------------------===//
// Finalize
//===--------------------------------------------------------------------===//
class MergeSIPJoinFinalizeTask : public ExecutorTask {
public:
  MergeSIPJoinFinalizeTask(shared_ptr<Event> event_p, ClientContext &context,
                           MergeSIPJoinGlobalSinkState &sink_p,
                           idx_t chunk_idx_from_p, idx_t chunk_idx_to_p,
                           bool parallel_p)
      : ExecutorTask(context), event(std::move(event_p)), sink(sink_p),
        chunk_idx_from(chunk_idx_from_p), chunk_idx_to(chunk_idx_to_p),
        parallel(parallel_p) {}

  TaskExecutionResult ExecuteTask(TaskExecutionMode mode) override {
    sink.hash_table->Finalize(chunk_idx_from, chunk_idx_to, parallel);

    idx_t non_empty_hash_slots = 0;
    auto pointers =
        reinterpret_cast<data_ptr_t *>(sink.hash_table->hash_map.get());
    for (idx_t i = 0; i < sink.hash_table->bitmask; ++i) {
      non_empty_hash_slots += (pointers[i] != nullptr);
    }

    auto rai_stat = make_uniq<relgo::PushDownStatistics>();
    auto &rai_info = sink.op.conditions[0].rais[0];
    double filter_passing_ratio =
        (double)non_empty_hash_slots / (double)rai_info->left_cardinalities[0];
    if (filter_passing_ratio <= 0.8) {
      sink.hash_table->GenerateBitmaskFilter(*rai_info, *rai_stat, false);
      sink.op.PassZoneFilter(*rai_stat);
    }
    event->FinishTask();
    return TaskExecutionResult::TASK_FINISHED;
  }

private:
  shared_ptr<Event> event;
  MergeSIPJoinGlobalSinkState &sink;
  idx_t chunk_idx_from;
  idx_t chunk_idx_to;
  bool parallel;
};

class MergeSIPJoinFinalizeEvent : public BasePipelineEvent {
public:
  MergeSIPJoinFinalizeEvent(Pipeline &pipeline_p,
                            MergeSIPJoinGlobalSinkState &sink)
      : BasePipelineEvent(pipeline_p), sink(sink) {}

  MergeSIPJoinGlobalSinkState &sink;

public:
  void Schedule() override {
    auto &context = pipeline->GetClientContext();

    vector<shared_ptr<Task>> finalize_tasks;
    auto &ht = *sink.hash_table;
    const auto chunk_count = ht.GetDataCollection().ChunkCount();
    const idx_t num_threads =
        TaskScheduler::GetScheduler(context).NumberOfThreads();
    if (num_threads == 1 || (ht.Count() < PARALLEL_CONSTRUCT_THRESHOLD &&
                             !context.config.verify_parallelism)) {
      // Single-threaded finalize
      finalize_tasks.push_back(make_uniq<MergeSIPJoinFinalizeTask>(
          shared_from_this(), context, sink, 0, chunk_count, false));
    } else {
      // Parallel finalize
      auto chunks_per_thread =
          MaxValue<idx_t>((chunk_count + num_threads - 1) / num_threads, 1);

      idx_t chunk_idx = 0;
      for (idx_t thread_idx = 0; thread_idx < num_threads; thread_idx++) {
        auto chunk_idx_from = chunk_idx;
        auto chunk_idx_to =
            MinValue<idx_t>(chunk_idx_from + chunks_per_thread, chunk_count);
        finalize_tasks.push_back(make_uniq<MergeSIPJoinFinalizeTask>(
            shared_from_this(), context, sink, chunk_idx_from, chunk_idx_to,
            true));
        chunk_idx = chunk_idx_to;
        if (chunk_idx == chunk_count) {
          break;
        }
      }
    }
    SetTasks(std::move(finalize_tasks));
  }

  void FinishEvent() override {
    sink.hash_table->GetDataCollection().VerifyEverythingPinned();
    sink.hash_table->finalized = true;
  }

  static constexpr const idx_t PARALLEL_CONSTRUCT_THRESHOLD = 1048576;
};

void MergeSIPJoinGlobalSinkState::ScheduleFinalize(Pipeline &pipeline,
                                                   Event &event) {
  if (hash_table->Count() == 0) {
    hash_table->finalized = true;
    return;
  }
  hash_table->InitializePointerTable();
  auto new_event = make_shared<MergeSIPJoinFinalizeEvent>(pipeline, *this);
  event.InsertEvent(std::move(new_event));
}

void MergeSIPJoinGlobalSinkState::InitializeProbeSpill() {
  lock_guard<mutex> guard(lock);
  if (!probe_spill) {
    probe_spill = make_uniq<SIPHashTable::SIPProbeSpill>(*hash_table, context,
                                                         probe_types);
  }
}

class MergeSIPJoinRepartitionTask : public ExecutorTask {
public:
  MergeSIPJoinRepartitionTask(shared_ptr<Event> event_p, ClientContext &context,
                              SIPHashTable &global_ht, SIPHashTable &local_ht)
      : ExecutorTask(context), event(std::move(event_p)), global_ht(global_ht),
        local_ht(local_ht) {}

  TaskExecutionResult ExecuteTask(TaskExecutionMode mode) override {
    local_ht.Partition(global_ht);
    event->FinishTask();
    return TaskExecutionResult::TASK_FINISHED;
  }

private:
  shared_ptr<Event> event;

  SIPHashTable &global_ht;
  SIPHashTable &local_ht;
};

class MergeSIPJoinPartitionEvent : public BasePipelineEvent {
public:
  MergeSIPJoinPartitionEvent(Pipeline &pipeline_p,
                             MergeSIPJoinGlobalSinkState &sink,
                             vector<unique_ptr<SIPHashTable>> &local_hts)
      : BasePipelineEvent(pipeline_p), sink(sink), local_hts(local_hts) {}

  MergeSIPJoinGlobalSinkState &sink;
  vector<unique_ptr<SIPHashTable>> &local_hts;

public:
  void Schedule() override {
    auto &context = pipeline->GetClientContext();
    vector<shared_ptr<Task>> partition_tasks;
    partition_tasks.reserve(local_hts.size());
    for (auto &local_ht : local_hts) {
      partition_tasks.push_back(make_uniq<MergeSIPJoinRepartitionTask>(
          shared_from_this(), context, *sink.hash_table, *local_ht));
    }
    SetTasks(std::move(partition_tasks));
  }

  void FinishEvent() override {
    local_hts.clear();
    sink.hash_table->PrepareExternalFinalize();
    sink.ScheduleFinalize(*pipeline, *this);
  }
};

SinkFinalizeType
PhysicalNeighborJoin::Finalize(Pipeline &pipeline, Event &event,
                               ClientContext &context,
                               OperatorSinkFinalizeInput &input) const {
  auto &sink = input.global_state.Cast<MergeSIPJoinGlobalSinkState>();
  auto &ht = *sink.hash_table;

  sink.external =
      ht.RequiresExternalJoin(context.config, sink.local_hash_tables);
  if (sink.external) {
    // sink.perfect_join_executor.reset();
    if (ht.RequiresPartitioning(context.config, sink.local_hash_tables)) {
      auto new_event = make_shared<MergeSIPJoinPartitionEvent>(
          pipeline, sink, sink.local_hash_tables);
      event.InsertEvent(std::move(new_event));
    } else {
      for (auto &local_ht : sink.local_hash_tables) {
        ht.Merge(*local_ht);
      }
      sink.local_hash_tables.clear();
      sink.hash_table->PrepareExternalFinalize();
      sink.ScheduleFinalize(pipeline, event);
    }
    sink.finalized = true;
    return SinkFinalizeType::READY;
  } else {
    for (auto &local_ht : sink.local_hash_tables) {
      ht.Merge(*local_ht);
    }
    sink.local_hash_tables.clear();
    ht.Unpartition();
  }

  // check for possible perfect hash table
  /*auto use_perfect_hash = sink.perfect_join_executor->CanDoPerfectHashJoin();
  if (use_perfect_hash) {
      D_ASSERT(ht.equality_types.size() == 1);
      auto key_type = ht.equality_types[0];
      use_perfect_hash =
  sink.perfect_join_executor->BuildPerfectHashTable(key_type);
  }
  // In case of a large build side or duplicates, use regular hash join
  if (!use_perfect_hash) {*/
  //    sink.perfect_join_executor.reset();
  sink.ScheduleFinalize(pipeline, event);
  // }
  sink.finalized = true;
  if (ht.Count() == 0 && EmptyResultIfRHSIsEmpty()) {
    return SinkFinalizeType::NO_OUTPUT_POSSIBLE;
  }
  return SinkFinalizeType::READY;
}

//===--------------------------------------------------------------------===//
// Operator
//===--------------------------------------------------------------------===//
class MergeSIPJoinOperatorState : public CachingOperatorState {
public:
  explicit MergeSIPJoinOperatorState(ClientContext &context)
      : probe_executor(context), initialized(false) {}

  DataChunk join_keys;
  ExpressionExecutor probe_executor;
  unique_ptr<SIPHashTable::SIPScanStructure> scan_structure;
  unique_ptr<OperatorState> perfect_hash_join_state;

  bool initialized;
  SIPHashTable::SIPProbeSpillLocalAppendState spill_state;
  //! Chunk to sink data into for external join
  DataChunk spill_chunk;

public:
  void Finalize(const PhysicalOperator &op,
                ExecutionContext &context) override {
    context.thread.profiler.Flush(op, probe_executor, "probe_executor", 0);
  }
};

unique_ptr<OperatorState>
PhysicalNeighborJoin::GetOperatorState(ExecutionContext &context) const {
  auto &allocator = BufferAllocator::Get(context.client);
  auto &sink = sink_state->Cast<MergeSIPJoinGlobalSinkState>();
  auto state = make_uniq<MergeSIPJoinOperatorState>(context.client);
  // if (sink.perfect_join_executor) {
  //    state->perfect_hash_join_state =
  //    sink.perfect_join_executor->GetOperatorState(context);
  // } else {
  state->join_keys.Initialize(allocator, condition_types);
  for (auto &cond : conditions) {
    state->probe_executor.AddExpression(*cond.left);
  }
  //}
  if (sink.external) {
    state->spill_chunk.Initialize(allocator, sink.probe_types);
    sink.InitializeProbeSpill();
  }

  return std::move(state);
}

string PhysicalNeighborJoin::ParamsToString() const {
  string extra_info =
      "Neighbor Join\n"; // EnumUtil::ToString(join_type) + "\n";

  if (right_projection_map.empty()) {
    extra_info += " empty ";
  } else {
    extra_info += " ";
    for (int i = 0; i < right_projection_map.size(); ++i) {
      extra_info += to_string(right_projection_map[i]) + ",";
    }
  }

  for (auto &it : conditions) {
    string op = ExpressionTypeToOperator(it.comparison);
    BoundReferenceExpression *left = (BoundReferenceExpression *)it.left.get();
    BoundReferenceExpression *right =
        (BoundReferenceExpression *)it.right.get();
    extra_info += to_string(left->index) + op + to_string(right->index) + "--";
    extra_info +=
        it.left->GetName() + " " + op + " " + it.right->GetName() + "\n";
  }
  extra_info += "\n[INFOSEPARATOR]\n";
  extra_info += StringUtil::Format("EC: %llu\n", estimated_cardinality);
  return extra_info;
}

// equals ProbeHashTable
OperatorResultType PhysicalNeighborJoin::ExecuteInternal(
    ExecutionContext &context, DataChunk &input, DataChunk &chunk,
    GlobalOperatorState &gstate, OperatorState &state_p) const {
  auto &state = state_p.Cast<MergeSIPJoinOperatorState>();
  auto &sink = sink_state->Cast<MergeSIPJoinGlobalSinkState>();
  D_ASSERT(sink.finalized);
  D_ASSERT(!sink.scanned_data);

  // some initialization for external hash join
  if (sink.external && !state.initialized) {
    if (!sink.probe_spill) {
      sink.InitializeProbeSpill();
    }
    state.spill_state = sink.probe_spill->RegisterThread();
    state.initialized = true;
  }

  // after BuildHashTable in GetChunkInternal
  if (sink.hash_table->Count() == 0 && EmptyResultIfRHSIsEmpty()) {
    return OperatorResultType::FINISHED;
  }

  // if (sink.perfect_join_executor) {
  //    D_ASSERT(!sink.external);
  //    return sink.perfect_join_executor->ProbePerfectHashTable(context, input,
  //    chunk, *state.perfect_hash_join_state);
  //}

  if (state.scan_structure) {
    // still have elements remaining (i.e. we got >STANDARD_VECTOR_SIZE elements
    // in the previous probe)
    state.scan_structure->Next(state.join_keys, input, chunk);

    if (chunk.size() > 0) {
      return OperatorResultType::HAVE_MORE_OUTPUT;
    }
    state.scan_structure = nullptr;
    return OperatorResultType::NEED_MORE_INPUT;
  }

  // probe the HT
  if (sink.hash_table->Count() == 0) {
    ConstructEmptyJoinResult(sink.hash_table->join_type,
                             sink.hash_table->has_null, input, chunk);
    return OperatorResultType::NEED_MORE_INPUT;
  }

  // resolve the join keys for the left chunk
  state.join_keys.Reset();
  state.probe_executor.Execute(input, state.join_keys);

  // perform the actual probe
  if (sink.external) {
    state.scan_structure = sink.hash_table->ProbeAndSpill(
        state.join_keys, input, *sink.probe_spill, state.spill_state,
        state.spill_chunk);
  } else {
    // ProbeHashTable -> Probe
    state.scan_structure = sink.hash_table->Probe(state.join_keys);
  }
  state.scan_structure->Next(state.join_keys, input, chunk);

  return OperatorResultType::HAVE_MORE_OUTPUT;
}

//===--------------------------------------------------------------------===//
// Source
//===--------------------------------------------------------------------===//
enum class MergeSIPJoinSourceStage : uint8_t {
  INIT,
  BUILD,
  PROBE,
  SCAN_HT,
  DONE
};

class MergeSIPJoinLocalSourceState;

class MergeSIPJoinGlobalSourceState : public GlobalSourceState {
public:
  MergeSIPJoinGlobalSourceState(const PhysicalNeighborJoin &op,
                                ClientContext &context);

  //! Initialize this source state using the info in the sink
  void Initialize(MergeSIPJoinGlobalSinkState &sink);
  //! Try to prepare the next stage
  void TryPrepareNextStage(MergeSIPJoinGlobalSinkState &sink);
  //! Prepare the next build/probe/scan_ht stage for external hash join (must
  //! hold lock)
  void PrepareBuild(MergeSIPJoinGlobalSinkState &sink);
  void PrepareProbe(MergeSIPJoinGlobalSinkState &sink);
  void PrepareScanHT(MergeSIPJoinGlobalSinkState &sink);
  //! Assigns a task to a local source state
  bool AssignTask(MergeSIPJoinGlobalSinkState &sink,
                  MergeSIPJoinLocalSourceState &lstate);

  idx_t MaxThreads() override {
    D_ASSERT(op.sink_state);
    auto &gstate = op.sink_state->Cast<MergeSIPJoinGlobalSinkState>();

    idx_t count;
    if (gstate.probe_spill) {
      count = probe_count;
    } else if (IsRightOuterJoin(op.join_type)) {
      count = gstate.hash_table->Count();
    } else {
      return 0;
    }
    return count / ((idx_t)STANDARD_VECTOR_SIZE * parallel_scan_chunk_count);
  }

public:
  const PhysicalNeighborJoin &op;

  //! For synchronizing the external hash join
  atomic<MergeSIPJoinSourceStage> global_stage;
  mutex lock;

  //! For HT build synchronization
  idx_t build_chunk_idx;
  idx_t build_chunk_count;
  idx_t build_chunk_done;
  idx_t build_chunks_per_thread;

  //! For probe synchronization
  idx_t probe_chunk_count;
  idx_t probe_chunk_done;

  //! To determine the number of threads
  idx_t probe_count;
  idx_t parallel_scan_chunk_count;

  //! For full/outer synchronization
  idx_t full_outer_chunk_idx;
  idx_t full_outer_chunk_count;
  idx_t full_outer_chunk_done;
  idx_t full_outer_chunks_per_thread;
};

class MergeSIPJoinLocalSourceState : public LocalSourceState {
public:
  MergeSIPJoinLocalSourceState(const PhysicalNeighborJoin &op,
                               Allocator &allocator);

  //! Do the work this thread has been assigned
  void ExecuteTask(MergeSIPJoinGlobalSinkState &sink,
                   MergeSIPJoinGlobalSourceState &gstate, DataChunk &chunk);
  //! Whether this thread has finished the work it has been assigned
  bool TaskFinished();
  //! Build, probe and scan for external hash join
  void ExternalBuild(MergeSIPJoinGlobalSinkState &sink,
                     MergeSIPJoinGlobalSourceState &gstate);
  void ExternalProbe(MergeSIPJoinGlobalSinkState &sink,
                     MergeSIPJoinGlobalSourceState &gstate, DataChunk &chunk);
  void ExternalScanHT(MergeSIPJoinGlobalSinkState &sink,
                      MergeSIPJoinGlobalSourceState &gstate, DataChunk &chunk);

public:
  //! The stage that this thread was assigned work for
  MergeSIPJoinSourceStage local_stage;
  //! Vector with pointers here so we don't have to re-initialize
  Vector addresses;

  //! Chunks assigned to this thread for building the pointer table
  idx_t build_chunk_idx_from;
  idx_t build_chunk_idx_to;

  //! Local scan state for probe spill
  ColumnDataConsumerScanState probe_local_scan;
  //! Chunks for holding the scanned probe collection
  DataChunk probe_chunk;
  DataChunk join_keys;
  DataChunk payload;
  //! Column indices to easily reference the join keys/payload columns in
  //! probe_chunk
  vector<idx_t> join_key_indices;
  vector<idx_t> payload_indices;
  //! Scan structure for the external probe
  unique_ptr<SIPHashTable::SIPScanStructure> scan_structure;
  bool empty_ht_probe_in_progress;

  //! Chunks assigned to this thread for a full/outer scan
  idx_t full_outer_chunk_idx_from;
  idx_t full_outer_chunk_idx_to;
  unique_ptr<SIPHTScanState> full_outer_scan_state;
};

unique_ptr<GlobalSourceState>
PhysicalNeighborJoin::GetGlobalSourceState(ClientContext &context) const {
  return make_uniq<MergeSIPJoinGlobalSourceState>(*this, context);
}

unique_ptr<LocalSourceState>
PhysicalNeighborJoin::GetLocalSourceState(ExecutionContext &context,
                                          GlobalSourceState &gstate) const {
  return make_uniq<MergeSIPJoinLocalSourceState>(
      *this, BufferAllocator::Get(context.client));
}

MergeSIPJoinGlobalSourceState::MergeSIPJoinGlobalSourceState(
    const PhysicalNeighborJoin &op, ClientContext &context)
    : op(op), global_stage(MergeSIPJoinSourceStage::INIT), build_chunk_count(0),
      build_chunk_done(0), probe_chunk_count(0), probe_chunk_done(0),
      probe_count(op.children[0]->estimated_cardinality),
      parallel_scan_chunk_count(context.config.verify_parallelism ? 1 : 120) {}

void MergeSIPJoinGlobalSourceState::Initialize(
    MergeSIPJoinGlobalSinkState &sink) {
  lock_guard<mutex> init_lock(lock);
  if (global_stage != MergeSIPJoinSourceStage::INIT) {
    // Another thread initialized
    return;
  }

  // Finalize the probe spill
  if (sink.probe_spill) {
    sink.probe_spill->Finalize();
  }

  global_stage = MergeSIPJoinSourceStage::PROBE;
  TryPrepareNextStage(sink);
}

void MergeSIPJoinGlobalSourceState::TryPrepareNextStage(
    MergeSIPJoinGlobalSinkState &sink) {
  switch (global_stage.load()) {
  case MergeSIPJoinSourceStage::BUILD:
    if (build_chunk_done == build_chunk_count) {
      sink.hash_table->GetDataCollection().VerifyEverythingPinned();
      sink.hash_table->finalized = true;
      PrepareProbe(sink);
    }
    break;
  case MergeSIPJoinSourceStage::PROBE:
    if (probe_chunk_done == probe_chunk_count) {
      if (IsRightOuterJoin(op.join_type)) {
        PrepareScanHT(sink);
      } else {
        PrepareBuild(sink);
      }
    }
    break;
  case MergeSIPJoinSourceStage::SCAN_HT:
    if (full_outer_chunk_done == full_outer_chunk_count) {
      PrepareBuild(sink);
    }
    break;
  default:
    break;
  }
}

void MergeSIPJoinGlobalSourceState::PrepareBuild(
    MergeSIPJoinGlobalSinkState &sink) {
  D_ASSERT(global_stage != MergeSIPJoinSourceStage::BUILD);
  auto &ht = *sink.hash_table;

  // Try to put the next partitions in the block collection of the HT
  if (!sink.external || !ht.PrepareExternalFinalize()) {
    global_stage = MergeSIPJoinSourceStage::DONE;
    return;
  }

  auto &data_collection = ht.GetDataCollection();
  if (data_collection.Count() == 0 && op.EmptyResultIfRHSIsEmpty()) {
    PrepareBuild(sink);
    return;
  }

  build_chunk_idx = 0;
  build_chunk_count = data_collection.ChunkCount();
  build_chunk_done = 0;

  auto num_threads =
      TaskScheduler::GetScheduler(sink.context).NumberOfThreads();
  build_chunks_per_thread =
      MaxValue<idx_t>((build_chunk_count + num_threads - 1) / num_threads, 1);

  ht.InitializePointerTable();

  global_stage = MergeSIPJoinSourceStage::BUILD;
}

void MergeSIPJoinGlobalSourceState::PrepareProbe(
    MergeSIPJoinGlobalSinkState &sink) {
  sink.probe_spill->PrepareNextProbe();
  const auto &consumer = *sink.probe_spill->consumer;

  probe_chunk_count = consumer.Count() == 0 ? 0 : consumer.ChunkCount();
  probe_chunk_done = 0;

  global_stage = MergeSIPJoinSourceStage::PROBE;
  if (probe_chunk_count == 0) {
    TryPrepareNextStage(sink);
    return;
  }
}

void MergeSIPJoinGlobalSourceState::PrepareScanHT(
    MergeSIPJoinGlobalSinkState &sink) {
  D_ASSERT(global_stage != MergeSIPJoinSourceStage::SCAN_HT);
  auto &ht = *sink.hash_table;

  auto &data_collection = ht.GetDataCollection();
  full_outer_chunk_idx = 0;
  full_outer_chunk_count = data_collection.ChunkCount();
  full_outer_chunk_done = 0;

  auto num_threads =
      TaskScheduler::GetScheduler(sink.context).NumberOfThreads();
  full_outer_chunks_per_thread = MaxValue<idx_t>(
      (full_outer_chunk_count + num_threads - 1) / num_threads, 1);

  global_stage = MergeSIPJoinSourceStage::SCAN_HT;
}

bool MergeSIPJoinGlobalSourceState::AssignTask(
    MergeSIPJoinGlobalSinkState &sink, MergeSIPJoinLocalSourceState &lstate) {
  D_ASSERT(lstate.TaskFinished());

  lock_guard<mutex> guard(lock);
  switch (global_stage.load()) {
  case MergeSIPJoinSourceStage::BUILD:
    if (build_chunk_idx != build_chunk_count) {
      lstate.local_stage = global_stage;
      lstate.build_chunk_idx_from = build_chunk_idx;
      build_chunk_idx = MinValue<idx_t>(
          build_chunk_count, build_chunk_idx + build_chunks_per_thread);
      lstate.build_chunk_idx_to = build_chunk_idx;
      return true;
    }
    break;
  case MergeSIPJoinSourceStage::PROBE:
    if (sink.probe_spill->consumer &&
        sink.probe_spill->consumer->AssignChunk(lstate.probe_local_scan)) {
      lstate.local_stage = global_stage;
      lstate.empty_ht_probe_in_progress = false;
      return true;
    }
    break;
  case MergeSIPJoinSourceStage::SCAN_HT:
    if (full_outer_chunk_idx != full_outer_chunk_count) {
      lstate.local_stage = global_stage;
      lstate.full_outer_chunk_idx_from = full_outer_chunk_idx;
      full_outer_chunk_idx =
          MinValue<idx_t>(full_outer_chunk_count,
                          full_outer_chunk_idx + full_outer_chunks_per_thread);
      lstate.full_outer_chunk_idx_to = full_outer_chunk_idx;
      return true;
    }
    break;
  case MergeSIPJoinSourceStage::DONE:
    break;
  default:
    throw InternalException("Unexpected HashJoinSourceStage in AssignTask!");
  }
  return false;
}

MergeSIPJoinLocalSourceState::MergeSIPJoinLocalSourceState(
    const PhysicalNeighborJoin &op, Allocator &allocator)
    : local_stage(MergeSIPJoinSourceStage::INIT),
      addresses(LogicalType::POINTER) {
  auto &chunk_state = probe_local_scan.current_chunk_state;
  chunk_state.properties = ColumnDataScanProperties::ALLOW_ZERO_COPY;

  auto &sink = op.sink_state->Cast<MergeSIPJoinGlobalSinkState>();
  probe_chunk.Initialize(allocator, sink.probe_types);
  join_keys.Initialize(allocator, op.condition_types);
  payload.Initialize(allocator, op.children[0]->types);

  // Store the indices of the columns to reference them easily
  idx_t col_idx = 0;
  for (; col_idx < op.condition_types.size(); col_idx++) {
    join_key_indices.push_back(col_idx);
  }
  for (; col_idx < sink.probe_types.size() - 1; col_idx++) {
    payload_indices.push_back(col_idx);
  }
}

void MergeSIPJoinLocalSourceState::ExecuteTask(
    MergeSIPJoinGlobalSinkState &sink, MergeSIPJoinGlobalSourceState &gstate,
    DataChunk &chunk) {
  switch (local_stage) {
  case MergeSIPJoinSourceStage::BUILD:
    ExternalBuild(sink, gstate);
    break;
  case MergeSIPJoinSourceStage::PROBE:
    ExternalProbe(sink, gstate, chunk);
    break;
  case MergeSIPJoinSourceStage::SCAN_HT:
    ExternalScanHT(sink, gstate, chunk);
    break;
  default:
    throw InternalException("Unexpected HashJoinSourceStage in ExecuteTask!");
  }
}

bool MergeSIPJoinLocalSourceState::TaskFinished() {
  switch (local_stage) {
  case MergeSIPJoinSourceStage::INIT:
  case MergeSIPJoinSourceStage::BUILD:
    return true;
  case MergeSIPJoinSourceStage::PROBE:
    return scan_structure == nullptr && !empty_ht_probe_in_progress;
  case MergeSIPJoinSourceStage::SCAN_HT:
    return full_outer_scan_state == nullptr;
  default:
    throw InternalException("Unexpected HashJoinSourceStage in TaskFinished!");
  }
}

void MergeSIPJoinLocalSourceState::ExternalBuild(
    MergeSIPJoinGlobalSinkState &sink, MergeSIPJoinGlobalSourceState &gstate) {
  D_ASSERT(local_stage == MergeSIPJoinSourceStage::BUILD);

  auto &ht = *sink.hash_table;
  ht.Finalize(build_chunk_idx_from, build_chunk_idx_to, true);

  lock_guard<mutex> guard(gstate.lock);
  gstate.build_chunk_done += build_chunk_idx_to - build_chunk_idx_from;
}

void MergeSIPJoinLocalSourceState::ExternalProbe(
    MergeSIPJoinGlobalSinkState &sink, MergeSIPJoinGlobalSourceState &gstate,
    DataChunk &chunk) {
  D_ASSERT(local_stage == MergeSIPJoinSourceStage::PROBE &&
           sink.hash_table->finalized);

  if (scan_structure) {
    // Still have elements remaining (i.e. we got >STANDARD_VECTOR_SIZE elements
    // in the previous probe)
    scan_structure->Next(join_keys, payload, chunk);
    if (chunk.size() != 0) {
      return;
    }
  }

  if (scan_structure || empty_ht_probe_in_progress) {
    // Previous probe is done
    scan_structure = nullptr;
    empty_ht_probe_in_progress = false;
    sink.probe_spill->consumer->FinishChunk(probe_local_scan);
    lock_guard<mutex> lock(gstate.lock);
    gstate.probe_chunk_done++;
    return;
  }

  // Scan input chunk for next probe
  sink.probe_spill->consumer->ScanChunk(probe_local_scan, probe_chunk);

  // Get the probe chunk columns/hashes
  join_keys.ReferenceColumns(probe_chunk, join_key_indices);
  payload.ReferenceColumns(probe_chunk, payload_indices);
  auto precomputed_hashes = &probe_chunk.data.back();

  if (sink.hash_table->Count() == 0 && !gstate.op.EmptyResultIfRHSIsEmpty()) {
    gstate.op.ConstructEmptyJoinResult(
        sink.hash_table->join_type, sink.hash_table->has_null, payload, chunk);
    empty_ht_probe_in_progress = true;
    return;
  }

  // Perform the probe
  scan_structure = sink.hash_table->Probe(join_keys, precomputed_hashes);
  scan_structure->Next(join_keys, payload, chunk);
}

void MergeSIPJoinLocalSourceState::ExternalScanHT(
    MergeSIPJoinGlobalSinkState &sink, MergeSIPJoinGlobalSourceState &gstate,
    DataChunk &chunk) {
  D_ASSERT(local_stage == MergeSIPJoinSourceStage::SCAN_HT);

  if (!full_outer_scan_state) {
    full_outer_scan_state = make_uniq<SIPHTScanState>(
        sink.hash_table->GetDataCollection(), full_outer_chunk_idx_from,
        full_outer_chunk_idx_to);
  }
  sink.hash_table->ScanFullOuter(*full_outer_scan_state, addresses, chunk);

  if (chunk.size() == 0) {
    full_outer_scan_state = nullptr;
    lock_guard<mutex> guard(gstate.lock);
    gstate.full_outer_chunk_done +=
        full_outer_chunk_idx_to - full_outer_chunk_idx_from;
  }
}

SourceResultType
PhysicalNeighborJoin::GetData(ExecutionContext &context, DataChunk &chunk,
                              OperatorSourceInput &input) const {
  auto &sink = sink_state->Cast<MergeSIPJoinGlobalSinkState>();
  auto &gstate = input.global_state.Cast<MergeSIPJoinGlobalSourceState>();
  auto &lstate = input.local_state.Cast<MergeSIPJoinLocalSourceState>();
  sink.scanned_data = true;

  if (!sink.external && !IsRightOuterJoin(join_type)) {
    return SourceResultType::FINISHED;
  }

  if (gstate.global_stage == MergeSIPJoinSourceStage::INIT) {
    gstate.Initialize(sink);
  }

  // Any call to GetData must produce tuples, otherwise the pipeline executor
  // thinks that we're done Therefore, we loop until we've produced tuples, or
  // until the operator is actually done
  while (gstate.global_stage != MergeSIPJoinSourceStage::DONE &&
         chunk.size() == 0) {
    if (!lstate.TaskFinished() || gstate.AssignTask(sink, lstate)) {
      lstate.ExecuteTask(sink, gstate, chunk);
    } else {
      lock_guard<mutex> guard(gstate.lock);
      gstate.TryPrepareNextStage(sink);
    }
  }

  return chunk.size() == 0 ? SourceResultType::FINISHED
                           : SourceResultType::HAVE_MORE_OUTPUT;
}

} // namespace duckdb
