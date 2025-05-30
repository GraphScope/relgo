//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/storage/table/indexed_row_group.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/enums/scan_options.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/vector_size.hpp"
#include "duckdb/parser/column_list.hpp"
#include "duckdb/storage/block.hpp"
#include "duckdb/storage/statistics/segment_statistics.hpp"
#include "duckdb/storage/table/chunk_info.hpp"
#include "duckdb/storage/table/segment_base.hpp"
#include "indexed_scan_state.hpp"

namespace duckdb {
class AttachedDatabase;
class BlockManager;
class ColumnData;
class DatabaseInstance;
class DataTable;
class PartialBlockManager;
struct DataTableInfo;
class ExpressionExecutor;
class IndexedRowGroupCollection;
class RowGroupWriter;
class UpdateSegment;
class TableStatistics;
struct ColumnSegmentInfo;
class Vector;
struct ColumnCheckpointState;
struct RowGroupPointer;
struct TransactionData;
class IndexedCollectionScanState;
class TableFilterSet;
struct ColumnFetchState;
struct IndexedRowGroupAppendState;
class MetadataManager;
class RowVersionManager;

struct IndexedRowGroupWriteData {
  vector<unique_ptr<ColumnCheckpointState>> states;
  vector<BaseStatistics> statistics;
};

class IndexedRowGroup : public SegmentBase<IndexedRowGroup> {
public:
  friend class ColumnData;

public:
  IndexedRowGroup(IndexedRowGroupCollection &collection, idx_t start,
                  idx_t count);
  IndexedRowGroup(IndexedRowGroupCollection &collection,
                  RowGroupPointer &&pointer);
  IndexedRowGroup(IndexedRowGroupCollection &collection, RowGroup &rg);
  ~IndexedRowGroup();

private:
  //! The RowGroupCollection this row-group is a part of
  reference<IndexedRowGroupCollection> collection;
  //! The version info of the row_group (inserted and deleted tuple info)
  shared_ptr<RowVersionManager> version_info;
  //! The column data of the row_group
  vector<shared_ptr<ColumnData>> columns;

public:
  void MoveToCollection(IndexedRowGroupCollection &collection, idx_t new_start);
  IndexedRowGroupCollection &GetCollection() { return collection.get(); }
  BlockManager &GetBlockManager();
  DataTableInfo &GetTableInfo();

  unique_ptr<IndexedRowGroup> AlterType(IndexedRowGroupCollection &collection,
                                        const LogicalType &target_type,
                                        idx_t changed_idx,
                                        ExpressionExecutor &executor,
                                        IndexedCollectionScanState &scan_state,
                                        DataChunk &scan_chunk);
  unique_ptr<IndexedRowGroup> AddColumn(IndexedRowGroupCollection &collection,
                                        ColumnDefinition &new_column,
                                        ExpressionExecutor &executor,
                                        Expression &default_value,
                                        Vector &intermediate);
  unique_ptr<IndexedRowGroup>
  AddColumn(IndexedRowGroupCollection &new_collection, int &column_index,
            LogicalType &new_type, Vector &intermediate, int rows_to_write);
  unique_ptr<IndexedRowGroup>
  RemoveColumn(IndexedRowGroupCollection &collection, idx_t removed_column);

  void CommitDrop();
  void CommitDropColumn(idx_t index);

  void InitializeEmpty(const vector<LogicalType> &types);

  //! Initialize a scan over this row_group
  bool InitializeScan(IndexedCollectionScanState &state);
  bool InitializeScanWithOffset(IndexedCollectionScanState &state,
                                idx_t vector_offset);
  //! Checks the given set of table filters against the row-group statistics.
  //! Returns false if the entire row group can be skipped.
  bool CheckZonemap(TableFilterSet &filters,
                    const vector<column_t> &column_ids);
  //! Checks the given set of table filters against the per-segment statistics.
  //! Returns false if any segments were skipped.
  bool CheckZonemapSegments(IndexedCollectionScanState &state,
                            idx_t &current_row, SelectionVector &valid_sel,
                            idx_t &sel_count);
  void Scan(TransactionData transaction, IndexedCollectionScanState &state,
            DataChunk &result);
  void ScanCommitted(IndexedCollectionScanState &state, DataChunk &result,
                     TableScanType type);

