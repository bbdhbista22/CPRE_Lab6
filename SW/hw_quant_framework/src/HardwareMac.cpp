#include "HardwareMac.h"
#include <cstdio>
#include <algorithm>

#ifdef ZEDBOARD
#include "xil_io.h"
#include "xllfifo_hw.h"
#include "xparameters.h"
#endif

namespace ML {
namespace {
#if defined(ZEDBOARD)
constexpr uint32_t kFifoBaseAddr = XPAR_AXI_FIFO_0_BASEADDR;
#endif
}  // namespace

int32_t HardwareMac::run(const uint16_t* packed_pairs, std::size_t pair_count) {
#if defined(ZEDBOARD)
    // 1. Reset FIFO to ensure clean state
    Xil_Out32(kFifoBaseAddr + XLLF_LLR_OFFSET, 0xA5);
    
    // 2. Clear Interrupt Status Register
    Xil_Out32(kFifoBaseAddr + XLLF_ISR_OFFSET, 0xFFFFFFFF);

    // 3. Delay to ensure FIFO reset completion
    for(volatile int i=0; i<100000; i++);

    int32_t total_accumulator = 0;
    const std::size_t CHUNK_SIZE = 16; 

    // Define TDR Offset if not available
    #ifndef XLLF_TDR_OFFSET
    #define XLLF_TDR_OFFSET 0x0000002C
    #endif

    for (std::size_t offset = 0; offset < pair_count; offset += CHUNK_SIZE) {
        std::size_t chunk_len = std::min(CHUNK_SIZE, pair_count - offset);
        uint32_t chunk_bytes = static_cast<uint32_t>(chunk_len * 4);

        // Clear ISR before transaction to detect new completion
        Xil_Out32(kFifoBaseAddr + XLLF_ISR_OFFSET, 0xFFFFFFFF);

        // A. Write TDR (Transmit Destination)
        Xil_Out32(kFifoBaseAddr + XLLF_TDR_OFFSET, 0x0);

        // 4. Send Data to FIFO
        // Ensure FIFO has space before writing!
        int space_timeout = 1000000;
        while (Xil_In32(kFifoBaseAddr + XLLF_TDFV_OFFSET) < chunk_len) { 
            if (--space_timeout <= 0) {
                 xil_printf("Error: Hardware MAC FIFO Full (Vacancy Wait Timeout) at chunk %d\r\n", offset/CHUNK_SIZE);
                 return total_accumulator;
            }
        }

        // A. Write Data (Input + Weight pairs)
        for (std::size_t i = 0; i < chunk_len; ++i) {
            Xil_Out32(kFifoBaseAddr + XLLF_TDFD_OFFSET, static_cast<uint32_t>(packed_pairs[offset + i]));
        }

        // B. Write TLF (Transmit Length) triggers transfer
        Xil_Out32(kFifoBaseAddr + XLLF_TLF_OFFSET, chunk_bytes);

        // WAIT FOR TRANSMIT COMPLETE (Bit 26) OR RECEIVE COMPLETE (Bit 27)
        // If we received data (RC), it implies TX finished and IP processed it.
        int tx_timeout = 1000000; 
        while ((Xil_In32(kFifoBaseAddr + XLLF_ISR_OFFSET) & 0x0C000000) == 0) { // Check TC or RC
             if (--tx_timeout <= 0) {
                 xil_printf("Error: Hardware MAC TX Timeout at chunk %d (ISR=%08x)\r\n", 
                            offset/CHUNK_SIZE, Xil_In32(kFifoBaseAddr + XLLF_ISR_OFFSET));
                 return total_accumulator;
             }
        }

        // 5. Read Result from FIFO
        // C. Check Receive Length (Optional but good practice)
        // D. Wait for Result (RDFO > 0) or Receive Complete (ISR Bit 27)
        int rx_timeout = 1000000; 
        while (Xil_In32(kFifoBaseAddr + XLLF_RDFO_OFFSET) == 0) {
            // If RC (Bit 27) is set, checking occupancy before breaking
             if ((Xil_In32(kFifoBaseAddr + XLLF_ISR_OFFSET) & 0x08000000) != 0) {
                  // RC is set, but ensure RDFO > 0 or wait a tiny bit more?
                  // Just break, subsequent check handles empty.
                  break;
             }
            
            if (--rx_timeout <= 0) {
                xil_printf("Error: Hardware MAC RX Timeout at chunk %d (ISR=%08x)\r\n", 
                           offset/CHUNK_SIZE, Xil_In32(kFifoBaseAddr + XLLF_ISR_OFFSET));
                return total_accumulator; 
            }
        }

        // SAFETY: Do not read if FIFO is empty (Verified hang cause)
        if (Xil_In32(kFifoBaseAddr + XLLF_RDFO_OFFSET) == 0) {
            // Check RLF to see if hardware sent a 0-length packet
            volatile uint32_t len = Xil_In32(kFifoBaseAddr + XLLF_RLF_OFFSET);
            
            // This happens if RC fired but no data was returned.
            // Hardware Failure (Zero-Length packet or no TLAST+TVALID).
            xil_printf("Error: Hardware MAC RC set but FIFO Empty at chunk %d (ISR=%08x, RLF=%d)\r\n", 
                       offset/CHUNK_SIZE, Xil_In32(kFifoBaseAddr + XLLF_ISR_OFFSET), len);
            // Return current accumulator, effectively dropping this chunk
            return total_accumulator;
        }

        // E. Read Result (RLF then RDFD)
        volatile uint32_t read_len = Xil_In32(kFifoBaseAddr + XLLF_RLF_OFFSET);
        (void)read_len; // Suppress unused variable warning
        int32_t chunk_result = static_cast<int32_t>(Xil_In32(kFifoBaseAddr + XLLF_RDFD_OFFSET));
        total_accumulator += chunk_result;
    }

    return total_accumulator;
#else
    (void)packed_pairs;
    (void)pair_count;
    return 0;
#endif
}

}  // namespace ML
