//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/planner/operator/logical_create_rai.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "create_rai_info.hpp"
#include "duckdb/catalog/catalog_entry/table_catalog_entry.hpp"
#include "duckdb/planner/logical_operator.hpp"

namespace duckdb {

class LogicalCreateRAI : public LogicalOperator {
public:
	static constexpr const LogicalOperatorType TYPE = LogicalOperatorType::LOGICAL_CREATE_RAI_INDEX;

public:
	LogicalCreateRAI(string name, TableCatalogEntry &table, relgo::GraphIndexDirection rai_direction,
	                 vector<column_t> column_ids, vector<TableCatalogEntry *> referenced_tables,
	                 vector<column_t> referenced_columns)
	    : LogicalOperator(LogicalOperatorType::LOGICAL_CREATE_RAI_INDEX), name(name), table(table),
	      rai_direction(rai_direction), column_ids(column_ids), referenced_tables(referenced_tables),
	      referenced_columns(referenced_columns) {
	}

	string name;
	TableCatalogEntry &table;
	relgo::GraphIndexDirection rai_direction;
	vector<column_t> column_ids;
	vector<TableCatalogEntry *> referenced_tables;
	vector<column_t> referenced_columns;

protected:
	void ResolveTypes() override {
		types.push_back(LogicalType::BIGINT);
	}
};
} // namespace duckdb
