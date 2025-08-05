#include "../include/database_engine.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <type_traits>
#include <sstream>

using namespace std;

// Display help information for available database commands
// Shows users what SQL statements and operations they can perform
void printHelp()
{
    cout << "Mini Database Engine Commands:" << endl;
    cout << "  CREATE TABLE <name> (<col1> <type1>, <col2> <type2>, ...)" << endl;
    cout << "  INSERT INTO <table> VALUES (<val1>, <val2>, ...)" << endl;
    cout << "  SELECT * FROM <table> [WHERE <column> = <value>]" << endl;
    cout << "  DROP TABLE <name>" << endl;
    cout << "  CREATE INDEX <table>.<column>" << endl;
    cout << "  BEGIN" << endl;
    cout << "  COMMIT" << endl;
    cout << "  ROLLBACK" << endl;
    cout << "  STATS" << endl;
    cout << "  LOGS" << endl;           // Show WAL logs
    cout << "  VERBOSE ON/OFF" << endl; // Toggle verbose logging
    cout << "  HELP" << endl;
    cout << "  EXIT" << endl;
    cout << endl;
    cout << "Data Types: INTEGER, VARCHAR(n), BOOLEAN, DOUBLE" << endl;
}

// Print a single database row (tuple) with its schema information
// Shows column names and values in a readable format
void printTuple(const db::Tuple &tuple, const db::Schema &schema)
{
    cout << "ID: " << tuple.id << " | ";
    for (size_t i = 0; i < tuple.values.size() && i < schema.columns.size(); i++)
    {
        cout << schema.columns[i].name << ": ";
        visit([](const auto &v)
              { cout << v; }, tuple.values[i]);
        if (i < tuple.values.size() - 1)
            cout << " | ";
    }
    cout << endl;
}

// Extract table name from SELECT query
string extractTableName(const string &query)
{
    string upper_query = query;
    transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);

    size_t from_pos = upper_query.find("FROM");
    if (from_pos == string::npos)
        return "";

    size_t start = from_pos + 4; // Skip "FROM"
    while (start < upper_query.length() && isspace(upper_query[start]))
        start++; // Skip whitespace

    size_t end = start;
    while (end < upper_query.length() && !isspace(upper_query[end]) &&
           upper_query[end] != ';' && upper_query[end] != '\n')
        end++; // Find end of table name

    if (start < end)
        return query.substr(start, end - start); // Return original case
    return "";
}

// Display query results with dynamic schema-based formatting
void displayQueryResults(const db::QueryResult &result, const string &query,
                         db::Database &db, bool verbose_mode)
{
    if (!result.tuples.empty())
    {
        cout << "Query returned " << result.tuples.size() << " rows:" << endl;
        if (verbose_mode)
        {
            const string BLUE = "\033[34m";
            const string RESET = "\033[0m";
            cout << BLUE << "[LOG] Reading " << result.tuples.size() << " tuples from storage" << RESET << endl;
        }

        // Extract table name and get schema for dynamic formatting
        string table_name = extractTableName(query);
        if (!table_name.empty())
        {
            auto schema = db.getTableSchema(table_name);
            if (!schema.columns.empty())
            {
                // Calculate column widths
                vector<size_t> col_widths;
                for (const auto &column : schema.columns)
                {
                    col_widths.push_back(max(static_cast<size_t>(12), column.name.length() + 2));
                }

                // Print top border
                cout << "+";
                for (size_t width : col_widths)
                {
                    cout << string(width, '-') << "+";
                }
                cout << endl;

                // Print header row
                cout << "|";
                for (size_t i = 0; i < schema.columns.size(); i++)
                {
                    cout << " " << setw(col_widths[i] - 1) << left << schema.columns[i].name << "|";
                }
                cout << endl;

                // Print separator
                cout << "+";
                for (size_t width : col_widths)
                {
                    cout << string(width, '-') << "+";
                }
                cout << endl;

                // Print data rows
                for (const auto &tuple : result.tuples)
                {
                    cout << "|";
                    for (size_t i = 0; i < tuple.values.size() && i < col_widths.size(); i++)
                    {
                        cout << " ";
                        visit([&](const auto &v)
                              {
                            ostringstream ss;
                            if constexpr (is_same_v<decay_t<decltype(v)>, bool>) {
                                ss << (v ? "true" : "false");
                            } else {
                                ss << v;
                            }
                            cout << setw(col_widths[i]-1) << left << ss.str(); }, tuple.values[i]);
                        cout << "|";
                    }
                    cout << endl;
                }

                // Print bottom border
                cout << "+";
                for (size_t width : col_widths)
                {
                    cout << string(width, '-') << "+";
                }
                cout << endl;
                return;
            }
        }

        // Fallback: simple display if schema not available
        for (const auto &tuple : result.tuples)
        {
            cout << "Row ID " << tuple.id << ": ";
            for (size_t i = 0; i < tuple.values.size(); i++)
            {
                visit([](const auto &v)
                      { cout << v; }, tuple.values[i]);
                if (i < tuple.values.size() - 1)
                    cout << " | ";
            }
            cout << endl;
        }
    }
}

