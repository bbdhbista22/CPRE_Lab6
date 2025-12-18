# Lab 6: Hardware Accelerator for CNNs - Final Day Status Report

**Course**: 5870 - Hardware Design for AI/ML
**Report Date**: December 16, 2025
**Project Status**: Day 2 - Ready for Final Implementation Push
**Author**: Lab 6 Team

---

## Executive Summary

This report documents the current status of Lab 6 hardware accelerator implementation and outlines the pending tasks for the final project day. The project aims to design and implement a custom CNN hardware accelerator on the Xilinx Zynq-7000 platform, leveraging proven components from Lab 3 while adding new specialized modules.

### Current Achievement Level: **75% Complete**

**Completed (Day 1)**:
- ✅ Complete system architecture specification
- ✅ Microarchitecture design for all components
- ✅ Full software reference implementation (C++ and Python)
- ✅ 50+ test cases validated (100% passing in Python)
- ✅ Comprehensive documentation (15,000+ words)
- ✅ Lab 3 improvements analysis and integration planning

**Pending (Day 2 - Final Day)**:
- ⏳ C++ test suite compilation and validation
- ⏳ VHDL implementation of 3 new modules
- ⏳ Hardware testbench development
- ⏳ System integration and timing closure
- ⏳ Final lab report completion

---

## 1. Project Overview

### 1.1 Objective

Design and implement a hardware accelerator for the first convolutional layer (Conv1) of a quantized CNN, achieving:
- **Target Layer**: Conv1 (64×64×3 → 64×64×64)
- **Total Operations**: 7,077,888 MACs
- **Target Speedup**: 5× over CPU baseline
- **Clock Frequency**: 112 MHz (proven from Lab 3)
- **Throughput**: 448 MMAC/s (4 parallel MAC units)

### 1.2 Architecture Overview

```
DDR Memory (PS)
    ↓
CDMA Controller (PL)
    ↓
BRAM Banks (315 KB)
    ↓
┌──────────────────────────────────────┐
│  Hardware Accelerator Pipeline       │
│  ┌────────────────────────────────┐  │
│  │ Index Generator (NEW)          │  │
│  │ - Generates 7M MAC addresses   │  │
│  │ - TLAST every 27 MACs          │  │
│  └─────────────┬──────────────────┘  │
│                ↓                     │
│  ┌────────────────────────────────┐  │
│  │ MAC Stream Provider (Template) │  │
│  │ - Fetches data from BRAM       │  │
│  │ - Packs weight/activation pairs│  │
│  └─────────────┬──────────────────┘  │
│                ↓                     │
│  ┌────────────────────────────────┐  │
│  │ 4× Staged MAC Units (Lab 3)    │  │
│  │ - Proven 3-stage pipeline      │  │
│  │ - 112 MHz operation            │  │
│  └─────────────┬──────────────────┘  │
│                ↓                     │
│  ┌────────────────────────────────┐  │
│  │ Dequantization (NEW)           │  │
│  │ - Q8.24 fixed-point arithmetic │  │
│  │ - ReLU activation              │  │
│  │ - Saturation logic             │  │
│  └─────────────┬──────────────────┘  │
│                ↓                     │
│  ┌────────────────────────────────┐  │
│  │ Output Storage (NEW)           │  │
│  │ - BRAM read-modify-write       │  │
│  │ - Byte-level packing           │  │
│  │ - Optional max pooling         │  │
│  └────────────────────────────────┘  │
└──────────────────────────────────────┘
    ↓
Output BRAM (262 KB)
```

---

## 2. Completed Work (Day 1)

### 2.1 Architecture & Specification (Morning Session)

#### 2.1.1 System Architecture
**File**: [docs/system_architecture_spec.md](docs/system_architecture_spec.md)

**Key Accomplishments**:
- ✅ Complete memory map with 315 KB BRAM allocation
- ✅ Double-buffering strategy for inputs/outputs
- ✅ Weight banking for 4-way parallelism
- ✅ DDR → CDMA → BRAM dataflow specification
- ✅ 16×16 pixel tiling strategy (51× memory reduction)

**Memory Breakdown**:
```
Input Activation Buffers:  2 × 262 KB = 524 KB (conceptual - actual 80 KB per tile)
Output Activation Buffers: 2 × 524 KB = 1,048 KB (conceptual - actual 80 KB per tile)
Weight Banks:              4 × 2 × 4 KB = 32 KB (double-buffered)
Total On-Chip:             ~315 KB BRAM (optimized with tiling)
```

#### 2.1.2 Microarchitecture Design
**File**: [docs/micro_architecture_spec.md](docs/micro_architecture_spec.md)

**Modules Specified**:

1. **Index Generator**
   - FSM with 7 nested loops
   - States: IDLE, TILE_LOOP, PIXEL_LOOP, OC_BATCH_LOOP, FILTER_Y_LOOP, FILTER_X_LOOP, IC_LOOP, OUTPUT
   - Address calculation: `input_addr = (in_y * IW + in_x) * IC + ic`
   - TLAST generation: Every 27 MACs (filter_h × filter_w × input_channels)
   - Output: AXI-Stream with (input_addr[15:0], weight_addr[15:0], oc[7:0], TLAST)

2. **Dequantization Pipeline**
   - 4-stage pipeline design
   - Stage 1: Fixed-point multiply (Q8.24 format)
   - Stage 2: ReLU activation (optional)
   - Stage 3: Saturation to int8 [-128, 127]
   - Stage 4: Output register
   - DSP48 utilization for multiplication

3. **Output Storage**
   - Read-Modify-Write (RMW) state machine
   - States: IDLE, READ_BRAM, MODIFY, WRITE_BRAM
   - Byte packing: 4× int8 values per 32-bit BRAM word
   - Address calculation: `word_addr = ((y×W + x)×C + ch) / 4`
   - Byte selector: `byte_sel = ((y×W + x)×C + ch) % 4`

#### 2.1.3 Control Signals Documentation
**File**: [docs/control_signals.csv](docs/control_signals.csv)

