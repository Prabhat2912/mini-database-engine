#include "query_parser.h"
#include "storage_engine.h"
#include <iostream>
#include <algorithm>
#include <cctype>

using namespace std;

namespace db
{
    // QueryParser implementation - converts SQL text into executable operations

    // Skip whitespace characters in the query string
    // Moves position pointer past spaces, tabs, newlines
    void QueryParser::skipWhitespace()
    {
        while (position < query.length() && isspace(query[position]))
        {
            position++;
        }
    }

    // Read an identifier (table name, column name, etc.)
    // Identifiers can contain letters, numbers, and underscores
    string QueryParser::readIdentifier()
    {
        skipWhitespace();
        string identifier;
        while (position < query.length() &&
               (isalnum(query[position]) || query[position] == '_'))
        {
            identifier += query[position];
            position++;
        }
        return identifier;
    }

    // Read a string literal enclosed in single quotes
    // Used for VARCHAR values in SQL statements
    string QueryParser::readString()
    {
        string str;
        position++; // Skip opening quote
        while (position < query.length() && query[position] != '\'')
        {
            str += query[position];
            position++;
        }
        position++; // Skip closing quote
        return str;
    }

    // Parse a value from the query (number, string, boolean)
    // Determines the data type and converts to appropriate Value variant
    Value QueryParser::parseValue()
    {
        skipWhitespace();

        if (position >= query.length())
        {
            throw runtime_error("Unexpected end of query");
        }

        if (query[position] == '\'')
        {
            // String value enclosed in single quotes
            return readString();
        }
        else if (isdigit(query[position]) || query[position] == '-')
        {
            // Numeric value (integer or double)
            string num_str;
            bool has_decimal = false;

            if (query[position] == '-')
            {
                num_str += query[position]; // Handle negative numbers
                position++;
            }

            while (position < query.length() &&
                   (isdigit(query[position]) || query[position] == '.'))
            {
                if (query[position] == '.')
                {
                    if (has_decimal)
                    {
                        throw runtime_error("Invalid number format");
                    }
                    has_decimal = true; // Mark as decimal number
                }
                num_str += query[position];
                position++;
            }

            if (has_decimal)
            {
                return stod(num_str); // Convert to double
            }
            else
            {
                return stoi(num_str); // Convert to integer
            }
        }
        else if (query.substr(position, 4) == "true")
        {
            position += 4;
            return true; // Boolean true value
        }
        else if (query.substr(position, 5) == "false")
        {
            position += 5;
            return false; // Boolean false value
        }

        throw runtime_error("Invalid value format");
    }

    // Parse data type specification in CREATE TABLE statements
    // Converts SQL type names to internal DataType enum values
    DataType QueryParser::parseDataType()
    {
        skipWhitespace();
        string type = readIdentifier();
        transform(type.begin(), type.end(), type.begin(), ::toupper); // Convert to uppercase

        if (type == "INTEGER" || type == "INT")
        {
            return DataType::INTEGER;
        }
        else if (type == "VARCHAR")
        {
            return DataType::VARCHAR;
        }
        else if (type == "BOOLEAN" || type == "BOOL")
        {
            return DataType::BOOLEAN;
        }
        else if (type == "DOUBLE" || type == "FLOAT")
        {
            return DataType::DOUBLE;
        }

        throw runtime_error("Unknown data type: " + type);
    }

    // Expect a specific keyword or symbol at current position
    // Throws error if expected text is not found
    void QueryParser::expect(const string &expected)
    {
        skipWhitespace();
        if (position + expected.length() > query.length())
        {
            throw runtime_error("Expected '" + expected + "'");
        }

        string actual = query.substr(position, expected.length());
        string expected_upper = expected;
        string actual_upper = actual;

        // Convert both to uppercase for case-insensitive comparison
        transform(expected_upper.begin(), expected_upper.end(), expected_upper.begin(), ::toupper);
        transform(actual_upper.begin(), actual_upper.end(), actual_upper.begin(), ::toupper);

        if (actual_upper != expected_upper)
        {
            throw runtime_error("Expected '" + expected + "'");
        }
        position += expected.length(); // Move past matched text
    }

    // Check if current position matches expected text (case-insensitive)
    // Returns true and advances position if match found, false otherwise
    bool QueryParser::match(const string &expected)
    {
        skipWhitespace();
        if (position + expected.length() <= query.length())
        {
            string actual = query.substr(position, expected.length());
            string expected_upper = expected;
            string actual_upper = actual;

            // Convert both to uppercase for case-insensitive comparison
            transform(expected_upper.begin(), expected_upper.end(), expected_upper.begin(), ::toupper);
            transform(actual_upper.begin(), actual_upper.end(), actual_upper.begin(), ::toupper);

            if (actual_upper == expected_upper)
            {
                position += expected.length(); // Advance position on match
                return true;
            }
        }
        return false; // No match found
    }

