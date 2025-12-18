# Lab 3 Recent Improvements Analysis & Lab 6 Reuse Planning

**Project:** Hardware Design for AI/ML - Lab 3
**Analysis Date:** December 16, 2025
**Recent Commits Analyzed:** b5de971, f46669d, 1add25a, b3e7af7, bb93ca6

---

## Executive Summary

This codebase implements a **quantized convolutional neural network (CNN)** framework with hardware acceleration on the Xilinx Zynq-7000 (Zedboard). Recent improvements focused on:
1. **Hardware MAC reliability** (timing, error handling, FIFO control)
2. **Debug log cleanup** for production deployment
3. **Calibrated quantization** using pre-computed statistics

The code is well-structured for **reuse in Lab 6**, with clear separation between SW-only and HW-accelerated paths.

---

## 1. Recent Improvements Timeline

### Commit b5de971 (Dec 15, 2025) - "updated hardware"
**What Changed:**
- Added new hardware design file: `second_updated_staged_mac_bd_wrapper.xsa`
- Updated FPGA bitstream with improved MAC accelerator design

**Impact:** Hardware design updates that enable more reliable MAC operations

---

### Commit f46669d (Dec 15, 2025) - "removing debugs"
**What Changed:**
- Commented out 17 debug logging statements in `Convolutional_new.cpp`
- Removed verbose output for:
  - Weight scale calculations (line 508)
  - Input scale/zero-point values (lines 517-518)
  - Bias scale calculations (line 524)
  - Quantization progress messages (lines 542, 559, 575, 586)
  - Progress indicator dots (lines 601-604)
  - Output statistics (lines 699-703)

**Why This Matters:**
- **Performance**: Reduced I/O overhead during inference
- **Clean Output**: Only layer completion messages remain (`logInfo`)
- **Production Ready**: Keeps critical info, removes debugging noise

**Example - Before:**
```cpp
logDebug("Weight scale Sw = " + std::to_string(Sw) + " (max_weight = " + std::to_string(max_weight) + ")");
logDebug("Using calibrated input scale Si = " + std::to_string(Si) + ", zero point zi = " + std::to_string(static_cast<int>(zi)));
```

**After:**
```cpp
// logDebug("Weight scale Sw = " + std::to_string(Sw) + " (max_weight = " + std::to_string(max_weight) + ")");
// logDebug("Using calibrated input scale Si = " + std::to_string(Si) + ", zero point zi = " + std::to_string(static_cast<int>(zi)));
```

**Retained Important Logs:**
```cpp
logInfo("Processing layer: " + current_layer_name + " (dims: " + std::to_string(P) + "x" + std::to_string(Q) + "x" + std::to_string(M) + ")");
logInfo("Using calibration stats: " + input_stats_name + " - Si=" + std::to_string(input_stats.Si) + ", zi=" + std::to_string(static_cast<int>(input_stats.zi)));
logInfo("Layer " + current_layer_name + " quantized convolution complete\n");
```

---

### Commit b3e7af7 (Dec 15, 2025) - "hw fix"
**What Changed:** Major improvements to `HardwareMac.cpp` FIFO communication protocol

#### Problem: Hardware Hangs and Timeouts
The original implementation had several critical issues:
1. **Insufficient FIFO reset delay** (1,000 cycles → 100,000 cycles)
2. **No pre-write space checking** → FIFO overflow
3. **Single timeout for TX+RX** → poor error diagnostics
4. **No ISR clearing between chunks** → stale status bits
5. **Reading empty FIFO** → system hangs

#### Solutions Implemented

**A. Extended FIFO Reset Delay**
```cpp
// BEFORE: Small delay (insufficient for FPGA timing)
for(volatile int i=0; i<1000; i++);

// AFTER: Adequate delay for hardware stabilization
for(volatile int i=0; i<100000; i++);
```
**Reason:** FPGA peripherals need sufficient time after reset before becoming ready

---

**B. Pre-Write Space Verification**
```cpp
// NEW: Check FIFO vacancy BEFORE writing
int space_timeout = 1000000;
while (Xil_In32(kFifoBaseAddr + XLLF_TDFV_OFFSET) < chunk_len) {
    if (--space_timeout <= 0) {
         xil_printf("Error: Hardware MAC FIFO Full (Vacancy Wait Timeout) at chunk %d\r\n", offset/CHUNK_SIZE);
         return total_accumulator;
    }
}
```
**Benefit:** Prevents FIFO overflow and data corruption

---

