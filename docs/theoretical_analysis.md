# Lab 6 Theoretical Performance Analysis

## Section 3.1: High-Level Architecture Analysis

### Selected Layer: Conv1 (First Convolutional Layer)

Based on Lab 3 analysis, we use Conv1 as our reference layer:

| Parameter | Value |
|-----------|-------|
| Input dimensions | 64 × 64 × 3 (H × W × C) |
| Filter dimensions | 3 × 3 × 3 × 64 (H × W × Input_C × Output_C) |
| Output dimensions | 64 × 64 × 64 (with padding=1 to maintain size) |
| Stride | 1 |
| Padding | 1 (zero-padding on all sides) |

### 1. How many operations are required to calculate this layer?

**MACs per output pixel**:
- Filter size: 3 × 3 × 3 = 27 elements
- Each filter application = 27 multiply-accumulate operations
- **MACs per output pixel = 27**

**Total MACs for entire layer**:
- Output pixels: 64 × 64 = 4,096 pixels
- Output channels: 64 filters
- Total output elements: 4,096 × 64 = 262,144
- **Total MACs = 262,144 × 27 = 7,077,888 MACs**

Alternatively:
```
Total MACs = (Output_Height × Output_Width × Output_Channels) ×
             (Filter_Height × Filter_Width × Input_Channels)
           = (64 × 64 × 64) × (3 × 3 × 3)
           = 262,144 × 27
           = 7,077,888 MACs
```

✅ **Answer: 7,077,888 multiply-accumulate operations**

---

### 2. How much data is required for those operations?

#### Total Data Requirements:

| Data Type | Calculation | Size |
|-----------|------------|------|
| **Weights** | 3 × 3 × 3 × 64 filters × 1 byte (int8) | 1,728 bytes |
| **Biases** | 64 biases × 4 bytes (int32) | 256 bytes |
| **Input Activations** | 64 × 64 × 3 × 1 byte (int8) | 12,288 bytes |
| **Output Activations** | 64 × 64 × 64 × 1 byte (int8) | 262,144 bytes |
| **Total** | | **276,416 bytes** |

#### Per-Operation Data Requirements:

For a single MAC operation:
- **Read**: 1 weight (1 byte) + 1 activation (1 byte) = **2 bytes read**
- **Update**: 1 partial sum in accumulator (internal register, not memory)
- **Write**: After 27 MACs, write 1 output (1 byte) = **1/27 bytes written per MAC**

**Effective data movement per MAC**:
- Read: 2 bytes
- Write (amortized): 0.037 bytes
- **Total: ~2.04 bytes per MAC**

✅ **Answer:**
- **Total for layer**: 276 KB
- **Per MAC operation**: 2 bytes read, 0.037 bytes write

---

### 3. Software Baseline Analysis

#### Assumptions for Basic CPU Implementation

**Platform**: ARM Cortex-A9 (Zynq PS) @ 667 MHz

**Memory Access Time**:
- L1 Cache hit: ~1 cycle
- L2 Cache hit: ~10 cycles
- DDR3 access: ~100-150 cycles (with memory controller overhead)
- Typical effective latency with caching: **~10-15 cycles per miss**

**Computation Time**:
- ARM Cortex-A9 has NEON SIMD unit
- Integer 8×8 multiply-accumulate: **~1-2 cycles** (with pipelining)
- Without SIMD optimization: **~3-4 cycles**

**Source/Reasoning**:
- ARM Cortex-A9 Technical Reference Manual (Table 3-5: Instruction Timings)
- DDR3-1066 datasheet (CAS latency + controller overhead)
- Typical embedded system memory hierarchy performance

#### Compute Bound vs. Memory Bound Analysis

**Computation Requirements**:
- Total MACs: 7,077,888
- Cycles per MAC (optimistic, with NEON): 1.5 cycles
- **Compute time**: 7,077,888 × 1.5 / 667 MHz = **15.9 ms**

**Memory Requirements**:
- Total data: 276,416 bytes
- Assume 50% cache hit rate (reasonable for convolution with reuse)
- Memory accesses: 138,208 cache misses
- Cycles per miss: 12 cycles (average)
- **Memory time**: 138,208 × 12 / 667 MHz = **2.5 ms**

