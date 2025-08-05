#pragma once

#include "types.h"
#include <string>
#include <vector>
#include <memory>
#include <variant>

using namespace std;

namespace db
{
    // Forward declaration - tells compiler StorageEngine exists
    class StorageEngine;

    // Query AST (Abstract Syntax Tree) nodes
    // These represent parsed SQL commands as tree structures

    // Base class for all query nodes
    struct QueryNode
    {
        virtual ~QueryNode() = default; // Virtual destructor for polymorphism
    };

    // SELECT statement representation: SELECT columns FROM table WHERE condition
    struct SelectNode : public QueryNode
    {
        vector<string> columns; // Column names to select (empty = SELECT *)
        string table_name;      // Which table to select from
        string where_column;    // Column name in WHERE clause
        Value where_value;      // Value to compare against in WHERE
        bool has_where;         // Does this query have a WHERE clause?

        SelectNode() : has_where(false) {} // Default: no WHERE clause
    };

    // INSERT statement representation: INSERT INTO table VALUES (...)
    struct InsertNode : public QueryNode
    {
        string table_name;    // Which table to insert into
        vector<Value> values; // The values to insert (one per column)
    };

    // UPDATE statement representation: UPDATE table SET col=val WHERE condition
    struct UpdateNode : public QueryNode
    {
        string table_name;                      // Which table to update
        vector<pair<string, Value>> set_values; // Columns and new values to set
        string where_column;                    // Column name in WHERE clause
        Value where_value;                      // Value to compare in WHERE
        bool has_where;                         // Does this update have WHERE?

        UpdateNode() : has_where(false) {} // Default: no WHERE (update all rows)
    };

    // DELETE statement representation: DELETE FROM table WHERE condition
    struct DeleteNode : public QueryNode
    {
        string table_name;   // Which table to delete from
        string where_column; // Column name in WHERE clause
        Value where_value;   // Value to compare in WHERE clause
        bool has_where;      // Does this delete have WHERE? (false = delete all!)

        DeleteNode() : has_where(false) {} // Default: no WHERE (deletes ALL rows!)
    };

    // CREATE TABLE statement representation: CREATE TABLE name (columns...)
    struct CreateTableNode : public QueryNode
    {
        string table_name; // Name of the new table to create
        Schema schema;     // Column definitions (names, types, sizes)
    };

    // DROP TABLE statement representation: DROP TABLE name
    struct DropTableNode : public QueryNode
    {
        string table_name; // Name of table to completely remove
    };

    // Query result structure - contains the outcome of executing any SQL command
    struct QueryResult
    {
        bool success;         // Did the query execute successfully?
        string message;       // Success message or error description
        vector<Tuple> tuples; // Rows returned (for SELECT queries)

        QueryResult() : success(false) {} // Default: failed query

        // Constructor for queries that return a message but no data
        QueryResult(bool success, const string &message)
            : success(success), message(message) {}
    };

    // QueryParser class - converts SQL text into Abstract Syntax Tree (AST)
    // This is like a translator that understands SQL grammar and builds a tree structure
    class QueryParser
    {
    private:
        string query;    // The SQL string we're parsing
        size_t position; // Current position in the string (like a reading cursor)

        // Helper functions for parsing different parts of SQL

        // Skip over spaces, tabs, and newlines
        void skipWhitespace();

        // Read table names, column names, etc. (alphanumeric identifiers)
        string readIdentifier();

        // Read quoted string values like 'Hello World'
        string readString();

        // Parse different types of values (numbers, strings, booleans)
        Value parseValue();

        // Parse column data types (INTEGER, VARCHAR, BOOLEAN, DOUBLE)
        DataType parseDataType();

        // Expect a specific keyword or symbol (throw error if not found)
        void expect(const string &expected);

        // Try to match a keyword or symbol (return true/false)
        bool match(const string &expected);

        // Parsing functions for different SQL statement types

        // Parse SELECT statement and build SelectNode
        unique_ptr<SelectNode> parseSelect();

        // Parse INSERT statement and build InsertNode
        unique_ptr<InsertNode> parseInsert();

        // Parse UPDATE statement and build UpdateNode
        unique_ptr<UpdateNode> parseUpdate();

        // Parse DELETE statement and build DeleteNode
        unique_ptr<DeleteNode> parseDelete();

        // Parse CREATE TABLE statement and build CreateTableNode
        unique_ptr<CreateTableNode> parseCreateTable();

        // Parse DROP TABLE statement and build DropTableNode
        unique_ptr<DropTableNode> parseDropTable();

    public:
        // Constructor - initialize parser with SQL string to parse
        QueryParser(const string &query) : query(query), position(0) {}

        // Main parsing method - determines query type and calls appropriate parser
        unique_ptr<QueryNode> parse();

        // Utility method to identify what type of SQL command this is
        QueryType getQueryType(const string &query);
    };

    // QueryExecutor class - takes parsed AST and executes it against the database
    // This is like the "action taker" that performs the actual database operations
    class QueryExecutor
    {
    private:
        StorageEngine *storage_engine; // Pointer to storage system for accessing tables

    public:
        // Constructor - connect executor to the database storage engine
        QueryExecutor(StorageEngine *storage_engine) : storage_engine(storage_engine) {}

        // Main execution method - takes SQL string, parses it, and executes it
        QueryResult execute(const string &query);

        // Specific execution methods for each query type

        // Execute SELECT query - retrieve and return matching rows
        QueryResult executeSelect(const SelectNode &node);

        // Execute INSERT query - add new row to table
        QueryResult executeInsert(const InsertNode &node);

        // Execute UPDATE query - modify existing rows
        QueryResult executeUpdate(const UpdateNode &node);

        // Execute DELETE query - remove rows from table
        QueryResult executeDelete(const DeleteNode &node);

        // Execute CREATE TABLE query - create new table structure
        QueryResult executeCreateTable(const CreateTableNode &node);

        // Execute DROP TABLE query - completely remove table
        QueryResult executeDropTable(const DropTableNode &node);
    };

} // namespace db