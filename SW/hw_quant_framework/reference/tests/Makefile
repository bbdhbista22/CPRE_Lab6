# Makefile for C++ Accelerator Tests
# Builds all test executables

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -I..
LDFLAGS =

# Source directories
SRC_DIR = ..
TEST_DIR = .

# Implementation files
IMPL_SOURCES = $(SRC_DIR)/IndexGenerator.cpp \
               $(SRC_DIR)/Dequantization.cpp \
               $(SRC_DIR)/OutputStorage.cpp \
               $(SRC_DIR)/StagedMAC.cpp

# Test executables
TESTS = test_index_generator \
        test_dequantization \
        test_output_storage \
        test_staged_mac \
        test_accelerator_integration \
        test_complete_pipeline

# Default target: build all tests
all: $(TESTS)

# Individual test targets
test_index_generator: test_index_generator.cpp $(SRC_DIR)/IndexGenerator.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test_dequantization: test_dequantization.cpp $(SRC_DIR)/Dequantization.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test_output_storage: test_output_storage.cpp $(SRC_DIR)/OutputStorage.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test_staged_mac: test_staged_mac.cpp $(SRC_DIR)/StagedMAC.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test_accelerator_integration: test_accelerator_integration.cpp $(IMPL_SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

test_complete_pipeline: test_complete_pipeline.cpp $(IMPL_SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Run all tests
test: all
	@echo ""
	@echo "Running all tests..."
	@echo ""
	@./test_index_generator
	@echo ""
	@./test_dequantization
	@echo ""
	@./test_output_storage
	@echo ""
	@./test_staged_mac
	@echo ""
	@./test_accelerator_integration
	@echo ""
	@./test_complete_pipeline
	@echo ""
	@echo "========================================="
	@echo "All tests completed!"
	@echo "========================================="

# Run individual tests
run_index: test_index_generator
	./test_index_generator

run_dequant: test_dequantization
	./test_dequantization

run_storage: test_output_storage
	./test_output_storage

run_mac: test_staged_mac
	./test_staged_mac

run_integration: test_accelerator_integration
	./test_accelerator_integration

run_pipeline: test_complete_pipeline
	./test_complete_pipeline

# Clean build artifacts
clean:
	rm -f $(TESTS) *.o *.exe

# Help target
help:
	@echo "Accelerator C++ Test Suite"
	@echo ""
	@echo "Available targets:"
	@echo "  make           - Build all tests"
	@echo "  make test      - Build and run all tests"
	@echo "  make clean     - Remove build artifacts"
	@echo ""
	@echo "Individual test targets:"
	@echo "  make test_index_generator         - Build IndexGenerator test"
	@echo "  make test_dequantization          - Build Dequantization test"
	@echo "  make test_output_storage          - Build OutputStorage test"
	@echo "  make test_staged_mac              - Build StagedMAC test"
	@echo "  make test_accelerator_integration - Build Integration test"
	@echo "  make test_complete_pipeline       - Build Complete Pipeline test"
	@echo ""
	@echo "Run individual tests:"
	@echo "  make run_index      - Run IndexGenerator test"
	@echo "  make run_dequant    - Run Dequantization test"
	@echo "  make run_storage    - Run OutputStorage test"
	@echo "  make run_mac        - Run StagedMAC test"
	@echo "  make run_integration - Run Integration test"
	@echo "  make run_pipeline   - Run Complete Pipeline test"

.PHONY: all test clean help run_index run_dequant run_storage run_mac run_integration run_pipeline