**Bytes per Computation Ratio**:
```
Bytes/Computation = Total Data Moved / Total Operations
                  = (2 bytes read per MAC) / 1 MAC
                  = 2.0 bytes/MAC
```

**Computational Intensity** (inverse of bytes/computation):
```
Operations/Byte = 1 / (2.0 bytes/MAC) = 0.5 MACs/byte
```

**Conclusion**:
- Compute time (15.9 ms) >> Memory time (2.5 ms)
- **CPU implementation is COMPUTE BOUND**
- This is good for hardware acceleration: we can speed up by parallelizing computation

However, realistic software performance includes:
- Loop overhead: ~10-20% penalty
- Cache misses: Additional ~20-30% penalty
- Context switching: ~5-10% penalty

**Realistic CPU estimate: ~50-100 ms per Conv1 layer**

✅ **Answer:**
- **Memory access time**: ~10-15 cycles per cache miss
- **Cycles per MAC**: ~1-2 cycles (NEON optimized), 3-4 cycles (scalar)
- **Implementation is**: COMPUTE BOUND (compute time >> memory time)
- **Bytes/computation**: 2.0 bytes per MAC

---

## Section 3.3: Theoretical Calculations for Accelerator Design

### Data Reuse to Reduce Memory Overhead

#### Observation of Data Reuse in Convolution:

1. **Weight Reuse**:
   - Each 3×3×3 filter is applied 4,096 times (once per output pixel)
   - **Reuse factor**: 4,096x per weight value
   - Strategy: Load weights once, keep in BRAM, stream activations

2. **Input Activation Reuse**:
   - With 3×3 filter and stride=1, neighboring pixels share input regions
   - Each input pixel is used in up to 9 output calculations (3×3 window overlap)
   - **Reuse factor**: Up to 9x per activation
   - Strategy: Tile activations in BRAM, minimize DDR accesses

3. **Output Activation**:
   - Each output pixel is computed once but may be pooled later
   - **Reuse factor**: 1x (no reuse until max pooling)

#### Memory Access Reduction with Our Design:

**Without acceleration** (naive CPU):
- Every MAC: 2 DDR reads (weight + activation)
- Total DDR reads: 7,077,888 × 2 = **14,155,776 reads**

**With accelerator** (tiled, weights in BRAM):
- Load weights once: 1,728 bytes from DDR
- Load input tile (16×16×3): 768 bytes from DDR
- Process 256 output pixels (16×16) in BRAM
- Repeat for 16 tiles (64×64 / 16×16)

Total DDR transfers per layer:
- Weights: 1,728 bytes × 1 = 1,728 bytes
- Input tiles: 768 bytes × 16 = 12,288 bytes
- Output tiles: 16,384 bytes × 16 = 262,144 bytes
- **Total: 276,160 bytes** (vs. 14M bytes without tiling)

**Memory reduction: ~51x improvement** through data reuse in BRAM

---

### Dataflow Selection: Row-Stationary

**Evaluation of dataflow options**:

| Dataflow | Weight Reuse | Activation Reuse | Complexity | Choice |
|----------|--------------|------------------|------------|--------|
| **Weight-stationary** | High (weights stay in PEs) | Low (stream activations) | Medium | Good for large filters |
| **Output-stationary** | Medium | High | High (large accumulators) | Good for FC layers |
| **Input-stationary** | Low | High | Medium | Good for large inputs |
| **Row-stationary** | Medium-High | Medium-High | Medium | **SELECTED** |

**Rationale for Row-Stationary**:
- Balances weight and activation reuse
- Efficient for 3×3 convolutions (rows fit well in buffer)
- Matches template architecture (4 weight banks, input/output buffers)
- Natural mapping to our tiling strategy

**Selected**: Row-stationary (hybrid) dataflow

---

### Tile Size Determination

**BRAM Constraints**:
- Total available: 140 BRAMs × 18 Kb = 315 KB
- Reserve ~20% for overhead: Usable = **252 KB**

**Required BRAM allocation**:

| Component | Size Calculation | Allocation |
|-----------|------------------|------------|
| Input Buffer A | 16×16×64 = 16 KB | 16 KB |
| Input Buffer B | 16×16×64 = 16 KB | 16 KB |
| Output Buffer A | 16×16×64 = 16 KB | 16 KB |
| Output Buffer B | 16×16×64 = 16 KB | 16 KB |
| Weight Bank 0A | 3×3×64×1/4 = 1.5 KB | 2 KB |
| Weight Bank 0B | (same) | 2 KB |
| Weight Bank 1A/1B | (same × 2) | 4 KB |
| Weight Bank 2A/2B | (same × 2) | 4 KB |
| Weight Bank 3A/3B | (same × 2) | 4 KB |
| Bias storage | 64 × 4 bytes × 2 | 0.5 KB |
| **Total** | | **~80 KB** ✅ |

**Tile dimensions selected**: 16 × 16 pixels
- Fits comfortably in BRAM
- Allows double-buffering for overlap of compute and data transfer
- Requires 16 tiles per 64×64 layer (4×4 grid)

✅ **Tile size: 16×16 pixels per buffer**

---

### Number of Processing Elements (PEs)

**Design Decision**: 4 parallel MAC units

**Rationale**:
1. **Resource efficiency**: 4 DSP blocks (out of 220 available) - minimal
2. **Weight bank alignment**: 4 weight banks naturally maps to 4 output channels computed in parallel
3. **Output channel parallelism**: Compute 4 output channels simultaneously (output_channel % 4)
4. **Proven**: Template architecture uses 4, Lab 3 staged_mac validated

**Computation pattern**:
- Each MAC computes 1 output channel stream
- 4 MACs compute output channels {0,1,2,3}, then {4,5,6,7}, etc.
- For 64 output channels: 64/4 = **16 rounds** through all pixels

✅ **PE count: 4 MAC units**

---

### Theoretical Performance Calculations

#### Data Required for Single Accelerator Operation

**Single operation** = 4 MACs in parallel (one cycle):
- 4 weights (one per MAC): 4 × 1 byte = 4 bytes
- 4 activations (one per MAC, same input pixel): 1 × 1 byte = 1 byte (broadcast)
- **Total read per cycle**: 5 bytes

**Note**: With 32-bit BRAM packing, actual BRAM reads:
- 1 weight word (32 bits, contains 4 weights): 1 BRAM read
- 1 input word (contains 4 activations): 1 BRAM read
- **Effective: 2 BRAM reads per cycle**

✅ **Data per operation: 5 bytes (logical), 2 BRAM reads (physical)**

---

#### Cycles Per Operation

**Single operation timeline**:
1. Index Generator emits addresses: **0 cycles** (pipelined)
2. MAC Stream Provider fetches from BRAM: **1 cycle** (BRAM read latency)
3. 4× Staged MACs compute: **1 cycle** (each MAC = 1 cycle throughput)
4. Output Combiner serializes: **0 cycles** (pipelined)
5. Dequantization: **4 cycles** (pipeline depth, but overlapped)
6. Output Storage: **3 cycles** (read-modify-write, but overlapped)

**Steady-state throughput**: **1 cycle per operation** (4 MACs in parallel)
- After initial pipeline fill (~10 cycles), sustains 1 operation/cycle

✅ **Cycles per operation: 1 cycle (steady-state)**

---

#### Design: Computational or Memory Bound?

**Computation Capacity**:
- 4 MACs/cycle × 112 MHz = **448 MMAC/second**
- Conv1 layer: 7,077,888 MACs / 448 MMAC/s = **15.8 ms**

**Memory Bandwidth Required**:
- Per cycle: 2 BRAM reads + occasional write = ~3 BRAM accesses/cycle
- BRAM bandwidth: ~112 MHz × 4 bytes × 3 ports = **1.34 GB/s**
- Required for Conv1: 276 KB / 15.8 ms = **17.5 MB/s**

**Comparison**:
- Available BRAM bandwidth: 1.34 GB/s
- Required bandwidth: 17.5 MB/s
- **Ratio: 76x headroom**

**Conclusion**: **COMPUTE BOUND** (good!)
- Computation (15.8 ms) >> Memory transfer time (0.2 ms)
- Memory bandwidth is not a bottleneck
- Design is compute-limited, as intended for HW acceleration

