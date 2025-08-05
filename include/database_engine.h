#pragma once

#include "types.h"
#include "storage_engine.h"
#include "query_parser.h"
#include "transaction_manager.h"
#include "buffer_pool.h"
#include <memory>
#include <string>

using namespace std;

namespace db
{
    // DatabaseEngine class - the main entry point for all database operations
    // Coordinates all subsystems: storage, transactions, query processing, recovery
    class DatabaseEngine
    {
    private:
        // Core database subsystems
        unique_ptr<StorageEngine> storage_engine;           // Manages tables and data storage
        unique_ptr<QueryParser> query_parser;               // Parses SQL statements
        unique_ptr<QueryExecutor> query_executor;           // Executes parsed queries
        unique_ptr<TransactionManager> transaction_manager; // Handles ACID transactions
        unique_ptr<WALManager> wal_manager;                 // Write-ahead logging for recovery

        // File system paths
        string db_file_path;       // Where database data is stored
        string log_file_path;      // Where transaction log is stored
        string metadata_file_path; // Where table schemas are stored

        // Current transaction state
        TransactionId current_transaction_id; // ID of active transaction
        bool in_transaction;                  // Are we currently in a transaction?

        // Metadata persistence methods
        void saveTableMetadata(); // Save all table schemas to disk
        void loadTableMetadata(); // Load table schemas from disk

    public:
        // Constructor - initialize database engine with file path
        DatabaseEngine(const string &db_file_path);

        // Destructor - properly shut down all subsystems
        ~DatabaseEngine();

        // Database lifecycle management
        bool initialize(); // Start up database systems
        void shutdown();   // Safely shut down database

        // Transaction management - ACID compliance
        bool beginTransaction();    // Start new transaction
        bool commitTransaction();   // Make changes permanent
        bool rollbackTransaction(); // Cancel and undo changes

        // Query execution - SQL interface
        QueryResult executeQuery(const string &query); // Execute any SQL statement

        // Table operations - DDL (Data Definition Language)
        bool createTable(const string &name, const Schema &schema); // Create new table
        bool dropTable(const string &name);                         // Delete table and all data
        vector<string> getTableNames();                             // List all table names

        // Data operations - DML (Data Manipulation Language)
        bool insertTuple(const string &table_name, const vector<Value> &values); // Add new row to table
        vector<Tuple> selectAll(const string &table_name);                       // Get all rows from table
        vector<Tuple> selectWhere(const string &table_name,                      // Get rows matching condition
                                  const string &column, const Value &value);
        bool deleteTuple(const string &table_name, TupleId tuple_id); // Remove specific row
        bool updateTuple(const string &table_name, TupleId tuple_id,  // Modify existing row
                         const vector<Value> &new_values);

        // Index operations - performance optimization
        bool createIndex(const string &table_name, const string &column_name); // Create index for fast lookups

        // Utility functions - monitoring and debugging
        void printStats();                             // Show database statistics
        void printTableInfo(const string &table_name); // Show table structure

        // Schema access - get table structure information
        Schema getTableSchema(const string &table_name); // Get table column definitions

        // Recovery operations - crash recovery and data integrity
        void checkpoint(); // Save current state to disk
        void recover();    // Restore from crash

        // Status checking - transaction state management
        bool isInTransaction() const { return in_transaction; }                          // Are we in a transaction?
        TransactionId getCurrentTransactionId() const { return current_transaction_id; } // Get current transaction ID
    };

    // Database class - simplified high-level interface for common operations
    // Wraps DatabaseEngine with convenient methods for typical database tasks
    class Database
    {
    private:
        unique_ptr<DatabaseEngine> engine; // Internal database engine

    public:
        // Constructor - create database with specified file path
        Database(const string &db_file_path);

        // High-level table operations - simplified interface for beginners
        bool createTable(const string &name, const vector<string> &columns, // Create table with column names/types
                         const vector<DataType> &types);
        bool insert(const string &table_name, const vector<Value> &values); // Insert row into table
        vector<Tuple> select(const string &table_name,                      // Select rows (with optional filtering)
                             const string &where_column = "",
                             const Value &where_value = Value{});
        bool update(const string &table_name, const vector<string> &columns, // Update rows with new values
                    const vector<Value> &values, const string &where_column = "",
                    const Value &where_value = Value{});
        bool remove(const string &table_name, const string &where_column = "", // Delete rows (with optional filtering)
                    const Value &where_value = Value{});

        // Query execution - direct SQL interface
        QueryResult executeQuery(const string &query); // Execute raw SQL statement

        // Transaction support - ACID compliance
        bool begin();    // Start transaction
        bool commit();   // Save changes permanently
        bool rollback(); // Cancel and undo changes

        // Utility functions - monitoring and debugging
        void printStats();                         // Display database statistics
        void printTable(const string &table_name); // Show table contents

        // Schema access - get table structure information
        Schema getTableSchema(const string &table_name); // Get table column definitions
    };

} // namespace db