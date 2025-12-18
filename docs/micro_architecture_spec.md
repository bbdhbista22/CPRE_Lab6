# Lab 6 Micro-Architecture Specifications

## Module 1: Index Generator

### Purpose
Generates sequential BRAM addresses for input activations and weights to feed the MAC units during convolution operation.

### State Machine Diagram

```
┌─────────────┐
│    IDLE     │ ◄─────────────────────────────────┐
└──────┬──────┘                                   │
       │ conv_start=1                             │
       │ load config registers                    │
       ▼                                          │
┌─────────────┐                                   │
│  GENERATE   │                                   │
│   INDICES   │                                   │
└──────┬──────┘                                   │
       │                                          │
       │ Loop structure:                          │
       │  for out_y = 0 to out_height-1           │
       │    for out_x = 0 to out_width-1          │
       │      for fy = 0 to filter_h-1            │
       │        for fx = 0 to filter_w-1          │
       │          for ic = 0 to in_channels-1     │
       │            for oc = 0 to out_channels-1  │
       │              emit addresses              │
       │              TLAST when ic==last         │
       │                                          │
       │ all iterations complete                  │
       └──────────────────────────────────────────┘
```

### Block Diagram

```
┌────────────────────────────────────────────────────────────────────┐
│                      INDEX GENERATOR                               │
│                                                                    │
│  AXI-Lite Slave                                                    │
│  (Configuration)                                                   │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ Config Registers (from CPU):                                 │  │
│  │  - input_height, input_width, input_channels                 │  │
│  │  - filter_height, filter_width                               │  │
│  │  - output_channels (num_filters)                             │  │
│  │  - stride, padding                                           │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ Counter Logic                                                │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐              │  │
│  │  │   out_y    │  │   out_x    │  │     fy     │              │  │
│  │  │  counter   │  │  counter   │  │  counter   │              │  │
│  │  └────────────┘  └────────────┘  └────────────┘              │  │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────┐              │  │
│  │  │     fx     │  │     ic     │  │     oc     │              │  │
│  │  │  counter   │  │  counter   │  │  counter   │              │  │
│  │  └────────────┘  └────────────┘  └────────────┘              │  │
│  │                                                              │  │
│  │  Increment logic:                                            │  │
│  │  - oc increments every cycle when generating                 │  │
│  │  - ic increments when oc wraps (oc == out_channels-1)        │  │
│  │  - fx increments when ic wraps                               │  │
│  │  - fy increments when fx wraps                               │  │
│  │  - out_x increments when fy wraps                            │  │
│  │  - out_y increments when out_x wraps                         │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ Address Calculation Logic                                    │  │
│  │                                                              │  │
│  │  Input Address:                                              │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │ in_y = out_y × stride + fy - padding                   │  │  │
│  │  │ in_x = out_x × stride + fx - padding                   │  │  │
│  │  │                                                        │  │  │
│  │  │ input_addr = (in_y × input_width + in_x) × in_channels │  │  │
│  │  │              + ic                                      │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  │                                                              │  │
│  │  Weight Address:                                             │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │ weight_addr = ((oc × filter_h × filter_w × in_channels)│  │  │
│  │  │                + (fy × filter_w × in_channels)         │  │  │
│  │  │                + (fx × in_channels)                    │  │  │
│  │  │                + ic) / 4  (for 32-bit BRAM packing)    │  │  │
│  │  │                                                        │  │  │
│  │  │ weight_byte_sel = weight_addr[1:0]                     │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  │                                                              │  │
│  │  Boundary Checking:                                          │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │ if (in_y < 0 || in_y >= input_height ||                │  │  │
│  │  │     in_x < 0 || in_x >= input_width)                   │  │  │
│  │  │   input_addr = ZERO_ADDR (padding)                     │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ Control Signal Generation                                    │  │
│  │                                                              │  │
│  │  TLAST Logic:                                                │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │ TLAST = (ic == in_channels-1) &&                       │  │  │
│  │  │         (fx == filter_w-1) &&                          │  │  │
│  │  │         (fy == filter_h-1) &&                          │  │  │
│  │  │         (oc == 3)  // every 4th output channel         │  │  │
│  │  │                                                        │  │  │
│  │  │ // TLAST marks end of MAC accumulation for one pixel   │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  │                                                              │  │
│  │  TVALID = (state == GENERATE) && !output_stall               │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ AXI-Stream Master Output                                     │  │
│  │                                                              │  │
│  │  TDATA[31:0] = {input_addr[15:0], weight_addr[15:0]}         │  │
│  │  TVALID                                                      │  │
│  │  TLAST                                                       │  │
│  │  TREADY ◄── (from downstream)                                │  │
│  └──────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────┘
```

### Interface Signals

| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| **AXI-Lite Slave (Config)** | | | |
| s_axi_aclk | Input | 1 | Clock |
| s_axi_aresetn | Input | 1 | Active-low reset |
| s_axi_awaddr | Input | 32 | Write address |
| s_axi_awvalid | Input | 1 | Write address valid |
| s_axi_awready | Output | 1 | Write address ready |
| s_axi_wdata | Input | 32 | Write data |
| s_axi_wvalid | Input | 1 | Write data valid |
| s_axi_wready | Output | 1 | Write data ready |
| s_axi_araddr | Input | 32 | Read address |
| s_axi_arvalid | Input | 1 | Read address valid |
| s_axi_arready | Output | 1 | Read address ready |
| s_axi_rdata | Output | 32 | Read data |
| s_axi_rvalid | Output | 1 | Read data valid |
| s_axi_rready | Input | 1 | Read data ready |
| **AXI-Stream Master (Output)** | | | |
| m_axis_aclk | Input | 1 | Clock (same as AXI-Lite) |
| m_axis_aresetn | Input | 1 | Active-low reset |
| m_axis_tdata | Output | 32 | {input_addr, weight_addr} |
| m_axis_tvalid | Output | 1 | Data valid |
| m_axis_tready | Input | 1 | Ready for data |
| m_axis_tlast | Output | 1 | End of MAC accumulation |
| **Control** | | | |
| conv_start | Input | 1 | Start index generation |
| conv_idle | Output | 1 | Ready for new layer |
| conv_done | Output | 1 | All indices generated |

---

## Module 2: Dequantization

### Purpose
Converts 32-bit MAC accumulator outputs to 8-bit quantized values for the next layer, applying scale factors, ReLU activation, and saturation.

### Datapath Diagram

```
┌────────────────────────────────────────────────────────────────────┐
│                      DEQUANTIZATION MODULE                         │
│                                                                    │
│  AXI-Lite Slave (Scale Factors)                                    │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ Configuration Registers:                                     │  │
│  │  - scale_factor_fixed [31:0] (Q8.24 format)                  │  │
│  │  - zero_point_out [7:0]                                      │  │
│  │  - enable_relu [0]                                           │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ AXI-Stream Slave Input                                       │  │
│  │  TDATA[31:0] ───┐                                            │  │
│  │  TID[1:0]   ────┤ (int32 accumulator + MAC ID)               │  │
│  │  TVALID     ────┤                                            │  │
│  │  TLAST      ────┤                                            │  │
│  │  TREADY ◄───────┘                                            │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ STAGE 1: Fixed-Point Multiply                                │  │
│  │                                                              │  │
│  │  ┌─────────────────────────────────────────────┐             │  │
│  │  │  32-bit × 32-bit Signed Multiplier (DSP48)  │             │  │
│  │  │                                             │             │  │
│  │  │  Input A: mac_accumulator [31:0]            │             │  │
│  │  │  Input B: scale_factor_fixed [31:0]         │             │  │
│  │  │                                             │             │  │
│  │  │  Output: product [63:0] (signed)            │             │  │
│  │  └─────────────────────────────────────────────┘             │  │
│  │                                                              │  │
│  │  Pipeline Register: product_reg [63:0]                       │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ STAGE 2: Round and Shift                                     │  │
│  │                                                              │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │  // Q8.24 format: shift right by 24 bits               │  │  │
│  │  │  // Add 0.5 for rounding before shift                  │  │  │
│  │  │                                                        │  │  │
│  │  │  rounded = product_reg + (1 << 23);  // +0.5 in Q24    │  │  │
│  │  │  scaled = rounded[47:24];  // shift right 24, keep 24  │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  │                                                              │  │
│  │  Pipeline Register: scaled_reg [23:0]                        │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ STAGE 3: ReLU Activation                                     │  │
│  │                                                              │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │  if (enable_relu && scaled_reg[23] == 1)  // negative  │  │  │
│  │  │    relu_out = 0;                                       │  │  │
│  │  │  else                                                  │  │  │
│  │  │    relu_out = scaled_reg[23:0];                        │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  │                                                              │  │
│  │  Pipeline Register: relu_reg [23:0]                          │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ STAGE 4: Add Zero Point & Saturate                           │  │
│  │                                                              │  │
│  │  ┌──────────────────────────────────────────────────────────┐│  │
│  │  │  // Add output layer's zero point                        ││  │
│  │  │  adjusted = relu_reg[15:0] + sign_extend(zero_point_out);││  │
│  │  │                                                          ││  │
│  │  │  // Saturate to int8 range [-128, 127]                   ││  │
│  │  │  if (adjusted > 127)                                     ││  │
│  │  │    output = 127;                                         ││  │
│  │  │  else if (adjusted < -128)                               ││  │
│  │  │    output = -128;                                        ││  │
│  │  │  else                                                    ││  │
│  │  │    output = adjusted[7:0];                               ││  │
│  │  └──────────────────────────────────────────────────────────┘│  │
│  │                                                              │  │
│  │  Pipeline Register: output_reg [7:0]                         │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ AXI-Stream Master Output                                     │  │
│  │                                                              │  │
│  │  TDATA[7:0] = output_reg                                     │  │
│  │  TID[1:0] = tid_delayed (propagate through pipeline)         │  │
│  │  TVALID = valid_delayed                                      │  │
│  │  TLAST = tlast_delayed                                       │  │
│  │  TREADY ◄── (from downstream)                                │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                                                    │
│  Pipeline Depth: 4 stages                                          │
│  Latency: 4 clock cycles                                           │
│  Throughput: 1 output per cycle (when not backpressured)           │
└────────────────────────────────────────────────────────────────────┘
```

