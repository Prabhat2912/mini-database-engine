@echo off
REM build_db_engine.bat - Build script for Mini Database Engine (Windows)
REM Usage: build_db_engine.bat

REM Check for cmake
where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: cmake is not installed. Please install cmake and try again.
    exit /b 1
)

REM Create build and db directories
if not exist build mkdir build
if not exist db mkdir db

REM Build the project
cd build
cmake ..
cmake --build .

REM Success message
echo Build complete! To run the database engine:
echo   cd build && db_engine.exe
