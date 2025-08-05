#pragma once

#include "types.h"
#include "buffer_pool.h"
#include "b_tree.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

using namespace std;

namespace db
{

    // Tuple header structure - metadata stored before each row on a page
    // This tells us how to read the row data that follows
    struct TupleHeader
    {
        uint32_t tuple_size;        // Total bytes this tuple occupies (header + data)
        uint32_t next_tuple_offset; // Byte offset to next tuple on this page (or 0 if last)
        TupleId tuple_id;           // Unique identifier for this row across entire database
    };

    // Table class - manages storage for one database table
    // Handles inserting, reading, updating, and deleting rows
    class Table
    {
    private:
        // Basic table information
        string name;           // Name of this table (like "users", "orders")
        Schema schema;         // Structure: what columns and types this table has
        PageId first_page_id;  // First page where this table's data is stored
        PageId next_page_id;   // Next available page ID for this table
        TupleId next_tuple_id; // Next available row ID (auto-incrementing)

        // Core storage components
        unique_ptr<BufferPool> buffer_pool;                                // Manages pages in memory vs disk
        unordered_map<string, unique_ptr<BTree<string, TupleId>>> indexes; // Fast lookup indexes

        // Helper methods for converting rows to/from disk storage format

        // Convert a row object to raw bytes for storage on disk
        size_t serializeTuple(const Tuple &tuple, vector<uint8_t> &buffer, size_t offset);

        // Convert raw bytes from disk back to a row object
        Tuple deserializeTuple(const vector<uint8_t> &buffer, size_t offset);

        // Calculate how many bytes a row will need when stored
        size_t getTupleSize(const Tuple &tuple);

        // Page management methods

        // Get a new empty page for storing more data
        PageId allocateNewPage();

        // Try to fit a row into a specific page
        bool insertTupleIntoPage(PageId page_id, const Tuple &tuple);

        // Read all rows stored on a specific page
        vector<Tuple> readTuplesFromPage(PageId page_id);

        // Load existing table data and metadata from disk
        void loadExistingTableData();

    public:
        // Constructor - create a new table with given name and structure
        Table(const string &name, const Schema &schema, const string &db_file_path);

        // Main database operations that users can perform

        // Add a new row to the table
        bool insertTuple(const Tuple &tuple);

        // Get all rows from the table (full table scan)
        vector<Tuple> selectAll();

        // Get rows that match a condition (filtered query)
        vector<Tuple> selectWhere(const string &column, const Value &value);

        // Remove a specific row by its ID
        bool deleteTuple(TupleId tuple_id);

        // Change the data in a specific row
        bool updateTuple(TupleId tuple_id, const vector<Value> &new_values);

        // Index operations - create fast lookup structures for queries

        // Build a B-tree index on a specific column for faster searching
        void createIndex(const string &column_name);

        // Use an index to quickly find rows matching a value (much faster than full scan)
        vector<Tuple> selectUsingIndex(const string &column, const Value &value);

        // Schema operations - access table structure information

        // Get the table's schema (column definitions)
        const Schema &getSchema() const { return schema; }

        // Get the table's name
        const string &getName() const { return name; }

        // Statistics and monitoring

        // Count how many rows are in this table
        size_t getTupleCount() const;

        // Print debugging information about this table
        void printStats() const;

        // Get buffer pool for flushing pages to disk
        BufferPool *getBufferPool() const { return buffer_pool.get(); }
    };

    // StorageEngine class - manages all tables in the database
    // This is the main interface for table operations
    class StorageEngine
    {
    private:
        // All tables in this database, keyed by table name
        unordered_map<string, unique_ptr<Table>> tables; // Map: table name -> Table object
        string db_file_path;                             // Path to database file on disk

    public:
        // Constructor - initialize storage engine with database file location
        StorageEngine(const string &db_file_path);

        // Table management operations

        // Create a new table with specified name and column structure
        bool createTable(const string &name, const Schema &schema);

        // Delete an entire table and all its data
        bool dropTable(const string &name);

        // Get pointer to a table for direct operations (returns null if not found)
        Table *getTable(const string &name);

        // Data operations - work with rows across all tables

        // Insert a new row into a specific table
        bool insertTuple(const string &table_name, const Tuple &tuple);

        // Get all rows from a table
        vector<Tuple> selectAll(const string &table_name);

        // Get filtered rows from a table
        vector<Tuple> selectWhere(const string &table_name,
                                  const string &column, const Value &value);

        // Delete a specific row from a table
        bool deleteTuple(const string &table_name, TupleId tuple_id);

        // Update/modify a specific row in a table
        bool updateTuple(const string &table_name, TupleId tuple_id,
                         const vector<Value> &new_values);

        // Index operations - improve query performance

        // Create an index on a column for faster lookups
        bool createIndex(const string &table_name, const string &column_name);

        // Utility methods

        // Get list of all table names in this database
        vector<string> getTableNames() const;

        // Print statistics about all tables in the database
        void printStats() const;

        // Flush all buffer pools to disk
        void flushAllPages();
    };

} // namespace db