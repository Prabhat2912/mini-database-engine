#include "index_manager.h"
#include "b_tree.h"
#include "storage_engine.h"
#include <iostream>

using namespace std;

namespace db
{
    // IndexManager implementation - manages B-tree indexes for fast data lookup

    // Constructor - initialize with reference to storage engine
    IndexManager::IndexManager(StorageEngine *storage_engine) : storage_engine(storage_engine) {}

    // Create a new index on a table column for faster searches
    // Builds B-tree index from existing table data
    bool IndexManager::createIndex(const string &table_name, const string &column_name)
    {
        if (!storage_engine)
        {
            return false; // No storage engine available
        }

        string index_name = table_name + "_" + column_name; // Unique index identifier

        // Check if index already exists to avoid duplicates
        if (indexes.find(index_name) != indexes.end())
        {
            return false; // Index already exists
        }

        // Create new B-tree index structure
        auto index = make_unique<BTree<string, TupleId>>();

        // Get table to build index from existing data
        auto table = storage_engine->getTable(table_name);
        if (!table)
        {
            return false; // Table doesn't exist
        }

        auto all_tuples = table->selectAll(); // Get all existing rows
        const auto &schema = table->getSchema();

        // Find the column index in the schema
        size_t col_idx = 0;
        for (size_t i = 0; i < schema.columns.size(); i++)
        {
            if (schema.columns[i].name == column_name)
            {
                col_idx = i;
                break;
            }
        }

        // Build index by inserting all existing values
        for (const auto &tuple : all_tuples)
        {
            if (col_idx < tuple.values.size())
            {
                // Convert value to string key for B-tree storage
                string key = visit([](const auto &v)
                                   {
                    if constexpr (is_same_v<decay_t<decltype(v)>, string>) {
                        return v;                           // Already a string
                    } else {
                        return to_string(v);               // Convert to string
                    } }, tuple.values[col_idx]);
                index->insert(key, tuple.id); // Add to B-tree index
            }
        }

        indexes[index_name] = move(index); // Store completed index
        return true;
    }

    // Look up all tuples with a specific value using an index
    // Returns tuple IDs that match the search value
    vector<TupleId> IndexManager::lookupIndex(const string &table_name,
                                              const string &column_name,
                                              const Value &value)
    {
        string index_name = table_name + "_" + column_name;

        auto it = indexes.find(index_name);
        if (it == indexes.end())
        {
            return {}; // Index doesn't exist
        }

        // Convert search value to string key
        string key = visit([](const auto &v)
                           {
            if constexpr (is_same_v<decay_t<decltype(v)>, string>) {
                return v;                                   // Already a string
            } else {
                return to_string(v);                       // Convert to string
            } }, value);

        auto tuple_id_ptr = it->second->search(key); // Search B-tree
        if (tuple_id_ptr)
        {
            return {*tuple_id_ptr}; // Return found tuple ID
        }

        return {}; // No match found
    }

    // Remove an index (frees memory but makes searches slower)
    void IndexManager::dropIndex(const string &table_name, const string &column_name)
    {
        string index_name = table_name + "_" + column_name;
        indexes.erase(index_name); // Remove from index map
    }

    // Check if an index exists on a specific table column
    bool IndexManager::indexExists(const string &table_name, const string &column_name)
    {
        string index_name = table_name + "_" + column_name;
        return indexes.find(index_name) != indexes.end();
    }

    // Display statistics about all indexes
    void IndexManager::printStats() const
    {
        cout << "Index Manager Statistics:" << endl;
        cout << "  Total indexes: " << indexes.size() << endl;
        for (const auto &[name, index] : indexes)
        {
            cout << "    Index: " << name << endl; // Display each index name
        }
    }

} // namespace db