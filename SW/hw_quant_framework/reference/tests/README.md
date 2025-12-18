# C++ Accelerator Test Suite

Comprehensive test suite for the hardware accelerator reference implementation. These tests validate all components and provide hardware-comparable output for FPGA verification.

## Overview

This test suite validates the complete hardware accelerator pipeline:
- **IndexGenerator**: Address generation for 7M MAC operations
- **Dequantization**: Q8.24 fixed-point arithmetic with saturation and ReLU
- **OutputStorage**: BRAM read-modify-write with byte packing
- **StagedMAC**: 3-stage pipelined MAC units with 4-unit clustering
- **Integration**: End-to-end dataflow through all components
- **Complete Pipeline**: Verbose cycle-by-cycle logging for FPGA comparison

## Test Files

| File | Tests | Description |
|------|-------|-------------|
| `test_index_generator.cpp` | 6 functions | Address generation, TLAST patterns, bounds checking |
| `test_dequantization.cpp` | 4 functions | Q8.24 arithmetic, saturation, ReLU, vector ops |
| `test_output_storage.cpp` | 5 functions | RMW operations, byte packing, pooling |
| `test_staged_mac.cpp` | 5 functions | Pipeline behavior, zero-point adjustment, clustering |
| `test_accelerator_integration.cpp` | 3 functions | Multi-component integration, full layer simulation |
| `test_complete_pipeline.cpp` | 3 functions | Verbose logging, performance metrics |

**Total**: 26 test functions covering 50+ test cases

## Building

### Prerequisites

- C++11 compatible compiler (g++, clang++, MSVC)
- Make (optional, for Unix-like systems)

### Build Options

#### Option 1: Windows (Batch Script)
```batch
build_and_test.bat
```

To build without running tests:
```batch
build_and_test.bat --no-run
```

#### Option 2: Unix/Linux/Mac (Makefile)
```bash
make           # Build all tests
make test      # Build and run all tests
make clean     # Remove build artifacts
```

#### Option 3: Manual Compilation

Individual tests:
```bash
# IndexGenerator test
g++ -std=c++11 -Wall -O2 -I.. -o test_index_generator test_index_generator.cpp ../IndexGenerator.cpp

# Dequantization test
g++ -std=c++11 -Wall -O2 -I.. -o test_dequantization test_dequantization.cpp ../Dequantization.cpp

# OutputStorage test
g++ -std=c++11 -Wall -O2 -I.. -o test_output_storage test_output_storage.cpp ../OutputStorage.cpp

# StagedMAC test
g++ -std=c++11 -Wall -O2 -I.. -o test_staged_mac test_staged_mac.cpp ../StagedMAC.cpp

# Integration test (requires all implementations)
g++ -std=c++11 -Wall -O2 -I.. -o test_accelerator_integration \
    test_accelerator_integration.cpp \
    ../IndexGenerator.cpp ../Dequantization.cpp ../OutputStorage.cpp ../StagedMAC.cpp

# Complete pipeline test (requires all implementations)
g++ -std=c++11 -Wall -O2 -I.. -o test_complete_pipeline \
    test_complete_pipeline.cpp \
    ../IndexGenerator.cpp ../Dequantization.cpp ../OutputStorage.cpp ../StagedMAC.cpp
```

## Running Tests

### Run All Tests
```bash
# Unix/Linux/Mac
make test

# Windows
build_and_test.bat
```

### Run Individual Tests

Unix/Linux/Mac:
```bash
make run_index       # IndexGenerator
make run_dequant     # Dequantization
make run_storage     # OutputStorage
make run_mac         # StagedMAC
make run_integration # Integration
make run_pipeline    # Complete Pipeline
```

Windows:
```batch
test_index_generator.exe
test_dequantization.exe
test_output_storage.exe
test_staged_mac.exe
test_accelerator_integration.exe
test_complete_pipeline.exe
```

## Test Details

### 1. IndexGenerator Tests (test_index_generator.cpp)

Tests address generation for convolution operations.

