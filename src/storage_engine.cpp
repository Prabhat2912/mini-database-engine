#include "storage_engine.h"
#include <cstring>
#include <iostream>
#include <algorithm>

using namespace std;

namespace db
{
    // Table implementation - manages data storage for a single database table
    // Constructor - create new table with name, schema, and file path
    Table::Table(const string &name, const Schema &schema, const string &db_file_path)
        : name(name), schema(schema), first_page_id(0), next_page_id(1), next_tuple_id(1)
    {
        // Create buffer pool for this table's data pages
        buffer_pool = make_unique<BufferPool>(db_file_path);

        // Try to load existing table data
        loadExistingTableData();

        // Initialize first page if this is a new table
        if (first_page_id == 0)
        {
            first_page_id = allocateNewPage(); // Allocate initial storage page
        }
    }

    // Serialize a tuple (row) into binary format for storage
    // Converts in-memory tuple to byte array that can be written to disk
    size_t Table::serializeTuple(const Tuple &tuple, vector<uint8_t> &buffer, size_t offset)
    {
        size_t current_offset = offset;

        // Write tuple header with metadata
        TupleHeader header;
        header.tuple_size = getTupleSize(tuple); // Calculate total size needed
        header.tuple_id = tuple.id;              // Unique identifier
        header.next_tuple_offset = 0;            // Will be set later for chaining

        memcpy(buffer.data() + current_offset, &header, sizeof(TupleHeader));
        current_offset += sizeof(TupleHeader);

        // Write each value in the tuple based on its data type
        for (const auto &value : tuple.values)
        {
            visit([&](const auto &v)
                  {
            using T = decay_t<decltype(v)>;
            if constexpr (is_same_v<T, int32_t>) {
                memcpy(buffer.data() + current_offset, &v, sizeof(int32_t));
                current_offset += sizeof(int32_t);
            } else if constexpr (is_same_v<T, double>) {
                memcpy(buffer.data() + current_offset, &v, sizeof(double));
                current_offset += sizeof(double);
            } else if constexpr (is_same_v<T, bool>) {
                memcpy(buffer.data() + current_offset, &v, sizeof(bool));
                current_offset += sizeof(bool);
            } else if constexpr (is_same_v<T, string>) {
                uint32_t length = static_cast<uint32_t>(v.length());
                memcpy(buffer.data() + current_offset, &length, sizeof(uint32_t));
                current_offset += sizeof(uint32_t);
                memcpy(buffer.data() + current_offset, v.data(), length);
                current_offset += length;
            } }, value);
        }

        return current_offset - offset;
    }

    // Deserialize tuple from binary format back to in-memory representation
    // Reads byte array from disk and converts back to Tuple object
    Tuple Table::deserializeTuple(const vector<uint8_t> &buffer, size_t offset)
    {
        size_t current_offset = offset;

        // Read tuple header first to get metadata
        TupleHeader header;
        memcpy(&header, buffer.data() + current_offset, sizeof(TupleHeader));
        current_offset += sizeof(TupleHeader);

        Tuple tuple;
        tuple.id = header.tuple_id;

        // Read each value based on the schema definition
        for (const auto &column : schema.columns)
        {
            Value value;
            switch (column.type)
            {
            case DataType::INTEGER:
            {
                int32_t int_val;
                memcpy(&int_val, buffer.data() + current_offset, sizeof(int32_t));
                value = int_val;
                current_offset += sizeof(int32_t);
                break;
            }
            case DataType::DOUBLE:
            {
                double double_val;
                memcpy(&double_val, buffer.data() + current_offset, sizeof(double));
                value = double_val;
                current_offset += sizeof(double);
                break;
            }
            case DataType::BOOLEAN:
            {
                bool bool_val;
                memcpy(&bool_val, buffer.data() + current_offset, sizeof(bool));
                value = bool_val;
                current_offset += sizeof(bool);
                break;
            }
            case DataType::VARCHAR:
            {
                uint32_t length;
                memcpy(&length, buffer.data() + current_offset, sizeof(uint32_t));
                current_offset += sizeof(uint32_t);
                string str_val(reinterpret_cast<const char *>(buffer.data() + current_offset), length);
                value = str_val;
                current_offset += length;
                break;
            }
            }
            tuple.values.push_back(value);
        }

        return tuple;
    }