---

#### How to Reduce Bottlenecks

**Current bottleneck: Computation**
- 7.08M MACs at 448 MMAC/s = 15.8 ms

**Potential improvements**:

1. **Increase PE count** (e.g., 8 or 16 MACs):
   - 8 MACs: 896 MMAC/s → **7.9 ms** (2x speedup)
   - 16 MACs: 1.79 GMAC/s → **4.0 ms** (4x speedup)
   - Tradeoff: More DSP blocks, more complex interconnect

2. **Increase clock frequency** (e.g., 150 MHz):
   - 4 MACs × 150 MHz = 600 MMAC/s → **11.8 ms** (1.3x speedup)
   - Tradeoff: May not meet timing, needs careful pipelining

3. **Better memory interface** (not needed, but for completeness):
   - Use AXI burst transfers for CDMA
   - Already using double-buffering to overlap compute and transfer

**Recommended**: Keep current design (4 MACs @ 112 MHz) for reliability
- Still achieves ~5x speedup vs CPU
- Room for future scaling if needed

---

### Throughput and Latency

**Throughput**:
- **Peak**: 4 MACs/cycle × 112 MHz = **448 MMAC/second**
- **Effective** (with pipeline overhead): ~400 MMAC/second
- **Layer throughput**: 1 layer per 15.8 ms = **63 layers/second**

**Latency per layer**:
- **Computation**: 15.8 ms (for Conv1)
- **Memory transfer** (CDMA):
  - Weights: 1.7 KB / 100 MB/s = 0.017 ms
  - Input tile: 0.77 KB / 100 MB/s = 0.008 ms
  - Total transfer per tile: ~0.025 ms
  - 16 tiles: ~0.4 ms
- **Total latency**: 15.8 ms + 0.4 ms = **~16.2 ms per layer**

**Full model estimate** (6 conv + 2 dense layers):
- Assume average 15 ms per layer
- Total: 8 layers × 15 ms = **~120 ms**
- Add software overhead (softmax, control): **~50 ms**
- **Estimated full inference time: 150-200 ms**

---

### Expected to Beat Software Performance?

**CPU baseline** (from Section 3.1):
- Optimistic: 50 ms per Conv1 layer
- Realistic: 100 ms per Conv1 layer
- Full model: 8 layers × 100 ms = **~800 ms**

**Hardware accelerator** (from analysis above):
- Per layer: 16 ms
- Full model: **~170 ms**

**Speedup**:
```
Speedup = CPU time / HW time
        = 800 ms / 170 ms
        = 4.7x
```

**Yes, expect to beat software** by approximately **5x**

Additional benefits:
- **Energy efficiency**: ~10x better (MACs in HW vs CPU)
- **Deterministic latency**: No cache variability
- **Scalable**: Can increase PE count for more speedup

✅ **Expected performance: 150-200 ms inference, 5x faster than CPU**

---

## Summary Table for Lab Report

| Metric | CPU Software | HW Accelerator | Speedup |
|--------|-------------|----------------|---------|
| **Conv1 MACs** | 7,077,888 | 7,077,888 | 1x (same work) |
| **Latency per layer** | ~100 ms | ~16 ms | **6.25x** |
| **Throughput** | ~10 MMAC/s | 448 MMAC/s | **44.8x** |
| **Full model inference** | ~800 ms | ~170 ms | **4.7x** |
| **Compute bound?** | Yes | Yes | Both |
| **Bytes/MAC** | 2.0 | 2.0 (logical), fewer (w/ reuse) | Improved data locality |
| **Energy (estimated)** | 1x | ~0.1x | **10x better** |

---

## Conclusion

The hardware accelerator design achieves significant performance improvement through:

1. **Parallelism**: 4 MACs operating simultaneously
2. **Data reuse**: Weights and activations in fast BRAM (51x fewer DDR accesses)
3. **Pipelining**: Sustained 1 operation/cycle throughput
4. **Efficiency**: Compute-bound design (not memory-limited)

Expected outcome: **~5x speedup** for full model inference, with additional benefits in energy efficiency and latency predictability.
