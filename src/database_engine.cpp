#include "database_engine.h"
#include <iostream>
#include <fstream>

using namespace std;

namespace db
{

    // DatabaseEngine implementation - main database coordinator class
    // Constructor - initialize all database subsystems with file paths
    DatabaseEngine::DatabaseEngine(const string &db_file_path)
        : db_file_path(db_file_path), log_file_path(db_file_path + ".log"),
          metadata_file_path(db_file_path + ".meta"),
          current_transaction_id(0), in_transaction(false)
    {

        // Create all subsystem components
        storage_engine = make_unique<StorageEngine>(db_file_path);            // Data storage and retrieval
        query_parser = make_unique<QueryParser>("");                          // SQL parsing
        query_executor = make_unique<QueryExecutor>(storage_engine.get());    // Query execution
        transaction_manager = make_unique<TransactionManager>(log_file_path); // ACID transactions
        wal_manager = make_unique<WALManager>(log_file_path);                 // Write-ahead logging

        // Load existing table metadata if available
        loadTableMetadata();
    }

    // Destructor - properly shut down all systems
    DatabaseEngine::~DatabaseEngine()
    {
        shutdown();
    }

    // Initialize database systems and prepare for operations
    bool DatabaseEngine::initialize()
    {
        // Initialize components (placeholder for future initialization logic)
        return true;
    }

    // Safely shut down database, ensuring data integrity
    void DatabaseEngine::shutdown()
    {
        if (in_transaction)
        {
            rollbackTransaction(); // Cancel any pending transaction
        }

        // Save table metadata before shutdown
        saveTableMetadata();

        // Perform full checkpoint including buffer pool flush
        checkpoint();
    }

    // Start a new ACID transaction
    bool DatabaseEngine::beginTransaction()
    {
        if (in_transaction)
        {
            return false; // Already in transaction
        }

        current_transaction_id = transaction_manager->beginTransaction();
        in_transaction = true;
        return true;
    }

    // Commit current transaction (make all changes permanent)
    bool DatabaseEngine::commitTransaction()
    {
        if (!in_transaction)
        {
            return false; // Not in transaction
        }

        bool success = transaction_manager->commitTransaction(current_transaction_id);
        if (success)
        {
            in_transaction = false;
            current_transaction_id = 0;
        }
        return success;
    }

    // Rollback current transaction (cancel and undo all changes)
    bool DatabaseEngine::rollbackTransaction()
    {
        if (!in_transaction)
        {
            return false; // Not in transaction
        }

        bool success = transaction_manager->abortTransaction(current_transaction_id);
        if (success)
        {
            in_transaction = false;
            current_transaction_id = 0;
        }
        return success;
    }

    // Execute a SQL query and return results
    QueryResult DatabaseEngine::executeQuery(const string &query)
    {
        return query_executor->execute(query);
    }

    // Create a new table with specified schema
    bool DatabaseEngine::createTable(const string &name, const Schema &schema)
    {
        bool success = storage_engine->createTable(name, schema);
        if (success)
        {
            // Save metadata after successful table creation
            saveTableMetadata();
        }
        return success;
    }

    // Drop (delete) an existing table
    bool DatabaseEngine::dropTable(const string &name)
    {
        return storage_engine->dropTable(name);
    }

    // Get list of all table names in the database
    vector<string> DatabaseEngine::getTableNames()
    {
        return storage_engine->getTableNames();
    }

    // Insert a new row (tuple) into a table
    bool DatabaseEngine::insertTuple(const string &table_name, const vector<Value> &values)
    {
        Tuple tuple;
        tuple.values = values;
        return storage_engine->insertTuple(table_name, tuple);
    }

    // Select all rows from a table
    vector<Tuple> DatabaseEngine::selectAll(const string &table_name)
    {
        return storage_engine->selectAll(table_name);
    }

    // Select rows matching a specific condition
    vector<Tuple> DatabaseEngine::selectWhere(const string &table_name,
                                              const string &column, const Value &value)
    {
        return storage_engine->selectWhere(table_name, column, value);
    }