    // Calculate the total size needed to store a tuple in bytes
    // Used for memory allocation and storage planning
    size_t Table::getTupleSize(const Tuple &tuple)
    {
        size_t size = sizeof(TupleHeader); // Start with header size

        // Add size for each value based on its data type
        for (size_t i = 0; i < tuple.values.size() && i < schema.columns.size(); i++)
        {
            const auto &column = schema.columns[i];
            switch (column.type)
            {
            case DataType::INTEGER:
                size += sizeof(int32_t); // Fixed size for integers
                break;
            case DataType::DOUBLE:
                size += sizeof(double); // Fixed size for doubles
                break;
            case DataType::BOOLEAN:
                size += sizeof(bool); // Fixed size for booleans
                break;
            case DataType::VARCHAR:
            {
                const string &str = get<string>(tuple.values[i]);
                size += sizeof(uint32_t) + str.length(); // Length prefix + string data
                break;
            }
            }
        }

        return size;
    }

    // Allocate a new page for storing tuples
    // Creates a fresh page with proper header and metadata
    PageId Table::allocateNewPage()
    {
        PageId new_page_id = next_page_id++; // Generate unique page ID

        // Get the page from buffer pool and initialize it
        auto frame = buffer_pool->getPage(new_page_id);
        PageHeader header;
        header.page_id = new_page_id;
        header.free_space = PAGE_SIZE - sizeof(PageHeader); // Available space for tuples
        header.tuple_count = 0;                             // No tuples yet
        header.next_page = 0;                               // No next page linked

        // Write header to the beginning of the page
        memcpy(frame->data.data(), &header, sizeof(PageHeader));
        buffer_pool->markDirty(new_page_id);   // Mark page as modified
        buffer_pool->releasePage(new_page_id); // Release page back to pool

        return new_page_id;
    }

    // Insert a tuple into a specific page
    // Returns true if successful, false if page is full
    bool Table::insertTupleIntoPage(PageId page_id, const Tuple &tuple)
    {
        auto frame = buffer_pool->getPage(page_id);

        // Read page header to check available space
        PageHeader header;
        memcpy(&header, frame->data.data(), sizeof(PageHeader));

        size_t tuple_size = getTupleSize(tuple);
        if (tuple_size > header.free_space)
        {
            buffer_pool->releasePage(page_id);
            return false; // Page is full, can't insert
        }

        // Find insertion point (after header and all existing tuples)
        size_t offset = sizeof(PageHeader);

        // Skip over all existing tuples to find the end
        for (uint32_t i = 0; i < header.tuple_count; i++)
        {
            TupleHeader existing_tuple_header;
            memcpy(&existing_tuple_header, frame->data.data() + offset, sizeof(TupleHeader));
            offset += existing_tuple_header.tuple_size;
        }

        // Serialize and write tuple to the page
        size_t written_size = serializeTuple(tuple, frame->data, offset);

        // Update page header with new tuple information
        header.tuple_count++;              // Increment tuple count
        header.free_space -= written_size; // Decrease available space
        memcpy(frame->data.data(), &header, sizeof(PageHeader));

        buffer_pool->markDirty(page_id);   // Mark page as modified
        buffer_pool->releasePage(page_id); // Release page back to pool

        return true;
    }

    // Read all tuples from a specific page
    // Returns vector of all tuples stored on the page
    vector<Tuple> Table::readTuplesFromPage(PageId page_id)
    {
        auto frame = buffer_pool->getPage(page_id);
        vector<Tuple> tuples;

        // Read page header to get tuple count
        PageHeader header;
        memcpy(&header, frame->data.data(), sizeof(PageHeader));

        size_t offset = sizeof(PageHeader); // Start after page header

        // Read all tuples sequentially from the page
        for (uint32_t i = 0; i < header.tuple_count; i++)
        {
            Tuple tuple = deserializeTuple(frame->data, offset);
            tuples.push_back(tuple);

            // Move to next tuple position
            TupleHeader tuple_header;
            memcpy(&tuple_header, frame->data.data() + offset, sizeof(TupleHeader));
            offset += tuple_header.tuple_size;
        }

        buffer_pool->releasePage(page_id);
        return tuples;
    }

