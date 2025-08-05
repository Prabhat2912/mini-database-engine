#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <variant>

using namespace std;

namespace db
{

    // Basic type aliases for database components
    // These make our code more readable and allow easy changes later
    using PageId = uint32_t;        // Unique identifier for each 4KB page (allows 16TB database)
    using TupleId = uint64_t;       // Unique identifier for each row (virtually unlimited rows)
    using TransactionId = uint32_t; // Unique identifier for each transaction
    using BufferFrameId = uint32_t; // Identifier for frames in our memory buffer pool

    // Database size constants - these control memory usage and performance
    constexpr size_t PAGE_SIZE = 4096;        // Each page is exactly 4KB (matches OS page size)
    constexpr size_t BUFFER_POOL_SIZE = 1000; // Keep 1000 pages in RAM (4MB total cache)
    constexpr size_t MAX_TUPLE_SIZE = 1024;   // Maximum size for a single row (1KB)

    // Data types that our database can store in columns
    enum class DataType
    {
        INTEGER, // 32-bit signed integers (-2 billion to +2 billion)
        VARCHAR, // Variable-length text strings (like names, emails)
        BOOLEAN, // True/false values
        DOUBLE   // 64-bit floating point numbers (for prices, scores, etc.)
    };

    // Value type that can hold any of our supported data types
    // std::variant is like a type-safe union - can hold one type at a time
    using Value = variant<int32_t, string, bool, double>;

    // Tuple structure - represents one row of data in a table
    struct Tuple
    {
        TupleId id;           // Unique identifier for this row
        vector<Value> values; // The actual column data (can be mixed types)

        // Default constructor creates empty tuple
        Tuple() = default;

        // Constructor that takes ID and values, using move semantics for efficiency
        Tuple(TupleId id, vector<Value> values) : id(id), values(move(values)) {}
    };

    // Column definition - describes the structure of one column in a table
    struct Column
    {
        string name;   // Column name (like "user_id", "email", "age")
        DataType type; // What kind of data (INTEGER, VARCHAR, etc.)
        size_t size;   // For VARCHAR(50), this would be 50 (max length)

        // Constructor using move semantics to avoid copying the name string
        Column(string name, DataType type, size_t size = 0)
            : name(move(name)), type(type), size(size) {}
    };

    // Schema definition - describes the complete structure of a table
    struct Schema
    {
        vector<Column> columns; // List of all columns in this table

        // Helper method to add a new column to the table schema
        void addColumn(const string &name, DataType type, size_t size = 0)
        {
            columns.emplace_back(name, type, size); // Create column object directly in vector
        }
    };

    // Page header structure - metadata stored at the beginning of each 4KB page
    struct PageHeader
    {
        PageId page_id;       // Which page number this is (unique identifier)
        uint32_t free_space;  // How many bytes are left for storing new tuples
        uint32_t tuple_count; // How many rows are currently stored on this page
        uint32_t next_page;   // Link to next page (for when data overflows)

        // Constructor - initialize a new page header with default values
        PageHeader() : page_id(0),
                       free_space(PAGE_SIZE - sizeof(PageHeader)), // 4096 - 16 = 4080 bytes available
                       tuple_count(0), next_page(0)
        {
        }
    };

    // Transaction states - tracks the lifecycle of database transactions
    enum class TransactionState
    {
        ACTIVE,    // Transaction is currently running and making changes
        COMMITTED, // Transaction completed successfully, changes are permanent
        ABORTED    // Transaction failed or was canceled, changes are rolled back
    };

    // Lock types - controls how transactions can access data simultaneously
    enum class LockType
    {
        SHARED,   // Multiple readers can access the same data (read-only lock)
        EXCLUSIVE // Only one writer can access, no readers allowed (write lock)
    };

    // Query types - different kinds of SQL operations our database supports
    enum class QueryType
    {
        SELECT,       // Read data from tables
        INSERT,       // Add new rows to tables
        UPDATE,       // Modify existing rows
        DELETE,       // Remove rows from tables
        CREATE_TABLE, // Create new table structure
        DROP_TABLE    // Delete entire table
    };

    // Index types - different data structures for fast data lookup
    enum class IndexType
    {
        B_TREE, // Balanced tree index - good for range queries and sorted access
        HASH    // Hash table index - very fast for exact matches
    };

} // namespace db