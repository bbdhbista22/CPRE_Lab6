# Lab 6 System Architecture Specification

## High-Level System Architecture Diagram

**For draw.io implementation**: Create this as a block diagram with the following components and connections.

### Component Hierarchy

```
┌─────────────────────────────────────────────────────────────────────┐
│                      ARM CORTEX-A9 (PS)                             │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                    DDR3 Memory                               │   │
│  │    - Model weights (770KB int8)                              │   │
│  │    - Input images                                            │   │
│  │    - Intermediate activations (staging area)                 │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                              │                                      │
│                              │ AXI HP0 (High Performance Port)      │
└──────────────────────────────┼──────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     PROGRAMMABLE LOGIC (PL)                         │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │           CENTRAL DMA (CDMA) Controller                      │   │
│  │    - DDR ↔ BRAM data transfers                               │   │
│  │    - Controlled by PS via AXI-Lite                           │   │
│  └────────────────────┬─────────────────────────────────────────┘   │
│                       │                                             │
│                       │ AXI (32-bit data)                           │
│                       ▼                                             │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                    BRAM BANKS (315 KB total)                 │   │
│  │                                                              │   │
│  │  ┌─────────────────────────────────────────────────────┐     │   │
│  │  │  INPUT ACTIVATION BUFFERS (Double-buffered)         │     │   │
│  │  │  - Buffer A: 64×64×64 = 262 KB max                  │     │   │
│  │  │  - Buffer B: 64×64×64 = 262 KB max                  │     │   │
│  │  │  - swap_input_buffer signal selects active buffer   │     │   │
│  │  └─────────────────────────────────────────────────────┘     │   │
│  │                                                              │   │
│  │  ┌─────────────────────────────────────────────────────┐     │   │
│  │  │  OUTPUT ACTIVATION BUFFERS (Double-buffered)        │     │   │
│  │  │  - Buffer A: 64×64×128 = 524 KB max                 │     │   │
│  │  │  - Buffer B: 64×64×128 = 524 KB max                 │     │   │
│  │  │  - swap_output_buffer signal selects active buffer  │     │   │
│  │  └─────────────────────────────────────────────────────┘     │   │
│  │                                                              │   │
│  │  ┌─────────────────────────────────────────────────────┐     │   │
│  │  │  WEIGHT BANKS (4 banks × 2 buffers each)            │     │   │
│  │  │  - Bank 0A/0B: Weights for output channel 0 mod 4   │     │   │
│  │  │  - Bank 1A/1B: Weights for output channel 1 mod 4   │     │   │
│  │  │  - Bank 2A/2B: Weights for output channel 2 mod 4   │     │   │
│  │  │  - Bank 3A/3B: Weights for output channel 3 mod 4   │     │   │
│  │  │  - Each bank: ~2-4 KB per layer                     │     │   │
│  │  │  - Double-buffering allows CDMA overlap with compute│     │   │
│  │  └─────────────────────────────────────────────────────┘     │   │
│  └────────────────────┬─────────────────────────────────────────┘   │
│                       │                                             │
│                       │ BRAM Read/Write Interfaces (32-bit)         │
│                       ▼                                             │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │              CONVOLUTION ACCELERATOR CORE                    │   │
│  │                                                              │   │
│  │  ┌───────────────────────────────────────────────────────┐   │   │
│  │  │  INDEX GENERATOR (NEW - TO IMPLEMENT)                 │   │   │
│  │  │  - Inputs: Layer config (via AXI-Lite registers)      │   │   │
│  │  │  - Outputs: input_addr[15:0], weight_addr[15:0]       │   │   │
│  │  │  - AXI-Stream master interface                        │   │   │
│  │  │  - Generates addresses for (out_y, out_x, fy, fx, ic) │   │   │
│  │  │  - Asserts TLAST every 27 MACs (for 3×3×3 filter)     │   │   │
│  │  └────────────────────┬──────────────────────────────────┘   │   │
│  │                       │ AXI-Stream (32-bit addresses)        │   │
│  │                       ▼                                      │   │
│  │  ┌─────────────────────────────────────────────────────┐     │   │
│  │  │  MAC STREAM PROVIDER (FROM TEMPLATE)                │     │   │
│  │  │  - Fetches data from BRAM using addresses           │     │   │
│  │  │  - Outputs: weight[7:0], activation[7:0] × 4        │     │   │
│  │  │  - Handles BRAM read latency (1 cycle)              │     │   │
│  │  │  - Manages backpressure via TREADY                  │     │   │
│  │  └────────────────────┬────────────────────────────────┘     │   │
│  │                       │ AXI-Stream (4× 16-bit data pairs)    │   │
│  │                       ▼                                      │   │
│  │  ┌─────────────────────────────────────────────────────┐     │   │
│  │  │  4× STAGED MAC UNITS (FROM LAB 3)                   │     │   │
│  │  │  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────┐│     │   │
│  │  │  │ Staged MAC │ │ Staged MAC │ │ Staged MAC │ │ ...││     │   │
│  │  │  │    #0      │ │    #1      │ │    #2      │ │ #3 ││     │   │
│  │  │  └────────────┘ └────────────┘ └────────────┘ └────┘│     │   │
│  │  │  - Each MAC: 8×8→16, accumulate to 32-bit           │     │   │
│  │  │  - Frequency: 112 MHz (proven from Lab 3)           │     │   │
│  │  │  - Throughput: 1 MAC/cycle per unit                 │     │   │
│  │  │  - Bias addition: via TUSER signal or config reg    │     │   │
│  │  └────────────────────┬────────────────────────────────┘     │   │
│  │                       │ 4× AXI-Stream (32-bit accumulators)  │   │
│  │                       ▼                                      │   │
│  │  ┌─────────────────────────────────────────────────────┐     │   │
│  │  │  OUTPUT COMBINER (FROM TEMPLATE)                    │     │   │
│  │  │  - Round-robin arbitration of 4 MAC outputs         │     │   │
│  │  │  - Uses TID[1:0] to tag which MAC produced output   │     │   │
│  │  │  - Serializes 4 parallel streams to 1               │     │   │
│  │  │  - Maintains TLAST signaling                        │     │   │
│  │  └────────────────────┬────────────────────────────────┘     │   │
│  │                       │ AXI-Stream (32-bit + TID[1:0])       │   │
│  │                       ▼                                      │   │
│  │  ┌─────────────────────────────────────────────────────┐     │   │
│  │  │  DEQUANTIZATION MODULE (NEW - TO IMPLEMENT)         │     │   │
│  │  │  - Input: int32 MAC accumulator                     │     │   │
│  │  │  - Fixed-point multiply by scale factor (Q8.24)     │     │   │
│  │  │  - Round (add 0.5)                                  │     │   │
│  │  │  - ReLU activation                                  │     │   │
│  │  │  - Add zero point for next layer                    │     │   │
│  │  │  - Saturate to [-128, 127]                          │     │   │
│  │  │  - Output: int8 quantized value                     │     │   │
│  │  └────────────────────┬────────────────────────────────┘     │   │
│  │                       │ AXI-Stream (8-bit + TID[1:0])        │   │
│  │                       ▼                                      │   │
│  │  ┌─────────────────────────────────────────────────────┐     │   │
│  │  │  OUTPUT STORAGE (NEW - TO IMPLEMENT)                │     │   │
│  │  │  - Read-Modify-Write to 32-bit BRAM                 │     │   │
│  │  │  - Extract byte based on address LSBs               │     │   │
│  │  │  - 2×2 Max Pooling (optional per config)            │     │   │
│  │  │  - Uses TID to determine output channel             │     │   │
│  │  │  - Writes to output BRAM buffer                     │     │   │
│  │  └─────────────────────────────────────────────────────┘     │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │           AXI-LITE CONFIGURATION REGISTERS                   │   │
│  │    - Base address: 0x4000_0000 (from Vivado Address Editor)  │   │
│  │                                                              │   │
│  │    Register Map (32-bit registers):                          │   │
│  │    0x00: CONTROL - [0]=start, [1]=reset, [2]=swap_in_buf,    │   │
│  │                    [3]=swap_out_buf, [31]=done               │   │
│  │    0x04: INPUT_HEIGHT                                        │   │
│  │    0x08: INPUT_WIDTH                                         │   │
│  │    0x0C: INPUT_CHANNELS                                      │   │
│  │    0x10: FILTER_HEIGHT                                       │   │
│  │    0x14: FILTER_WIDTH                                        │   │
│  │    0x18: NUM_FILTERS (output channels)                       │   │
│  │    0x1C: STRIDE                                              │   │
│  │    0x20: PADDING                                             │   │
│  │    0x24: SCALE_FACTOR_FIXED (Q8.24 format)                   │   │
│  │    0x28: ZERO_POINT_IN                                       │   │
│  │    0x2C: ZERO_POINT_OUT                                      │   │
│  │    0x30: ENABLE_POOLING (0=no pool, 1=2×2 max pool)          │   │
│  │    0x34: BIAS_BASE_ADDR (pointer to bias values in BRAM)     │   │
│  └──────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘

                               ▲
                               │ AXI-Lite (control/status)
                               │
┌─────────────────────────────────────────────────────────────────────┐
│                      ARM CORTEX-A9 (PS)                             │
│                    Software Control                                 │
└─────────────────────────────────────────────────────────────────────┘
```