    // Load existing table data and metadata from disk
    void Table::loadExistingTableData()
    {
        // Check if page 1 exists and has data
        auto frame = buffer_pool->getPage(1);
        PageHeader header;
        memcpy(&header, frame->data.data(), sizeof(PageHeader));

        // If page 1 has a valid page ID and tuple count, this table exists
        if (header.page_id == 1 && header.tuple_count > 0)
        {
            first_page_id = 1;

            // Find the highest page ID and tuple ID
            PageId current_page = first_page_id;
            TupleId max_tuple_id = 0;
            PageId max_page_id = 1;

            while (current_page != 0)
            {
                auto page_frame = buffer_pool->getPage(current_page);
                PageHeader page_header;
                memcpy(&page_header, page_frame->data.data(), sizeof(PageHeader));

                max_page_id = max(max_page_id, current_page);

                // Check tuples on this page to find max tuple ID
                auto tuples = readTuplesFromPage(current_page);
                for (const auto &tuple : tuples)
                {
                    max_tuple_id = max(max_tuple_id, tuple.id);
                }

                current_page = page_header.next_page;
                buffer_pool->releasePage(current_page);
            }

            // Set next IDs to avoid conflicts
            next_page_id = max_page_id + 1;
            next_tuple_id = max_tuple_id + 1;
        }

        buffer_pool->releasePage(1);
    }

    // Insert a new tuple into the table
    // Finds appropriate page with space or creates new page if needed
    bool Table::insertTuple(const Tuple &tuple)
    {
        // Assign tuple ID if not set
        Tuple new_tuple = tuple;
        if (new_tuple.id == 0)
        {
            new_tuple.id = next_tuple_id++; // Generate unique tuple ID
        }

        // Try to insert into existing pages
        PageId current_page = first_page_id;
        while (current_page != 0)
        {
            if (insertTupleIntoPage(current_page, new_tuple))
            {
                // Update indexes - add new tuple to all column indexes
                for (auto &[column_name, index] : indexes)
                {
                    size_t col_idx = 0;
                    // Find column index by name in schema
                    for (size_t i = 0; i < schema.columns.size(); i++)
                    {
                        if (schema.columns[i].name == column_name)
                        {
                            col_idx = i;
                            break;
                        }
                    }
                    if (col_idx < new_tuple.values.size())
                    {
                        // Convert tuple value to string key for indexing
                        string key = visit([](const auto &v)
                                           {
                        if constexpr (is_same_v<decay_t<decltype(v)>, string>) {
                            return v;
                        } else {
                            return to_string(v);
                        } }, new_tuple.values[col_idx]);
                        index->insert(key, new_tuple.id);
                    }
                }
                return true;
            }

            // Try next page - read page header to get next page ID
            auto frame = buffer_pool->getPage(current_page);
            PageHeader header;
            memcpy(&header, frame->data.data(), sizeof(PageHeader));
            current_page = header.next_page;
            buffer_pool->releasePage(current_page);
        }

        // All pages are full, allocate new page for more storage
        PageId new_page = allocateNewPage();
        if (insertTupleIntoPage(new_page, new_tuple))
        {
            // Link new page to the chain - update first page header
            auto frame = buffer_pool->getPage(first_page_id);
            PageHeader header;
            memcpy(&header, frame->data.data(), sizeof(PageHeader));
            header.next_page = new_page; // Link to new page
            memcpy(frame->data.data(), &header, sizeof(PageHeader));
            buffer_pool->markDirty(first_page_id); // Mark as modified
            buffer_pool->releasePage(first_page_id);

            // Update indexes - add new tuple to all indexed columns
            for (auto &[column_name, index] : indexes)
            {
                size_t col_idx = 0;
                // Find column index by name
                for (size_t i = 0; i < schema.columns.size(); i++)
                {
                    if (schema.columns[i].name == column_name)
                    {
                        col_idx = i;
                        break;
                    }
                }
                if (col_idx < new_tuple.values.size())
                {
                    // Convert tuple value to string key for indexing
                    string key = visit([](const auto &v)
                                       {
                    if constexpr (is_same_v<decay_t<decltype(v)>, string>) {
                        return v;
                    } else {
                        return to_string(v);
                    } }, new_tuple.values[col_idx]);
                    index->insert(key, new_tuple.id);
                }
            }
            return true;
        }

        return false; // Failed to insert
    }

