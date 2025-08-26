#!/bin/bash
# build_db_engine.sh - Build script for Mini Database Engine
# Usage: ./build_db_engine.sh

set -e

# Check for cmake
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is not installed. Please install cmake and try again."
    exit 1
fi

# Create build and db directories
mkdir -p build
mkdir -p db

# Build the project
cd build
cmake ..
cmake --build .

# Success message
echo "Build complete! To run the database engine:"
echo "  cd build && ./db_engine"
