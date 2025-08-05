# Buffer Pool Library
add_library(buffer_pool
    buffer_pool.cpp
)

target_include_directories(buffer_pool PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

# B-Tree Library
add_library(b_tree
    b_tree.cpp
)

target_include_directories(b_tree PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

# Storage Engine Library
add_library(storage_engine
    storage_engine.cpp
)

target_include_directories(storage_engine PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(storage_engine 
    buffer_pool
    b_tree
)

# Query Parser Library
add_library(query_parser
    query_parser.cpp
)

target_include_directories(query_parser PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(query_parser 
    storage_engine
)

# Transaction Manager Library
add_library(transaction_manager
    transaction_manager.cpp
)

target_include_directories(transaction_manager PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

# Index Manager Library
add_library(index_manager
    index_manager.cpp
)

target_include_directories(index_manager PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(index_manager 
    b_tree
    storage_engine
) 