**Functions**:
- `test_configuration()` - Validates Conv1 config (64×64×3 → 64×64×64)
- `test_output_dimensions()` - Verifies output size calculation
- `test_address_generation()` - Generates first 100 addresses
- `test_tlast_pattern()` - Verifies TLAST every 27 MACs (3×3×3)
- `test_complete_generation()` - Generates all 7,077,888 addresses
- `test_address_bounds()` - Validates all addresses within BRAM bounds

**Key Validations**:
- Total MACs: 7,077,888 (64×64×64×27)
- TLAST count: 262,144 (64×64×64 pixels)
- Input address range: 0 to 12,287
- Weight address range: 0 to 110,591
- Output channel indices: 0-3

**Expected Output**:
```
Configuration:
  Input:       64×64×3
  Filter:      3×3×3 (stride=1, padding=1)
  Output:      64×64×64
  MACs/pixel:  27
  Tiles:       4×4 (16 total)

✓ All tests PASSED
```

### 2. Dequantization Tests (test_dequantization.cpp)

Tests Q8.24 fixed-point arithmetic.

**Functions**:
- `test_basic_dequantization()` - Scale factor 0.5, ReLU enabled
- `test_saturation()` - int8 saturation [-128, 127]
- `test_relu()` - ReLU activation (clip negatives)
- `test_vector_operations()` - Batch processing

**Key Test Cases**:
```
Input: 0     → Output: 0
Input: 100   → Output: 50    (100 × 0.5)
Input: 512   → Output: 127   (saturated)
Input: -100  → Output: 0     (ReLU clipped)
```

**Q8.24 Format**:
- 8 integer bits + 24 fractional bits
- Scale factor 0x00800000 = 0.5
- Scale factor 0x01000000 = 1.0

### 3. OutputStorage Tests (test_output_storage.cpp)

Tests BRAM read-modify-write operations.

**Functions**:
- `test_basic_rmw()` - 6 RMW operations at different positions
- `test_byte_packing()` - Pack 4 int8 values into 32-bit word
- `test_address_calculation()` - Verify addressing for 64×64×64 output
- `test_streaming()` - AXI-Stream processing with tid/tlast
- `test_max_pooling()` - 2×2 max pooling

**Key Validations**:
- 4 int8 values per 32-bit word (little-endian)
- Total BRAM words for 64×64×64: 65,536
- Address formula: `word_addr = ((y×W + x)×C + ch) / 4`
- Byte selector: `byte_sel = ((y×W + x)×C + ch) % 4`

**Example Byte Packing**:
```
Store (0,0,0)=10  → addr=0x000000, byte[0]=10
Store (0,0,1)=20  → addr=0x000000, byte[1]=20
Store (0,0,2)=30  → addr=0x000000, byte[2]=30
Store (0,0,3)=40  → addr=0x000000, byte[3]=40
Final word: 0x281E140A (little-endian)
```

### 4. StagedMAC Tests (test_staged_mac.cpp)

Tests 3-stage pipelined MAC units.

**Functions**:
- `test_single_mac_pipeline()` - Pipeline fill latency (3 cycles)
- `test_zero_point_adjustment()` - Quantization offset correction
- `test_accumulator_reset()` - Reset between output pixels
- `test_mac_cluster()` - 4 parallel MAC units
- `test_mac_cluster_reset()` - Cluster reset after TLAST

**Pipeline Stages**:
1. **Stage 0**: Multiply (input × weight)
2. **Stage 1**: Accumulate (sum + product)
3. **Stage 2**: Register (output)

**Throughput**: 1 MAC/cycle after 3-cycle fill

### 5. Integration Tests (test_accelerator_integration.cpp)

Tests complete dataflow through all components.

**Functions**:
- `test_small_integration()` - First 108 MACs (4 pixels)
- `test_end_to_end_dataflow()` - Smaller layer (8×8×3 → 8×8×4)
- `test_full_layer_simulation()` - Complete Conv1 (7M MACs)