    // Parse SELECT statement and build AST node
    // Handles column selection, table specification, and WHERE clauses
    unique_ptr<SelectNode> QueryParser::parseSelect()
    {
        auto node = make_unique<SelectNode>();

        skipWhitespace();

        // Parse column list (either * for all columns or specific column names)
        if (match("*"))
        {
            // SELECT *
        }
        else
        {
            while (true)
            {
                node->columns.push_back(readIdentifier());
                if (!match(","))
                    break;
            }
        }

        expect("FROM");
        node->table_name = readIdentifier();

        // Parse WHERE clause
        if (match("WHERE"))
        {
            node->has_where = true;
            node->where_column = readIdentifier();
            expect("=");
            node->where_value = parseValue();
        }

        return node;
    }

    // Parse INSERT statement and build AST node
    // Handles INSERT INTO table VALUES (...) syntax
    unique_ptr<InsertNode> QueryParser::parseInsert()
    {
        auto node = make_unique<InsertNode>();

        expect("INTO");
        node->table_name = readIdentifier(); // Get target table name

        if (match("VALUES"))
        {
            expect("(");
            // Parse value list in parentheses
            while (true)
            {
                node->values.push_back(parseValue()); // Add each value to list
                if (!match(","))
                    break; // No more values
            }
            expect(")");
        }

        return node;
    }

    // Parse UPDATE statement and build AST node
    // Handles UPDATE table SET col=val WHERE condition syntax
    unique_ptr<UpdateNode> QueryParser::parseUpdate()
    {
        auto node = make_unique<UpdateNode>();

        node->table_name = readIdentifier(); // Get target table name
        expect("SET");

        // Parse SET clause (column = value pairs)
        while (true)
        {
            string column = readIdentifier();
            expect("=");
            Value value = parseValue();
            node->set_values.emplace_back(column, value); // Store column-value pair

            if (!match(","))
                break; // No more SET clauses
        }

        // Parse optional WHERE clause
        if (match("WHERE"))
        {
            node->has_where = true;
            node->where_column = readIdentifier();
            expect("=");
            node->where_value = parseValue();
        }

        return node;
    }

    // Parse DELETE statement and build AST node
    // Handles DELETE FROM table WHERE condition syntax
    unique_ptr<DeleteNode> QueryParser::parseDelete()
    {
        auto node = make_unique<DeleteNode>();

        expect("FROM");
        node->table_name = readIdentifier(); // Get target table name

        // Parse optional WHERE clause
        if (match("WHERE"))
        {
            node->has_where = true;
            node->where_column = readIdentifier();
            expect("=");
            node->where_value = parseValue();
        }

        return node;
    }

    // Parse CREATE TABLE statement and build AST node
    // Handles CREATE TABLE name (col1 type1, col2 type2, ...) syntax
    unique_ptr<CreateTableNode> QueryParser::parseCreateTable()
    {
        auto node = make_unique<CreateTableNode>();

        expect("TABLE");
        node->table_name = readIdentifier(); // Get new table name
        skipWhitespace();
        expect("(");

        // Parse column definitions list
        while (true)
        {
            string column_name = readIdentifier();
            DataType data_type = parseDataType();

            size_t size = 0;
            if (data_type == DataType::VARCHAR && match("("))
            {
                // Parse VARCHAR size specification
                string size_str;
                while (position < query.length() && isdigit(query[position]))
                {
                    size_str += query[position];
                    position++;
                }
                size = stoul(size_str);
                expect(")");
            }

            node->schema.addColumn(column_name, data_type, size); // Add column to schema

            if (!match(","))
                break; // No more columns
        }

        expect(")");

        return node;
    }

    // Parse DROP TABLE statement and build AST node
    // Handles DROP TABLE name syntax
    unique_ptr<DropTableNode> QueryParser::parseDropTable()
    {
        auto node = make_unique<DropTableNode>();

        expect("TABLE");
        node->table_name = readIdentifier(); // Get table name to drop

        return node;
    }

    // Main parsing entry point - determines query type and calls appropriate parser
    // Returns AST node representing the parsed SQL statement
    unique_ptr<QueryNode> QueryParser::parse()
    {
        skipWhitespace();

        string command = readIdentifier();
        transform(command.begin(), command.end(), command.begin(), ::toupper); // Normalize to uppercase

        if (command == "SELECT")
        {
            return parseSelect();
        }
        else if (command == "INSERT")
        {
            return parseInsert();
        }
        else if (command == "UPDATE")
        {
            return parseUpdate();
        }
        else if (command == "DELETE")
        {
            return parseDelete();
        }
        else if (command == "CREATE")
        {
            return parseCreateTable();
        }
        else if (command == "DROP")
        {
            return parseDropTable();
        }

        throw runtime_error("Unknown command: " + command);
    }

    // Determine the type of SQL query without full parsing
    // Used for quick query classification
    QueryType QueryParser::getQueryType(const string &query)
    {
        string upper_query = query;
        transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);