// Display recent WAL log entries for educational purposes
// Shows users what database operations are being logged
void showLogs(const string &log_file_path, int num_lines = 10)
{
    ifstream log_file(log_file_path);
    if (!log_file.is_open())
    {
        cout << "No log file found or unable to open." << endl;
        return;
    }

    vector<string> lines;
    string line;
    while (getline(log_file, line))
    {
        lines.push_back(line);
    }

    cout << "=== Recent Transaction Log Entries ===" << endl;
    if (lines.empty())
    {
        cout << "No log entries found." << endl;
    }
    else
    {
        int start = max(0, (int)lines.size() - num_lines);
        for (int i = start; i < (int)lines.size(); i++)
        {
            cout << "[" << (i + 1) << "] " << lines[i] << endl;
        }
    }
    cout << endl;
    log_file.close();
}

// Print verbose operation information for educational purposes
void logOperation(bool verbose, const string &operation, const string &details = "")
{
    if (verbose)
    {
        // ANSI color codes for colored output
        const string CYAN = "\033[36m";
        const string RESET = "\033[0m";
        const string BRIGHT_BLUE = "\033[94m";

        cout << CYAN << "[LOG] " << RESET << BRIGHT_BLUE << operation << RESET;
        if (!details.empty())
        {
            cout << " - " << details;
        }
        cout << endl;
    }
}

