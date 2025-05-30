#include "../includes/indexed_row_group.hpp"

#include "../../graph_index/physical_graph_index.hpp"
#include "../includes/indexed_append_state.hpp"
#include "../includes/indexed_row_group_collection.hpp"
#include "../includes/indexed_row_group_segment_tree.hpp"
#include "../includes/indexed_scan_state.hpp"
#include "duckdb/common/chrono.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/serializer/binary_serializer.hpp"
#include "duckdb/common/serializer/deserializer.hpp"
#include "duckdb/common/serializer/serializer.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/execution/expression_executor.hpp"
#include "duckdb/main/attached_database.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/planner/table_filter.hpp"
#include "duckdb/storage/checkpoint/table_data_writer.hpp"
#include "duckdb/storage/metadata/metadata_reader.hpp"
#include "duckdb/storage/table/column_checkpoint_state.hpp"
#include "duckdb/storage/table/column_data.hpp"
#include "duckdb/storage/table/row_version_manager.hpp"
#include "duckdb/storage/table/update_segment.hpp"
#include "duckdb/storage/table_storage_info.hpp"
#include "duckdb/transaction/duck_transaction.hpp"
#include "duckdb/transaction/duck_transaction_manager.hpp"

namespace duckdb {

IndexedRowGroup::IndexedRowGroup(IndexedRowGroupCollection &collection, idx_t start, idx_t count)
    : SegmentBase<IndexedRowGroup>(start, count), collection(collection) {
	Verify();
}

IndexedRowGroup::IndexedRowGroup(IndexedRowGroupCollection &collection, RowGroupPointer &&pointer)
    : SegmentBase<IndexedRowGroup>(pointer.row_start, pointer.tuple_count), collection(collection) {
	// deserialize the columns
	if (pointer.data_pointers.size() != collection.GetTypes().size()) {
		throw IOException("Row group column count is unaligned with table column "
		                  "count. Corrupt file?");
	}
	this->column_pointers = std::move(pointer.data_pointers);
	this->columns.resize(column_pointers.size());
	this->is_loaded = unique_ptr<atomic<bool>[]>(new atomic<bool>[columns.size()]);
	for (idx_t c = 0; c < columns.size(); c++) {
		this->is_loaded[c] = false;
	}
	this->deletes_pointers = std::move(pointer.deletes_pointers);
	this->deletes_is_loaded = false;

	Verify();
}

IndexedRowGroup::IndexedRowGroup(IndexedRowGroupCollection &collection, RowGroup &rg)
    : SegmentBase<IndexedRowGroup>(rg.start, rg.count), collection(collection) {
	this->column_pointers = rg.column_pointers;
	this->version_info = rg.version_info;
	this->columns = rg.columns;
	this->is_loaded = unique_ptr<atomic<bool>[]>(new atomic<bool>[columns.size()]);
	for (idx_t c = 0; c < columns.size(); c++) {
		this->is_loaded[c] = true;
	}
	this->deletes_pointers = rg.deletes_pointers;
	this->deletes_is_loaded = false;
}

void IndexedRowGroup::MoveToCollection(IndexedRowGroupCollection &collection, idx_t new_start) {
	this->collection = collection;
	this->start = new_start;
	for (auto &column : GetColumns()) {
		column->SetStart(new_start);
	}
	if (!HasUnloadedDeletes()) {
		auto &vinfo = GetVersionInfo();
		if (vinfo) {
			vinfo->SetStart(new_start);
		}
	}
}

IndexedRowGroup::~IndexedRowGroup() {
}

vector<shared_ptr<ColumnData>> &IndexedRowGroup::GetColumns() {
	// ensure all columns are loaded
	for (idx_t c = 0; c < GetColumnCount(); c++) {
		GetColumn(c);
	}
	return columns;
}

idx_t IndexedRowGroup::GetColumnCount() const {
	return columns.size();
}

ColumnData &IndexedRowGroup::GetColumn(storage_t c) {
	D_ASSERT(c < columns.size());
	if (!is_loaded) {
		// not being lazy loaded
		D_ASSERT(columns[c]);
		return *columns[c];
	}
	if (is_loaded[c]) {
		D_ASSERT(columns[c]);
		return *columns[c];
	}
	lock_guard<mutex> l(row_group_lock);
	if (columns[c]) {
		D_ASSERT(is_loaded[c]);
		return *columns[c];
	}
	if (column_pointers.size() != columns.size()) {
		throw InternalException("Lazy loading a column but the pointer was not set");
	}
	auto &metadata_manager = GetCollection().GetMetadataManager();
	auto &types = GetCollection().GetTypes();
	auto &block_pointer = column_pointers[c];
	MetadataReader column_data_reader(metadata_manager, block_pointer);
	this->columns[c] =
	    ColumnData::Deserialize(GetBlockManager(), GetTableInfo(), c, start, column_data_reader, types[c], nullptr);
	is_loaded[c] = true;
	if (this->columns[c]->count != this->count) {
		throw InternalException("Corrupted database - loaded column with index "
		                        "%llu at row start %llu, count %llu did "
		                        "not match count of row group %llu",
		                        c, start, this->columns[c]->count, this->count.load());
	}
	return *columns[c];
}

BlockManager &IndexedRowGroup::GetBlockManager() {
	return GetCollection().GetBlockManager();
}
DataTableInfo &IndexedRowGroup::GetTableInfo() {
	return GetCollection().GetTableInfo();
}

void IndexedRowGroup::InitializeEmpty(const vector<LogicalType> &types) {
	// set up the segment trees for the column segments
	D_ASSERT(columns.empty());
	for (idx_t i = 0; i < types.size(); i++) {
		auto column_data = ColumnData::CreateColumn(GetBlockManager(), GetTableInfo(), i, start, types[i]);
		columns.push_back(std::move(column_data));
	}
}

void IndexedCollectionScanState::Initialize(const vector<LogicalType> &types) {
	auto &column_ids = GetColumnIds();
	column_scans = make_unsafe_uniq_array<ColumnScanState>(column_ids.size());
	for (idx_t i = 0; i < column_ids.size(); i++) {
		if (column_ids[i] == COLUMN_IDENTIFIER_ROW_ID || column_ids[i] >= relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) {
			continue;
		}
		column_scans[i].Initialize(types[column_ids[i]]);
	}
}

bool IndexedRowGroup::InitializeScanWithOffset(IndexedCollectionScanState &state, idx_t vector_offset) {
	auto &column_ids = state.GetColumnIds();
	auto filters = state.GetFilters();
	if (filters) {
		if (!CheckZonemap(*filters, column_ids)) {
			return false;
		}
	}

	state.row_group = this;
	state.vector_index = vector_offset;
	state.max_row_group_row =
	    this->start > state.max_row ? 0 : MinValue<idx_t>(this->count, state.max_row - this->start);
	D_ASSERT(state.column_scans);
	for (idx_t i = 0; i < column_ids.size(); i++) {
		const auto &column = column_ids[i];
		if (column != COLUMN_IDENTIFIER_ROW_ID && column < relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) {
			auto &column_data = GetColumn(column);
			column_data.InitializeScanWithOffset(state.column_scans[i], start + vector_offset * STANDARD_VECTOR_SIZE);
		} else {
			state.column_scans[i].current = nullptr;
		}
	}
	return true;
}

bool IndexedRowGroup::InitializeScan(IndexedCollectionScanState &state) {
	auto &column_ids = state.GetColumnIds();
	auto filters = state.GetFilters();
	if (filters) {
		if (!CheckZonemap(*filters, column_ids)) {
			return false;
		}
	}
	state.row_group = this;
	state.vector_index = 0;
	state.max_row_group_row =
	    this->start > state.max_row ? 0 : MinValue<idx_t>(this->count, state.max_row - this->start);
	if (state.max_row_group_row == 0) {
		return false;
	}
	D_ASSERT(state.column_scans);
	for (idx_t i = 0; i < column_ids.size(); i++) {
		auto column = column_ids[i];
		if (column != COLUMN_IDENTIFIER_ROW_ID && column < relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) {
			auto &column_data = GetColumn(column);
			column_data.InitializeScan(state.column_scans[i]);
		} else {
			state.column_scans[i].current = nullptr;
		}
	}
	return true;
}

unique_ptr<IndexedRowGroup> IndexedRowGroup::AlterType(IndexedRowGroupCollection &new_collection,
                                                       const LogicalType &target_type, idx_t changed_idx,
                                                       ExpressionExecutor &executor,
                                                       IndexedCollectionScanState &scan_state, DataChunk &scan_chunk) {
	Verify();

	// construct a new column data for this type
	auto column_data = ColumnData::CreateColumn(GetBlockManager(), GetTableInfo(), changed_idx, start, target_type);

	ColumnAppendState append_state;
	column_data->InitializeAppend(append_state);

	// scan the original table, and fill the new column with the transformed value
	scan_state.Initialize(GetCollection().GetTypes());
	InitializeScan(scan_state);

	DataChunk append_chunk;
	vector<LogicalType> append_types;
	append_types.push_back(target_type);
	append_chunk.Initialize(Allocator::DefaultAllocator(), append_types);
	auto &append_vector = append_chunk.data[0];
	while (true) {
		// scan the table
		scan_chunk.Reset();
		ScanCommitted(scan_state, scan_chunk, TableScanType::TABLE_SCAN_COMMITTED_ROWS);
		if (scan_chunk.size() == 0) {
			break;
		}
		// execute the expression
		append_chunk.Reset();
		executor.ExecuteExpression(scan_chunk, append_vector);
		column_data->Append(append_state, append_vector, scan_chunk.size());
	}

	// set up the row_group based on this row_group
	auto row_group = make_uniq<IndexedRowGroup>(new_collection, this->start, this->count);
	row_group->version_info = GetOrCreateVersionInfoPtr();
	auto &cols = GetColumns();
	for (idx_t i = 0; i < cols.size(); i++) {
		if (i == changed_idx) {
			// this is the altered column: use the new column
			row_group->columns.push_back(std::move(column_data));
		} else {
			// this column was not altered: use the data directly
			row_group->columns.push_back(cols[i]);
		}
	}
	row_group->Verify();
	return row_group;
}

unique_ptr<IndexedRowGroup> IndexedRowGroup::AddColumn(IndexedRowGroupCollection &new_collection,
                                                       ColumnDefinition &new_column, ExpressionExecutor &executor,
                                                       Expression &default_value, Vector &result) {
	Verify();

	// construct a new column data for the new column
	auto added_column =
	    ColumnData::CreateColumn(GetBlockManager(), GetTableInfo(), GetColumnCount(), start, new_column.Type());

	idx_t rows_to_write = this->count;
	if (rows_to_write > 0) {
		DataChunk dummy_chunk;

		ColumnAppendState state;
		added_column->InitializeAppend(state);
		for (idx_t i = 0; i < rows_to_write; i += STANDARD_VECTOR_SIZE) {
			idx_t rows_in_this_vector = MinValue<idx_t>(rows_to_write - i, STANDARD_VECTOR_SIZE);
			dummy_chunk.SetCardinality(rows_in_this_vector);
			executor.ExecuteExpression(dummy_chunk, result);
			added_column->Append(state, result, rows_in_this_vector);
		}
	}

	// set up the row_group based on this row_group
	auto row_group = make_uniq<IndexedRowGroup>(new_collection, this->start, this->count);
	row_group->version_info = GetOrCreateVersionInfoPtr();
	row_group->columns = GetColumns();
	// now add the new column
	row_group->columns.push_back(std::move(added_column));

	row_group->Verify();
	return row_group;
}

unique_ptr<IndexedRowGroup> IndexedRowGroup::AddColumn(IndexedRowGroupCollection &new_collection, int &column_index,
                                                       LogicalType &new_type, Vector &result, int rows_to_write) {
	Verify();

	// construct a new column data for the new column
	auto added_column = ColumnData::CreateColumn(GetBlockManager(), GetTableInfo(), GetColumnCount(), start, new_type);

	// idx_t rows_to_write = this->count;
	if (rows_to_write > 0) {
		// DataChunk dummy_chunk;
		Vector cur_vector(result.GetType());
		idx_t offset = 0;
		ColumnAppendState state;
		added_column->InitializeAppend(state);
		for (idx_t i = 0; i < rows_to_write; i += STANDARD_VECTOR_SIZE) {
			idx_t rows_in_this_vector = MinValue<idx_t>(rows_to_write - i, STANDARD_VECTOR_SIZE);
			cur_vector.Slice(result, offset, offset + rows_in_this_vector);

			// dummy_chunk.SetCardinality(rows_in_this_vector);
			added_column->Append(state, cur_vector, rows_in_this_vector);
			offset += rows_in_this_vector;
		}
	}

	// set up the row_group based on this row_group
	auto row_group = make_uniq<IndexedRowGroup>(new_collection, this->start, this->count);
	row_group->version_info = GetOrCreateVersionInfoPtr();
	row_group->columns = GetColumns();
	// now add the new column
	row_group->columns.push_back(std::move(added_column));

	row_group->Verify();
	return row_group;
}

unique_ptr<IndexedRowGroup> IndexedRowGroup::RemoveColumn(IndexedRowGroupCollection &new_collection,
                                                          idx_t removed_column) {
	Verify();

	D_ASSERT(removed_column < columns.size());

	auto row_group = make_uniq<IndexedRowGroup>(new_collection, this->start, this->count);
	row_group->version_info = GetOrCreateVersionInfoPtr();
	// copy over all columns except for the removed one
	auto &cols = GetColumns();
	for (idx_t i = 0; i < cols.size(); i++) {
		if (i != removed_column) {
			row_group->columns.push_back(cols[i]);
		}
	}

	row_group->Verify();
	return row_group;
}

void IndexedRowGroup::CommitDrop() {
	for (idx_t column_idx = 0; column_idx < GetColumnCount(); column_idx++) {
		CommitDropColumn(column_idx);
	}
}

void IndexedRowGroup::CommitDropColumn(idx_t column_idx) {
	GetColumn(column_idx).CommitDropColumn();
}

void IndexedRowGroup::NextVector(IndexedCollectionScanState &state) {
	state.vector_index++;
	const auto &column_ids = state.GetColumnIds();
	for (idx_t i = 0; i < column_ids.size(); i++) {
		const auto &column = column_ids[i];
		if (column == COLUMN_IDENTIFIER_ROW_ID || column >= relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) {
			continue;
		}
		D_ASSERT(column < columns.size());
		GetColumn(column).Skip(state.column_scans[i]);
	}
}

bool IndexedRowGroup::CheckZonemap(TableFilterSet &filters, const vector<storage_t> &column_ids) {
	for (auto &entry : filters.filters) {
		auto column_index = entry.first;
		auto &filter = entry.second;
		const auto &base_column_index = column_ids[column_index];
		if (!GetColumn(base_column_index).CheckZonemap(*filter)) {
			return false;
		}
	}
	return true;
}

bool IndexedRowGroup::CheckZonemapSegments(IndexedCollectionScanState &state, idx_t &current_row,
                                           SelectionVector &valid_sel, idx_t &sel_count) {
	if (state.parent.zones) {
		auto &current_zones = *state.parent.zones;
		auto zone_id = (state.row_group->start + current_row) / STANDARD_VECTOR_SIZE;

		if ((state.row_group->start + current_row) % STANDARD_VECTOR_SIZE == 0) {
			auto zone_count = MinValue<idx_t>(state.parent.zones_sel->operator[](zone_id) * STANDARD_VECTOR_SIZE,
			                                  state.max_row_group_row - current_row);
			sel_count = 0;
			for (uint16_t i = 0; i < zone_count; i++) {
				valid_sel.set_index(sel_count, i);
				sel_count += current_zones[i + state.row_group->start + current_row];
			}
		} else {
			auto num_zone_1 =
			    MinValue<idx_t>(state.max_row_group_row - current_row,
			                    (zone_id + 1) * STANDARD_VECTOR_SIZE - current_row - state.row_group->start);
			auto zone_count_1 =
			    MinValue<idx_t>(state.parent.zones_sel->operator[](zone_id) * STANDARD_VECTOR_SIZE, num_zone_1);
			idx_t num_zone_2 = 0;
			idx_t zone_count_2 = 0;
			if (num_zone_1 < state.max_row_group_row - current_row) {
				num_zone_2 = MinValue<idx_t>(state.row_group->start + state.row_group->count -
				                                 (zone_id + 1) * STANDARD_VECTOR_SIZE,
				                             STANDARD_VECTOR_SIZE - num_zone_1);
				zone_count_2 =
				    MinValue<idx_t>(state.parent.zones_sel->operator[](zone_id + 1) * STANDARD_VECTOR_SIZE, num_zone_2);
			}

			sel_count = 0;
			for (uint16_t i = 0; i < zone_count_1; i++) {
				valid_sel.set_index(sel_count, i);
				sel_count += current_zones[i + state.row_group->start + current_row];
			}
			for (uint16_t i = num_zone_1; i < num_zone_1 + zone_count_2; i++) {
				valid_sel.set_index(sel_count, i);
				sel_count += current_zones[i + state.row_group->start + current_row];
			}
		}
	}

	auto &column_ids = state.GetColumnIds();
	auto filters = state.GetFilters();
	if (!filters) {
		return true;
	}
	for (auto &entry : filters->filters) {
		D_ASSERT(entry.first < column_ids.size());
		auto column_idx = entry.first;
		const auto &base_column_idx = column_ids[column_idx];
		bool read_segment = GetColumn(base_column_idx).CheckZonemap(state.column_scans[column_idx], *entry.second);
		if (!read_segment) {
			idx_t target_row =
			    state.column_scans[column_idx].current->start + state.column_scans[column_idx].current->count;
			D_ASSERT(target_row >= this->start);
			D_ASSERT(target_row <= this->start + this->count);
			idx_t target_vector_index = (target_row - this->start) / STANDARD_VECTOR_SIZE;
			if (state.vector_index == target_vector_index) {
				// we can't skip any full vectors because this segment contains less
				// than a full vector for now we just bail-out
				// FIXME: we could check if we can ALSO skip the next segments, in which
				// case skipping a full vector might be possible we don't care that much
				// though, since a single segment that fits less than a full vector is
				// exceedingly rare
				return true;
			}
			while (state.vector_index < target_vector_index) {
				NextVector(state);
			}
			return false;
		}
	}

	return true;
}

// template <class T>
// idx_t inline IndexedRowGroup::LookupRows(
//     shared_ptr<ColumnData> column, const shared_ptr<std::vector<row_t>>
//     &rowids, Vector &result, idx_t offset, idx_t count, idx_t type_size) {
//   std::cout << "lookup rows" << std::endl;
//   D_ASSERT(1 == 0);
//   auto result_data = FlatVector::GetData(result);
//   idx_t new_num = 0;
//   idx_t s_size = this->GetCollection().GetRowGroup(0)->count;
//   for (idx_t i = 0; i < count; i++) {
//     row_t row_id = rowids->operator[](i + offset);
//     if (row_id >= this->start + this->count) {
//       return new_num;
//     }
//     idx_t s_offset = row_id - this->start;
//     idx_t vector_index = s_offset / STANDARD_VECTOR_SIZE;
//     idx_t id_in_vector = s_offset - vector_index * STANDARD_VECTOR_SIZE;

//     auto transient_segment =
//         (ColumnSegment *)column->data.GetSegment(vector_index);
//     auto s_data = transient_segment->block->buffer->buffer +
//                   ValidityMask::STANDARD_MASK_SIZE;
//     memcpy(result_data + (i * type_size), s_data + (id_in_vector *
//     type_size),
//            type_size);
//     new_num++;
//   }

//   return new_num;
// }

// idx_t IndexedRowGroup::PerformLookups(TransactionData transaction,
//                                       IndexedCollectionScanState &state,
//                                       DataChunk &result,
//                                       shared_ptr<std::vector<row_t>> &rowids,
//                                       idx_t offset, idx_t &new_num,
//                                       idx_t size) {
//   for (idx_t col_idx = 0; col_idx < state.parent.column_ids.size();
//   col_idx++) {
//     auto cid = state.parent.column_ids[col_idx];
//     if (cid == COLUMN_IDENTIFIER_ROW_ID) {
//       assert(result.data[col_idx].GetType() == LogicalType::BIGINT);
//       result.data[col_idx].SetVectorType(VectorType::FLAT_VECTOR);
//       auto data = FlatVector::GetData(result.data[col_idx]);
//       memcpy(data, (data_ptr_t)rowids->data() + (offset * sizeof(int64_t)),
//              size * sizeof(int64_t));
//       continue;
//     } else if (cid >= COLUMN_IDENTIFIER_GRAPH_INDEX_ID) {
//       idx_t rai_index = (cid - COLUMN_IDENTIFIER_GRAPH_INDEX_ID) / 2;
//       idx_t rai_direction = (cid - COLUMN_IDENTIFIER_GRAPH_INDEX_ID) % 2;
//       result.data[col_idx].SetVectorType(VectorType::FLAT_VECTOR);
//       auto data = FlatVector::GetData<row_t>(result.data[col_idx]);

//       auto &table_rai_index =
//       this->GetTableInfo().indexes.Indexes()[rai_index]; auto table_rai =
//           dynamic_cast<PhysicalGraphIndex *>(table_rai_index.get());

//       for (idx_t i = 0; i < size; i++) {
//         row_t row_id = rowids->operator[](i + offset);
//         if (rai_direction == 0) {
//           data[i] = table_rai->graph_index->alist->compact_edge_forward_list
//                         .vertices.get()[row_id];
//         } else {
//           data[i] = table_rai->graph_index->alist->compact_edge_backward_list
//                         .vertices.get()[row_id];
//         }
//       }
//       continue;
//     }

//     auto col = columns[cid];
//     if (col->type == LogicalType::TINYINT) {
//       new_num = LookupRows<int8_t>(col, state.parent.rowids,
//                                    result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::TINYINT) {
//       new_num = LookupRows<int8_t>(col, state.parent.rowids,
//                                    result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::UTINYINT) {
//       new_num = LookupRows<uint8_t>(col, state.parent.rowids,
//                                     result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::SMALLINT) {
//       new_num = LookupRows<int16_t>(col, state.parent.rowids,
//                                     result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::HASH ||
//                col->type == LogicalType::USMALLINT) {
//       new_num = LookupRows<uint16_t>(col, state.parent.rowids,
//                                      result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::INTEGER) {
//       new_num = LookupRows<int32_t>(col, state.parent.rowids,
//                                     result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::UINTEGER) {
//       new_num = LookupRows<uint32_t>(col, state.parent.rowids,
//                                      result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::TIMESTAMP ||
//                col->type == LogicalType::BIGINT) {
//       new_num = LookupRows<int64_t>(col, state.parent.rowids,
//                                     result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::UBIGINT) {
//       new_num = LookupRows<uint64_t>(col, state.parent.rowids,
//                                      result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::FLOAT) {
//       new_num = LookupRows<float_t>(col, state.parent.rowids,
//                                     result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::DOUBLE) {
//       new_num = LookupRows<double_t>(col, state.parent.rowids,
//                                      result.data[col_idx], offset, size);
//     } else if (col->type == LogicalType::POINTER) {
//       new_num = LookupRows<uintptr_t>(col, state.parent.rowids,
//                                       result.data[col_idx], offset, size);
//     } else {
//       // for (idx_t i = 0; i < size; i++) {
//       //     this->GetColumn(cid).FetchRow(transaction, state,
//       //     rowids->operator[](i + offset), result.data[col_idx], i);
//       // }
//     }
//   }

//   return new_num;
// }

template <TableScanType TYPE>
void IndexedRowGroup::TemplatedScan(TransactionData transaction, IndexedCollectionScanState &state, DataChunk &result) {
	const bool ALLOW_UPDATES = TYPE != TableScanType::TABLE_SCAN_COMMITTED_ROWS_DISALLOW_UPDATES &&
	                           TYPE != TableScanType::TABLE_SCAN_COMMITTED_ROWS_OMIT_PERMANENTLY_DELETED;
	auto table_filters = state.GetFilters();
	const auto &column_ids = state.GetColumnIds();
	auto adaptive_filter = state.GetAdaptiveFilter();

	while (true) {
		if (state.vector_index * STANDARD_VECTOR_SIZE >= state.max_row_group_row) {
			// exceeded the amount of rows to scan
			return;
		}

		// perform rowid lookups in a rowgroup
		if (state.parent.rows_count >= 0) {
			auto scan_count = state.parent.rows_count - state.parent.rows_offset;
			idx_t new_num = 0;
			std::cout << "visited parent in indexed_row_group.cpp" << std::endl;

			D_ASSERT(1 == 0);
			// PerformLookups(transaction, state, result, state.parent.rowids,
			//                state.parent.rows_offset, new_num, scan_count);
			result.SetCardinality(new_num);
			state.parent.rows_offset += new_num;
			return;
		}

		idx_t current_row = state.vector_index * STANDARD_VECTOR_SIZE;
		auto max_count = MinValue<idx_t>(STANDARD_VECTOR_SIZE, state.max_row_group_row - current_row);

		SelectionVector valid_sel(STANDARD_VECTOR_SIZE);
		idx_t sel_count = max_count;

		//! first check the zonemap if we have to scan this partition
		if (!CheckZonemapSegments(state, current_row, valid_sel, sel_count)) {
			continue;
		}

		// if (state.parent.zones)
		//    std::cout << "row group: " << sel_count << std::endl;
		// if (sel_count == 46)
		//    int stop_here = 0;
		// second, scan the version chunk manager to figure out which tuples to load
		// for this transaction
		idx_t count;
		if (TYPE == TableScanType::TABLE_SCAN_REGULAR) {
			count = state.row_group->GetSelVector(transaction, state.vector_index, valid_sel, sel_count);
			if (count == 0) {
				// nothing to scan for this vector, skip the entire vector
				NextVector(state);
				continue;
			}
		} else if (TYPE == TableScanType::TABLE_SCAN_COMMITTED_ROWS_OMIT_PERMANENTLY_DELETED) {
			count = state.row_group->GetCommittedSelVector(transaction.start_time, transaction.transaction_id,
			                                               state.vector_index, valid_sel, sel_count);
			if (count == 0) {
				// nothing to scan for this vector, skip the entire vector
				NextVector(state);
				continue;
			}
		} else {
			count = sel_count;
		}

		if (count == max_count && !table_filters) {
			// scan all vectors completely: full scan without deletions or table
			// filters
			for (idx_t i = 0; i < column_ids.size(); i++) {
				const auto &column = column_ids[i];
				if (column == COLUMN_IDENTIFIER_ROW_ID) {
					// scan row id
					D_ASSERT(result.data[i].GetType().InternalType() == ROW_TYPE);
					result.data[i].Sequence(this->start + current_row, 1, count);
				} else if (column >= relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) {
					idx_t rai_index = (column - relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) / 2;
					idx_t rai_direction = (column - relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) % 2;
					idx_t current_index = this->start + current_row;
					result.data[i].SetVectorType(VectorType::FLAT_VECTOR);
					auto r0data = (int64_t *)FlatVector::GetData(result.data[i]);

					auto &table_rai_index = state.parent.storage_info->indexes.Indexes()[rai_index];
					// auto &table_rai_index = this->GetTableInfo().indexes.Indexes()[rai_index];
					auto table_rai = dynamic_cast<PhysicalGraphIndex *>(table_rai_index.get());

					if (rai_direction == 0) {
						memcpy(r0data,
						       table_rai->graph_index->alist->compact_edge_forward_list.vertices.get() + current_index,
						       count * sizeof(int64_t));
					} else {
						memcpy(r0data,
						       (int64_t *)(table_rai->graph_index->alist->compact_edge_backward_list.vertices.get()) +
						           current_index,
						       count * sizeof(int64_t));
					}
				} else {
					auto &col_data = GetColumn(column);
					if (TYPE != TableScanType::TABLE_SCAN_REGULAR) {
						col_data.ScanCommitted(state.vector_index, state.column_scans[i], result.data[i],
						                       ALLOW_UPDATES);
					} else {
						col_data.Scan(transaction, state.vector_index, state.column_scans[i], result.data[i]);
					}
				}
			}
		} else {
			// partial scan: we have deletions or table filters
			idx_t approved_tuple_count = count;
			SelectionVector sel;
			if (count != max_count) {
				sel.Initialize(valid_sel);
			} else {
				sel.Initialize(nullptr);
			}
			//! first, we scan the columns with filters, fetch their data and generate
			//! a selection vector. get runtime statistics

			auto start_time = high_resolution_clock::now();
			if (table_filters) {
				D_ASSERT(adaptive_filter);
				D_ASSERT(ALLOW_UPDATES);
				for (idx_t i = 0; i < table_filters->filters.size(); i++) {
					auto tf_idx = adaptive_filter->permutation[i];
					auto col_idx = column_ids[tf_idx];
					auto &col_data = GetColumn(col_idx);
					col_data.Select(transaction, state.vector_index, state.column_scans[tf_idx], result.data[tf_idx],
					                sel, approved_tuple_count, *table_filters->filters[tf_idx]);
				}
				for (auto &table_filter : table_filters->filters) {
					result.data[table_filter.first].Slice(sel, approved_tuple_count);
				}
			}
			if (approved_tuple_count == 0) {
				// all rows were filtered out by the table filters
				// skip this vector in all the scans that were not scanned yet
				D_ASSERT(table_filters);
				result.Reset();
				for (idx_t i = 0; i < column_ids.size(); i++) {
					auto col_idx = column_ids[i];
					if (col_idx == COLUMN_IDENTIFIER_ROW_ID || col_idx >= relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) {
						continue;
					}
					if (table_filters->filters.find(i) == table_filters->filters.end()) {
						auto &col_data = GetColumn(col_idx);
						col_data.Skip(state.column_scans[i]);
					}
				}
				state.vector_index++;
				continue;
			}
			//! Now we use the selection vector to fetch data for the other columns.
			for (idx_t i = 0; i < column_ids.size(); i++) {
				if (!table_filters || table_filters->filters.find(i) == table_filters->filters.end()) {
					auto column = column_ids[i];
					if (column == COLUMN_IDENTIFIER_ROW_ID) {
						D_ASSERT(result.data[i].GetType().InternalType() == PhysicalType::INT64);
						result.data[i].SetVectorType(VectorType::FLAT_VECTOR);
						auto result_data = FlatVector::GetData<int64_t>(result.data[i]);
						for (size_t sel_idx = 0; sel_idx < approved_tuple_count; sel_idx++) {
							result_data[sel_idx] = this->start + current_row + sel.get_index(sel_idx);
						}
					} else if (column >= relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) {
						idx_t rai_index = (column - relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) / 2;
						idx_t rai_direction = (column - relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) % 2;
						result.data[i].SetVectorType(VectorType::FLAT_VECTOR);
						auto result_data = FlatVector::GetData<int64_t>(result.data[i]);

						auto &table_rai_index = state.parent.storage_info->indexes.Indexes()[rai_index];
						auto table_rai = dynamic_cast<PhysicalGraphIndex *>(table_rai_index.get());

						for (size_t sel_idx = 0; sel_idx < approved_tuple_count; sel_idx++) {
							idx_t current_index = this->start + current_row + sel.get_index(sel_idx);
							if (rai_direction == 0) {
								result_data[sel_idx] = table_rai->graph_index->alist->compact_edge_forward_list.vertices
								                           .get()[current_index];
							} else {
								result_data[sel_idx] = table_rai->graph_index->alist->compact_edge_backward_list
								                           .vertices.get()[current_index];
							}
						}
					} else {
						auto &col_data = GetColumn(column);
						if (TYPE == TableScanType::TABLE_SCAN_REGULAR) {
							// result.data[i].SetVectorType(VectorType::DICTIONARY_VECTOR);
							col_data.FilterScan(transaction, state.vector_index, state.column_scans[i], result.data[i],
							                    sel, approved_tuple_count);
						} else {
							col_data.FilterScanCommitted(state.vector_index, state.column_scans[i], result.data[i], sel,
							                             approved_tuple_count, ALLOW_UPDATES);
						}
					}
				}
			}

			auto end_time = high_resolution_clock::now();
			if (adaptive_filter && table_filters->filters.size() > 1) {
				adaptive_filter->AdaptRuntimeStatistics(duration_cast<duration<double>>(end_time - start_time).count());
			}
			D_ASSERT(approved_tuple_count > 0);
			count = approved_tuple_count;
		}

		result.SetCardinality(count);
		state.vector_index++;
		break;
	}
}

void IndexedRowGroup::Scan(TransactionData transaction, IndexedCollectionScanState &state, DataChunk &result) {
	TemplatedScan<TableScanType::TABLE_SCAN_REGULAR>(transaction, state, result);
}

void IndexedRowGroup::ScanCommitted(IndexedCollectionScanState &state, DataChunk &result, TableScanType type) {
	auto &transaction_manager = DuckTransactionManager::Get(GetCollection().GetAttached());

	auto lowest_active_start = transaction_manager.LowestActiveStart();
	auto lowest_active_id = transaction_manager.LowestActiveId();
	TransactionData data(lowest_active_id, lowest_active_start);
	switch (type) {
	case TableScanType::TABLE_SCAN_COMMITTED_ROWS:
		TemplatedScan<TableScanType::TABLE_SCAN_COMMITTED_ROWS>(data, state, result);
		break;
	case TableScanType::TABLE_SCAN_COMMITTED_ROWS_DISALLOW_UPDATES:
		TemplatedScan<TableScanType::TABLE_SCAN_COMMITTED_ROWS_DISALLOW_UPDATES>(data, state, result);
		break;
	case TableScanType::TABLE_SCAN_COMMITTED_ROWS_OMIT_PERMANENTLY_DELETED:
		TemplatedScan<TableScanType::TABLE_SCAN_COMMITTED_ROWS_OMIT_PERMANENTLY_DELETED>(data, state, result);
		break;
	default:
		throw InternalException("Unrecognized table scan type");
	}
}

shared_ptr<RowVersionManager> &IndexedRowGroup::GetVersionInfo() {
	if (!HasUnloadedDeletes()) {
		// deletes are loaded - return the version info
		return version_info;
	}
	lock_guard<mutex> lock(row_group_lock);
	// double-check after obtaining the lock whether or not deletes are still not
	// loaded to avoid double load
	if (HasUnloadedDeletes()) {
		// deletes are not loaded - reload
		auto root_delete = deletes_pointers[0];
		version_info = RowVersionManager::Deserialize(root_delete, GetBlockManager().GetMetadataManager(), start);
		deletes_is_loaded = true;
	}
	return version_info;
}

shared_ptr<RowVersionManager> &IndexedRowGroup::GetOrCreateVersionInfoPtr() {
	auto vinfo = GetVersionInfo();
	if (!vinfo) {
		lock_guard<mutex> lock(row_group_lock);
		if (!version_info) {
			version_info = make_shared<RowVersionManager>(start);
		}
	}
	return version_info;
}

RowVersionManager &IndexedRowGroup::GetOrCreateVersionInfo() {
	return *GetOrCreateVersionInfoPtr();
}

idx_t IndexedRowGroup::GetSelVector(TransactionData transaction, idx_t vector_idx, SelectionVector &sel_vector,
                                    idx_t max_count) {
	auto &vinfo = GetVersionInfo();
	if (!vinfo) {
		return max_count;
	}
	return vinfo->GetSelVector(transaction, vector_idx, sel_vector, max_count);
}

idx_t IndexedRowGroup::GetCommittedSelVector(transaction_t start_time, transaction_t transaction_id, idx_t vector_idx,
                                             SelectionVector &sel_vector, idx_t max_count) {
	auto &vinfo = GetVersionInfo();
	if (!vinfo) {
		return max_count;
	}
	return vinfo->GetCommittedSelVector(start_time, transaction_id, vector_idx, sel_vector, max_count);
}

bool IndexedRowGroup::Fetch(TransactionData transaction, idx_t row) {
	D_ASSERT(row < this->count);
	auto &vinfo = GetVersionInfo();
	if (!vinfo) {
		return true;
	}
	return vinfo->Fetch(transaction, row);
}

void IndexedRowGroup::FetchRow(TransactionData transaction, ColumnFetchState &state, const vector<column_t> &column_ids,
                               row_t row_id, DataChunk &result, idx_t result_idx) {
	for (idx_t col_idx = 0; col_idx < column_ids.size(); col_idx++) {
		auto column = column_ids[col_idx];
		if (column == COLUMN_IDENTIFIER_ROW_ID) {
			// row id column: fill in the row ids
			D_ASSERT(result.data[col_idx].GetType().InternalType() == PhysicalType::INT64);
			result.data[col_idx].SetVectorType(VectorType::FLAT_VECTOR);
			auto data = FlatVector::GetData<row_t>(result.data[col_idx]);
			data[result_idx] = row_id;
		} else if (column >= relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) {
			idx_t rai_index = (column - relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) / 2;
			idx_t rai_direction = (column - relgo::COLUMN_IDENTIFIER_GRAPH_INDEX_ID) % 2;
			result.data[col_idx].SetVectorType(VectorType::FLAT_VECTOR);
			auto data = FlatVector::GetData<row_t>(result.data[col_idx]);

			auto &table_rai_index = this->GetTableInfo().indexes.Indexes()[rai_index];
			auto table_rai = dynamic_cast<PhysicalGraphIndex *>(table_rai_index.get());

			if (rai_direction == 0) {
				data[result_idx] = table_rai->graph_index->alist->compact_edge_forward_list.vertices.get()[row_id];
			} else {
				data[result_idx] = table_rai->graph_index->alist->compact_edge_backward_list.vertices.get()[row_id];
			}
		} else {
			// regular column: fetch data from the base column
			auto &col_data = GetColumn(column);
			col_data.FetchRow(transaction, state, row_id, result.data[col_idx], result_idx);
		}
	}
}

void IndexedRowGroup::AppendVersionInfo(TransactionData transaction, idx_t count) {
	idx_t row_group_start = this->count.load();
	idx_t row_group_end = row_group_start + count;
	if (row_group_end > Storage::ROW_GROUP_SIZE) {
		row_group_end = Storage::ROW_GROUP_SIZE;
	}
	// create the version_info if it doesn't exist yet
	auto &vinfo = GetOrCreateVersionInfo();
	vinfo.AppendVersionInfo(transaction, count, row_group_start, row_group_end);
	this->count = row_group_end;
}

void IndexedRowGroup::CommitAppend(transaction_t commit_id, idx_t row_group_start, idx_t count) {
	auto &vinfo = GetOrCreateVersionInfo();
	vinfo.CommitAppend(commit_id, row_group_start, count);
}

void IndexedRowGroup::RevertAppend(idx_t row_group_start) {
	auto &vinfo = GetOrCreateVersionInfo();
	vinfo.RevertAppend(row_group_start - this->start);
	for (auto &column : columns) {
		column->RevertAppend(row_group_start);
	}
	this->count = MinValue<idx_t>(row_group_start - this->start, this->count);
	Verify();
}

void IndexedRowGroup::InitializeAppend(IndexedRowGroupAppendState &append_state) {
	append_state.row_group = this;
	append_state.offset_in_row_group = this->count;
	// for each column, initialize the append state
	append_state.states = make_unsafe_uniq_array<ColumnAppendState>(GetColumnCount());
	for (idx_t i = 0; i < GetColumnCount(); i++) {
		auto &col_data = GetColumn(i);
		col_data.InitializeAppend(append_state.states[i]);
	}
}

void IndexedRowGroup::Append(IndexedRowGroupAppendState &state, DataChunk &chunk, idx_t append_count) {
	// append to the current row_group
	for (idx_t i = 0; i < GetColumnCount(); i++) {
		auto &col_data = GetColumn(i);
		col_data.Append(state.states[i], chunk.data[i], append_count);
	}
	state.offset_in_row_group += append_count;
}

void IndexedRowGroup::Update(TransactionData transaction, DataChunk &update_chunk, row_t *ids, idx_t offset,
                             idx_t count, const vector<PhysicalIndex> &column_ids) {
#ifdef DEBUG
	for (size_t i = offset; i < offset + count; i++) {
		D_ASSERT(ids[i] >= row_t(this->start) && ids[i] < row_t(this->start + this->count));
	}
#endif
	for (idx_t i = 0; i < column_ids.size(); i++) {
		auto column = column_ids[i];
		D_ASSERT(column.index != COLUMN_IDENTIFIER_ROW_ID);
		auto &col_data = GetColumn(column.index);
		D_ASSERT(col_data.type.id() == update_chunk.data[i].GetType().id());
		if (offset > 0) {
			Vector sliced_vector(update_chunk.data[i], offset, offset + count);
			sliced_vector.Flatten(count);
			col_data.Update(transaction, column.index, sliced_vector, ids + offset, count);
		} else {
			col_data.Update(transaction, column.index, update_chunk.data[i], ids, count);
		}
		MergeStatistics(column.index, *col_data.GetUpdateStatistics());
	}
}

void IndexedRowGroup::UpdateColumn(TransactionData transaction, DataChunk &updates, Vector &row_ids,
                                   const vector<column_t> &column_path) {
	D_ASSERT(updates.ColumnCount() == 1);
	auto ids = FlatVector::GetData<row_t>(row_ids);

	auto primary_column_idx = column_path[0];
	D_ASSERT(primary_column_idx != COLUMN_IDENTIFIER_ROW_ID);
	D_ASSERT(primary_column_idx < columns.size());
	auto &col_data = GetColumn(primary_column_idx);
	col_data.UpdateColumn(transaction, column_path, updates.data[0], ids, updates.size(), 1);
	MergeStatistics(primary_column_idx, *col_data.GetUpdateStatistics());
}

unique_ptr<BaseStatistics> IndexedRowGroup::GetStatistics(idx_t column_idx) {
	auto &col_data = GetColumn(column_idx);
	lock_guard<mutex> slock(stats_lock);
	return col_data.GetStatistics();
}

void IndexedRowGroup::MergeStatistics(idx_t column_idx, const BaseStatistics &other) {
	auto &col_data = GetColumn(column_idx);
	lock_guard<mutex> slock(stats_lock);
	col_data.MergeStatistics(other);
}

void IndexedRowGroup::MergeIntoStatistics(idx_t column_idx, BaseStatistics &other) {
	auto &col_data = GetColumn(column_idx);
	lock_guard<mutex> slock(stats_lock);
	col_data.MergeIntoStatistics(other);
}

IndexedRowGroupWriteData IndexedRowGroup::WriteToDisk(PartialBlockManager &manager,
                                                      const vector<CompressionType> &compression_types) {
	IndexedRowGroupWriteData result;
	result.states.reserve(columns.size());
	result.statistics.reserve(columns.size());

	// Checkpoint the individual columns of the row group
	// Here we're iterating over columns. Each column can have multiple segments.
	// (Some columns will be wider than others, and require different numbers
	// of blocks to encode.) Segments cannot span blocks.
	//
	// Some of these columns are composite (list, struct). The data is written
	// first sequentially, and the pointers are written later, so that the
	// pointers all end up densely packed, and thus more cache-friendly.
	for (idx_t column_idx = 0; column_idx < GetColumnCount(); column_idx++) {
		auto &column = GetColumn(column_idx);
		ColumnCheckpointInfo checkpoint_info {compression_types[column_idx]};
		// auto checkpoint_state = column.Checkpoint(*this, manager,
		// checkpoint_info); D_ASSERT(checkpoint_state);

		// auto stats = checkpoint_state->GetStatistics();
		// D_ASSERT(stats);

		// result.statistics.push_back(stats->Copy());
		// result.states.push_back(std::move(checkpoint_state));
	}
	D_ASSERT(result.states.size() == result.statistics.size());
	return result;
}

bool IndexedRowGroup::AllDeleted() {
	if (HasUnloadedDeletes()) {
		// deletes aren't loaded yet - we know not everything is deleted
		return false;
	}
	auto &vinfo = GetVersionInfo();
	if (!vinfo) {
		return false;
	}
	return vinfo->GetCommittedDeletedCount(count) == count;
}

bool IndexedRowGroup::HasUnloadedDeletes() const {
	if (deletes_pointers.empty()) {
		// no stored deletes at all
		return false;
	}
	// return whether or not the deletes have been loaded
	return !deletes_is_loaded;
}

RowGroupPointer IndexedRowGroup::Checkpoint(RowGroupWriter &writer, TableStatistics &global_stats) {
	RowGroupPointer row_group_pointer;

	vector<CompressionType> compression_types;
	compression_types.reserve(columns.size());
	for (idx_t column_idx = 0; column_idx < GetColumnCount(); column_idx++) {
		compression_types.push_back(writer.GetColumnCompressionType(column_idx));
	}
	auto result = WriteToDisk(writer.GetPartialBlockManager(), compression_types);
	for (idx_t column_idx = 0; column_idx < GetColumnCount(); column_idx++) {
		global_stats.GetStats(column_idx).Statistics().Merge(result.statistics[column_idx]);
	}

	// construct the row group pointer and write the column meta data to disk
	D_ASSERT(result.states.size() == columns.size());
	row_group_pointer.row_start = start;
	row_group_pointer.tuple_count = count;
	for (auto &state : result.states) {
		// get the current position of the table data writer
		auto &data_writer = writer.GetPayloadWriter();
		auto pointer = data_writer.GetMetaBlockPointer();

		// store the stats and the data pointers in the row group pointers
		row_group_pointer.data_pointers.push_back(pointer);

		// Write pointers to the column segments.
		//
		// Just as above, the state can refer to many other states, so this
		// can cascade recursively into more pointer writes.
		BinarySerializer serializer(data_writer);
		serializer.Begin();
		state->WriteDataPointers(writer, serializer);
		serializer.End();
	}
	row_group_pointer.deletes_pointers = CheckpointDeletes(writer.GetPayloadWriter().GetManager());
	Verify();
	return row_group_pointer;
}

vector<MetaBlockPointer> IndexedRowGroup::CheckpointDeletes(MetadataManager &manager) {
	if (HasUnloadedDeletes()) {
		// deletes were not loaded so they cannot be changed
		// re-use them as-is
		manager.ClearModifiedBlocks(deletes_pointers);
		return deletes_pointers;
	}
	if (!version_info) {
		// no version information: write nothing
		return vector<MetaBlockPointer>();
	}
	return version_info->Checkpoint(manager);
}

void IndexedRowGroup::Serialize(RowGroupPointer &pointer, Serializer &serializer) {
	serializer.WriteProperty(100, "row_start", pointer.row_start);
	serializer.WriteProperty(101, "tuple_count", pointer.tuple_count);
	serializer.WriteProperty(102, "data_pointers", pointer.data_pointers);
	serializer.WriteProperty(103, "delete_pointers", pointer.deletes_pointers);
}

RowGroupPointer IndexedRowGroup::Deserialize(Deserializer &deserializer) {
	RowGroupPointer result;
	result.row_start = deserializer.ReadProperty<uint64_t>(100, "row_start");
	result.tuple_count = deserializer.ReadProperty<uint64_t>(101, "tuple_count");
	result.data_pointers = deserializer.ReadProperty<vector<MetaBlockPointer>>(102, "data_pointers");
	result.deletes_pointers = deserializer.ReadProperty<vector<MetaBlockPointer>>(103, "delete_pointers");
	return result;
}

//===--------------------------------------------------------------------===//
// GetColumnSegmentInfo
//===--------------------------------------------------------------------===//
void IndexedRowGroup::GetColumnSegmentInfo(idx_t row_group_index, vector<ColumnSegmentInfo> &result) {
	for (idx_t col_idx = 0; col_idx < GetColumnCount(); col_idx++) {
		auto &col_data = GetColumn(col_idx);
		col_data.GetColumnSegmentInfo(row_group_index, {col_idx}, result);
	}
}

//===--------------------------------------------------------------------===//
// Version Delete Information
//===--------------------------------------------------------------------===//
class IndexedVersionDeleteState {
public:
	IndexedVersionDeleteState(IndexedRowGroup &info, TransactionData transaction, DataTable &table, idx_t base_row)
	    : info(info), transaction(transaction), table(table), current_chunk(DConstants::INVALID_INDEX), count(0),
	      base_row(base_row), delete_count(0) {
	}