  idx_t GetSelVector(TransactionData transaction, idx_t vector_idx,
                     SelectionVector &sel_vector, idx_t max_count);
  idx_t GetCommittedSelVector(transaction_t start_time,
                              transaction_t transaction_id, idx_t vector_idx,
                              SelectionVector &sel_vector, idx_t max_count);

  //! For a specific row, returns true if it should be used for the transaction
  //! and false otherwise.
  bool Fetch(TransactionData transaction, idx_t row);
  //! Fetch a specific row from the row_group and insert it into the result at
  //! the specified index
  void FetchRow(TransactionData transaction, ColumnFetchState &state,
                const vector<column_t> &column_ids, row_t row_id,
                DataChunk &result, idx_t result_idx);

  //! Append count rows to the version info
  void AppendVersionInfo(TransactionData transaction, idx_t count);
  //! Commit a previous append made by RowGroup::AppendVersionInfo
  void CommitAppend(transaction_t commit_id, idx_t start, idx_t count);
  //! Revert a previous append made by RowGroup::AppendVersionInfo
  void RevertAppend(idx_t start);

  //! Delete the given set of rows in the version manager
  idx_t Delete(TransactionData transaction, DataTable &table, row_t *row_ids,
               idx_t count);

  IndexedRowGroupWriteData
  WriteToDisk(PartialBlockManager &manager,
              const vector<CompressionType> &compression_types);
  bool AllDeleted();
  RowGroupPointer Checkpoint(RowGroupWriter &writer,
                             TableStatistics &global_stats);

  void InitializeAppend(IndexedRowGroupAppendState &append_state);
  void Append(IndexedRowGroupAppendState &append_state, DataChunk &chunk,
              idx_t append_count);

  void Update(TransactionData transaction, DataChunk &updates, row_t *ids,
              idx_t offset, idx_t count,
              const vector<PhysicalIndex> &column_ids);
  //! Update a single column; corresponds to DataTable::UpdateColumn
  //! This method should only be called from the WAL
  void UpdateColumn(TransactionData transaction, DataChunk &updates,
                    Vector &row_ids, const vector<column_t> &column_path);

  void MergeStatistics(idx_t column_idx, const BaseStatistics &other);
  void MergeIntoStatistics(idx_t column_idx, BaseStatistics &other);
  unique_ptr<BaseStatistics> GetStatistics(idx_t column_idx);

  void GetColumnSegmentInfo(idx_t row_group_index,
                            vector<ColumnSegmentInfo> &result);

  void Verify();

  void NextVector(IndexedCollectionScanState &state);

  idx_t DeleteRows(idx_t vector_idx, transaction_t transaction_id, row_t rows[],
                   idx_t count);
  RowVersionManager &GetOrCreateVersionInfo();

  // Serialization
  static void Serialize(RowGroupPointer &pointer, Serializer &serializer);
  static RowGroupPointer Deserialize(Deserializer &deserializer);

  ColumnData &GetColumn(storage_t c);

private:
  shared_ptr<RowVersionManager> &GetVersionInfo();
  shared_ptr<RowVersionManager> &GetOrCreateVersionInfoPtr();

  idx_t GetColumnCount() const;
  vector<shared_ptr<ColumnData>> &GetColumns();

  template <TableScanType TYPE>
  void TemplatedScan(TransactionData transaction,
                     IndexedCollectionScanState &state, DataChunk &result);

  vector<MetaBlockPointer> CheckpointDeletes(MetadataManager &manager);

  bool HasUnloadedDeletes() const;

  //! Perform lookup
  // template <class T>
  // idx_t inline LookupRows(shared_ptr<ColumnData> column,
  //                         const shared_ptr<std::vector<row_t>> &rowids,
  //                         Vector &result, idx_t offset, idx_t count,
  //                         idx_t type_size = sizeof(T));

  // idx_t PerformLookups(TransactionData transaction,
  //                      IndexedCollectionScanState &state, DataChunk &result,
  //                      shared_ptr<std::vector<row_t>> &rowids, idx_t offset,
  //                      idx_t &new_num, idx_t size);

private:
  mutex row_group_lock;
  mutex stats_lock;
  vector<MetaBlockPointer> column_pointers;
  unique_ptr<atomic<bool>[]> is_loaded;
  vector<MetaBlockPointer> deletes_pointers;
  atomic<bool> deletes_is_loaded;
};

} // namespace duckdb