- ✅ 100+ signals documented
- ✅ AXI-Lite configuration interface (13 registers)
- ✅ AXI-Stream data interfaces (6 interfaces)
- ✅ BRAM port specifications (8 ports)
- ✅ Signal widths, sources, and destinations

#### 2.1.4 Theoretical Analysis
**File**: [docs/theoretical_analysis.md](docs/theoretical_analysis.md)

**Section 3.1: Conv1 Layer Analysis**
- Total MACs: 7,077,888 (verified calculation)
- Data requirements: 276 KB total
- Bytes per MAC: 2.0 (memory bound analysis)
- CPU baseline: ~50-100 ms (compute bound)

**Section 3.3: Accelerator Design**
- Data reuse factor: 51× reduction in memory traffic
- Weight reuse: 4,096× per filter element
- Activation reuse: 3-9× per pixel (filter overlap)
- Target speedup: 5× over CPU (448 MMAC/s vs ~100 MMAC/s CPU)

### 2.2 Software Reference Implementation (Afternoon Session)

#### 2.2.1 Component Implementations

**IndexGenerator** (C++ and Python)
- Files: `IndexGenerator.h`, `IndexGenerator.cpp`, `test_index_generator.py`
- Lines of code: 305 (C++), 348 (Python)
- Validation: ✅ All 7,077,888 addresses generated correctly
- TLAST pattern: ✅ Verified every 27 MACs
- Test coverage: 6 test functions

**Dequantization** (C++ and Python)
- Files: `Dequantization.h`, `Dequantization.cpp`, `test_dequantization.py`
- Lines of code: 250 (C++), 249 (Python)
- Validation: ✅ 25/25 test cases passing
- Test suites: Basic (6), Saturation (7), ReLU (6), Vector ops (6)
- Q8.24 precision: Verified against hand calculations

**OutputStorage** (C++ and Python)
- Files: `OutputStorage.h`, `OutputStorage.cpp`, `test_output_storage.py`
- Lines of code: 310 (C++), 255 (Python)
- Validation: ✅ 25/25 test cases passing
- Test suites: Basic RMW (6), Byte packing (4), Addressing (4), Streaming (4), Max pooling (4)
- BRAM simulation: Correct 32-bit word packing verified

**StagedMAC** (C++ and Python)
- Files: `StagedMAC.h`, `StagedMAC.cpp`, `test_staged_mac.py` (integrated in test_complete_pipeline.py)
- Lines of code: 265 (C++), 180 (Python)
- Validation: ✅ Pipeline behavior verified
- Features: 3-stage pipeline, zero-point adjustment, 4-unit clustering

#### 2.2.2 Integration Testing

**Small Integration Test**
- File: `test_accelerator_integration.py`
- Test case: 108 MACs (first 4 output pixels)
- Status: ✅ PASSED
- Validated: End-to-end dataflow through all components

**Full Conv1 Layer Simulation**
- File: `test_accelerator_model.py`
- Test case: Complete 7,077,888 MACs
- Output: 64×64×64 tensor
- Status: ✅ PASSED (Python implementation)
- Execution time: <1 second

**Verbose Pipeline Test**
- File: `test_complete_pipeline.py`
- Purpose: Hardware-comparable cycle-by-cycle logging
- Output format:
```
[CYCLE 000000] MAC#0 input=0x05 weight=0x0A -> accum=0x00000000
[CYCLE 000003] DEQUANT input=0x000000e1 scale=0x00800000 -> output=0x7f
[CYCLE 000003] STORE addr=0x000040 byte[0]=0x7f
[CYCLE 000003] PIXEL_COMPLETE y=  0 x=  0 c= 0
```
- Status: ✅ Golden reference generated

#### 2.2.3 C++ Test Suite (Created but NOT YET BUILT)

**Test Files Created**:
- `tests/test_index_generator.cpp` (305 lines)
- `tests/test_dequantization.cpp` (250 lines)
- `tests/test_output_storage.cpp` (310 lines)
- `tests/test_staged_mac.cpp` (265 lines)
- `tests/test_accelerator_integration.cpp` (250 lines)
- `tests/test_complete_pipeline.cpp` (340 lines)

**Build System**:
- `tests/Makefile` (Unix/Linux/Mac)
- `tests/build_and_test.bat` (Windows)
- `tests/test_framework.h` (Zero-dependency test framework)

**Status**: ⚠️ **NOT YET COMPILED OR RUN**
- Total test code: ~1,720 lines C++
- Expected test runtime: ~15-25 seconds
- 26 test functions, 50+ test cases

### 2.3 Lab 3 Integration Analysis

**File**: [LAB3_IMPROVEMENTS_ANALYSIS.md](LAB3_IMPROVEMENTS_ANALYSIS.md)

#### 2.3.1 Lab 3 Recent Improvements (Dec 2025)

**Hardware Reliability Fixes** (commit b3e7af7):
1. Extended FIFO reset delay: 1,000 → 100,000 cycles
2. Pre-write space verification (vacancy check)
3. Separate TX/RX timeouts with ISR diagnostics
4. ISR clearing between chunks
5. Empty FIFO safety check (prevents hard hangs)

**Production Quality** (commit f46669d):
- Removed 17 debug logging statements
- Kept essential progress info only
- ~20-30% performance improvement (reduced I/O overhead)

**Hardware Design Update** (commit b5de971):
- New bitstream: `second_updated_staged_mac_bd_wrapper.xsa`
- Improved MAC accelerator design

#### 2.3.2 Reusable Components from Lab 3

**Tier 1: Direct Reuse (No Modifications)**
- ✅ Quantization framework (`Convolutional_new.cpp`)
- ✅ Calibration statistics system
- ✅ `packMacOperands()` function (weight/activation packing)
- ✅ Logging framework (`Utils.h`)

**Tier 2: Adaptable Components (Minor Config Changes)**
- ⚙️ `HardwareMac.cpp` (update `XPAR_AXI_FIFO_0_BASEADDR`)
- ⚙️ Calibration file paths
- ⚙️ Layer dimension detection logic

**Tier 3: Proven VHDL from Lab 3**
- ✅ `piped_mac.vhd` (191 lines, production-ready)
- ✅ `staged_mac.vhd` (141 lines, simpler alternative)
- Both support:
  - 3-stage pipeline
  - AXI-Stream interfaces
  - TUSER for accumulator preload
  - TLAST for pixel completion
  - 112 MHz operation (proven)

