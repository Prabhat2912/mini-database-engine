#include "transaction_manager.h"
#include <iostream>
#include <algorithm>

using namespace std;

namespace db
{
    // LockManager implementation - controls concurrent access to database pages

    // Check if a new lock request can be granted without conflicts
    // Shared locks are compatible with other shared locks, but exclusive locks conflict with everything
    bool LockManager::canGrantLock(const LockRequest &request)
    {
        auto &requests = lock_table[request.page_id];

        // Check all existing granted locks on this page
        for (const auto &existing_request : requests)
        {
            if (existing_request.granted)
            {
                // Check lock compatibility rules
                if (request.lock_type == LockType::EXCLUSIVE ||
                    existing_request.lock_type == LockType::EXCLUSIVE)
                {
                    return false; // Incompatible locks found
                }
            }
        }

        return true; // No conflicts, can grant lock
    }

    // Actually grant a lock request (mark it as granted)
    void LockManager::grantLock(LockRequest &request)
    {
        request.granted = true;
    }

    // Try to acquire a lock on a page for a transaction
    // Handles lock upgrades and compatibility checking
    bool LockManager::acquireLock(PageId page_id, LockType lock_type, TransactionId transaction_id)
    {
        lock_guard<mutex> lock(lock_manager_mutex); // Thread safety

        // Check if transaction already has a lock on this page
        auto &requests = lock_table[page_id];
        for (auto &request : requests)
        {
            if (request.transaction_id == transaction_id)
            {
                // Handle lock upgrade (shared to exclusive)
                if (request.lock_type == LockType::SHARED && lock_type == LockType::EXCLUSIVE)
                {
                    if (canGrantLock(LockRequest(page_id, LockType::EXCLUSIVE, transaction_id)))
                    {
                        request.lock_type = LockType::EXCLUSIVE;
                        return true;
                    }
                }
                return request.granted; // Already have appropriate lock
            }
        }

        // Create new lock request for this transaction
        LockRequest new_request(page_id, lock_type, transaction_id);

        if (canGrantLock(new_request))
        {
            grantLock(new_request); // Grant immediately if no conflicts
            requests.push_back(new_request);
            return true;
        }
        else
        {
            // Add to waiting queue (simplified - real implementation would handle waiting)
            requests.push_back(new_request);
            return false; // Lock conflicts, request denied
        }
    }

    // Release a specific lock held by a transaction
    void LockManager::releaseLock(PageId page_id, TransactionId transaction_id)
    {
        lock_guard<mutex> lock(lock_manager_mutex); // Thread safety

        auto &requests = lock_table[page_id];
        // Remove all lock requests from this transaction on this page
        requests.erase(
            remove_if(requests.begin(), requests.end(),
                      [transaction_id](const LockRequest &req)
                      {
                          return req.transaction_id == transaction_id;
                      }),
            requests.end());

        // Clean up empty entries to save memory
        if (requests.empty())
        {
            lock_table.erase(page_id);
        }
    }

    // Release all locks held by a transaction (called when transaction ends)
    void LockManager::releaseAllLocks(TransactionId transaction_id)
    {
        lock_guard<mutex> lock(lock_manager_mutex); // Thread safety

        vector<PageId> pages_to_release;
        // Find all pages that this transaction has locks on
        for (const auto &[page_id, requests] : lock_table)
        {
            for (const auto &request : requests)
            {
                if (request.transaction_id == transaction_id)
                {
                    pages_to_release.push_back(page_id);
                    break; // Found lock on this page
                }
            }
        }

        // Release locks on all pages
        for (PageId page_id : pages_to_release)
        {
            releaseLock(page_id, transaction_id);
        }
    }

    // Check if a transaction has a lock on a specific page
    bool LockManager::hasLock(PageId page_id, TransactionId transaction_id)
    {
        auto it = lock_table.find(page_id);
        if (it == lock_table.end())
        {
            return false; // No locks on this page
        }

        // Check if transaction has granted lock on this page
        for (const auto &request : it->second)
        {
            if (request.transaction_id == transaction_id && request.granted)
            {
                return true;
            }
        }

        return false;
    }