## Data Flow Summary

1. **Setup Phase** (CPU → PL):
   - CPU writes layer configuration to AXI-Lite registers
   - CPU triggers CDMA to transfer weights from DDR → Weight BRAM banks
   - CPU triggers CDMA to transfer input activations from DDR → Input BRAM buffer A

2. **Compute Phase** (PL autonomous):
   - CPU writes CONTROL[0]=1 (start)
   - Index Generator produces (input_addr, weight_addr) pairs
   - MAC Stream Provider fetches data from BRAM
   - 4× MAC units compute in parallel (4 output channels at once)
   - Output Combiner serializes MAC results
   - Dequantization converts int32 → int8
   - Output Storage writes to Output BRAM buffer A (with optional pooling)
   - Accelerator sets CONTROL[31]=1 (done)

3. **Buffer Swap Phase** (CPU):
   - CPU writes swap_in_buf=1, swap_out_buf=1
   - Output buffer A becomes Input buffer B for next layer
   - Next layer can start immediately (no DDR transfer needed)

4. **Weight Update Phase** (Overlapped with compute if double-buffered):
   - While accelerator computes with Weight Banks *A
   - CDMA loads next layer's weights into Weight Banks *B
   - After compute done, swap weight bank select

## Memory Sizing

Based on Conv1 analysis and scaling to larger layers:

| Component | Size per Buffer | Number of Buffers | Total |
|-----------|----------------|-------------------|-------|
| Input Activations | 64×64×64 = 262 KB | 2 | 524 KB |
| Output Activations | 64×64×128 = 524 KB | 2 | 1048 KB |
| Weights | ~4 KB max/bank | 4×2 = 8 | 32 KB |
| Biases | ~256 B max | 8 layers | 2 KB |
| **Total BRAM needed** | | | **~1606 KB** |

**Available on Zynq-7020**: 140 BRAMs × 18 Kb = 315 KB

⚠️ **ISSUE**: Full layer sizes don't fit! Must use **tiling**.

### Tiling Strategy

Tile activations to fit in BRAM:
- Input tile: 16×16×64 = 16 KB (fits easily)
- Output tile: 16×16×64 = 16 KB
- Process image in 4×4 = 16 tiles per layer
- Keep full weight set in BRAM (1.7 KB for Conv1)

**Revised BRAM allocation**:
- Input Buffer A: 16 KB
- Input Buffer B: 16 KB
- Output Buffer A: 16 KB
- Output Buffer B: 16 KB
- Weight Banks 0-3 (×2): 8 × 2 KB = 16 KB
- **Total: 80 KB** ✅ Fits in 315 KB available

## Bit Widths

| Signal | Width | Notes |
|--------|-------|-------|
| Activation (int8) | 8 bits | Signed, range [-128, 127] |
| Weight (int8) | 8 bits | Signed, range [-128, 127] |
| MAC product | 16 bits | 8×8 signed multiplication |
| Accumulator | 32 bits | Sum of up to 27 MACs for Conv1 |
| Dequant output | 8 bits | Saturated back to int8 |
| BRAM data | 32 bits | Packs 4× int8 values |
| BRAM address | 16 bits | Up to 64K addressable locations |
| TID (MAC ID) | 2 bits | 4 MACs → 2-bit ID |

## Control Signals

See separate control_signals.xlsx for complete table.

Key control signals:
- `conv_start`: CPU → Accelerator (start computation)
- `conv_idle`: Accelerator → CPU (ready for new layer)
- `conv_done`: Accelerator → CPU (computation complete)
- `swap_input_buffer`: CPU → BRAM mux (select A or B)
- `swap_output_buffer`: CPU → BRAM mux (select A or B)
- `weight_bank_select[3:0]`: Select A or B for each weight bank

## Timing Characteristics

**Clock frequency**: 112 MHz (based on Lab 3 staged_mac timing)

**Conv1 layer latency** (64×64 image, 3×3×3 filter, 64 outputs):
- Total MACs: 7,077,888
- MACs per cycle: 4 (parallel)
- Cycles: 7,077,888 / 4 = 1,769,472 cycles
- Time: 1,769,472 / 112 MHz = **15.8 ms** (compute only)
- Add memory transfer: ~5 ms (weights + input tile loading)
- **Total per layer: ~20-25 ms**

**Full model** (6 conv + 2 dense):
- Estimated: 8 layers × 20 ms average = **160 ms**
- With software overhead: **< 300 ms target** ✅

## Next Steps

1. Transfer this specification to draw.io for visual diagram
2. Create micro-architecture diagrams for 3 new modules
3. Create detailed control signal table (Excel)
4. Begin C++ reference implementation