**C. Separate TX/RX Timeouts with Status Reporting**
```cpp
// BEFORE: Single generic timeout
int timeout = 10000000;
while (Xil_In32(kFifoBaseAddr + XLLF_RDFO_OFFSET) == 0) {
    if (--timeout <= 0) {
        xil_printf("Error: Hardware MAC Timeout at chunk %d\r\n", offset/CHUNK_SIZE);
        return total_accumulator;
    }
}

// AFTER: Separate TX and RX phases with ISR diagnostics
// TX Phase
int tx_timeout = 1000000;
while ((Xil_In32(kFifoBaseAddr + XLLF_ISR_OFFSET) & 0x0C000000) == 0) {
     if (--tx_timeout <= 0) {
         xil_printf("Error: Hardware MAC TX Timeout at chunk %d (ISR=%08x)\r\n",
                    offset/CHUNK_SIZE, Xil_In32(kFifoBaseAddr + XLLF_ISR_OFFSET));
         return total_accumulator;
     }
}

// RX Phase
int rx_timeout = 1000000;
while (Xil_In32(kFifoBaseAddr + XLLF_RDFO_OFFSET) == 0) {
     if ((Xil_In32(kFifoBaseAddr + XLLF_ISR_OFFSET) & 0x08000000) != 0) {
          break;  // RC bit set, proceed to read check
     }
     if (--rx_timeout <= 0) {
         xil_printf("Error: Hardware MAC RX Timeout at chunk %d (ISR=%08x)\r\n",
                    offset/CHUNK_SIZE, Xil_In32(kFifoBaseAddr + XLLF_ISR_OFFSET));
         return total_accumulator;
     }
}
```
**Benefits:**
- Pinpoint exact failure location (TX vs RX)
- ISR register dump for hardware debugging
- Better error recovery

---

**D. ISR Clearing Between Chunks**
```cpp
// NEW: Clear status bits before each transaction
Xil_Out32(kFifoBaseAddr + XLLF_ISR_OFFSET, 0xFFFFFFFF);
```
**Reason:** Prevents stale status flags from previous operations causing false positives

---

**E. Empty FIFO Safety Check**
```cpp
// SAFETY: Do not read if FIFO is empty (Verified hang cause)
if (Xil_In32(kFifoBaseAddr + XLLF_RDFO_OFFSET) == 0) {
    volatile uint32_t len = Xil_In32(kFifoBaseAddr + XLLF_RLF_OFFSET);

    xil_printf("Error: Hardware MAC RC set but FIFO Empty at chunk %d (ISR=%08x, RLF=%d)\r\n",
               offset/CHUNK_SIZE, Xil_In32(kFifoBaseAddr + XLLF_ISR_OFFSET), len);
    return total_accumulator;
}
```
**Critical Fix:** Reading from an empty FIFO causes **hard system hang** (verified bug)

---

**F. Removed Redundant Null Checks**
```cpp
// REMOVED: Unnecessary early return
if (packed_pairs == nullptr || pair_count == 0) {
    return 0;
}
```
**Reason:** Caller guarantees valid data; early returns complicate control flow

---

### Commit bb93ca6 (Dec 15, 2025) - "debug logs"
- Added initial debug logging (later removed in f46669d)
- File permission fixes for build scripts

---

## 2. Core Architecture Analysis

### 2.1 Calibrated Quantization System

**Key Innovation:** Pre-computed quantization parameters from calibration dataset

#### Calibration Statistics Structure
```cpp
struct CalibrationStats {
    fp32 min, max, mean, Si;  // Statistics and scale factor
    i8 zi;                    // Zero-point offset
};
```

**Loaded from:** `calibration_stats.json`
**Layers:** `_input`, `conv2d`, `conv2d_1` through `conv2d_5`

#### Two Operating Modes

**Mode 1: Individual Layer Testing** (`use_layer_specific_calibration = false`)
- Every layer uses `_input` calibration stats
- Assumes input is always the raw image
- Counter NOT incremented
- **Use Case:** Testing individual layers in isolation

**Mode 2: Full Inference Chain** (`use_layer_specific_calibration = true`)
- Layer counter tracks position in network
- `conv_layer_count = 0` → use `_input`
- `conv_layer_count = 1` → use `conv2d`
- `conv_layer_count = 2-6` → use `conv2d_1` through `conv2d_5`
- Counter incremented after each layer
- **Use Case:** Complete network inference

#### Quantization Formula
```
Input:  ix = round(Si * Ix) + zi        (8-bit signed)
Weight: wx = round(Sw * Wx)             (8-bit signed, symmetric)
Bias:   bx = round(Sb * Bx)             (32-bit signed)
Where:  Sb = Si * Sw

Accumulator = Σ(ix * wx) + bx           (32-bit signed)

Dequantization = (accumulator - zi*Σwx) / (Si * Sw)
```

