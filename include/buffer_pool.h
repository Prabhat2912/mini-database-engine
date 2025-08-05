#pragma once

#include "types.h"
#include <vector>
#include <unordered_map>
#include <list>
#include <memory>
#include <mutex>
#include <fstream>
#include <iostream>

using namespace std;

namespace db
{

    // Buffer frame structure - represents one page of data cached in memory
    // Think of this as a "slot" in RAM that holds a copy of disk data
    struct BufferFrame
    {
        PageId page_id;       // Which page from disk is stored here
        bool is_dirty;        // Has this page been modified since loading from disk?
        bool is_pinned;       // Is this page currently being used by someone?
        vector<uint8_t> data; // The actual 4KB of page data (raw bytes)

        // Constructor - creates an empty buffer frame ready to hold page data
        BufferFrame() : page_id(0), is_dirty(false), is_pinned(false)
        {
            data.resize(PAGE_SIZE); // Allocate exactly 4096 bytes for one page
        }

        // Reset this frame to empty state so it can be reused
        void clear()
        {
            page_id = 0;                       // No page loaded
            is_dirty = false;                  // No modifications
            is_pinned = false;                 // Not in use
            fill(data.begin(), data.end(), 0); // Zero out all the data bytes
        }
    };

    // BufferPool class - manages database pages in memory for faster access
    // This is like a smart cache that keeps frequently used disk pages in RAM
    class BufferPool
    {
    private:
        // Core data structures for managing memory frames
        vector<unique_ptr<BufferFrame>> frames;          // Array of 1000 memory slots for pages
        unordered_map<PageId, BufferFrameId> page_table; // Quick lookup: "which frame has page X?"
        list<BufferFrameId> lru_list;                    // Tracks which frames were used recently
        mutex buffer_pool_mutex;                         // Thread safety for concurrent access

        // File I/O for reading/writing pages to disk
        string db_file_path; // Path to the database file on disk
        fstream db_file;     // File handle for reading/writing pages

        // Performance tracking - helps us know how well our cache is working
        size_t page_hits;   // How many times we found page already in memory
        size_t page_misses; // How many times we had to load page from disk

        // Find a victim frame to evict when memory is full
        // Uses LRU (Least Recently Used) algorithm - evict the oldest unused page
        BufferFrameId findVictim()
        {
            // Walk through LRU list from oldest to newest
            for (auto it = lru_list.begin(); it != lru_list.end(); it++)
            {
                // Can only evict frames that aren't currently being used
                if (!frames[*it]->is_pinned)
                {
                    BufferFrameId victim = *it; // Found our victim!
                    lru_list.erase(it);         // Remove from LRU tracking
                    return victim;              // Return the frame ID to reuse
                }
            }
            // If we get here, all frames are pinned (in use) - this is bad!
            throw runtime_error("No unpinned frames available");
        }

        // Evict a frame from memory, saving to disk if needed
        void evictFrame(BufferFrameId frame_id)
        {
            auto &frame = frames[frame_id];

            // If page was modified, we must write it back to disk before evicting
            if (frame->is_dirty)
            {
                writePageToDisk(frame->page_id, frame->data);
            }

            // Remove the page-to-frame mapping and clean up the frame
            page_table.erase(frame->page_id);
            frame->clear(); // Reset frame to empty state
        }

        // Load a page from disk into memory buffer
        void readPageFromDisk(PageId page_id, vector<uint8_t> &data)
        {
            // Calculate byte offset: page_id * 4096 bytes per page
            db_file.seekg(static_cast<streamoff>(page_id * PAGE_SIZE));
            // Read exactly 4096 bytes into our buffer
            db_file.read(reinterpret_cast<char *>(data.data()), PAGE_SIZE);

            // If read failed, this might be a new page that doesn't exist yet
            if (db_file.fail())
            {
                // Initialize new page with all zeros
                fill(data.begin(), data.end(), 0);
            }
        }

        // Write a page from memory buffer back to disk
        void writePageToDisk(PageId page_id, const vector<uint8_t> &data)
        {
            // Clear any error flags first
            db_file.clear();

            // Get current file size
            db_file.seekg(0, ios::end);
            if (db_file.fail())
            {
                db_file.clear();
                db_file.seekg(0, ios::end);
            }

            streampos pos = db_file.tellg();
            size_t current_size = (pos == -1) ? 0 : static_cast<size_t>(pos);

            // Calculate target position
            size_t target_position = page_id * PAGE_SIZE;

            // If target position is beyond file end, extend the file
            if (target_position >= current_size)
            {
                // Clear errors and seek to end
                db_file.clear();
                db_file.seekp(0, ios::end);
                size_t bytes_to_add = target_position + PAGE_SIZE - current_size;
                vector<uint8_t> padding(bytes_to_add, 0);
                db_file.write(reinterpret_cast<const char *>(padding.data()), bytes_to_add);
                db_file.flush();
            }

            // Clear errors and seek to target position
            db_file.clear();
            db_file.seekp(target_position);

            // Write exactly 4096 bytes to disk
            db_file.write(reinterpret_cast<const char *>(data.data()), PAGE_SIZE);

            db_file.flush(); // Force write to disk immediately
        }

        // Update LRU (Least Recently Used) tracking when a frame is accessed
        // Move the accessed frame to front of list (most recently used position)
        void updateLRU(BufferFrameId frame_id)
        {
            lru_list.remove(frame_id);     // Remove from current position in list
            lru_list.push_front(frame_id); // Add to front (marks as most recently used)
        }