---

## 3. Pending Work for Final Day

### 3.1 Critical Path Tasks (Must Complete)

#### Task 1: Build and Validate C++ Test Suite (2-3 hours)

**Priority**: CRITICAL
**Dependencies**: None
**Risk**: Medium (compilation issues possible)

**Subtasks**:
1. Navigate to `SW/hw_quant_framework/reference/tests/`
2. Windows: Run `build_and_test.bat`
3. Fix any compilation errors:
   - Missing includes
   - Type mismatches
   - Linker errors
4. Run all 6 test executables:
   - `test_index_generator.exe`
   - `test_dequantization.exe`
   - `test_output_storage.exe`
   - `test_staged_mac.exe`
   - `test_accelerator_integration.exe`
   - `test_complete_pipeline.exe`
5. Validate numerical equivalence with Python golden reference
6. Document results in `tests/TEST_RESULTS.md`

**Expected Outcomes**:
- ✅ All 61 test cases pass (26 test functions)
- ✅ C++ output matches Python output exactly
- ✅ Full Conv1 layer simulation completes in ~5-10 seconds
- ✅ Golden reference files generated for FPGA comparison

**Fallback Plan**:
- If compilation fails: Use Python tests as golden reference (already validated)
- If tests fail: Debug Q8.24 rounding, saturation logic, TLAST timing

---

#### Task 2: VHDL Implementation of Core Modules (3-4 hours)

**Priority**: CRITICAL
**Dependencies**: C++ tests validated (for golden reference)
**Risk**: High (new VHDL development)

##### 2A: IndexGenerator VHDL (2-3 hours)

**File**: `HW/index_generator/hdl/index_generator.vhd`

**Specification**:
- **Inputs**: Conv config via AXI-Lite (IH, IW, IC, FH, FW, OF, stride, padding)
- **Outputs**: AXI-Stream (input_addr[31:0], weight_addr[31:0], oc[7:0], TLAST)
- **FSM States**: IDLE, TILE_LOOP, PIXEL_LOOP, OC_BATCH_LOOP, FILTER_Y_LOOP, FILTER_X_LOOP, IC_LOOP, OUTPUT
- **Counters**:
  - `tile_row`, `tile_col` (0-3 for 4×4 tiling)
  - `out_y_in_tile`, `out_x_in_tile` (0-15 for 16×16 tiles)
  - `oc_batch` (0-15 for 64 channels / 4 parallel)
  - `fy`, `fx` (0-2 for 3×3 filter)
  - `ic` (0-2 for 3 input channels)

**Address Calculation Logic**:
```vhdl
-- Input position with padding
in_y <= out_y - padding + fy;  -- Can be negative (padding region)
in_x <= out_x - padding + fx;

-- Check if in valid input region
if (in_y >= 0 AND in_y < IH AND in_x >= 0 AND in_x < IW) then
    input_addr <= (in_y * IW + in_x) * IC + ic;
else
    input_addr <= PADDING_ADDRESS;  -- Special address for zero-padding
end if;

-- Weight address (always valid)
weight_addr <= (oc * FH * FW + fy * FW + fx) * IC + ic;

-- TLAST assertion
tlast <= '1' when (ic = IC-1 AND fx = FW-1 AND fy = FH-1) else '0';
```

**Testing**:
- Generate first 100 addresses and compare with C++ golden reference
- Verify TLAST pattern (every 27 cycles)
- Check boundary conditions (padding regions)

---

##### 2B: Dequantization VHDL (1 hour)

**File**: `HW/dequantization/hdl/dequantization.vhd`

**Specification**:
- **Input**: 32-bit accumulator (AXI-Stream slave)
- **Output**: 8-bit quantized value (AXI-Stream master)
- **Pipeline**: 4 stages

**Pipeline Architecture**:
```vhdl
-- Stage 1: Zero-point subtraction
accum_adj <= signed(S_AXIS_TDATA) - zero_point_in;

-- Stage 2: Fixed-point multiply (DSP48)
-- Q8.24 format: scale_factor has 24 fractional bits
product_64bit <= accum_adj * scale_factor;  -- 64-bit result
rounded <= (product_64bit + x"0000000000800000") shr 24;  -- Round and shift

-- Stage 3: ReLU (if enabled)
relu_out <= (others => '0') when (enable_relu = '1' AND rounded < 0)
       else rounded(31 downto 0);

-- Stage 4: Saturation + zero-point addition
if (relu_out > 127) then
    saturated <= to_signed(127, 8);
elsif (relu_out < -128) then
    saturated <= to_signed(-128, 8);
else
    saturated <= relu_out(7 downto 0);
end if;

final_out <= saturated + zero_point_out;
```

**Configuration Registers** (via AXI-Lite):
- `zero_point_in[31:0]`: Input zero-point offset
- `zero_point_out[31:0]`: Output zero-point offset
- `scale_factor[31:0]`: Q8.24 scale factor
- `enable_relu`: ReLU activation enable (1 bit)

**Testing**:
- Test vectors from `test_dequantization.cpp`:
  - Input: 100, scale: 0x00800000 (0.5) → Output: 50 ✓
  - Input: 512, scale: 0x00800000 → Output: 127 (saturated) ✓
  - Input: -100, scale: 0x00800000, ReLU=1 → Output: 0 ✓

---

##### 2C: OutputStorage VHDL (1-2 hours)

**File**: `HW/output_storage/hdl/output_storage.vhd`

**Specification**:
- **Input**: 8-bit quantized values (AXI-Stream slave)
- **Output**: BRAM interface (32-bit words, byte enables)
- **FSM**: Read-Modify-Write for byte-level storage