**Zero-Point Correction:** Critical step that removes accumulated offset from using asymmetric quantization

---

### 2.2 Layer Architecture

**Detected Layers (by output dimensions):**
```
conv2d:   60x60x32  (Conv1)
conv2d_1: 56x56x32  (Conv2)
conv2d_2: 26x26x64  (Conv3)
conv2d_3: 24x24x64  (Conv4)
conv2d_4: 10x10x64  (Conv5)
conv2d_5: 8x8x128   (Conv6)
```

**Fallback Strategy:** Layers beyond conv2d_5 use conv2d_5 calibration (conservative approach for downstream layers like pooling, dense, etc.)

---

### 2.3 Hardware Acceleration Interface

**Function:** `HardwareMac::run(const uint16_t* packed_pairs, std::size_t pair_count)`

**Data Format:** 16-bit packed operands
```cpp
uint16_t packMacOperands(ML::i8 weight, ML::i8 activation) {
    return (static_cast<uint16_t>(static_cast<uint8_t>(weight)) << 8) |
           static_cast<uint16_t>(static_cast<uint8_t>(activation));
}
```
**Layout:** `[weight:8][activation:8]`

**Chunking Strategy:**
- Processes 16 pairs per chunk (configurable)
- Accumulates results across chunks
- Returns total 32-bit MAC result

**FIFO Protocol:**
1. Reset FIFO + clear ISR
2. Check space availability (TDFV)
3. Write TDR (destination register)
4. Write data to TDFD
5. Write TLF (triggers transfer)
6. Wait for TX complete (ISR bit 26) or RX complete (ISR bit 27)
7. Wait for RX data available (RDFO > 0)
8. Safety check: verify FIFO not empty
9. Read RLF then RDFD
10. Accumulate result

---

## 3. Code Quality Improvements

### 3.1 Error Handling Enhancements
✅ **Before:** Silent failures or generic timeouts
✅ **After:** Detailed error messages with ISR states, chunk numbers, and failure modes

### 3.2 Timing Robustness
✅ **Before:** Race conditions from insufficient delays
✅ **After:** 100x longer reset delay, proper synchronization

### 3.3 Production Readiness
✅ **Before:** Verbose debug output slowing execution
✅ **After:** Clean output with essential progress info only

### 3.4 Hardware Protocol Compliance
✅ **Before:** Violated FIFO write preconditions
✅ **After:** Full compliance with Xilinx AXI FIFO handshaking

---

## 4. Lab 6 Reuse Strategy

### 4.1 Reusable Components

#### **Tier 1: Direct Reuse (No Modifications)**
```
✅ Convolutional_new.cpp (entire quantization framework)
✅ CalibrationStats structure & JSON loader
✅ Layer counter & mode management system
✅ packMacOperands() packing function
✅ Quantization/dequantization math
✅ Utils.h logging framework
```

#### **Tier 2: Adaptable Components (Minor Config Changes)**
```
⚙️ HardwareMac.cpp
   - Update XPAR_AXI_FIFO_0_BASEADDR for new hardware
   - Adjust CHUNK_SIZE if needed
   - Tune timeout values for different clock frequencies

⚙️ Calibration file paths
   - Update paths in computeQuantizedInternal() (lines 336-343)
   - Generate new calibration_stats.json for Lab 6 model

⚙️ Layer dimension detection (lines 450-464)
   - Update dimension checks for Lab 6 network architecture
```

#### **Tier 3: Extension Points (New Development)**
```
🔧 Additional layer types (if Lab 6 uses new ops)
🔧 Multi-MAC parallelization
🔧 Dynamic batch processing
🔧 Mixed-precision quantization (e.g., 4-bit weights)
```

---

### 4.2 Calibration Pipeline for Lab 6

**Step 1: Generate Calibration Dataset**
```python
# Run Lab 6 model on representative data in FP32
# Collect activation statistics per layer
```

**Step 2: Compute Quantization Parameters**
```python
for layer in model.layers:
    stats[layer.name] = {
        "min": min(activations),
        "max": max(activations),
        "mean": mean(activations),
        "Si": 255.0 / (max - min),  # Scale factor
        "zi": round(-min * Si) - 128  # Zero point
    }
```

**Step 3: Export JSON**
```json
{
  "_input": {"min": -0.45, "max": 0.55, "Si": 230.49, "zi": -103},
  "conv2d": {"min": 0.0, "max": 2.5, "Si": 50.8, "zi": 0},
  ...
}
```