    public:
        // Constructor - Initialize the buffer pool with a database file
        BufferPool(const string &file_path)
            : db_file_path(file_path), page_hits(0), page_misses(0)
        {
            // Create 1000 empty buffer frames (our memory cache slots)
            frames.reserve(BUFFER_POOL_SIZE);             // Reserve space for efficiency
            for (size_t i = 0; i < BUFFER_POOL_SIZE; i++) // Create each frame
            {
                frames.push_back(make_unique<BufferFrame>()); // Make a new empty frame
                lru_list.push_back(i);                        // Add to LRU tracking list
            }

            // Open the database file for reading and writing
            // Try to open existing file first
            db_file.open(db_file_path, ios::binary | ios::in | ios::out);
            if (!db_file.is_open())
            {
                // File doesn't exist, create it
                db_file.clear();                                    // Clear any error flags
                db_file.open(db_file_path, ios::binary | ios::out); // Create new file
                db_file.close();                                    // Close it
                // Reopen for read/write
                db_file.open(db_file_path, ios::binary | ios::in | ios::out);
            }
        }

        // Destructor - Clean up when BufferPool is destroyed
        ~BufferPool()
        {
            // Before shutting down, save all modified pages back to disk
            for (auto &frame : frames)
            {
                if (frame->is_dirty) // If page was modified
                {
                    writePageToDisk(frame->page_id, frame->data); // Save it to disk
                }
            }
            db_file.close(); // Close the database file
        }

        // Main function to get a page - loads from disk if not in memory
        // This is the heart of the buffer pool system!
        BufferFrame *getPage(PageId page_id)
        {
            lock_guard<mutex> lock(buffer_pool_mutex); // Thread safety

            // Step 1: Check if page is already in memory (cache hit)
            auto it = page_table.find(page_id);
            if (it != page_table.end()) // Found in memory!
            {
                BufferFrameId frame_id = it->second; // Get the frame number
                auto &frame = frames[frame_id];      // Get pointer to frame

                frame->is_pinned = true; // Mark as "in use"
                updateLRU(frame_id);     // Mark as recently accessed
                page_hits++;             // Update statistics

                return frame.get(); // Return the frame
            }

            // Step 2: Page not in memory (cache miss) - need to load from disk
            page_misses++; // Update statistics

            // Find a frame to use (might need to evict an old page)
            BufferFrameId victim_frame = findVictim();
            auto &frame = frames[victim_frame];

            // If this frame has an old page, save it first if needed
            if (frame->page_id != 0)
            {
                evictFrame(victim_frame); // Save old page and clean frame
            }

            // Step 3: Load the requested page from disk into the frame
            frame->page_id = page_id;               // Record which page this is
            frame->is_pinned = true;                // Mark as in use
            frame->is_dirty = false;                // Just loaded, so not modified
            readPageFromDisk(page_id, frame->data); // Read 4KB from disk

            // Step 4: Update our tracking structures
            page_table[page_id] = victim_frame; // Map page to frame
            lru_list.push_front(victim_frame);  // Mark as most recently used

            return frame.get(); // Return the loaded page
        }

        // Release a page when done using it (unpinning)
        void releasePage(PageId page_id)
        {
            lock_guard<mutex> lock(buffer_pool_mutex); // Thread safety

            auto it = page_table.find(page_id); // Find the page
            if (it != page_table.end())         // If page is in memory
            {
                BufferFrameId frame_id = it->second; // Get frame number
                frames[frame_id]->is_pinned = false; // Mark as not in use
            }
        }

        // Mark a page as dirty (modified) without unpinning it
        void markDirty(PageId page_id)
        {
            lock_guard<mutex> lock(buffer_pool_mutex); // Thread safety

            auto it = page_table.find(page_id); // Find the page
            if (it != page_table.end())         // If page is in memory
            {
                frames[it->second]->is_dirty = true; // Mark it as modified
            }
        }

        // Force write a specific page back to disk immediately
        void flushPage(PageId page_id)
        {
            lock_guard<mutex> lock(buffer_pool_mutex); // Thread safety

            auto it = page_table.find(page_id); // Find the page
            if (it != page_table.end())         // If page is in memory
            {
                auto &frame = frames[it->second]; // Get the frame
                if (frame->is_dirty)              // If page was modified
                {
                    writePageToDisk(page_id, frame->data); // Write to disk
                    frame->is_dirty = false;               // Mark as clean (saved)
                }
            }
        }

        // Force write all modified pages back to disk
        void flushAllPages()
        {
            lock_guard<mutex> lock(buffer_pool_mutex); // Thread safety

            // Loop through all frames and save any dirty ones
            for (auto &frame : frames)
            {
                if (frame->is_dirty) // If page was modified
                {
                    writePageToDisk(frame->page_id, frame->data); // Write to disk
                    frame->is_dirty = false;                      // Mark as clean
                }
            }
        }

        // Performance statistics - helps us know how well our cache works

        // Calculate cache hit ratio (percentage of requests served from memory)
        double getHitRatio() const
        {
            size_t total = page_hits + page_misses; // Total requests
            return total > 0 ? static_cast<double>(page_hits) / total : 0.0;
        }

        size_t getPageHits() const { return page_hits; }     // Getter for hits
        size_t getPageMisses() const { return page_misses; } // Getter for misses

        // Print detailed statistics about buffer pool performance
        void printStats() const
        {
            cout << "Buffer Pool Statistics:" << endl;
            cout << "  Page hits: " << page_hits << endl;
            cout << "  Page misses: " << page_misses << endl;
            cout << "  Hit ratio: " << (getHitRatio() * 100) << "%" << endl;
            cout << "  Pinned frames: ";
            size_t pinned = 0;
            // Count how many frames are currently in use
            for (const auto &frame : frames)
            {
                if (frame->is_pinned)
                    pinned++; // Count pinned frames
            }
            cout << pinned << endl;
        }
    };

} // namespace db