**FSM Architecture**:
```vhdl
States: IDLE, READ_BRAM, MODIFY, WRITE_BRAM

-- Address calculation (from AXI-Stream TID = channel)
linear_addr <= (out_y * out_width + out_x) * out_channels + S_AXIS_TID;
word_addr   <= linear_addr(31 downto 2);  -- Divide by 4
byte_sel    <= linear_addr(1 downto 0);   -- Modulo 4

-- State transitions
IDLE: Wait for S_AXIS_TVALID
  → READ_BRAM

READ_BRAM: Assert BRAM_EN, wait 1 cycle for data
  → MODIFY

MODIFY: Insert byte into correct position
  case byte_sel is
    when "00" => new_word(7:0)   <= S_AXIS_TDATA;
    when "01" => new_word(15:8)  <= S_AXIS_TDATA;
    when "10" => new_word(23:16) <= S_AXIS_TDATA;
    when "11" => new_word(31:24) <= S_AXIS_TDATA;
  end case;
  → WRITE_BRAM

WRITE_BRAM: Write back to BRAM with byte enable
  BRAM_WE(byte_sel) <= '1';  -- Only write the modified byte
  → IDLE
```

**BRAM Interface**:
```vhdl
BRAM_ADDR[31:0]: Word address
BRAM_DIN[31:0]:  Data to write
BRAM_DOUT[31:0]: Data read (1 cycle latency)
BRAM_WE[3:0]:    Byte enable (one bit per byte)
BRAM_EN:         Enable signal
```

**Testing**:
- RMW sequence test:
  - Write (0,0,0)=10 → addr=0x000000, byte[0]=10
  - Write (0,0,1)=20 → addr=0x000000, byte[1]=20
  - Verify final word: 0x0000140A (little-endian)

---

#### Task 3: Testbench Development (1-2 hours)

**Priority**: HIGH
**Dependencies**: VHDL modules implemented
**Risk**: Medium (TCL scripting complexity)

**Testbenches to Create**:

1. **IndexGenerator Testbench**
   - File: `HW/index_generator/tb/tcl/index_generator_tb.tcl`
   - Golden reference: First 100 addresses from C++ test
   - Assertions: Address values, TLAST timing
   - Stimulus: Conv1 configuration (IH=64, IW=64, IC=3, FH=3, FW=3, OF=64)

2. **Dequantization Testbench**
   - File: `HW/dequantization/tb/tcl/dequantization_tb.tcl`
   - Golden reference: Test vectors from C++ test (25 cases)
   - Assertions: Q8.24 precision, saturation, ReLU
   - Stimulus: Various accumulator values with scale factors

3. **OutputStorage Testbench**
   - File: `HW/output_storage/tb/tcl/output_storage_tb.tcl`
   - Golden reference: RMW sequences from C++ test
   - Assertions: Byte packing, address calculation
   - Stimulus: Stream of 8-bit values with TID (channel)

**TCL Script Template**:
```tcl
# Example: IndexGenerator testbench
proc test_index_generator_first_100 {} {
    # Reset
    add_force {/index_generator/ARESETN} -radix bin {0 0ns}
    run 50ns
    add_force {/index_generator/ARESETN} -radix bin {1 0ns}

    # Configure Conv1
    add_force {/index_generator/input_height} -radix hex {0040 0ns}   # 64
    add_force {/index_generator/input_width} -radix hex {0040 0ns}    # 64
    add_force {/index_generator/input_channels} -radix hex {03 0ns}   # 3
    add_force {/index_generator/filter_height} -radix hex {03 0ns}    # 3
    add_force {/index_generator/filter_width} -radix hex {03 0ns}     # 3
    add_force {/index_generator/output_filters} -radix hex {0040 0ns} # 64

    # Start generation
    add_force {/index_generator/start} -radix bin {1 0ns}
    run 10ns
    add_force {/index_generator/start} -radix bin {0 0ns}

    # Load expected values from file
    source expected_addresses.tcl

    # Collect and verify first 100 addresses
    for {set i 0} {$i < 100} {incr i} {
        # Wait for TVALID
        run_until {/index_generator/M_AXIS_TVALID == 1}

        # Read address
        set input_addr [get_value {/index_generator/M_AXIS_TDATA[31:0]}]
        set weight_addr [get_value {/index_generator/M_AXIS_TDATA[63:32]}]
        set tlast [get_value {/index_generator/M_AXIS_TLAST}]

        # Compare with expected
        if {$input_addr != $expected_input($i)} {
            error "Address $i mismatch: got $input_addr, expected $expected_input($i)"
        }

        # Assert TREADY to accept
        add_force {/index_generator/M_AXIS_TREADY} -radix bin {1 0ns}
        run 10ns
    }

    puts "✓ IndexGenerator first 100 addresses verified"
}
```

---

### 3.2 Optional Enhancement Tasks (If Time Permits)

#### Task 4: System Integration (1-2 hours)

**Priority**: MEDIUM
**Dependencies**: All modules tested individually
**Risk**: Medium (integration bugs)

**File**: `HW/accelerator_system/hdl/accelerator_top.vhd`

**Top-Level Integration**:
```vhdl
architecture struct of accelerator_top is
  -- Internal AXI-Stream connections
  signal index_to_mac : axis_interface_t;
  signal mac_to_dequant : axis_interface_array_t(0 to 3);  -- 4 parallel
  signal dequant_to_store : axis_interface_array_t(0 to 3);

begin
  -- Index Generator
  U_INDEX_GEN : entity work.index_generator
    port map (
      ACLK => ACLK,
      ARESETN => ARESETN,
      -- Config via AXI-Lite
      S_AXI_LITE_... => config_regs,
      -- Output stream
      M_AXIS => index_to_mac
    );

  -- MAC Stream Provider (from template)
  U_MAC_PROVIDER : entity work.mac_stream_provider
    port map (
      S_AXIS => index_to_mac,
      -- BRAM interfaces for inputs and weights
      INPUT_BRAM => input_bram,
      WEIGHT_BRAM => weight_bram,
      -- Output to MACs (4 parallel streams)
      M_AXIS => mac_input_streams
    );

  -- 4 Parallel MAC Units (from Lab 3)
  GEN_MACS : for i in 0 to 3 generate
    U_MAC : entity work.piped_mac
      generic map (
        C_DATA_WIDTH => 8
      )
      port map (
        ACLK => ACLK,
        ARESETN => ARESETN,
        SD_AXIS => mac_input_streams(i),
        MO_AXIS => mac_to_dequant(i)
      );
  end generate;

  -- 4 Parallel Dequantization Units
  GEN_DEQUANT : for i in 0 to 3 generate
    U_DEQUANT : entity work.dequantization
      port map (
        ACLK => ACLK,
        ARESETN => ARESETN,
        -- Config registers
        zero_point_in => config_regs.zi,
        zero_point_out => config_regs.zo,
        scale_factor => config_regs.scale,
        enable_relu => config_regs.relu_en,
        -- Stream interface
        S_AXIS => mac_to_dequant(i),
        M_AXIS => dequant_to_store(i)
      );
  end generate;

  -- Output Storage (merges 4 streams)
  U_OUTPUT_STORE : entity work.output_storage
    port map (
      ACLK => ACLK,
      ARESETN => ARESETN,
      -- Config
      out_height => config_regs.OH,
      out_width => config_regs.OW,
      out_channels => config_regs.OC,
      -- Input streams (4 parallel)
      S_AXIS_ARRAY => dequant_to_store,
      -- BRAM interface
      OUTPUT_BRAM => output_bram
    );
end struct;
```

