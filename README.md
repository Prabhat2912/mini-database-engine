# Mini Database Engine

A complete, production-ready database engine implemented in C++ featuring ACID transactions, B-tree indexing, buffer pool management, Write-Ahead Logging (WAL), and full data persistence. This educational project demonstrates core database management system concepts with clean, well-documented code.

## ✨ Features

- **🗃️ Complete SQL Support**: CREATE TABLE, INSERT, SELECT with type safety
- **💾 Data Persistence**: Full durability - data survives database restarts
- **⚡ Buffer Pool Management**: LRU-based memory cache with 4KB pages
- **🌳 B-tree Indexing**: Fast data retrieval and range queries
- **🔒 ACID Transactions**: Begin, Commit, Rollback with Write-Ahead Logging
- **📊 Performance Monitoring**: Built-in statistics and verbose logging
- **🏗️ Dynamic Schema**: Runtime table creation with metadata persistence
- **🧵 Thread Safety**: Concurrent access protection with mutexes

## 🚀 Quick Start

### Building the Project

#### Recommended: One-command build (cross-platform)

For Linux/macOS:
```sh
./build_db_engine.sh
```

For Windows:
```bat
build_db_engine.bat
```

These scripts will check for CMake, create all required directories, and build the project automatically.

#### Manual build (if you prefer)
```bash
# Clone and build
mkdir build && cd build
cmake ..
cmake --build .

# Create the database directory (required for data persistence)
cd ..
mkdir -p db

# Run the database engine
cd build
./db_engine
```

### Basic Usage

```sql
-- Enable detailed logging (optional)
VERBOSE ON

-- Create a table with multiple data types
CREATE TABLE students (id INTEGER, name VARCHAR, age INTEGER, gpa DOUBLE, active BOOLEAN)

-- Insert some data
INSERT INTO students VALUES (1, 'Alice Johnson', 20, 3.8, true)
INSERT INTO students VALUES (2, 'Bob Smith', 22, 3.2, true)

-- Query the data
SELECT * FROM students

-- View performance statistics
STATS

-- Exit (automatically saves all data)
EXIT
```

## 🧪 Testing

### Run Comprehensive Tests

```bash
# Full functionality test
Get-Content tests\comprehensive_test.txt | ./build/db_engine

# Test data persistence across sessions
Get-Content tests\comprehensive_test.txt | ./build/db_engine
Get-Content tests\persistence_test.txt | ./build/db_engine  # Verify data survived restart
```

The test suite covers:

- ✅ Table creation with multiple data types
- ✅ Data insertion and retrieval
- ✅ Buffer pool performance
- ✅ Transaction logging
- ✅ Data persistence across restarts
- ✅ System statistics and monitoring

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Database Engine                          │
├─────────────────────────────────────────────────────────────┤
│  Query Parser & Executor  │  Parses SQL and executes ops    │
├─────────────────────────────────────────────────────────────┤
│     Storage Engine        │  Manages tables and schemas     │
├─────────────────────────────────────────────────────────────┤
│     Index Manager         │  B-tree indexes for fast lookup │
├─────────────────────────────────────────────────────────────┤
│   Transaction Manager     │  ACID compliance + WAL logging  │
├─────────────────────────────────────────────────────────────┤
│     Buffer Pool           │  LRU cache (1000 × 4KB pages)   │
├─────────────────────────────────────────────────────────────┤
│     Disk Storage          │  .db files + .meta metadata     │
└─────────────────────────────────────────────────────────────┘
```

### Key Components

#### 1. **Query Parser & Executor** (`query_parser.cpp`)

- Parses SQL statements into Abstract Syntax Trees (AST)
- Supports CREATE TABLE, INSERT, SELECT operations
- Type validation and error handling

#### 2. **Storage Engine** (`storage_engine.cpp`)

- Manages table structures and data storage
- Handles schema persistence via `.meta` files
- Coordinates buffer pool operations

#### 3. **Buffer Pool** (`buffer_pool.cpp`)

- LRU-based page replacement algorithm
- 4KB page size with 1000-frame memory cache
- Automatic dirty page flushing to disk
- Thread-safe concurrent access

#### 4. **Transaction Manager** (`transaction_manager.cpp`)

- Write-Ahead Logging (WAL) for durability
- ACID transaction support
- Recovery mechanisms for crash consistency

#### 5. **B-tree Index Manager** (`index_manager.cpp`)

- Balanced tree structures for fast data access
- Range query support
- Automatic index maintenance

## 💾 Data Persistence

The database engine provides full ACID durability:

### File Structure

```
project/
├── build/           # Build artifacts
│   └── db_engine.exe
├── db/              # Database files
│   ├── test.db      # Binary data pages (4KB each)
│   ├── test.db.meta # Table schema metadata
│   └── wal.log      # Write-Ahead Log for recovery
├── tests/           # Test files
│   ├── comprehensive_test.txt
│   └── persistence_test.txt
├── src/             # Source code
└── include/         # Header files
```

### Persistence Process

1. **Data Modification**: Pages marked dirty in buffer pool
2. **Checkpoint Trigger**: On EXIT or manual checkpoint
3. **Page Flushing**: All dirty pages written to `.db` file
4. **Metadata Save**: Schema saved to `.meta` file
5. **WAL Checkpoint**: Transaction log updated

### Data Recovery

- Automatic schema loading from `.meta` files on startup
- Buffer pool loads pages on-demand from `.db` files
- WAL replay for crash recovery (if needed)

## 📊 Performance Features

### Built-in Monitoring

```sql
-- View buffer pool statistics
STATS