**Dataflow**:
```
IndexGenerator → generate 7M addresses
       ↓
MACStreamProvider → 4 parallel MACs
       ↓
Dequantization → Q8.24 scaling + ReLU
       ↓
OutputStorage → BRAM write (65,536 words)
```

### 6. Complete Pipeline Tests (test_complete_pipeline.cpp)

Tests with verbose cycle-by-cycle logging.

**Functions**:
- `test_mac_unit_only()` - MAC pipeline verification
- `test_complete_pipeline()` - Verbose hardware simulation
- `test_pipeline_performance_metrics()` - Performance analysis

**Verbose Output Format** (hardware-comparable):
```
[CYCLE 000000] MAC#0 input=0x05 weight=0x0A -> accum=0x00000000
[CYCLE 000003] DEQUANT input=0x000000e1 scale=0x00800000 -> output=0x7f
[CYCLE 000003] STORE addr=0x000040 byte[0]=0x7f
[CYCLE 000003] PIXEL_COMPLETE y=  0 x=  0 c= 0
```

**Performance Metrics**:
- Clock: 112 MHz
- Throughput: 4 MACs/cycle = 448 MACs/ns
- Conv1 execution: ~15.8 ms
- GMAC/s: ~447

## Test Framework

Simple, zero-dependency test framework (`test_framework.h`):

**Features**:
- Test assertions (ASSERT_EQ, ASSERT_TRUE, etc.)
- Automatic test counting
- Pass/fail reporting
- Colorized output

**Usage**:
```cpp
TEST_BEGIN("My Test Name");

ASSERT_EQ(expected, actual);
ASSERT_TRUE(condition);

TEST_END();
```

## Success Criteria

All tests must pass with output:
```
========================================
TEST SUMMARY
========================================
Passed: X
Failed: 0
Total:  X

✓ ALL TESTS PASSED!
```

## Troubleshooting

### Compilation Errors

**Missing headers**:
- Ensure parent directory (`..`) contains implementation files
- Check that `#include "../ComponentName.h"` paths are correct

**C++11 features**:
- Use `-std=c++11` flag
- MSVC: Use `/std:c++11` or newer Visual Studio version

### Test Failures

**Address generation**:
- Verify IndexGenerator produces exactly 7,077,888 MACs
- Check TLAST appears every 27 MACs

**Dequantization**:
- Verify Q8.24 rounding: add 0x00800000 before shifting
- Check saturation boundaries: [-128, 127]

**Output storage**:
- Verify little-endian byte packing
- Check address calculation formula

### Performance Issues

**Slow test execution**:
- Full layer tests (7M MACs) can take 10-30 seconds
- Use smaller test cases for debugging
- Compile with `-O2` optimization

## Comparison with Python Tests

These C++ tests are direct ports of the Python reference implementation:

| Python File | C++ File | Status |
|-------------|----------|--------|
| `test_index_generator.py` | `test_index_generator.cpp` | ✓ Ported |
| `test_dequantization.py` | `test_dequantization.cpp` | ✓ Ported |
| `test_output_storage.py` | `test_output_storage.cpp` | ✓ Ported |
| `test_complete_pipeline.py` | `test_staged_mac.cpp` | ✓ Ported (MAC portion) |
| `test_complete_pipeline.py` | `test_complete_pipeline.cpp` | ✓ Ported (full pipeline) |
| `test_accelerator_model.py` | `test_accelerator_integration.cpp` | ✓ Ported |

**Numerical Equivalence**:
- All test cases produce identical results
- Q8.24 arithmetic matches Python implementation
- Address generation is bit-exact

## FPGA Verification

The verbose output from `test_complete_pipeline` can be directly compared with FPGA simulation:

1. Run C++ test: `./test_complete_pipeline > cpp_output.log`
2. Run VHDL testbench: Capture output to `fpga_output.log`
3. Compare logs line-by-line

**Expected Match**:
- MAC operations (cycle, input, weight, accumulator)
- Dequantization (scale factor, output)
- Storage (address, byte position, value)
- Pixel completion markers

## License

Part of CPRE Lab 6 - Hardware Accelerator Implementation
Iowa State University - 2025
