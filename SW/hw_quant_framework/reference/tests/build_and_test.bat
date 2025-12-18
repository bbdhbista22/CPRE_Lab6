@echo off
REM Build and test script for Windows
REM Requires g++ (MinGW or similar) in PATH

echo ========================================
echo Building C++ Accelerator Tests
echo ========================================
echo.

set CXX=g++
set CXXFLAGS=-std=c++11 -Wall -Wextra -O2 -I..
set SRC_DIR=..

REM Build IndexGenerator test
echo Building test_index_generator...
%CXX% %CXXFLAGS% -o test_index_generator.exe test_index_generator.cpp %SRC_DIR%\IndexGenerator.cpp
if errorlevel 1 goto build_error

REM Build Dequantization test
echo Building test_dequantization...
%CXX% %CXXFLAGS% -o test_dequantization.exe test_dequantization.cpp %SRC_DIR%\Dequantization.cpp
if errorlevel 1 goto build_error

REM Build OutputStorage test
echo Building test_output_storage...
%CXX% %CXXFLAGS% -o test_output_storage.exe test_output_storage.cpp %SRC_DIR%\OutputStorage.cpp
if errorlevel 1 goto build_error

REM Build StagedMAC test
echo Building test_staged_mac...
%CXX% %CXXFLAGS% -o test_staged_mac.exe test_staged_mac.cpp %SRC_DIR%\StagedMAC.cpp
if errorlevel 1 goto build_error

REM Build Integration test
echo Building test_accelerator_integration...
%CXX% %CXXFLAGS% -o test_accelerator_integration.exe test_accelerator_integration.cpp %SRC_DIR%\IndexGenerator.cpp %SRC_DIR%\Dequantization.cpp %SRC_DIR%\OutputStorage.cpp %SRC_DIR%\StagedMAC.cpp
if errorlevel 1 goto build_error

REM Build Complete Pipeline test
echo Building test_complete_pipeline...
%CXX% %CXXFLAGS% -o test_complete_pipeline.exe test_complete_pipeline.cpp %SRC_DIR%\IndexGenerator.cpp %SRC_DIR%\Dequantization.cpp %SRC_DIR%\OutputStorage.cpp %SRC_DIR%\StagedMAC.cpp
if errorlevel 1 goto build_error

echo.
echo ========================================
echo All tests built successfully!
echo ========================================
echo.

if "%1"=="--no-run" goto end

echo Running all tests...
echo.

echo Running test_index_generator...
test_index_generator.exe
if errorlevel 1 goto test_error

echo.
echo Running test_dequantization...
test_dequantization.exe
if errorlevel 1 goto test_error

echo.
echo Running test_output_storage...
test_output_storage.exe
if errorlevel 1 goto test_error

echo.
echo Running test_staged_mac...
test_staged_mac.exe
if errorlevel 1 goto test_error

echo.
echo Running test_accelerator_integration...
test_accelerator_integration.exe
if errorlevel 1 goto test_error

echo.
echo Running test_complete_pipeline...
test_complete_pipeline.exe
if errorlevel 1 goto test_error

echo.
echo ========================================
echo All tests PASSED!
echo ========================================
goto end

:build_error
echo.
echo ========================================
echo BUILD FAILED!
echo ========================================
exit /b 1

:test_error
echo.
echo ========================================
echo TEST FAILED!
echo ========================================
exit /b 1

:end