-- Enable detailed operation logging
VERBOSE ON

-- View transaction history
LOGS
```

### Buffer Pool Metrics

- **Hit Ratio**: Percentage of pages served from memory
- **Page Hits/Misses**: Cache performance counters
- **Pinned Frames**: Currently active pages
- **LRU Effectiveness**: Memory utilization tracking

## 🔧 Supported SQL Operations

### Data Definition Language (DDL)

```sql
CREATE TABLE table_name (
    column1 INTEGER,
    column2 VARCHAR,
    column3 DOUBLE,
    column4 BOOLEAN
)
```

### Data Manipulation Language (DML)

```sql
-- Insert data
INSERT INTO table_name VALUES (1, 'text', 3.14, true)

-- Query data
SELECT * FROM table_name
SELECT column1, column2 FROM table_name
```

### Utility Commands

```sql
VERBOSE ON/OFF    -- Toggle detailed logging
STATS             -- Show performance statistics
LOGS              -- View transaction log entries
HELP              -- Show available commands
EXIT              -- Save all data and quit
```

## 🎯 Supported Data Types

| Type    | Description             | Example Values          |
| ------- | ----------------------- | ----------------------- |
| INTEGER | 32-bit signed integers  | `42`, `-1`, `0`         |
| VARCHAR | Variable-length strings | `'Hello'`, `'Database'` |
| DOUBLE  | 64-bit floating point   | `3.14`, `-2.5`          |
| BOOLEAN | True/false values       | `true`, `false`         |

## 🔍 Educational Value

This project demonstrates:

### Database Concepts

- **Storage Management**: Page-based storage with buffer pools
- **Indexing**: B-tree data structures for efficient queries
- **Concurrency**: Multi-threaded access patterns
- **Durability**: Write-Ahead Logging and checkpointing
- **Memory Management**: LRU cache replacement policies

### Software Engineering

- **Modular Design**: Clean separation of concerns
- **Resource Management**: RAII and smart pointers
- **Error Handling**: Comprehensive exception safety
- **Performance Optimization**: Efficient algorithms and data structures

## 🛠️ Development

### Project Structure

```
mini-database-engine/
├── include/          # Header files
│   ├── buffer_pool.h
│   ├── database_engine.h
│   ├── storage_engine.h
│   └── ...
├── src/              # Implementation files
│   ├── main.cpp
│   ├── buffer_pool.cpp
│   └── ...
├── tests/            # Test files
│   ├── comprehensive_test.txt
│   └── persistence_test.txt
├── db/               # Database files (created at runtime)
├── build/            # Build artifacts
├── CMakeLists.txt    # Build configuration
└── README.md         # This file
```

### Build Requirements

- **C++17** or later
- **CMake 3.10+**
- **Windows/Linux/macOS** compatible

### Adding Features

The modular architecture makes it easy to extend:

- Add new SQL commands in `query_parser.cpp`
- Implement new data types in `types.h`
- Extend indexing in `index_manager.cpp`
- Enhance transaction features in `transaction_manager.cpp`

## 📈 Performance Characteristics

- **Memory Usage**: ~4MB buffer pool + metadata overhead
- **Disk I/O**: 4KB page size optimized for SSD/HDD
- **Cache Hit Ratio**: Typically >90% for workloads with locality
- **Transaction Throughput**: Limited by disk I/O for durability
- **Storage Efficiency**: Minimal overhead, compact binary format

## 🤝 Contributing

This is an educational project! Contributions welcome:

1. **Bug Reports**: Issues with data corruption or crashes
2. **Feature Requests**: Additional SQL operations or optimizations
3. **Documentation**: Improvements to comments and guides
4. **Testing**: Additional test cases and edge conditions

## 📚 Learning Resources

To understand the concepts implemented here:

- **Database Systems Concepts** by Silberschatz, Galvin, Gagne
- **Database Management Systems** by Ramakrishnan & Gehrke
- **Modern Database Systems** by Kim, et al.
- **SQLite Architecture** documentation for real-world examples

## ⚠️ Educational Notice

This database engine is designed for learning and demonstration purposes. While it implements many production database features (ACID transactions, WAL, buffer pool management), it is not optimized for production workloads requiring:

- High concurrency (thousands of simultaneous connections)
- Petabyte-scale storage
- Advanced query optimization
- Distributed/replicated architectures

For production use, consider established systems like PostgreSQL, MySQL, or SQLite.

---

**Built with ❤️ for learning database internals** • C++17 • Cross-platform • ACID-compliant

```
"INSERT INTO users..." → TokenStream → AST
Result: InsertNode{table: "users", values: [1, "Alice", 25]}
```

2. **Execution Planning** 📋

   ```
   InsertNode → Find table "users" → Validate data types → Create tuple
   ```

3. **Storage Layer** 💾

   ```
   Calculate tuple size (17 bytes) → Find page with space → Serialize data
   Page Layout: [Header|Tuple1|Tuple2|...] → Update page header
   ```

4. **Buffer Management** 🧠

   ```
   Load page into memory → Modify in buffer → Mark as dirty → LRU tracking
   ```

5. **Index Updates** 🗂️

   ```
   For each indexed column → Convert value to key → Update B-tree
   Example: age column → B-tree.insert("25", tuple_id_001)
   ```

6. **Transaction Safety** 🔒
   ```
   WAL entry: "INSERT users tuple_001" → Commit → Persist to disk
   ```

### Example: `SELECT * FROM users WHERE age = 25` (with index)

1. **Parse Query** → `SelectNode{table: "users", where: {column: "age", value: 25}}`

2. **Execution Strategy**:

   ```cpp
   if (table.hasIndex("age")) {
       // O(log n) - Fast path using B-tree
       tuple_id = btree.search("25");
       return findTupleById(tuple_id);
   } else {
       // O(n) - Full table scan
       return scanAllTuples().filter(age == 25);
   }
   ```

3. **B-tree Lookup**:

   ```
   Root Node [key: "30"]
       ├── Left: [keys: "10", "20", "25"] → Found "25" → tuple_id: 001
       └── Right: [keys: "35", "40", "50"]
   ```

4. **Result**: Return tuple `{id: 1, name: "Alice", age: 25}` in O(log n) time!

## 📚 Core Features

| Feature                 | Description                          | Educational Value              |
| ----------------------- | ------------------------------------ | ------------------------------ |
| **B-tree Indexing**     | Order-5 B-trees for O(log n) lookups | Learn balanced tree algorithms |
| **Buffer Pool**         | LRU cache with 1000 4KB pages        | Understand memory management   |
| **ACID Transactions**   | Full transaction support with WAL    | Learn concurrency control      |
| **SQL Parser**          | Recursive descent parser             | Understand language processing |
| **Page Storage**        | 4KB pages with tuple serialization   | Learn storage systems          |
| **Multiple Data Types** | INTEGER, VARCHAR, BOOLEAN, DOUBLE    | Understand type systems        |

## 🎯 Supported SQL Commands

### Data Definition Language (DDL)

```sql
CREATE TABLE users (id INTEGER, name VARCHAR(50), age INTEGER, active BOOLEAN)
DROP TABLE users
CREATE INDEX users.age
```

### Data Manipulation Language (DML)

```sql
INSERT INTO users VALUES (1, 'Alice', 25, true)
SELECT * FROM users
SELECT * FROM users WHERE age = 25
UPDATE users SET age = 26 WHERE id = 1
DELETE FROM users WHERE id = 1
```

### Transaction Control

```sql
BEGIN
INSERT INTO users VALUES (2, 'Bob', 30, true)
COMMIT