    // Get list of all pages locked by a specific transaction
    vector<PageId> LockManager::getLockedPages(TransactionId transaction_id)
    {
        vector<PageId> locked_pages;

        // Scan all lock tables to find pages locked by this transaction
        for (const auto &[page_id, requests] : lock_table)
        {
            for (const auto &request : requests)
            {
                if (request.transaction_id == transaction_id && request.granted)
                {
                    locked_pages.push_back(page_id);
                    break; // Found lock, move to next page
                }
            }
        }

        return locked_pages;
    }

    // TransactionManager implementation - main ACID transaction coordinator
    // Constructor - initialize transaction management with logging
    TransactionManager::TransactionManager(const string &log_file_path)
        : next_transaction_id(1), log_file_path(log_file_path)
    {
        // Open transaction log file for crash recovery
        log_file.open(log_file_path, ios::app | ios::binary);
    }

    // Destructor - properly close log file
    TransactionManager::~TransactionManager()
    {
        if (log_file.is_open())
        {
            log_file.close();
        }
    }

    // Start a new transaction and assign unique ID
    TransactionId TransactionManager::beginTransaction()
    {
        lock_guard<mutex> lock(transaction_mutex); // Thread safety

        TransactionId transaction_id = next_transaction_id++; // Generate unique ID
        transactions[transaction_id] = make_unique<Transaction>(transaction_id);

        writeLogEntry("BEGIN " + to_string(transaction_id)); // Log transaction start

        return transaction_id;
    }

    // Commit a transaction (make all changes permanent)
    bool TransactionManager::commitTransaction(TransactionId transaction_id)
    {
        lock_guard<mutex> lock(transaction_mutex); // Thread safety

        auto it = transactions.find(transaction_id);
        if (it == transactions.end())
        {
            return false; // Transaction doesn't exist
        }

        auto &transaction = it->second;
        if (transaction->state != TransactionState::ACTIVE)
        {
            return false; // Transaction not active
        }

        // Release all locks held by this transaction
        lock_manager.releaseAllLocks(transaction_id);

        // Update transaction state to committed
        transaction->state = TransactionState::COMMITTED;

        writeLogEntry("COMMIT " + to_string(transaction_id)); // Log commit

        return true;
    }

    // Abort a transaction (cancel and undo all changes)
    bool TransactionManager::abortTransaction(TransactionId transaction_id)
    {
        lock_guard<mutex> lock(transaction_mutex); // Thread safety

        auto it = transactions.find(transaction_id);
        if (it == transactions.end())
        {
            return false; // Transaction doesn't exist
        }

        auto &transaction = it->second;
        if (transaction->state != TransactionState::ACTIVE)
        {
            return false; // Transaction not active
        }

        // Release all locks held by this transaction
        lock_manager.releaseAllLocks(transaction_id);

        // Update transaction state to aborted
        transaction->state = TransactionState::ABORTED;

        writeLogEntry("ABORT " + to_string(transaction_id)); // Log abort

        return true;
    }

    // Acquire a lock for a transaction (delegates to LockManager)
    bool TransactionManager::acquireLock(PageId page_id, LockType lock_type, TransactionId transaction_id)
    {
        return lock_manager.acquireLock(page_id, lock_type, transaction_id);
    }

    // Release a lock for a transaction (delegates to LockManager)
    void TransactionManager::releaseLock(PageId page_id, TransactionId transaction_id)
    {
        lock_manager.releaseLock(page_id, transaction_id);
    }

    // Write an entry to the transaction log for crash recovery
    void TransactionManager::writeLogEntry(const string &entry)
    {
        if (log_file.is_open())
        {
            log_file << entry << endl; // Write log entry
            log_file.flush();          // Force to disk immediately
        }
    }

    // Write a checkpoint marker to the log
    void TransactionManager::writeCheckpoint()
    {
        writeLogEntry("CHECKPOINT");
    }

    // Recover database state from transaction log after crash
    void TransactionManager::recover()
    {
        // Simple recovery implementation
        // In a real system, this would replay the log to restore state
        cout << "Recovery completed" << endl;
    }

