#include "IndexGenerator.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "====================================\n";
    std::cout << "IndexGenerator Test - Conv1 Layer\n";
    std::cout << "====================================\n\n";

    // Conv1 configuration from theoretical_analysis.md
    IndexGenerator::ConvConfig config;
    config.input_height = 64;
    config.input_width = 64;
    config.input_channels = 3;
    config.filter_height = 3;
    config.filter_width = 3;
    config.num_filters = 64;
    config.stride = 1;
    config.padding = 1;

    try {
        IndexGenerator gen(config, 0, 0, 16);
        
        std::cout << "Configuration:\n";
        std::cout << "  Input:       " << config.input_height << "x" << config.input_width 
                  << "x" << (int)config.input_channels << "\n";
        std::cout << "  Filter:      " << (int)config.filter_height << "x" << (int)config.filter_width 
                  << "x" << (int)config.input_channels << " (stride=" << (int)config.stride 
                  << ", padding=" << (int)config.padding << ")\n";
        std::cout << "  Output:      " << gen.getConvConfig().output_height << "x" 
                  << gen.getConvConfig().output_width << "x" << (int)config.num_filters << "\n";
        std::cout << "  MACs/pixel:  " << gen.getConvConfig().macs_per_pixel << "\n";
        std::cout << "  Tile size:   " << gen.getTileConfig().tile_size << "x" 
                  << gen.getTileConfig().tile_size << "\n";
        std::cout << "  Tiles:       " << gen.getTileConfig().tiles_per_row << "x" 
                  << gen.getTileConfig().tiles_per_col << " (" 
                  << gen.getTileConfig().total_tiles << " total)\n\n";

        // Calculate expected total MACs
        uint32_t expected_macs = (uint32_t)gen.getConvConfig().output_height 
                                * gen.getConvConfig().output_width 
                                * config.num_filters 
                                * gen.getConvConfig().macs_per_pixel;
        
        std::cout << "Expected total MACs: " << expected_macs << "\n";
        std::cout << "  = " << gen.getConvConfig().output_height << " × " 
                  << gen.getConvConfig().output_width << " × " 
                  << (int)config.num_filters << " × " 
                  << gen.getConvConfig().macs_per_pixel << "\n";
        std::cout << "  = " << expected_macs << " (should be 7,077,888)\n\n";

        // Generate first 100 addresses for inspection
        std::cout << "Generating first 100 addresses...\n\n";
        auto first_100 = gen.generateFirstN(100);
        
        std::cout << std::setw(5) << "Idx" << " | " 
                  << std::setw(8) << "Input" << " | " 
                  << std::setw(8) << "Weight" << " | " 
                  << "TLAST | OC\n";
        std::cout << std::string(50, '-') << "\n";

        for (size_t i = 0; i < first_100.size(); i++) {
            const auto& addr = first_100[i];
            std::cout << std::setw(5) << i << " | " 
                      << "0x" << std::hex << std::setw(6) << std::setfill('0') << addr.input_addr << std::dec << std::setfill(' ') << " | " 
                      << "0x" << std::hex << std::setw(6) << std::setfill('0') << addr.weight_addr << std::dec << std::setfill(' ') << " | " 
                      << (addr.tlast ? "  Y  " : "  N  ") << " | " 
                      << (int)addr.oc << "\n";
            
            // Print separator every 27 MACs (end of pixel)
            if ((i + 1) % 27 == 0) {
                std::cout << std::string(50, '-') << "\n";
            }
        }

        // Test TLAST pattern
        std::cout << "\nTLAST Pattern Verification:\n";
        int tlast_count = 0;
        for (const auto& addr : first_100) {
            if (addr.tlast) tlast_count++;
        }
        std::cout << "  First 100 MACs: " << tlast_count << " TLAST signals\n";
        std::cout << "  Expected: " << (100 / 27) << " TLAST signals\n";
        std::cout << "  Pattern: TLAST should appear every " << gen.getConvConfig().macs_per_pixel << " MACs\n\n";

        // Generate all addresses and verify
        std::cout << "Generating all addresses and verifying...\n";
        auto all_addresses = gen.generateAllAddresses();
        
        if (gen.verifyAddresses(all_addresses)) {
            std::cout << "\n All tests PASSED!\n";
            return 0;
        } else {
            std::cout << "\n Verification FAILED!\n";
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