BEGIN
INSERT INTO users VALUES (3, 'Charlie', 35, true)
ROLLBACK
```

## 🛠️ Technical Specifications

### Performance Characteristics

- **Storage**: 4KB pages, variable-length tuples
- **Memory**: 4MB buffer pool (1000 pages) with LRU eviction
- **Indexing**: O(log n) B-tree lookups
- **Concurrency**: ACID transactions with two-phase locking
- **Recovery**: Write-ahead logging (WAL) for crash recovery

### File Structure

```
mini-database-engine/
├── 📁 include/          # Header files (8 files)
│   ├── types.h          # Core data types and structures
│   ├── buffer_pool.h    # Memory management
│   ├── b_tree.h         # B-tree index implementation
│   ├── storage_engine.h # Table and tuple management
│   ├── query_parser.h   # SQL parsing and AST
│   ├── transaction_manager.h # ACID transaction support
│   ├── index_manager.h  # Index coordination
│   └── database_engine.h # Main database interface
├── 📁 src/              # Implementation files (6 files)
│   ├── main.cpp         # Interactive database shell
│   ├── database_engine.cpp
│   ├── storage_engine.cpp
│   ├── query_parser.cpp
│   ├── transaction_manager.cpp
│   └── index_manager.cpp
├── 📄 CMakeLists.txt    # Build configuration
├── 📄 README.md         # This file
└── 📄 DOCUMENTATION.md  # Detailed technical documentation
```

## 📖 Learning Resources

### 🔗 For Detailed Technical Information

- **[DOCUMENTATION.md](./DOCUMENTATION.md)** - Complete technical deep-dive
  - Component architecture and data flow
  - Implementation details and memory layouts
  - Q&A section covering common questions
  - Learning exercises and extension ideas

### 🎓 Educational Value

This project teaches:

- **Database Internals**: How SQL queries become disk operations
- **Data Structures**: B-trees, hash tables, LRU caches
- **Systems Programming**: Memory management, file I/O, concurrency
- **Algorithm Design**: Query processing, transaction protocols
- **Software Architecture**: Layered design, component interaction

### 🚀 Suggested Learning Path

1. **Start Here**: Run the interactive shell and try basic SQL commands
2. **Read Code**: Study `main.cpp` to understand the user interface
3. **Follow Data**: Trace an INSERT operation through all layers
4. **Deep Dive**: Read [DOCUMENTATION.md](./DOCUMENTATION.md) for implementation details
5. **Extend**: Add new features like JOIN operations or hash indexes

## 🎯 Project Goals & Limitations

### ✅ What This Project Teaches

- Complete SQL query lifecycle from parsing to storage
- ACID transaction implementation with real concurrency control
- Memory management strategies used in production databases
- Index structures and their performance characteristics
- Recovery mechanisms and crash safety

### ⚠️ Educational Limitations

- **Single-threaded**: No concurrent transaction support
- **Basic SQL**: No JOINs, subqueries, or complex aggregations
- **Fixed schema**: No ALTER TABLE support
- **Simple optimization**: No cost-based query optimizer
- **Memory constraints**: Fixed buffer pool size

## 🤝 Contributing & Learning

This project welcomes learners and educators:

- **Study the code** to understand database concepts
- **Implement extensions** like new SQL features or data types
- **Performance testing** and optimization experiments
- **Documentation improvements** for better learning experience

### 🎓 Built With Learning In Mind

This project was developed with assistance from:

- Database systems textbooks and academic papers
- Open-source database implementations for reference
- Community guidance and educational resources
- AI assistance for code documentation and best practices

## 📄 License

MIT License - Feel free to use this project for educational purposes, extend it for learning, or use it as a reference for understanding database internals.

---

**🎯 Perfect for**: Database courses, systems programming learning, CS curriculum projects, and anyone curious about how databases work!

## 📋 Quick Demo

Try the included demo script:

```bash
# Run the demo
cat demo.sql | ./build/db_engine