    // Select all tuples from table - performs full table scan
    // Returns vector of all tuples stored in this table
    vector<Tuple> Table::selectAll()
    {
        vector<Tuple> all_tuples;            // Result collection
        PageId current_page = first_page_id; // Start from first page

        // Traverse all pages in the table
        while (current_page != 0)
        {
            // Read all tuples from current page
            auto page_tuples = readTuplesFromPage(current_page);
            all_tuples.insert(all_tuples.end(), page_tuples.begin(), page_tuples.end());

            // Get next page from page header
            auto frame = buffer_pool->getPage(current_page);
            PageHeader header;
            memcpy(&header, frame->data.data(), sizeof(PageHeader));
            PageId next_page = header.next_page;    // Store next page ID
            buffer_pool->releasePage(current_page); // Release current page
            current_page = next_page;               // Move to next page
        }

        return all_tuples;
    }

    // Select tuples matching WHERE condition - optimized with indexes when available
    // Performs indexed lookup if column has index, otherwise full table scan
    vector<Tuple> Table::selectWhere(const string &column, const Value &value)
    {
        // Check if we have an index for this column - enables fast lookup
        auto index_it = indexes.find(column);
        if (index_it != indexes.end())
        {
            return selectUsingIndex(column, value); // Use B-tree index for O(log n) lookup
        }

        // Fallback to full table scan - O(n) linear search through all tuples
        vector<Tuple> result;
        auto all_tuples = selectAll(); // Get all tuples from table

        // Find column index in schema for comparison
        size_t col_idx = 0;
        for (size_t i = 0; i < schema.columns.size(); i++)
        {
            if (schema.columns[i].name == column)
            {
                col_idx = i;
                break;
            }
        }

        // Filter tuples by comparing column value
        for (const auto &tuple : all_tuples)
        {
            if (col_idx < tuple.values.size() && tuple.values[col_idx] == value)
            {
                result.push_back(tuple); // Include matching tuple
            }
        }

        return result;
    }

    // Create B-tree index on specified column for fast lookups
    // Builds index from existing data and enables O(log n) searches
    void Table::createIndex(const string &column_name)
    {
        if (indexes.find(column_name) != indexes.end())
        {
            return; // Index already exists
        }

        // Create new B-tree index for this column
        auto index = make_unique<BTree<string, TupleId>>();

        // Build index from existing data - scan all tuples
        auto all_tuples = selectAll();
        size_t col_idx = 0;
        // Find column index in schema
        for (size_t i = 0; i < schema.columns.size(); i++)
        {
            if (schema.columns[i].name == column_name)
            {
                col_idx = i;
                break;
            }
        }

        // Insert each tuple's column value into index
        for (const auto &tuple : all_tuples)
        {
            if (col_idx < tuple.values.size())
            {
                // Convert tuple value to string key for B-tree storage
                string key = visit([](const auto &v)
                                   {
                if constexpr (is_same_v<decay_t<decltype(v)>, string>) {
                    return v;
                } else {
                    return to_string(v);
                } }, tuple.values[col_idx]);
                index->insert(key, tuple.id); // Add to B-tree index
            }
        }

        // Store index in table's index map
        indexes[column_name] = move(index);
    }

    // Fast indexed lookup - O(log n) search using B-tree index
    // Returns tuples matching exact value on indexed column
    vector<Tuple> Table::selectUsingIndex(const string &column, const Value &value)
    {
        auto index_it = indexes.find(column);
        if (index_it == indexes.end())
        {
            return {}; // No index available
        }

        // Convert search value to string key for index lookup
        string key = visit([](const auto &v)
                           {
        if constexpr (is_same_v<decay_t<decltype(v)>, string>) {
            return v;
        } else {
            return to_string(v);
        } }, value);

        // Search B-tree index for matching key
        auto tuple_id_ptr = index_it->second->search(key);
        if (!tuple_id_ptr)
        {
            return {}; // Not found in index
        }

        // Find the tuple with this ID - could be optimized with direct page access
        auto all_tuples = selectAll();
        for (const auto &tuple : all_tuples)
        {
            if (tuple.id == *tuple_id_ptr)
            {
                return {tuple}; // Return single matching tuple
            }
        }

        return {}; // Tuple ID not found (data inconsistency)
    }

    // Count total number of tuples in table by scanning all pages
    // Returns total row count across all data pages
    size_t Table::getTupleCount() const
    {
        size_t count = 0;
        PageId current_page = first_page_id;

        // Traverse all pages and sum tuple counts from headers
        while (current_page != 0)
        {
            auto frame = buffer_pool->getPage(current_page);
            PageHeader header;
            memcpy(&header, frame->data.data(), sizeof(PageHeader));
            count += header.tuple_count;     // Add tuples from this page
            current_page = header.next_page; // Move to next page
            buffer_pool->releasePage(current_page);
        }

        return count;
    }