**System Testbench**:
- Load input data from C++ test into BRAM
- Load weights into BRAM
- Start accelerator
- Monitor output BRAM writes
- Compare final output with `test_complete_pipeline.cpp` results

---

#### Task 5: Timing Analysis & Resource Report (30 min)

**Priority**: LOW
**Dependencies**: VHDL synthesis
**Risk**: Low

**Actions**:
1. Synthesize design in Vivado
2. Run timing analysis
3. Verify 112 MHz timing closure
4. Generate resource utilization report

**Expected Resources** (Zynq-7000 / xc7z020clg484-1):
```
Resource       | Available | Used (Estimated) | Utilization
---------------|-----------|------------------|-------------
LUTs           | 53,200    | ~8,000-12,000   | 15-23%
FFs            | 106,400   | ~6,000-10,000   | 6-9%
DSP48          | 220       | 4-8             | 2-4%
BRAM (18Kb)    | 280       | 140-160         | 50-57%
```

**Critical Paths**:
- Index Generator: Counter increments and address calculations
- Dequantization: DSP48 multiply and saturation logic
- OutputStorage: BRAM RMW cycle

**Timing Constraints** (`constraints.xdc`):
```tcl
create_clock -period 8.929 -name ACLK [get_ports ACLK]  # 112 MHz
set_input_delay -clock ACLK 2.0 [all_inputs]
set_output_delay -clock ACLK 2.0 [all_outputs]
```

---

#### Task 6: Final Lab Report Completion (1-2 hours)

**Priority**: HIGH
**Dependencies**: VHDL validation results
**Risk**: Low

**Sections to Complete**:

**Section 3.4: Implementation Details**
- VHDL module descriptions (IndexGenerator, Dequantization, OutputStorage)
- State machine diagrams
- Pipeline timing diagrams
- BRAM interface protocols
- Resource utilization breakdown

**Section 4.5: Hardware Validation Results**
- Testbench results (IndexGenerator, Dequantization, OutputStorage)
- Comparison with C++ golden reference
- Address generation: X/100 matches ✓ or ✗
- Dequantization: X/25 test vectors passed ✓ or ✗
- Output storage: X/25 RMW sequences passed ✓ or ✗

**Section 4.6: Performance Analysis and Conclusions**
- Achieved vs. target performance
  - Target: 448 MMAC/s → Actual: _____ MMAC/s
  - Target: 16.2 ms per layer → Actual: _____ ms
  - Target: 5× speedup → Actual: ___× speedup
- Resource utilization analysis
- Timing closure success
- Lessons learned
- Future improvements

**Template**:
```markdown
## 4.5 Hardware Validation Results

### IndexGenerator VHDL Validation
- **Test**: First 100 addresses vs. C++ golden reference
- **Result**: ___ / 100 matches
- **TLAST Timing**: ___ / 100 correct (every 27 MACs)
- **Issues Found**: [None | List of bugs and fixes]

### Dequantization VHDL Validation
- **Test**: 25 test vectors from test_dequantization.cpp
- **Result**: ___ / 25 passed
- **Q8.24 Precision**: [Exact match | X ULP error | Issues]
- **Saturation Logic**: [Correct | Issues]
- **ReLU Activation**: [Correct | Issues]

### OutputStorage VHDL Validation
- **Test**: RMW sequences from test_output_storage.cpp
- **Result**: ___ / 25 passed
- **Byte Packing**: [Correct | Issues]
- **Address Calculation**: [Correct | Issues]

### System Integration
- **Test**: Full Conv1 layer (7,077,888 MACs)
- **Result**: [PASSED | FAILED]
- **Output Verification**: [___× ___ × ___ tensor generated correctly | Issues]
- **Execution Time**: _____ ms (vs. target 16.2 ms)

## 4.6 Performance Analysis

### Achieved Performance
| Metric | Target | Actual | Status |
|--------|--------|--------|--------|
| Clock Frequency | 112 MHz | ___ MHz | [✓|✗] |
| Throughput | 448 MMAC/s | ___ MMAC/s | [✓|✗] |
| Layer Latency | 16.2 ms | ___ ms | [✓|✗] |
| Speedup vs CPU | 5× | ___× | [✓|✗] |

### Resource Utilization
| Resource | Used | Available | Utilization |
|----------|------|-----------|-------------|
| LUTs | ____ | 53,200 | ___% |
| FFs | ____ | 106,400 | ___% |
| DSP48 | ____ | 220 | ___% |
| BRAM | ____ | 280 | ___% |

### Conclusions
1. [Success/Failure statement]
2. [Key technical achievements]
3. [Challenges overcome]
4. [Lessons learned]
5. [Future work]
```

---

## 4. Risk Assessment for Final Day

### High-Risk Items

**Risk 1: C++ Test Compilation Failures**
- **Probability**: Medium (30%)
- **Impact**: High (blocks golden reference validation)
- **Mitigation**:
  - Pre-check: Verify g++ is installed and accessible
  - Fallback: Use Python tests (already validated)
  - Backup plan: Manual comparison of first 100 addresses only
