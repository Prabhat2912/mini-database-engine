-- Mini Database Engine Demo Script
-- This script demonstrates the core features of the database

-- 1. Create a table with multiple data types
CREATE TABLE students (
    id INTEGER,
    name VARCHAR(50),
    age INTEGER,
    gpa DOUBLE,
    active BOOLEAN
)

-- 2. Insert sample data
INSERT INTO students VALUES (1, 'Alice Johnson', 20, 3.8, true)
INSERT INTO students VALUES (2, 'Bob Smith', 22, 3.2, true)
INSERT INTO students VALUES (3, 'Charlie Brown', 19, 3.9, false)
INSERT INTO students VALUES (4, 'Diana Prince', 21, 3.6, true)

-- 3. Query all data
SELECT * FROM students

-- 4. Query with filter
SELECT * FROM students WHERE age = 20

-- 5. Create an index for faster queries (if supported)
-- CREATE INDEX students.age

-- 6. Transaction example
BEGIN
INSERT INTO students VALUES (5, 'Eve Wilson', 23, 3.4, true)
COMMIT

-- 7. Query after transaction
SELECT * FROM students

-- 8. Exit
EXIT