    // Print table statistics for debugging and monitoring
    // Shows table name, tuple count, columns, and available indexes
    void Table::printStats() const
    {
        cout << "Table: " << name << endl;
        cout << "  Tuple count: " << getTupleCount() << endl;
        cout << "  Columns: ";
        for (const auto &column : schema.columns)
        {
            cout << column.name << " ";
        }
        cout << endl;
        cout << "  Indexes: ";
        for (const auto &[column_name, _] : indexes)
        {
            cout << column_name << " ";
        }
        cout << endl;
    }

    // StorageEngine implementation - manages multiple database tables

    // Constructor - initialize storage engine with database file path
    // Creates storage system that can manage multiple tables
    StorageEngine::StorageEngine(const string &db_file_path)
        : db_file_path(db_file_path)
    {
    }

    // Create new table with specified name and schema
    // Returns false if table already exists, true on success
    bool StorageEngine::createTable(const string &name, const Schema &schema)
    {
        if (tables.find(name) != tables.end())
        {
            return false; // Table already exists
        }

        // Create table-specific file path to avoid storage conflicts
        string table_file_path = db_file_path + "." + name;

        // Create new table instance and add to storage engine
        tables[name] = make_unique<Table>(name, schema, table_file_path);
        return true;
    }

    // Drop (delete) table by name - removes table and all its data
    // Returns false if table doesn't exist, true on successful deletion
    bool StorageEngine::dropTable(const string &name)
    {
        auto it = tables.find(name);
        if (it == tables.end())
        {
            return false; // Table doesn't exist
        }

        tables.erase(it); // Remove from table map
        return true;
    }

    // Get table pointer by name for internal operations
    // Returns nullptr if table doesn't exist
    Table *StorageEngine::getTable(const string &name)
    {
        auto it = tables.find(name);
        return it != tables.end() ? it->second.get() : nullptr;
    }

    // Insert tuple into specified table - delegates to table's insert method
    // Returns false if table doesn't exist or insert fails
    bool StorageEngine::insertTuple(const string &table_name, const Tuple &tuple)
    {
        auto table = getTable(table_name);
        if (!table)
        {
            return false; // Table not found
        }

        return table->insertTuple(tuple); // Delegate to table
    }

    // Select all tuples from specified table
    // Returns empty vector if table doesn't exist
    vector<Tuple> StorageEngine::selectAll(const string &table_name)
    {
        auto table = getTable(table_name);
        if (!table)
        {
            return {}; // Table not found
        }

        return table->selectAll(); // Delegate to table
    }

    // Select tuples matching WHERE condition from specified table
    // Uses indexed lookup if available, otherwise full table scan
    vector<Tuple> StorageEngine::selectWhere(const string &table_name,
                                             const string &column, const Value &value)
    {
        auto table = getTable(table_name);
        if (!table)
        {
            return {}; // Table not found
        }

        return table->selectWhere(column, value); // Delegate to table
    }

    // Create B-tree index on specified column of specified table
    // Enables fast O(log n) lookups on indexed column
    bool StorageEngine::createIndex(const string &table_name, const string &column_name)
    {
        auto table = getTable(table_name);
        if (!table)
        {
            return false; // Table not found
        }

        table->createIndex(column_name); // Delegate to table
        return true;
    }

    // Get list of all table names in the database
    // Returns vector of table name strings
    vector<string> StorageEngine::getTableNames() const
    {
        vector<string> names;
        for (const auto &[name, _] : tables)
        {
            names.push_back(name); // Collect table names
        }
        return names;
    }

    // Print comprehensive database statistics for monitoring
    // Shows table count and detailed stats for each table
    void StorageEngine::printStats() const
    {
        cout << "Storage Engine Statistics:" << endl;
        cout << "  Tables: " << tables.size() << endl;
        for (const auto &[name, table] : tables)
        {
            table->printStats(); // Print each table's stats
        }
    }

    // Flush all buffer pools from all tables to disk
    void StorageEngine::flushAllPages()
    {
        for (const auto &[name, table] : tables)
        {
            if (table && table->getBufferPool())
            {
                table->getBufferPool()->flushAllPages(); // Flush each table's buffer pool
            }
        }
    }

} // namespace db