    // Create an index on a table column for faster searches
    bool DatabaseEngine::createIndex(const string &table_name, const string &column_name)
    {
        return storage_engine->createIndex(table_name, column_name);
    }

    // Display database statistics and performance metrics
    void DatabaseEngine::printStats()
    {
        cout << "=== Database Engine Statistics ===" << endl;
        storage_engine->printStats();
        transaction_manager->printStats();
    }

    // Display information about a specific table
    void DatabaseEngine::printTableInfo(const string &table_name)
    {
        auto table = storage_engine->getTable(table_name);
        if (table)
        {
            table->printStats();
        }
        else
        {
            cout << "Table '" << table_name << "' not found" << endl;
        }
    }

    // Get table schema information
    Schema DatabaseEngine::getTableSchema(const string &table_name)
    {
        auto table = storage_engine->getTable(table_name);
        if (table)
        {
            return table->getSchema();
        }
        else
        {
            // Return empty schema if table not found
            return Schema{};
        }
    }

    // Create a checkpoint for recovery purposes
    void DatabaseEngine::checkpoint()
    {
        // First flush all dirty pages from buffer pool to disk
        if (storage_engine)
        {
            storage_engine->flushAllPages();
        }

        // Then write transaction checkpoint to WAL
        transaction_manager->checkpoint();
    }

    // Recover database state after a crash
    void DatabaseEngine::recover()
    {
        transaction_manager->recover();
        wal_manager->recover();
    }

    // Save table metadata (schemas) to disk for persistence
    void DatabaseEngine::saveTableMetadata()
    {
        ofstream metadata_file(metadata_file_path, ios::binary);
        if (!metadata_file.is_open())
        {
            return; // Failed to open file
        }

        // Get all table names from storage engine
        auto table_names = storage_engine->getTableNames();

        // Write number of tables
        uint32_t table_count = static_cast<uint32_t>(table_names.size());
        metadata_file.write(reinterpret_cast<const char *>(&table_count), sizeof(uint32_t));

        // Write each table's schema
        for (const string &table_name : table_names)
        {
            auto table = storage_engine->getTable(table_name);
            if (table)
            {
                const Schema &schema = table->getSchema();

                // Write table name
                uint32_t name_length = static_cast<uint32_t>(table_name.length());
                metadata_file.write(reinterpret_cast<const char *>(&name_length), sizeof(uint32_t));
                metadata_file.write(table_name.data(), name_length);

                // Write schema
                uint32_t column_count = static_cast<uint32_t>(schema.columns.size());
                metadata_file.write(reinterpret_cast<const char *>(&column_count), sizeof(uint32_t));

                for (const Column &column : schema.columns)
                {
                    // Write column name
                    uint32_t col_name_length = static_cast<uint32_t>(column.name.length());
                    metadata_file.write(reinterpret_cast<const char *>(&col_name_length), sizeof(uint32_t));
                    metadata_file.write(column.name.data(), col_name_length);

                    // Write column type
                    metadata_file.write(reinterpret_cast<const char *>(&column.type), sizeof(DataType));

                    // Write column size
                    metadata_file.write(reinterpret_cast<const char *>(&column.size), sizeof(uint32_t));
                }
            }
        }

        metadata_file.close();
    }