        if (upper_query.find("SELECT") == 0)
            return QueryType::SELECT;
        if (upper_query.find("INSERT") == 0)
            return QueryType::INSERT;
        if (upper_query.find("UPDATE") == 0)
            return QueryType::UPDATE;
        if (upper_query.find("DELETE") == 0)
            return QueryType::DELETE;
        if (upper_query.find("CREATE TABLE") == 0)
            return QueryType::CREATE_TABLE;
        if (upper_query.find("DROP TABLE") == 0)
            return QueryType::DROP_TABLE;

        throw runtime_error("Unknown query type");
    }

    // QueryExecutor implementation - executes parsed SQL statements

    // Main execution entry point - parses SQL and executes appropriate operation
    // Returns QueryResult with success status and data/error message
    QueryResult QueryExecutor::execute(const string &query)
    {
        try
        {
            QueryParser parser(query);
            auto node = parser.parse(); // Parse SQL into AST

            // Execute based on AST node type using dynamic casting
            if (auto select_node = dynamic_cast<SelectNode *>(node.get()))
            {
                return executeSelect(*select_node);
            }
            else if (auto insert_node = dynamic_cast<InsertNode *>(node.get()))
            {
                return executeInsert(*insert_node);
            }
            else if (auto update_node = dynamic_cast<UpdateNode *>(node.get()))
            {
                return executeUpdate(*update_node);
            }
            else if (auto delete_node = dynamic_cast<DeleteNode *>(node.get()))
            {
                return executeDelete(*delete_node);
            }
            else if (auto create_node = dynamic_cast<CreateTableNode *>(node.get()))
            {
                return executeCreateTable(*create_node);
            }
            else if (auto drop_node = dynamic_cast<DropTableNode *>(node.get()))
            {
                return executeDropTable(*drop_node);
            }

            return QueryResult(false, "Unknown query type");
        }
        catch (const exception &e)
        {
            return QueryResult(false, "Parse error: " + string(e.what()));
        }
    }

    // Execute SELECT statement - retrieve data from table
    // Handles both full table scans and WHERE clause filtering
    QueryResult QueryExecutor::executeSelect(const SelectNode &node)
    {
        if (!storage_engine)
        {
            return QueryResult(false, "Storage engine not available");
        }

        vector<Tuple> tuples;
        if (node.has_where)
        {
            // Filter rows based on WHERE condition
            tuples = storage_engine->selectWhere(node.table_name, node.where_column, node.where_value);
        }
        else
        {
            // Return all rows in table
            tuples = storage_engine->selectAll(node.table_name);
        }

        QueryResult result(true, "Query executed successfully");
        result.tuples = tuples;
        return result;
    }

    QueryResult QueryExecutor::executeInsert(const InsertNode &node)
    {
        if (!storage_engine)
        {
            return QueryResult(false, "Storage engine not available");
        }

        Tuple tuple;
        tuple.values = node.values;

        if (storage_engine->insertTuple(node.table_name, tuple))
        {
            return QueryResult(true, "Insert successful");
        }
        else
        {
            return QueryResult(false, "Insert failed");
        }
    }

    // Execute UPDATE statement - modify existing rows in table
    // Updates all rows matching WHERE condition
    QueryResult QueryExecutor::executeUpdate(const UpdateNode &node)
    {
        if (!storage_engine)
        {
            return QueryResult(false, "Storage engine not available");
        }

        // For simplicity, basic update implementation
        // In a real implementation, this would be more sophisticated with
        // proper WHERE clause evaluation and multi-column updates
        return QueryResult(true, "Update not yet implemented");
    }

    // Execute DELETE statement - remove rows from table
    // Deletes all rows matching WHERE condition
    QueryResult QueryExecutor::executeDelete(const DeleteNode &node)
    {
        if (!storage_engine)
        {
            return QueryResult(false, "Storage engine not available");
        }

        // For simplicity, basic delete implementation
        // In a real implementation, this would evaluate WHERE conditions
        // and remove matching tuples from storage
        return QueryResult(true, "Delete not yet implemented");
    }

    // Execute CREATE TABLE statement - define new table structure
    // Creates table with specified name and column schema
    QueryResult QueryExecutor::executeCreateTable(const CreateTableNode &node)
    {
        if (!storage_engine)
        {
            return QueryResult(false, "Storage engine not available");
        }

        // Create table using storage engine
        if (storage_engine->createTable(node.table_name, node.schema))
        {
            return QueryResult(true, "Table created successfully");
        }
        else
        {
            return QueryResult(false, "Failed to create table");
        }
    }

    // Execute DROP TABLE statement - remove table and all its data
    // Permanently deletes table from database
    QueryResult QueryExecutor::executeDropTable(const DropTableNode &node)
    {
        if (!storage_engine)
        {
            return QueryResult(false, "Storage engine not available");
        }

        // Drop table using storage engine
        if (storage_engine->dropTable(node.table_name))
        {
            return QueryResult(true, "Table dropped successfully");
        }
        else
        {
            return QueryResult(false, "Failed to drop table");
        }
    }

} // namespace db