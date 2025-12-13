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
    if (packed_pairs == nullptr || pair_count == 0) {
        return 0;
    }

    // 1. Reset FIFO to ensure clean state
    Xil_Out32(kFifoBaseAddr + XLLF_LLR_OFFSET, 0xA5);
    
    // 2. Clear Interrupt Status Register
    Xil_Out32(kFifoBaseAddr + XLLF_ISR_OFFSET, 0xFFFFFFFF);

    int32_t total_accumulator = 0;
    const std::size_t CHUNK_SIZE = 16; 

    // Define TDR Offset if not available
    #ifndef XLLF_TDR_OFFSET
    #define XLLF_TDR_OFFSET 0x0000002C
    #endif

    for (std::size_t offset = 0; offset < pair_count; offset += CHUNK_SIZE) {
        std::size_t chunk_len = std::min(CHUNK_SIZE, pair_count - offset);
        uint32_t chunk_bytes = static_cast<uint32_t>(chunk_len * 4);

        // A. Write TDR (Transmit Destination)
        // Required for some AXI Stream interconnect configurations
        Xil_Out32(kFifoBaseAddr + XLLF_TDR_OFFSET, 0x0);

        // B. Write TLF (Transmit Length)
        // Writing TLF triggers the FIFO to start transmitting once data is available
        Xil_Out32(kFifoBaseAddr + XLLF_TLF_OFFSET, chunk_bytes);

        // C. Write Data to TDFD
        for (std::size_t i = 0; i < chunk_len; ++i) {
            // Wait for vacancy in Transmit FIFO
            while ((Xil_In32(kFifoBaseAddr + XLLF_TDFV_OFFSET) & 0x1FF) == 0);
            Xil_Out32(kFifoBaseAddr + XLLF_TDFD_OFFSET, static_cast<uint32_t>(packed_pairs[offset + i]));
        }

        // D. Wait for Result (RDFO > 0)
        // Use a timeout to prevent infinite hangs if hardware is unresponsive
        int timeout = 10000000;
        while (Xil_In32(kFifoBaseAddr + XLLF_RDFO_OFFSET) == 0) {
            if (--timeout <= 0) {
                xil_printf("Error: Hardware MAC Timeout at chunk %d\r\n", offset/CHUNK_SIZE);
                return total_accumulator; // Return what we have so far
            }
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
