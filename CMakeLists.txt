cmake_minimum_required(VERSION 3.16)
project(MiniDatabaseEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# Add source directories
add_subdirectory(src)

# Main executable
add_executable(db_engine 
    src/main.cpp
    src/database_engine.cpp
)

# Link libraries
target_link_libraries(db_engine 
    storage_engine
    query_parser
    transaction_manager
    index_manager
    buffer_pool
)

# Include directories
target_include_directories(db_engine PRIVATE 
    include
    src
) 