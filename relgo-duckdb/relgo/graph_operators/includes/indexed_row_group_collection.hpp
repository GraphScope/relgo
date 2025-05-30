//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/storage/table/row_group_collection.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/storage/statistics/column_statistics.hpp"
#include "duckdb/storage/table/append_state.hpp"
#include "duckdb/storage/table/segment_tree.hpp"
#include "duckdb/storage/table/table_statistics.hpp"
#include "indexed_row_group.hpp"

namespace duckdb {
// struct ParallelTableScanState;
// struct ParallelCollectionScanState;
class IndexedCreateIndexScanState;
class IndexedCollectionScanState;
class PersistentTableData;
class TableDataWriter;
class TableIndexList;
class TableStatistics;
struct IndexedTableAppendState;
class DuckTransaction;
class BoundConstraint;
class IndexedRowGroupSegmentTree;
struct ColumnSegmentInfo;
class MetadataManager;

class IndexedRowGroupCollection {
public:
  IndexedRowGroupCollection(shared_ptr<DataTableInfo> info,
                            BlockManager &block_manager,
                            vector<LogicalType> types, idx_t row_start,
                            idx_t total_rows = 0);
  IndexedRowGroupCollection(RowGroupCollection &rgc);

public:
  idx_t GetTotalRows() const;
  Allocator &GetAllocator() const;

  void Initialize(PersistentTableData &data);
  void InitializeEmpty();

  bool IsEmpty() const;

  void AppendRowGroup(SegmentLock &l, idx_t start_row);
  //! Get the nth row-group, negative numbers start from the back (so -1 is the
  //! last row group, etc)
  IndexedRowGroup *GetRowGroup(int64_t index);
  void Verify();

  void InitializeScan(IndexedCollectionScanState &state,
                      const vector<column_t> &column_ids,
                      TableFilterSet *table_filters);
  void InitializeCreateIndexScan(IndexedCreateIndexScanState &state);
  void InitializeScanWithOffset(IndexedCollectionScanState &state,
                                const vector<column_t> &column_ids,
                                idx_t start_row, idx_t end_row);
  static bool InitializeScanInRowGroup(IndexedCollectionScanState &state,
                                       IndexedRowGroupCollection &collection,
                                       IndexedRowGroup &row_group,
                                       idx_t vector_index, idx_t max_row);
  void InitializeParallelScan(IndexedParallelCollectionScanState &state);
  bool NextParallelScan(ClientContext &context,
                        IndexedParallelCollectionScanState &state,
                        IndexedCollectionScanState &scan_state);

  bool Scan(DuckTransaction &transaction, const vector<column_t> &column_ids,
            const std::function<bool(DataChunk &chunk)> &fun);
  bool Scan(DuckTransaction &transaction,
            const std::function<bool(DataChunk &chunk)> &fun);

  void Fetch(TransactionData transaction, DataChunk &result,
             const vector<column_t> &column_ids, const Vector &row_identifiers,
             idx_t fetch_count, ColumnFetchState &state);

  //! Initialize an append of a variable number of rows. FinalizeAppend must be
  //! called after appending is done.
  void InitializeAppend(IndexedTableAppendState &state);
  //! Initialize an append with a known number of rows. FinalizeAppend should
  //! not be called after appending is done.
  void InitializeAppend(TransactionData transaction,
                        IndexedTableAppendState &state, idx_t append_count);
  //! Appends to the row group collection. Returns true if a new row group has
  //! been created to append to
  bool Append(DataChunk &chunk, IndexedTableAppendState &state);
  //! FinalizeAppend flushes an append with a variable number of rows.
  void FinalizeAppend(TransactionData transaction,
                      IndexedTableAppendState &state);
  void CommitAppend(transaction_t commit_id, idx_t row_start, idx_t count);
  void RevertAppendInternal(idx_t start_row, idx_t count);

  void MergeStorage(IndexedRowGroupCollection &data);

  void RemoveFromIndexes(TableIndexList &indexes, Vector &row_identifiers,
                         idx_t count);

  idx_t Delete(TransactionData transaction, DataTable &table, row_t *ids,
               idx_t count);
  void Update(TransactionData transaction, row_t *ids,
              const vector<PhysicalIndex> &column_ids, DataChunk &updates);
  void UpdateColumn(TransactionData transaction, Vector &row_ids,
                    const vector<column_t> &column_path, DataChunk &updates);

  void Checkpoint(TableDataWriter &writer, TableStatistics &global_stats);

  void CommitDropColumn(idx_t index);
  void CommitDropTable();

  vector<ColumnSegmentInfo> GetColumnSegmentInfo();
  const vector<LogicalType> &GetTypes() const;

  shared_ptr<IndexedRowGroupCollection> AddColumn(ClientContext &context,
                                                  ColumnDefinition &new_column,
                                                  Expression &default_value);
  shared_ptr<IndexedRowGroupCollection> RemoveColumn(idx_t col_idx);
  shared_ptr<IndexedRowGroupCollection>
  AlterType(ClientContext &context, idx_t changed_idx,
            const LogicalType &target_type, vector<column_t> bound_columns,
            Expression &cast_expr);
  void VerifyNewConstraint(DataTable &parent,
                           const BoundConstraint &constraint);

  void CopyStats(TableStatistics &stats);
  unique_ptr<BaseStatistics> CopyStats(column_t column_id);
  void SetDistinct(column_t column_id,
                   unique_ptr<DistinctStatistics> distinct_stats);

  AttachedDatabase &GetAttached();
  BlockManager &GetBlockManager() { return block_manager; }
  MetadataManager &GetMetadataManager();
  DataTableInfo &GetTableInfo() { return *info; }

private:
  bool IsEmpty(SegmentLock &) const;

private:
  //! BlockManager
  BlockManager &block_manager;
  //! The number of rows in the table
  atomic<idx_t> total_rows;
  //! The data table info
  shared_ptr<DataTableInfo> info;
  //! The column types of the row group collection
  vector<LogicalType> types;
  idx_t row_start;
  //! The segment trees holding the various row_groups of the table
  shared_ptr<IndexedRowGroupSegmentTree> row_groups;
  //! Table statistics
  TableStatistics stats;
};

} // namespace duckdb