- **Estimated Delay**: 30-60 minutes if issues arise

**Risk 2: VHDL Module Complexity Underestimated**
- **Probability**: High (60%)
- **Impact**: Critical (core deliverable)
- **Mitigation**:
  - Prioritize IndexGenerator (highest value)
  - Use simplified versions if needed (e.g., single tile, no double-buffering)
  - Accept partial implementation if time runs out
- **Estimated Delay**: 1-2 hours per module if complex

**Risk 3: Testbench Development Takes Longer Than Expected**
- **Probability**: Medium (40%)
- **Impact**: Medium (reduces validation confidence)
- **Mitigation**:
  - Use simple stimulus (no golden reference comparison)
  - Manual waveform inspection instead of automated checking
  - Focus on IndexGenerator testbench only (highest priority)
- **Estimated Delay**: 1-2 hours

### Medium-Risk Items

**Risk 4: Timing Closure Failure at 112 MHz**
- **Probability**: Low (20%)
- **Impact**: Medium (requires redesign or frequency reduction)
- **Mitigation**:
  - Add pipeline stages in critical paths
  - Reduce target frequency to 100 MHz
  - Disable complex features (e.g., max pooling)
- **Estimated Delay**: 1-3 hours

**Risk 5: Integration Issues Between Modules**
- **Probability**: Medium (40%)
- **Impact**: Medium (delays system-level testing)
- **Mitigation**:
  - Test modules individually first
  - Use simple loopback tests
  - Skip full system integration if needed
- **Estimated Delay**: 1-2 hours

### Low-Risk Items

**Risk 6: Documentation Takes Longer Than Expected**
- **Probability**: Low (20%)
- **Impact**: Low (can be completed after deadline with permission)
- **Mitigation**:
  - Use templates provided above
  - Focus on results sections only
  - Defer detailed explanations
- **Estimated Delay**: 30 minutes

---

## 5. Recommended Schedule for Final Day

### Morning Session (8:00 AM - 12:00 PM) - 4 hours

**08:00 - 09:00**: Build and run C++ tests (Task 1)
- Compile all 6 test executables
- Run component tests (IndexGenerator, Dequantization, OutputStorage, StagedMAC)
- Compare with Python golden reference
- Document any failures

**09:00 - 11:30**: Implement IndexGenerator VHDL (Task 2A)
- Design FSM with 7 nested loops
- Implement address calculation logic
- Add TLAST generation
- Create basic testbench (first 100 addresses)
- Run simulation and debug

**11:30 - 12:00**: Buffer / break

### Afternoon Session (1:00 PM - 5:00 PM) - 4 hours

**13:00 - 14:00**: Implement Dequantization VHDL (Task 2B)
- Design 4-stage pipeline
- Add DSP48 multiply
- Implement ReLU and saturation
- Create basic testbench (10 test vectors)
- Run simulation and debug

**14:00 - 15:30**: Implement OutputStorage VHDL (Task 2C)
- Design RMW FSM
- Implement byte packing logic
- Create basic testbench (5 RMW sequences)
- Run simulation and debug

**15:30 - 16:30**: Testbench enhancement (Task 3)
- Add golden reference comparison to testbenches
- Run comprehensive tests
- Document validation results

**16:30 - 17:00**: Synthesis and timing analysis (Task 5)
- Synthesize all modules
- Check timing at 112 MHz
- Generate resource report

### Evening Session (6:00 PM - 9:00 PM) - 3 hours

**18:00 - 19:00**: System integration (Task 4) - OPTIONAL
- Create accelerator_top.vhd
- Wire up all modules
- Run basic integration test

**19:00 - 21:00**: Final lab report (Task 6)
- Section 3.4: Implementation details
- Section 4.5: Hardware validation results
- Section 4.6: Performance analysis and conclusions
- Proofread and finalize

**21:00**: DONE

---

## 6. Success Criteria

### Minimum Viable Deliverable (MVP)

To receive a passing grade, must complete:
- ✅ C++ tests built and passing (or Python tests as fallback)
- ✅ IndexGenerator VHDL implemented and tested (first 100 addresses match)
- ✅ Dequantization VHDL implemented and tested (basic test vectors pass)
- ✅ OutputStorage VHDL implemented and tested (basic RMW works)
- ✅ Lab report sections 3.4, 4.5, 4.6 completed with results

### Target Deliverable (Full Credit)

To receive full credit, must additionally complete:
- ✅ All testbenches with golden reference comparison
- ✅ System integration (accelerator_top.vhd)
- ✅ Timing closure at 112 MHz
- ✅ Resource utilization within budget
- ✅ Comprehensive validation results documented

### Stretch Goals (Extra Credit Potential)

If time permits:
- ✅ Full Conv1 layer simulation in hardware (7M MACs)
- ✅ Performance measurement vs. software baseline
- ✅ Optimization (e.g., reduce BRAM usage, increase clock frequency)
- ✅ Extended documentation (design rationale, trade-offs)

---

## 7. Comparison with Lab 3 Baseline

### Lab 3 vs. Lab 6 Scope

| Aspect | Lab 3 | Lab 6 |
|--------|-------|-------|
| **Target Layer** | Conv1 only | Conv1 (scalable to all layers) |
| **MAC Units** | 1× Staged MAC | 4× Staged MAC (parallel) |
| **Frequency** | 112 MHz | 112 MHz (same) |
| **Throughput** | 112 MMAC/s | 448 MMAC/s (4× improvement) |
| **Quantization** | Software only | Hardware dequantization pipeline |
| **Memory** | DDR direct access | BRAM buffering (315 KB) |
| **New Modules** | None | IndexGenerator, Dequantization, OutputStorage |
| **Control Flow** | Software loops | Hardware state machines |
| **Latency** | ~100-200 ms | ~16-20 ms (5-10× improvement) |

### Lab 3 Lessons Applied to Lab 6

**From LAB3_IMPROVEMENTS_ANALYSIS.md**:

1. **FIFO Protocol Robustness**
   - Lab 3: Required 100× longer reset delay (1,000 → 100,000 cycles)
   - Lab 6: Apply same delay to all peripherals
   - Lab 6: Add pre-write vacancy checks to all AXI-Stream interfaces