### Interface Signals

| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| **AXI-Lite Slave (Config)** | | | |
| (Same as Index Generator) | | | |
| **AXI-Stream Slave (Input)** | | | |
| s_axis_tdata | Input | 32 | MAC accumulator (int32) |
| s_axis_tid | Input | 2 | MAC unit ID (0-3) |
| s_axis_tvalid | Input | 1 | Data valid |
| s_axis_tready | Output | 1 | Ready for data |
| s_axis_tlast | Input | 1 | End of packet |
| **AXI-Stream Master (Output)** | | | |
| m_axis_tdata | Output | 8 | Quantized output (int8) |
| m_axis_tid | Output | 2 | Propagated MAC ID |
| m_axis_tvalid | Output | 1 | Data valid |
| m_axis_tready | Input | 1 | Ready for data |
| m_axis_tlast | Output | 1 | End of packet |

---

## Module 3: Output Storage

### Purpose
Writes 8-bit quantized outputs to 32-bit BRAM, handling byte-level packing and optional 2×2 max pooling.

### State Machine Diagram

```
        ┌─────────────┐
        │    IDLE     │ ◄─────────────────────┐
        └──────┬──────┘                       │
               │ TVALID=1                     │
               │ capture TID, TDATA           │
               ▼                              │
        ┌─────────────┐                       │
        │  READ_BRAM  │                       │
        │             │                       │
        │ - calc addr │                       │
        │ - set EN=1  │                       │
        │ - set WE=0  │                       │
        └──────┬──────┘                       │
               │                              │
               │ wait 1 cycle (BRAM latency)  │
               ▼                              │
        ┌─────────────┐                       │
        │  WAIT_DATA  │                       │
        │             │                       │
        │ - BRAM dout │                       │
        │   now valid │                       │
        └──────┬──────┘                       │
               │                              │
               ▼                              │
        ┌─────────────┐                       │
        │   MODIFY    │                       │
        │             │                       │
        │ - Insert new│                       │
        │   byte into │                       │
        │   32-bit word                       │
        │             │                       │
        │ If pooling: │                       │
        │ - compare   │                       │
        │ - take max  │                       │
        └──────┬──────┘                       │
               │                              │
               ▼                              │
        ┌─────────────┐                       │
        │ WRITE_BRAM  │                       │
        │             │                       │
        │ - set WE=1  │                       │
        │ - write data│                       │
        └──────┬──────┘                       │
               │                              │
               │ write complete               │
               └──────────────────────────────┘
```

### Block Diagram

