#pragma once

#include "types.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <set>
#include <fstream>

using namespace std;

namespace db
{
    // Lock request structure - represents a transaction asking for access to a page
    struct LockRequest
    {
        PageId page_id;               // Which page does this transaction want to access?
        LockType lock_type;           // SHARED (read) or EXCLUSIVE (write) access?
        TransactionId transaction_id; // Which transaction is making this request?
        bool granted;                 // Has this lock been granted yet?

        // Constructor - create a new lock request (starts as not granted)
        LockRequest(PageId page_id, LockType lock_type, TransactionId transaction_id)
            : page_id(page_id), lock_type(lock_type), transaction_id(transaction_id), granted(false) {}
    };

    // Transaction information - tracks everything about one database transaction
    struct Transaction
    {
        TransactionId id;                               // Unique transaction identifier
        TransactionState state;                         // ACTIVE, COMMITTED, or ABORTED
        set<PageId> locked_pages;                       // Pages this transaction has locked
        vector<pair<PageId, vector<uint8_t>>> undo_log; // Original page contents (for rollback)

        // Constructor - create new transaction in ACTIVE state
        Transaction(TransactionId id) : id(id), state(TransactionState::ACTIVE) {}
    };

    // LockManager class - controls which transactions can access which pages
    // Prevents conflicts between concurrent transactions (ACID isolation)
    class LockManager
    {
    private:
        // Map each page to list of lock requests waiting for it
        unordered_map<PageId, vector<LockRequest>> lock_table;
        mutex lock_manager_mutex; // Thread safety

        // Check if a new lock request conflicts with existing locks
        bool canGrantLock(const LockRequest &request);

        // Actually grant a lock request (mark as granted)
        void grantLock(LockRequest &request);

    public:
        LockManager() = default;

        // Try to acquire a lock on a page for a transaction
        bool acquireLock(PageId page_id, LockType lock_type, TransactionId transaction_id);

        // Release a lock when transaction is done with the page
        void releaseLock(PageId page_id, TransactionId transaction_id);

        // Release all locks held by a transaction (called when transaction ends)
        void releaseAllLocks(TransactionId transaction_id);

        // Check if a transaction has a lock on a specific page
        bool hasLock(PageId page_id, TransactionId transaction_id);

        // Get list of all pages locked by a transaction
        vector<PageId> getLockedPages(TransactionId transaction_id);
    };

    // TransactionManager class - the main coordinator for ACID transactions
    // Ensures database operations are Atomic, Consistent, Isolated, and Durable
    class TransactionManager
    {
    private:
        unordered_map<TransactionId, unique_ptr<Transaction>> transactions; // All active transactions
        TransactionId next_transaction_id;                                  // ID generator
        LockManager lock_manager;                                           // Controls page access
        mutex transaction_mutex;                                            // Thread safety

        // Recovery system - transaction log for crash recovery
        string log_file_path; // Where to write transaction log
        fstream log_file;     // File stream for logging

        // Write an entry to the transaction log (for crash recovery)
        void writeLogEntry(const string &entry);

        // Write a checkpoint to speed up recovery
        void writeCheckpoint();

    public:
        // Constructor - initialize with path to transaction log file
        TransactionManager(const string &log_file_path);

        // Destructor - properly close log file
        ~TransactionManager();

        // Transaction management - the core ACID operations
        TransactionId beginTransaction();                     // Start new transaction
        bool commitTransaction(TransactionId transaction_id); // Make changes permanent
        bool abortTransaction(TransactionId transaction_id);  // Cancel and undo changes

        // Lock management - prevent concurrent access conflicts
        bool acquireLock(PageId page_id, LockType lock_type, TransactionId transaction_id);
        void releaseLock(PageId page_id, TransactionId transaction_id);

        // Recovery system - handle crashes and restore consistency
        void recover();    // Restore database after crash
        void checkpoint(); // Create recovery checkpoint

        // Utility functions - check transaction status
        bool isTransactionActive(TransactionId transaction_id);
        TransactionState getTransactionState(TransactionId transaction_id);
        vector<PageId> getTransactionLocks(TransactionId transaction_id);

        // Statistics and monitoring
        size_t getActiveTransactionCount() const;
        void printStats() const;
    };

    // WALManager class - Write-Ahead Logging for crash recovery
    // Ensures database can recover from crashes by logging changes before applying them
    class WALManager
    {
    private:
        string log_file_path; // Path to the log file
        fstream log_file;     // File stream for reading/writing
        mutex log_mutex;      // Thread safety for concurrent access

    public:
        // Constructor - initialize logging system with file path
        WALManager(const string &log_file_path);

        // Destructor - properly close log file
        ~WALManager();

        // Write-Ahead Logging - record operations before they happen
        void logBeginTransaction(TransactionId transaction_id);  // Log transaction start
        void logCommitTransaction(TransactionId transaction_id); // Log transaction commit
        void logAbortTransaction(TransactionId transaction_id);  // Log transaction abort

        // Log page modifications (old data for undo, new data for redo)
        void logPageWrite(TransactionId transaction_id, PageId page_id,
                          const vector<uint8_t> &old_data,
                          const vector<uint8_t> &new_data);

        // Recovery operations - restore database state after crash
        void recover();     // Read log and restore state
        void truncateLog(); // Remove old log entries

        // Utility functions
        void flush(); // Force log entries to disk
    };

} // namespace db