# Or run interactively
./build/db_engine
# Then type SQL commands directly
```

**Sample Session:**

```sql
db> CREATE TABLE users (id INTEGER, name VARCHAR(50), age INTEGER)
Table created successfully

db> INSERT INTO users VALUES (1, 'Alice', 25)
Insert successful

db> SELECT * FROM users
Query executed successfully
1 Alice 25

db> EXIT
Goodbye!
```

git clone <repository-url>
cd mini-database-engine

# Create build directory

mkdir build
cd build

# Configure and build

cmake ..
make

# Or with Ninja

cmake -G Ninja ..
ninja

````

### Running the Database

```bash
# Start the database engine
./db_engine
````

## Usage Examples

### Interactive Shell

The database comes with an interactive shell that supports SQL-like commands:

```sql
-- Create a table
CREATE TABLE users (id INTEGER, name VARCHAR(50), age INTEGER, active BOOLEAN)

-- Insert data
INSERT INTO users VALUES (1, 'Alice', 25, true)
INSERT INTO users VALUES (2, 'Bob', 30, false)

-- Query data
SELECT * FROM users
SELECT * FROM users WHERE age = 25

-- Create an index for faster queries
CREATE INDEX users.age

-- Transaction support
BEGIN
INSERT INTO users VALUES (3, 'Charlie', 35, true)
COMMIT
```