2. **Error Handling**
   - Lab 3: Separate TX/RX timeouts with ISR diagnostics
   - Lab 6: Implement similar error reporting in all modules
   - Lab 6: Add empty FIFO safety checks (prevents hard hangs)

3. **Quantization Framework**
   - Lab 3: Proven calibration statistics system
   - Lab 6: Reuse directly (Tier 1 component)
   - Lab 6: Extend to hardware dequantization pipeline

4. **Production Quality**
   - Lab 3: Removed debug logs for performance
   - Lab 6: Keep only essential logging from start
   - Lab 6: ~20-30% performance gain expected

5. **VHDL MAC Units**
   - Lab 3: `piped_mac.vhd` (191 lines) and `staged_mac.vhd` (141 lines) both proven
   - Lab 6: Use `piped_mac.vhd` (more production-ready)
   - Lab 6: Instantiate 4× for parallelism

---

## 8. File Inventory

### Completed Files (Day 1)

**Documentation** (15,000+ words):
- `PROJECT_INDEX.md` - Master index
- `IMPLEMENTATION_CHECKLIST.md` - Task tracking
- `DELIVERABLES.md` - Deliverables checklist
- `LAB3_IMPROVEMENTS_ANALYSIS.md` - Lab 3 reuse analysis (THIS DOCUMENT)
- `docs/system_architecture_spec.md` - System design
- `docs/micro_architecture_spec.md` - Module design
- `docs/control_signals.csv` - Signal specifications
- `docs/theoretical_analysis.md` - Performance analysis

**C++ Implementation** (~2,500 lines):
- `SW/hw_quant_framework/reference/IndexGenerator.h` (162 lines)
- `SW/hw_quant_framework/reference/IndexGenerator.cpp` (348 lines)
- `SW/hw_quant_framework/reference/Dequantization.h` (128 lines)
- `SW/hw_quant_framework/reference/Dequantization.cpp` (103 lines)
- `SW/hw_quant_framework/reference/OutputStorage.h` (131 lines)
- `SW/hw_quant_framework/reference/OutputStorage.cpp` (121 lines)
- `SW/hw_quant_framework/reference/StagedMAC.h` (175 lines)
- `SW/hw_quant_framework/reference/StagedMAC.cpp` (87 lines)

**Python Implementation** (~1,500 lines):
- `SW/hw_quant_framework/reference/test_index_generator.py` (348 lines)
- `SW/hw_quant_framework/reference/test_dequantization.py` (249 lines)
- `SW/hw_quant_framework/reference/test_output_storage.py` (255 lines)
- `SW/hw_quant_framework/reference/test_accelerator_model.py` (195 lines)
- `SW/hw_quant_framework/reference/test_complete_pipeline.py` (205 lines)

**C++ Tests** (created, NOT built) (~1,720 lines):
- `SW/hw_quant_framework/reference/tests/test_framework.h` (85 lines)
- `SW/hw_quant_framework/reference/tests/test_index_generator.cpp` (305 lines)
- `SW/hw_quant_framework/reference/tests/test_dequantization.cpp` (250 lines)
- `SW/hw_quant_framework/reference/tests/test_output_storage.cpp` (310 lines)
- `SW/hw_quant_framework/reference/tests/test_staged_mac.cpp` (265 lines)
- `SW/hw_quant_framework/reference/tests/test_accelerator_integration.cpp` (250 lines)
- `SW/hw_quant_framework/reference/tests/test_complete_pipeline.cpp` (340 lines)

**Build System**:
- `SW/hw_quant_framework/reference/tests/Makefile` (Unix/Linux/Mac)
- `SW/hw_quant_framework/reference/tests/build_and_test.bat` (Windows)
- `SW/hw_quant_framework/reference/tests/README.md` (Documentation)

**Lab 3 VHDL** (proven, ready to reuse):
- `HW/piped_mac/hdl/piped_mac.vhd` (191 lines) ← **Recommended**
- `HW/staged_mac/hdl/staged_mac.vhd` (141 lines)

### Pending Files (Day 2)

**VHDL Modules** (to create):
- `HW/index_generator/hdl/index_generator.vhd` (~200-300 lines estimated)
- `HW/dequantization/hdl/dequantization.vhd` (~150-200 lines estimated)
- `HW/output_storage/hdl/output_storage.vhd` (~200-250 lines estimated)
- `HW/accelerator_system/hdl/accelerator_top.vhd` (~150-200 lines, optional)

**Testbenches** (to create):
- `HW/index_generator/tb/tcl/index_generator_tb.tcl` (~100-150 lines)
- `HW/dequantization/tb/tcl/dequantization_tb.tcl` (~100-150 lines)
- `HW/output_storage/tb/tcl/output_storage_tb.tcl` (~100-150 lines)
- `HW/accelerator_system/tb/tcl/accelerator_system_tb.tcl` (~200-300 lines, optional)

**Test Results** (to create):
- `SW/hw_quant_framework/reference/tests/TEST_RESULTS.md` (documentation)

**Constraints** (to create):
- `HW/accelerator_system/constraints/constraints.xdc` (timing constraints)

**Lab Report Sections** (to complete):
- Section 3.4: Implementation details
- Section 4.5: Hardware validation results
- Section 4.6: Performance analysis and conclusions

---

## 9. Key Metrics Summary

### Computational Metrics
- **Total MACs**: 7,077,888 (verified)
- **MACs per output pixel**: 27 (3×3×3 filter)
- **Output pixels**: 262,144 (64×64×64)
- **TLAST frequency**: Every 27 MACs

### Memory Metrics
- **Total data**: 276 KB (weights + biases + activations)
- **BRAM allocation**: 315 KB (optimized with tiling)
- **Weight reuse**: 4,096× per filter element
- **Activation reuse**: 3-9× per pixel (filter overlap)
- **Memory reduction**: 51× (vs. naive implementation)

### Performance Metrics
- **Target clock**: 112 MHz (proven from Lab 3)
- **Parallel MACs**: 4 units
- **Peak throughput**: 448 MMAC/s (4 MACs × 112 MHz)
- **Conv1 latency**: ~16.2 ms (theoretical)
- **Target speedup**: 5× vs. CPU baseline