```
┌────────────────────────────────────────────────────────────────────┐
│                      OUTPUT STORAGE MODULE                         │
│                                                                    │
│  AXI-Stream Slave Input                                            │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │  TDATA[7:0]  ──┐ (8-bit quantized value)                     │  │
│  │  TID[1:0]    ──┤ (which MAC produced this)                   │  │
│  │  TVALID      ──┤                                             │  │
│  │  TLAST       ──┤                                             │  │
│  │  TREADY ◄──────┘                                             │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ Address Calculation                                          │  │
│  │                                                              │  │
│  │  Inputs from config regs:                                    │  │
│  │  - output_height, output_width, output_channels              │  │
│  │  - enable_pooling                                            │  │
│  │                                                              │  │
│  │  Internal counters:                                          │  │
│  │  - pixel_count (increments on each TLAST)                    │  │
│  │  - pool_y, pool_x (for 2×2 pooling position)                 │  │
│  │                                                              │  │
│  │  Calculate output position:                                  │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │  if (enable_pooling)                                   │  │  │
│  │  │    // 2×2 pooling: 4 inputs → 1 output                 │  │  │
│  │  │    out_y = pixel_count / (output_width/2);             │  │  │
│  │  │    out_x = pixel_count % (output_width/2);             │  │  │
│  │  │  else                                                  │  │  │
│  │  │    out_y = pixel_count / output_width;                 │  │  │
│  │  │    out_x = pixel_count % output_width;                 │  │  │
│  │  │                                                        │  │  │
│  │  │  out_c = TID × (output_channels/4) + channel_offset;   │  │  │
│  │  │                                                        │  │  │
│  │  │  // BRAM address (32-bit words)                        │  │  │
│  │  │  bram_addr = ((out_y × output_width + out_x)           │  │  │
│  │  │               × output_channels + out_c) / 4;          │  │  │
│  │  │                                                        │  │  │
│  │  │  // Which byte in 32-bit word                          │  │  │
│  │  │  byte_sel = bram_addr[1:0];                            │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ 2×2 Max Pooling Logic (Optional)                             │  │
│  │                                                              │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │  Pool Buffer: 4 registers per output channel           │  │  │
│  │  │  [pool_00] [pool_01]                                   │  │  │
│  │  │  [pool_10] [pool_11]                                   │  │  │
│  │  │                                                        │  │  │
│  │  │  On each input:                                        │  │  │
│  │  │    pool[pool_y][pool_x] = max(current, new_value)      │  │  │
│  │  │                                                        │  │  │
│  │  │  When pool_y==1 && pool_x==1 (4th value):              │  │  │
│  │  │    output = max(pool_00, pool_01, pool_10, pool_11)    │  │  │
│  │  │    reset pool buffers                                  │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ Read-Modify-Write State Machine                              │  │
│  │                                                              │  │
│  │  READ:                                                       │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │  bram_en = 1                                           │  │  │
│  │  │  bram_we = 0 (read)                                    │  │  │
│  │  │  bram_addr = calculated address                        │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  │                                                              │  │
│  │  WAIT: (1 cycle for BRAM read latency)                       │  │
│  │                                                              │  │
│  │  MODIFY:                                                     │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │  old_word = bram_dout[31:0]                            │  │  │
│  │  │                                                        │  │  │
│  │  │  case (byte_sel)                                       │  │  │
│  │  │    0: new_word = {old_word[31:8],  new_byte}           │  │  │
│  │  │    1: new_word = {old_word[31:16], new_byte, old[7:0]} │  │  │
│  │  │    2: new_word = {old_word[31:24], new_byte, old[15:0]}│  │  │
│  │  │    3: new_word = {new_byte, old_word[23:0]}            │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  │                                                              │  │
│  │  WRITE:                                                      │  │
│  │  ┌────────────────────────────────────────────────────────┐  │  │
│  │  │  bram_en = 1                                           │  │  │
│  │  │  bram_we = 1 (write)                                   │  │  │
│  │  │  bram_din = new_word[31:0]                             │  │  │
│  │  │  bram_addr = same as read                              │  │  │
│  │  └────────────────────────────────────────────────────────┘  │  │
│  └────────┬─────────────────────────────────────────────────────┘  │
│           │                                                        │
│           ▼                                                        │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │ BRAM Interface (Output Buffer)                               │  │
│  │                                                              │  │
│  │  bram_addr ───►┐                                             │  │
│  │  bram_din  ───►┤                                             │  │
│  │  bram_we   ───►┤  32-bit BRAM                                │  │
│  │  bram_en   ───►┤  Port B                                     │  │
│  │  bram_dout ◄───┘  (Write-only from this module)              │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                                                    │
│  Cycles per output: 3-4 cycles (READ + WAIT + MODIFY + WRITE)      │
│  With pooling: 3-4 cycles × 4 inputs, then 1 output write          │
└────────────────────────────────────────────────────────────────────┘
```

### Interface Signals

| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| **AXI-Stream Slave (Input)** | | | |
| s_axis_tdata | Input | 8 | Quantized value (int8) |
| s_axis_tid | Input | 2 | MAC unit ID |
| s_axis_tvalid | Input | 1 | Data valid |
| s_axis_tready | Output | 1 | Ready (may stall during RMW) |
| s_axis_tlast | Input | 1 | End of pixel accumulation |
| **BRAM Port (Output Buffer)** | | | |
| bram_addr | Output | 16 | BRAM address |
| bram_din | Output | 32 | Write data (32-bit word) |
| bram_dout | Input | 32 | Read data (32-bit word) |
| bram_en | Output | 1 | Enable |
| bram_we | Output | 4 | Write enable (byte-level) |
| **Control** | | | |
| enable_pooling | Input | 1 | Enable 2×2 max pooling |
| output_complete | Output | 1 | All outputs written |

---

## Summary

These three modules complete the accelerator datapath:

1. **Index Generator**: Orchestrates the convolution by generating addresses
2. **Dequantization**: Converts MAC outputs back to quantized int8 format
3. **Output Storage**: Handles byte-packing and optional max pooling

Together with the existing components from Lab 3 (staged MAC) and the template (MAC Stream Provider, Output Combiner), they form a complete hardware accelerator for convolutional layers.

**Next Step**: Create control signals table with all signals documented.