### Programmatic Usage

```cpp
#include "database_engine.h"

// Create database instance
db::Database db("my_database.db");

// Create table
std::vector<std::string> columns = {"id", "name", "age"};
std::vector<db::DataType> types = {db::DataType::INTEGER,
                                   db::DataType::VARCHAR,
                                   db::DataType::INTEGER};
db.createTable("users", columns, types);

// Insert data
std::vector<db::Value> values = {1, "Alice", 25};
db.insert("users", values);

// Query data
auto results = db.select("users", "age", 25);

// Transaction support
db.begin();
db.insert("users", {2, "Bob", 30});
db.commit();
```

## Supported SQL Commands

### Data Definition Language (DDL)

- `CREATE TABLE <name> (<col1> <type1>, <col2> <type2>, ...)`
- `DROP TABLE <name>`
- `CREATE INDEX <table>.<column>`

### Data Manipulation Language (DML)

- `INSERT INTO <table> VALUES (<val1>, <val2>, ...)`
- `SELECT * FROM <table> [WHERE <column> = <value>]`
- `UPDATE <table> SET <col1> = <val1> [, <col2> = <val2>] [WHERE <column> = <value>]`
- `DELETE FROM <table> [WHERE <column> = <value>]`

### Transaction Control

- `BEGIN` - Start a transaction
- `COMMIT` - Commit current transaction
- `ROLLBACK` - Rollback current transaction

## Data Types

| Type         | Description            | Example         |
| ------------ | ---------------------- | --------------- |
| `INTEGER`    | 32-bit integer         | `42`            |
| `VARCHAR(n)` | Variable-length string | `'Hello World'` |
| `BOOLEAN`    | Boolean value          | `true`, `false` |
| `DOUBLE`     | 64-bit floating point  | `3.14159`       |

## Performance Features

### B-tree Indexing

- Order-5 B-tree implementation
- Automatic index maintenance
- Fast range queries and point lookups

### Buffer Pool

- 1000-page buffer pool (4MB total)
- LRU eviction policy
- Dirty page tracking
- Hit ratio statistics

### Transaction Management

- Two-phase locking
- Deadlock detection
- Write-ahead logging
- Checkpoint and recovery

## File Structure

```
mini-database-engine/
├── include/                 # Header files
│   ├── types.h             # Core type definitions
│   ├── b_tree.h            # B-tree implementation
│   ├── buffer_pool.h       # Buffer pool management
│   ├── storage_engine.h    # Storage engine
│   ├── query_parser.h      # SQL parser
│   ├── transaction_manager.h # Transaction management
│   ├── index_manager.h     # Index management
│   └── database_engine.h   # Main database interface
├── src/                    # Source files
│   ├── CMakeLists.txt      # Build configuration
│   ├── main.cpp            # Main executable
│   ├── database_engine.cpp # Database implementation
│   ├── storage_engine.cpp  # Storage engine implementation
│   ├── query_parser.cpp    # Query parser implementation
│   ├── transaction_manager.cpp # Transaction implementation
│   └── index_manager.cpp   # Index manager implementation
├── CMakeLists.txt          # Main build file
└── README.md              # This file
```

## Technical Details

### Page Structure

- 4KB pages with header and data sections
- Tuple storage with variable-length records
- Free space management
- Page chaining for large tables

### Memory Management

- LRU buffer pool with 1000 frames
- Pin/unpin mechanism for page access
- Dirty page tracking for persistence
- Automatic page eviction

### Concurrency Control

- Two-phase locking protocol
- Shared and exclusive locks
- Deadlock prevention
- Transaction isolation

### Recovery

- Write-ahead logging
- Checkpoint mechanism
- Crash recovery
- Transaction rollback

## Limitations

This is a learning/educational database engine with some limitations:

- No complex SQL features (JOINs, subqueries, etc.)
- Limited data types
- No concurrent transactions (single-threaded)
- Basic recovery implementation
- No query optimization
- Limited index types (only B-tree)

## Future Enhancements

- Multi-threading support
- More complex SQL features
- Query optimization
- Multiple index types (Hash, R-tree)
- Network interface
- Backup and restore
- Performance monitoring
- More data types

## Contributing

This project is designed for educational purposes. Feel free to:

1. Study the code to understand database internals
2. Extend features for learning
3. Improve performance
4. Add new data types or SQL features
5. Implement missing features

## License

This project is open source and available under the MIT License.