    // Create a checkpoint (save current state for faster recovery)
    void TransactionManager::checkpoint()
    {
        writeCheckpoint();
        cout << "Checkpoint written" << endl;
    }

    // Check if a transaction is currently active
    bool TransactionManager::isTransactionActive(TransactionId transaction_id)
    {
        auto it = transactions.find(transaction_id);
        return it != transactions.end() && it->second->state == TransactionState::ACTIVE;
    }

    // Get the current state of a transaction
    TransactionState TransactionManager::getTransactionState(TransactionId transaction_id)
    {
        auto it = transactions.find(transaction_id);
        return it != transactions.end() ? it->second->state : TransactionState::ABORTED;
    }

    // Get list of pages locked by a specific transaction
    vector<PageId> TransactionManager::getTransactionLocks(TransactionId transaction_id)
    {
        return lock_manager.getLockedPages(transaction_id);
    }

    // Count how many transactions are currently active
    size_t TransactionManager::getActiveTransactionCount() const
    {
        size_t count = 0;
        for (const auto &[_, transaction] : transactions)
        {
            if (transaction->state == TransactionState::ACTIVE)
            {
                count++;
            }
        }
        return count;
    }

    // Display transaction manager statistics
    void TransactionManager::printStats() const
    {
        cout << "Transaction Manager Statistics:" << endl;
        cout << "  Active transactions: " << getActiveTransactionCount() << endl;
        cout << "  Total transactions: " << transactions.size() << endl;
    }

    // WALManager implementation - Write-Ahead Logging for crash recovery
    // Constructor - initialize WAL system with log file
    WALManager::WALManager(const string &log_file_path)
        : log_file_path(log_file_path)
    {
        // Open write-ahead log file
        log_file.open(log_file_path, ios::app | ios::binary);
    }

    // Destructor - properly close log file
    WALManager::~WALManager()
    {
        if (log_file.is_open())
        {
            log_file.close();
        }
    }

    // Log the beginning of a transaction
    void WALManager::logBeginTransaction(TransactionId transaction_id)
    {
        lock_guard<mutex> lock(log_mutex); // Thread safety
        log_file << "BEGIN " << transaction_id << endl;
        flush(); // Force to disk
    }

    // Log the commit of a transaction
    void WALManager::logCommitTransaction(TransactionId transaction_id)
    {
        lock_guard<mutex> lock(log_mutex); // Thread safety
        log_file << "COMMIT " << transaction_id << endl;
        flush(); // Force to disk
    }

    // Log the abort of a transaction
    void WALManager::logAbortTransaction(TransactionId transaction_id)
    {
        lock_guard<mutex> lock(log_mutex); // Thread safety
        log_file << "ABORT " << transaction_id << endl;
        flush(); // Force to disk
    }

    // Log a page write operation (before and after data for undo/redo)
    void WALManager::logPageWrite(TransactionId transaction_id, PageId page_id,
                                  const vector<uint8_t> &old_data,
                                  const vector<uint8_t> &new_data)
    {
        lock_guard<mutex> lock(log_mutex); // Thread safety
        log_file << "WRITE " << transaction_id << " " << page_id << " ";
        // Write old data (for undo) and new data (for redo)
        log_file.write(reinterpret_cast<const char *>(old_data.data()), old_data.size());
        log_file.write(reinterpret_cast<const char *>(new_data.data()), new_data.size());
        log_file << endl;
        flush(); // Force to disk
    }

    // Recover database state by replaying the write-ahead log
    void WALManager::recover()
    {
        // Simple recovery implementation
        // In a real system, this would read the log and replay operations
        cout << "WAL Recovery completed" << endl;
    }

    // Truncate the log file (remove old entries after checkpoint)
    void WALManager::truncateLog()
    {
        lock_guard<mutex> lock(log_mutex); // Thread safety
        log_file.close();
        log_file.open(log_file_path, ios::trunc | ios::binary); // Truncate file
    }

    // Force all pending log entries to disk
    void WALManager::flush()
    {
        if (log_file.is_open())
        {
            log_file.flush(); // Ensure data reaches disk
        }
    }

} // namespace db