// Main function - database command-line interface
// Provides interactive shell for database operations
int main()
{
    cout << "=== Mini Database Engine ===" << endl;
    cout << "Type 'HELP' for available commands" << endl;
    cout << "Type 'VERBOSE ON' to see detailed operation logs" << endl;
    cout << "Type 'LOGS' to view recent transaction log entries" << endl;
    cout << endl;

    db::Database db("db/test.db"); // Create database instance
    bool verbose_mode = false;     // Toggle for verbose logging

    string input;
    while (true)
    {
        cout << "db> ";           // Command prompt
        if (!getline(cin, input)) // Read user input
        {
            // EOF reached or input error
            break;
        }

        if (input.empty())
            continue; // Skip empty lines

        // Convert to uppercase for command matching
        string upper_input = input;
        transform(upper_input.begin(), upper_input.end(), upper_input.begin(), ::toupper);

        // Handle special commands that don't require SQL parsing
        if (upper_input == "EXIT" || upper_input == "QUIT")
        {
            logOperation(verbose_mode, "Shutting down database engine");
            break; // Exit the program
        }
        else if (upper_input == "HELP")
        {
            printHelp(); // Show available commands
        }
        else if (upper_input == "STATS")
        {
            logOperation(verbose_mode, "Displaying database statistics");
            db.printStats(); // Show database statistics
        }
        else if (upper_input == "LOGS")
        {
            showLogs("db/test.db.log", 15); // Show recent log entries
        }
        else if (upper_input == "VERBOSE ON")
        {
            verbose_mode = true;
            cout << "✓ Verbose logging enabled - you'll see detailed operation logs" << endl;
        }
        else if (upper_input == "VERBOSE OFF")
        {
            verbose_mode = false;
            cout << "✓ Verbose logging disabled" << endl;
        }
        else if (upper_input == "BEGIN")
        {
            logOperation(verbose_mode, "Starting transaction", "Acquiring locks and initializing WAL entry");
            if (db.begin())
            {
                cout << "Transaction started" << endl;
                if (verbose_mode)
                {
                    const string GREEN = "\033[32m";
                    const string RESET = "\033[0m";
                    cout << GREEN << "[LOG] Transaction ID assigned, WAL entry: BEGIN" << RESET << endl;
                }
            }
            else
            {
                cout << "Failed to start transaction" << endl;
            }
        }
        else if (upper_input == "COMMIT")
        {
            logOperation(verbose_mode, "Committing transaction", "Writing changes to disk and releasing locks");
            if (db.commit())
            {
                cout << "Transaction committed" << endl;
                if (verbose_mode)
                {
                    const string GREEN = "\033[32m";
                    const string RESET = "\033[0m";
                    cout << GREEN << "[LOG] All changes persisted, WAL entry: COMMIT" << RESET << endl;
                }
            }
            else
            {
                cout << "Failed to commit transaction" << endl;
            }
        }
        else if (upper_input == "ROLLBACK")
        {
            logOperation(verbose_mode, "Rolling back transaction", "Undoing changes and releasing locks");
            if (db.rollback())
            {
                cout << "Transaction rolled back" << endl;
                if (verbose_mode)
                {
                    const string YELLOW = "\033[33m";
                    const string RESET = "\033[0m";
                    cout << YELLOW << "[LOG] All changes undone, WAL entry: ABORT" << RESET << endl;
                }
            }
            else
            {
                cout << "Failed to rollback transaction" << endl;
            }
        }
        else
        {
            // Try to execute as a SQL query
            try
            {
                logOperation(verbose_mode, "Executing SQL query", "\"" + input + "\"");
                auto result = db.executeQuery(input);
                if (result.success)
                {
                    if (verbose_mode)
                    {
                        const string GREEN = "\033[32m";
                        const string RESET = "\033[0m";
                        cout << GREEN << "[LOG] Query parsed successfully, executing..." << RESET << endl;
                    }

                    // Check if this is an INSERT operation for better debugging
                    string upper_query = input;
                    transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);
                    if (upper_query.find("INSERT") == 0 && verbose_mode)
                    {
                        const string MAGENTA = "\033[35m";
                        const string RESET = "\033[0m";
                        cout << MAGENTA << "[LOG] INSERT operation - checking if data was stored..." << RESET << endl;
                    }

                    displayQueryResults(result, input, db, verbose_mode);

                    if (result.tuples.empty())
                    {
                        // Check if this was a SELECT query that should return data
                        if (upper_query.find("SELECT") == 0)
                        {
                            cout << "Query executed successfully, but no rows found." << endl;
                            if (verbose_mode)
                            {
                                const string YELLOW = "\033[33m";
                                const string RESET = "\033[0m";
                                cout << YELLOW << "[LOG] SELECT query returned empty result set" << RESET << endl;
                                cout << YELLOW << "[LOG] This might indicate that INSERT operations aren't persisting data" << RESET << endl;
                            }
                        }
                        else
                        {
                            cout << result.message << endl;
                            if (verbose_mode && !result.message.empty())
                            {
                                const string GREEN = "\033[32m";
                                const string RESET = "\033[0m";
                                cout << GREEN << "[LOG] Operation completed successfully" << RESET << endl;
                            }
                        }
                    }
                }
                else
                {
                    cout << "Error: " << result.message << endl;
                    if (verbose_mode)
                    {
                        const string RED = "\033[31m";
                        const string RESET = "\033[0m";
                        cout << RED << "[LOG] Query parsing or execution failed" << RESET << endl;
                    }
                }
            }
            catch (const exception &e)
            {
                cout << "Error: " << e.what() << endl; // Display any execution errors
                if (verbose_mode)
                {
                    const string RED = "\033[31m";
                    const string RESET = "\033[0m";
                    cout << RED << "[LOG] Exception caught during query execution" << RESET << endl;
                }
            }
        }
    }

    cout << "Goodbye!" << endl; // Exit message
    return 0;                   // Successful program termination
}