	IndexedRowGroup &info;
	TransactionData transaction;
	DataTable &table;
	idx_t current_chunk;
	row_t rows[STANDARD_VECTOR_SIZE];
	idx_t count;
	idx_t base_row;
	idx_t chunk_row;
	idx_t delete_count;

public:
	void Delete(row_t row_id);
	void Flush();
};

idx_t IndexedRowGroup::Delete(TransactionData transaction, DataTable &table, row_t *ids, idx_t count) {
	IndexedVersionDeleteState del_state(*this, transaction, table, this->start);

	// obtain a write lock
	for (idx_t i = 0; i < count; i++) {
		D_ASSERT(ids[i] >= 0);
		D_ASSERT(idx_t(ids[i]) >= this->start && idx_t(ids[i]) < this->start + this->count);
		del_state.Delete(ids[i] - this->start);
	}
	del_state.Flush();
	return del_state.delete_count;
}

void IndexedRowGroup::Verify() {
#ifdef DEBUG
	for (auto &column : GetColumns()) {
		// column->Verify(*this);
	}
#endif
}

idx_t IndexedRowGroup::DeleteRows(idx_t vector_idx, transaction_t transaction_id, row_t rows[], idx_t count) {
	return GetOrCreateVersionInfo().DeleteRows(vector_idx, transaction_id, rows, count);
}

void IndexedVersionDeleteState::Delete(row_t row_id) {
	D_ASSERT(row_id >= 0);
	idx_t vector_idx = row_id / STANDARD_VECTOR_SIZE;
	idx_t idx_in_vector = row_id - vector_idx * STANDARD_VECTOR_SIZE;
	if (current_chunk != vector_idx) {
		Flush();

		current_chunk = vector_idx;
		chunk_row = vector_idx * STANDARD_VECTOR_SIZE;
	}
	rows[count++] = idx_in_vector;
}

void IndexedVersionDeleteState::Flush() {
	if (count == 0) {
		return;
	}
	// it is possible for delete statements to delete the same tuple multiple
	// times when combined with a USING clause in the current_info->Delete, we
	// check which tuples are actually deleted (excluding duplicate deletions)
	// this is returned in the actual_delete_count
	auto actual_delete_count = info.DeleteRows(current_chunk, transaction.transaction_id, rows, count);
	delete_count += actual_delete_count;
	if (transaction.transaction && actual_delete_count > 0) {
		// now push the delete into the undo buffer, but only if any deletes were
		// actually performed
		transaction.transaction->PushDelete(table, info.GetOrCreateVersionInfo(), current_chunk, rows,
		                                    actual_delete_count, base_row + chunk_row);
	}
	count = 0;
}

} // namespace duckdb