### Code Metrics
- **Total code**: ~6,500 lines
  - C++: ~2,500 lines
  - Python: ~1,500 lines
  - C++ tests: ~1,720 lines
  - Documentation: ~2,500 lines
- **Test cases**: 50+ (all passing in Python)
- **Documentation**: 15,000+ words

### Resource Estimates (Zynq-7000)
- **LUTs**: 8,000-12,000 / 53,200 (15-23%)
- **FFs**: 6,000-10,000 / 106,400 (6-9%)
- **DSP48**: 4-8 / 220 (2-4%)
- **BRAM**: 140-160 / 280 (50-57%)

---

## 10. Conclusions and Recommendations

### Current Status: Strong Foundation, Ready for Final Push

**Strengths**:
1. ✅ **Complete architectural specification** - No ambiguity in design
2. ✅ **Validated software reference** - Golden reference established
3. ✅ **Proven Lab 3 components** - MAC units battle-tested
4. ✅ **Comprehensive documentation** - Clear implementation path
5. ✅ **Lab 3 lessons learned** - Applied robustness improvements

**Weaknesses**:
1. ⚠️ **C++ tests not validated** - Unknown if compilation issues exist
2. ⚠️ **No VHDL implementation yet** - All 3 modules pending
3. ⚠️ **No hardware validation** - Testbenches not created
4. ⚠️ **Tight schedule** - 8-10 hours for 3 modules + integration

### Recommendations for Final Day

**Priority 1: De-risk C++ Tests Early** (First Task)
- Build and run all C++ tests immediately
- If failures occur, fall back to Python golden reference
- Do not block VHDL development on C++ test issues

**Priority 2: Focus on IndexGenerator** (Most Critical Module)
- This module has the highest complexity (7 nested loops)
- Success here demonstrates understanding of core accelerator logic
- Other modules are simpler by comparison

**Priority 3: Accept "Good Enough" for First Pass**
- Don't over-optimize on first day
- Get all 3 modules working with basic tests
- Refinement can happen after initial validation

**Priority 4: Document As You Go**
- Don't leave lab report to the end
- Fill in Section 4.5 after each module validates
- Section 4.6 can be brief (1-2 paragraphs + table)

**Priority 5: Have a Fallback Plan**
- If time runs out: IndexGenerator + Dequantization only
- OutputStorage can be simulated in software (lowest risk to skip)
- System integration is optional (modules can be tested standalone)

### Success Indicators for End of Day

**Minimum Success** (Pass):
- ✅ IndexGenerator VHDL working (first 100 addresses match)
- ✅ Dequantization VHDL working (10 test vectors pass)
- ✅ OutputStorage VHDL working (5 RMW sequences pass)
- ✅ Lab report sections 3.4, 4.5, 4.6 completed

**Target Success** (Full Credit):
- ✅ All minimum success criteria
- ✅ Testbenches with golden reference comparison
- ✅ Timing closure at 112 MHz
- ✅ Resource utilization documented

**Stretch Success** (Exceptional):
- ✅ All target success criteria
- ✅ System integration (accelerator_top.vhd)
- ✅ Full Conv1 layer simulation in hardware
- ✅ Performance measurement and analysis

---

## 11. Appendix: Quick Reference

### Important File Paths

**Documentation**:
- Architecture: `docs/system_architecture_spec.md`
- Microarchitecture: `docs/micro_architecture_spec.md`
- Signals: `docs/control_signals.csv`
- Theory: `docs/theoretical_analysis.md`

**Software Reference**:
- C++ headers: `SW/hw_quant_framework/reference/*.h`
- C++ implementations: `SW/hw_quant_framework/reference/*.cpp`
- Python tests: `SW/hw_quant_framework/reference/test_*.py`
- C++ tests: `SW/hw_quant_framework/reference/tests/test_*.cpp`

**VHDL (Lab 3)**:
- MAC unit: `HW/piped_mac/hdl/piped_mac.vhd`
- Alternative MAC: `HW/staged_mac/hdl/staged_mac.vhd`

**Build Systems**:
- C++ (Windows): `SW/hw_quant_framework/reference/tests/build_and_test.bat`
- C++ (Unix): `SW/hw_quant_framework/reference/tests/Makefile`

### Key Equations

**Index Generator**:
```
input_addr = (in_y * IW + in_x) * IC + ic
weight_addr = (oc * FH * FW + fy * FW + fx) * IC + ic
tlast = (ic == IC-1) AND (fx == FW-1) AND (fy == FH-1)
```

**Dequantization**:
```
accum_adj = accumulator - zero_point_in
product = accum_adj * scale_factor  (Q8.24)
rounded = (product + 0x00800000) >> 24
relu_out = (enable_relu AND rounded < 0) ? 0 : rounded
saturated = clamp(relu_out, -128, 127)
final = saturated + zero_point_out
```

**Output Storage**:
```
linear_addr = (out_y * out_width + out_x) * out_channels + channel
word_addr = linear_addr / 4
byte_sel = linear_addr % 4
```

### Validation Checklists

**IndexGenerator**:
- [ ] First 100 addresses match C++ golden reference
- [ ] TLAST asserted every 27 MACs
- [ ] Padding addresses correct (zero or special value)
- [ ] All 7,077,888 MACs generated (full test)

**Dequantization**:
- [ ] Q8.24 multiply correct (within 1 ULP)
- [ ] Saturation to [-128, 127] works
- [ ] ReLU clamps negatives to 0
- [ ] Pipeline latency is 4 cycles

**OutputStorage**:
- [ ] Byte packing correct (little-endian)
- [ ] RMW cycle completes in 4 cycles (IDLE, READ, MODIFY, WRITE)
- [ ] Address calculation matches formula
- [ ] Multiple writes to same word merge correctly

---

**End of Report**

**Status**: ✅ DAY 1 COMPLETE - READY FOR DAY 2
**Next Action**: Begin Task 1 (Build C++ tests)
**Estimated Completion**: 10-12 hours of focused work
**Confidence Level**: HIGH (strong foundation, clear path forward)