**Step 4: Update Layer Detection Logic**
```cpp
// In computeQuantizedInternal(), update dimension checks
if (P == X && Q == Y && M == Z) {
    current_layer_name = "lab6_conv_layer_1";
}
```

---

### 4.3 Hardware Integration Checklist for Lab 6

```
□ Update XSA file path in build scripts
□ Verify XPAR_AXI_FIFO_0_BASEADDR in xparameters.h
□ Test HardwareMac::run() with Lab 6 hardware
□ Profile CHUNK_SIZE for optimal throughput
□ Calibrate timeout values for new clock domain
□ Validate MAC results against software baseline
□ Measure speedup and energy savings
□ Update documentation with new layer architectures
```

---

### 4.4 Recommended File Structure for Lab 6

```
Lab6/
├── SW/
│   ├── common/              # Shared from Lab 3
│   │   ├── Convolutional_new.cpp  (reuse as-is)
│   │   ├── HardwareMac.cpp        (update XPAR constants)
│   │   ├── Utils.cpp              (reuse as-is)
│   │   └── Types.h                (reuse as-is)
│   ├── lab6_model/
│   │   ├── Lab6Network.cpp        (new - specific architecture)
│   │   └── calibration_stats.json (new - Lab 6 model stats)
│   └── ...
├── HW/
│   └── lab6_design.xsa            (new hardware)
└── LAB3_CODE_REUSE_GUIDE.md       (this document)
```

---

## 5. Key Takeaways

### What Was Fixed
1. **Hardware Reliability:** FIFO timing, error handling, empty-read protection
2. **Production Quality:** Removed debug noise, kept essential logging
3. **Robustness:** Separate TX/RX error paths, ISR diagnostics

### Why It Matters for Lab 6
- **Proven Quantization Framework:** Calibration system is production-ready
- **Reliable HW Interface:** FIFO protocol is battle-tested
- **Clean Architecture:** Easy to adapt to new models and hardware
- **Comprehensive Error Handling:** Accelerates debugging new designs

### Critical Success Factors
✅ **Do:** Reuse quantization math, calibration system, logging framework
✅ **Do:** Generate Lab 6-specific calibration stats
✅ **Do:** Update hardware constants for new FPGA design
⚠️ **Don't:** Modify core quantization formulas (they're correct)
⚠️ **Don't:** Reduce FIFO reset delay (100k cycles is necessary)
⚠️ **Don't:** Skip empty FIFO safety check (causes hard hangs)

---

## 6. Code Metrics

### Lines of Code by Component
```
Convolutional_new.cpp:     759 lines
├── Calibration system:    ~170 lines (22%)
├── Quantization logic:    ~180 lines (24%)
├── Convolution kernel:    ~95 lines  (13%)
└── Documentation:         ~300 lines (39%)

HardwareMac.cpp:           124 lines
├── FIFO protocol:         ~95 lines  (77%)
└── Error handling:        ~29 lines  (23%)
```

### Test Coverage Needed for Lab 6
```
□ Unit tests: HardwareMac with mock FIFO
□ Integration tests: Single layer quantization accuracy
□ System tests: End-to-end Lab 6 inference
□ Hardware tests: MAC accelerator validation
□ Regression tests: Compare vs. FP32 baseline
```

---

## 7. References & Resources

### Important Code Sections
- [Convolutional_new.cpp:320-704](SW/hw_quant_framework/src/layers/Convolutional_new.cpp#L320-L704) - Main quantization implementation
- [HardwareMac.cpp:18-121](SW/hw_quant_framework/src/HardwareMac.cpp#L18-L121) - FIFO communication protocol
- [Convolutional_new.cpp:86-170](SW/hw_quant_framework/src/layers/Convolutional_new.cpp#L86-L170) - Calibration loader
- [Convolutional_new.cpp:392-446](SW/hw_quant_framework/src/layers/Convolutional_new.cpp#L392-L446) - Adaptive calibration selection

### Xilinx Documentation
- AXI4-Stream FIFO LogiCORE IP Product Guide (PG080)
- Zynq-7000 TRM (UG585)
- Vivado Design Suite User Guide: Programming and Debugging (UG908)

### Quantization Theory
- Integer Quantization for Deep Learning Inference (Google)
- Quantization and Training of Neural Networks for Efficient Integer-Arithmetic-Only Inference (arXiv:1712.05877)

---

**Document Version:** 1.0
**Last Updated:** December 16, 2025
**Maintainer:** Course Team
**Status:** Ready for Lab 6 Planning
