#pragma once

#include "types.h"
#include "b_tree.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

using namespace std;

namespace db
{
    // Forward declarations - tell compiler these classes exist
    class StorageEngine;

    // IndexManager class - manages all database indexes for fast data lookup
    // Creates and maintains B-tree indexes on table columns for quick searches
    class IndexManager
    {
    private:
        // Map from "table.column" names to their B-tree indexes
        // Each index maps values to tuple IDs (row locations)
        unordered_map<string, unique_ptr<BTree<string, TupleId>>> indexes;
        StorageEngine *storage_engine; // Reference to storage system

    public:
        // Constructor - initialize with reference to storage engine
        IndexManager(StorageEngine *storage_engine);

        // Create a new index on a table column (speeds up searches on that column)
        bool createIndex(const string &table_name, const string &column_name);

        // Look up all rows that have a specific value in an indexed column
        vector<TupleId> lookupIndex(const string &table_name,
                                    const string &column_name,
                                    const Value &value);

        // Remove an index (frees up space but slows down searches)
        void dropIndex(const string &table_name, const string &column_name);

        // Check if an index exists on a specific table column
        bool indexExists(const string &table_name, const string &column_name);

        // Display statistics about all indexes (size, performance, etc.)
        void printStats() const;
    };

} // namespace db