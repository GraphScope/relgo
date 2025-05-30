//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parser/statement/pgq_statement.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/unordered_map.hpp"
#include "duckdb/parser/parsed_expression.hpp"
#include "duckdb/parser/query_node.hpp"
#include "duckdb/parser/sql_statement.hpp"
#include "duckdb/parser/tableref.hpp"

namespace duckdb {

class QueryNode;
class Serializer;
class Deserializer;

//! SelectStatement is a typical SELECT clause
class RelGoStatement : public SQLStatement {
public:
	static constexpr const StatementType TYPE = StatementType::CALL_STATEMENT;

public:
	RelGoStatement() : SQLStatement(StatementType::CALL_STATEMENT) {
	}

	//! The main query node
	unique_ptr<QueryNode> node;

protected:
	RelGoStatement(const RelGoStatement &other);

public:
	//! Convert the SELECT statement to a string
	DUCKDB_API string ToString() const override;
	//! Create a copy of this SelectStatement
	DUCKDB_API unique_ptr<SQLStatement> Copy() const override;
	//! Whether or not the statements are equivalent
	bool Equals(const SQLStatement &other) const;

	void Serialize(Serializer &serializer) const;
	static unique_ptr<RelGoStatement> Deserialize(Deserializer &deserializer);
};
} // namespace duckdb