    // Load table metadata (schemas) from disk on startup
    void DatabaseEngine::loadTableMetadata()
    {
        ifstream metadata_file(metadata_file_path, ios::binary);
        if (!metadata_file.is_open())
        {
            return; // No metadata file exists (first run)
        }

        // Read number of tables
        uint32_t table_count;
        metadata_file.read(reinterpret_cast<char *>(&table_count), sizeof(uint32_t));
        if (metadata_file.fail())
        {
            metadata_file.close();
            return;
        }

        // Read each table's schema and recreate tables
        for (uint32_t i = 0; i < table_count; i++)
        {
            // Read table name
            uint32_t name_length;
            metadata_file.read(reinterpret_cast<char *>(&name_length), sizeof(uint32_t));
            if (metadata_file.fail())
                break;

            string table_name(name_length, '\0');
            metadata_file.read(&table_name[0], name_length);
            if (metadata_file.fail())
                break;

            // Read schema
            uint32_t column_count;
            metadata_file.read(reinterpret_cast<char *>(&column_count), sizeof(uint32_t));
            if (metadata_file.fail())
                break;

            Schema schema;
            for (uint32_t j = 0; j < column_count; j++)
            {
                // Read column name
                uint32_t col_name_length;
                metadata_file.read(reinterpret_cast<char *>(&col_name_length), sizeof(uint32_t));
                if (metadata_file.fail())
                    break;

                string col_name(col_name_length, '\0');
                metadata_file.read(&col_name[0], col_name_length);
                if (metadata_file.fail())
                    break;

                // Read column type
                DataType col_type;
                metadata_file.read(reinterpret_cast<char *>(&col_type), sizeof(DataType));
                if (metadata_file.fail())
                    break;

                // Read column size
                uint32_t col_size;
                metadata_file.read(reinterpret_cast<char *>(&col_size), sizeof(uint32_t));
                if (metadata_file.fail())
                    break;

                schema.columns.push_back({col_name, col_type, col_size});
            }

            // Recreate table with loaded schema
            storage_engine->createTable(table_name, schema);
        }

        metadata_file.close();
    }

    // Database implementation (high-level interface) - simplified database operations
    // Constructor - create database engine and initialize systems
    Database::Database(const string &db_file_path)
    {
        engine = make_unique<DatabaseEngine>(db_file_path);
        engine->initialize();
    }

    // Create table with column names and types (simplified interface)
    bool Database::createTable(const string &name, const vector<string> &columns,
                               const vector<DataType> &types)
    {
        if (columns.size() != types.size())
        {
            return false; // Column count mismatch
        }

        Schema schema;
        for (size_t i = 0; i < columns.size(); i++)
        {
            schema.addColumn(columns[i], types[i]); // Add each column to schema
        }

        return engine->createTable(name, schema);
    }

    // Insert a row of values into a table
    bool Database::insert(const string &table_name, const vector<Value> &values)
    {
        return engine->insertTuple(table_name, values);
    }

    // Select rows from table (with optional filtering)
    vector<Tuple> Database::select(const string &table_name,
                                   const string &where_column,
                                   const Value &where_value)
    {
        if (where_column.empty())
        {
            return engine->selectAll(table_name); // Select all rows
        }
        else
        {
            return engine->selectWhere(table_name, where_column, where_value); // Select with condition
        }
    }

    // Update rows in table (simplified implementation)
    bool Database::update(const string &table_name, const vector<string> &columns,
                          const vector<Value> &values, const string &where_column,
                          const Value &where_value)
    {
        // Simplified update implementation
        // In a real implementation, this would be more sophisticated
        return true;
    }

    // Delete rows from table (simplified implementation)
    bool Database::remove(const string &table_name, const string &where_column,
                          const Value &where_value)
    {
        // Simplified delete implementation
        // In a real implementation, this would be more sophisticated
        return true;
    }

    // Transaction management - start new transaction
    bool Database::begin()
    {
        return engine->beginTransaction();
    }

    // Transaction management - commit current transaction
    bool Database::commit()
    {
        return engine->commitTransaction();
    }

    // Transaction management - rollback current transaction
    bool Database::rollback()
    {
        return engine->rollbackTransaction();
    }

    // Execute raw SQL query
    QueryResult Database::executeQuery(const string &query)
    {
        return engine->executeQuery(query);
    }

    // Display database statistics
    void Database::printStats()
    {
        engine->printStats();
    }

    // Display information about a specific table
    void Database::printTable(const string &table_name)
    {
        engine->printTableInfo(table_name);
    }

    // Get table schema information
    Schema Database::getTableSchema(const string &table_name)
    {
        return engine->getTableSchema(table_name);
    }

} // namespace db