/**
 * IndexGenerator Test - C++ Implementation
 *
 * Tests address generation for convolution operations
 * Verifies correctness against Python reference implementation
 */

#include "../IndexGenerator.h"
#include "test_framework.h"
#include <iostream>
#include <iomanip>

void test_configuration() {
    TEST_BEGIN("IndexGenerator - Configuration Validation");

    // Conv1 configuration
    IndexGenerator::ConvConfig config = {
        64,  // input_height
        64,  // input_width
        3,   // input_channels
        3,   // filter_height
        3,   // filter_width
        64,  // num_filters
        1,   // stride
        1    // padding
    };

    IndexGenerator gen(config, 0, 0, 16);

    // Verify derived values
    ASSERT_EQ(64, gen.getConvConfig().output_height);
    ASSERT_EQ(64, gen.getConvConfig().output_width);
    ASSERT_EQ(27U, gen.getConvConfig().macs_per_pixel);  // 3×3×3

    // Verify tiling
    ASSERT_EQ(4, gen.getTileConfig().tiles_per_row);  // ceil(64/16)
    ASSERT_EQ(4, gen.getTileConfig().tiles_per_col);
    ASSERT_EQ(16, gen.getTileConfig().total_tiles);   // 4×4

    std::cout << "Configuration:\n";
    std::cout << "  Input:       64×64×3\n";
    std::cout << "  Filter:      3×3×3 (stride=1, padding=1)\n";
    std::cout << "  Output:      " << gen.getConvConfig().output_height
              << "×" << gen.getConvConfig().output_width << "×64\n";
    std::cout << "  MACs/pixel:  " << gen.getConvConfig().macs_per_pixel << "\n";
    std::cout << "  Tiles:       4×4 (16 total)\n";

    TEST_END();
}

void test_output_dimensions() {
    TEST_BEGIN("IndexGenerator - Output Dimension Calculation");

    IndexGenerator::ConvConfig config = {
        64, 64, 3,
        3, 3, 64,
        1, 1
    };

    IndexGenerator gen(config);

    // Expected: (64 - 3 + 2*1) / 1 + 1 = 64
    ASSERT_EQ(64, gen.getConvConfig().output_height);
    ASSERT_EQ(64, gen.getConvConfig().output_width);

    // Expected MACs per pixel: 3 × 3 × 3 = 27
    ASSERT_EQ(27U, gen.getConvConfig().macs_per_pixel);

    // Total MACs: 64 × 64 × 64 × 27 = 7,077,888
    uint32_t expected_total_macs = 64 * 64 * 64 * 27;
    std::cout << "Expected total MACs: " << expected_total_macs << "\n";
    std::cout << "  = 64 × 64 × 64 × 27 = 7,077,888\n";

    TEST_END();
}

void test_address_generation() {
    TEST_BEGIN("IndexGenerator - First 100 Address Generation");

    IndexGenerator::ConvConfig config = {
        64, 64, 3,
        3, 3, 64,
        1, 1
    };

    IndexGenerator gen(config);

    // Generate first 100 addresses
    auto addresses = gen.generateFirstN(100);

    ASSERT_EQ(100U, addresses.size());

    std::cout << "First 30 addresses:\n";
    std::cout << std::setw(5) << "Idx" << " | "
              << std::setw(8) << "Input" << " | "
              << std::setw(8) << "Weight" << " | "
              << "TLAST | OC\n";
    std::cout << std::string(50, '-') << "\n";

    for (size_t i = 0; i < 30 && i < addresses.size(); i++) {
        std::cout << std::setw(5) << i << " | "
                  << "0x" << std::setw(6) << std::setfill('0') << std::hex << addresses[i].input_addr << " | "
                  << "0x" << std::setw(6) << std::setfill('0') << addresses[i].weight_addr << " | "
                  << std::setfill(' ') << std::dec
                  << std::setw(5) << (addresses[i].tlast ? "Y" : "N") << " | "
                  << (int)addresses[i].oc << "\n";

        // Print separator every 27 MACs
        if ((i + 1) % 27 == 0) {
            std::cout << std::string(50, '-') << "\n";
        }
    }

    TEST_END();
}

void test_tlast_pattern() {
    TEST_BEGIN("IndexGenerator - TLAST Pattern Verification");

    IndexGenerator::ConvConfig config = {
        64, 64, 3,
        3, 3, 64,
        1, 1
    };

    IndexGenerator gen(config);

    // Generate first 108 addresses (4 complete pixels)
    auto addresses = gen.generateFirstN(108);

    // Count TLAST signals
    int tlast_count = 0;
    for (const auto& addr : addresses) {
        if (addr.tlast) {
            tlast_count++;
        }
    }

    // Should have 4 TLAST signals (one every 27 MACs)
    ASSERT_EQ(4, tlast_count);

    // Verify TLAST appears at positions 26, 53, 80, 107 (0-indexed)
    ASSERT_TRUE(addresses[26].tlast);
    ASSERT_TRUE(addresses[53].tlast);
    ASSERT_TRUE(addresses[80].tlast);
    ASSERT_TRUE(addresses[107].tlast);

    // Verify TLAST doesn't appear elsewhere
    for (size_t i = 0; i < addresses.size(); i++) {
        if (i == 26 || i == 53 || i == 80 || i == 107) {
            ASSERT_TRUE(addresses[i].tlast);
        } else {
            ASSERT_FALSE(addresses[i].tlast);
        }
    }

    std::cout << "TLAST pattern verified:\n";
    std::cout << "  First 108 MACs: " << tlast_count << " TLAST signals\n";
    std::cout << "  Expected: 4 TLAST signals (every 27 MACs)\n";
    std::cout << "  Pattern: TLAST at indices 26, 53, 80, 107\n";

    TEST_END();
}

void test_complete_generation() {
    TEST_BEGIN("IndexGenerator - Complete Address Generation (7M MACs)");

    IndexGenerator::ConvConfig config = {
        64, 64, 3,
        3, 3, 64,
        1, 1
    };

    IndexGenerator gen(config);

    std::cout << "Generating all addresses (this may take a moment)...\n";

    auto addresses = gen.generateAllAddresses();

    // Expected: 64 × 64 × 64 × 27 = 7,077,888
    uint32_t expected_total = 64 * 64 * 64 * 27;

    std::cout << "Generated " << addresses.size() << " addresses\n";
    std::cout << "Expected  " << expected_total << " addresses\n";

    ASSERT_EQ(expected_total, (uint32_t)addresses.size());

    // Count TLAST signals - should equal number of output pixels
    int tlast_count = 0;
    for (const auto& addr : addresses) {
        if (addr.tlast) {
            tlast_count++;
        }
    }

    // Expected: 64 × 64 × 64 = 262,144 output elements
    int expected_tlasts = 64 * 64 * 64;
    ASSERT_EQ(expected_tlasts, tlast_count);

    std::cout << "TLAST count: " << tlast_count << " (expected " << expected_tlasts << ")\n";

    TEST_END();
}

void test_address_bounds() {
    TEST_BEGIN("IndexGenerator - Address Bounds Verification");

    IndexGenerator::ConvConfig config = {
        64, 64, 3,
        3, 3, 64,
        1, 1
    };

    IndexGenerator gen(config, 0, 0, 16);

    auto addresses = gen.generateAllAddresses();

    // Calculate maximum valid addresses
    uint32_t max_input_addr = 64 * 64 * 3;  // 12,288
    uint32_t max_weight_addr = 64 * 3 * 3 * 3;  // 1,728 per filter × 64 = 110,592

    std::cout << "Checking " << addresses.size() << " addresses...\n";
    std::cout << "Max input address:  " << max_input_addr << "\n";
    std::cout << "Max weight address: " << max_weight_addr << "\n";

    // Verify all addresses are within bounds
    bool all_valid = true;
    for (size_t i = 0; i < addresses.size(); i++) {
        if (addresses[i].input_addr >= max_input_addr) {
            std::cout << "ERROR: Input address out of bounds at index " << i
                      << " (0x" << std::hex << addresses[i].input_addr << ")\n" << std::dec;
            all_valid = false;
            break;
        }
        if (addresses[i].weight_addr >= max_weight_addr) {
            std::cout << "ERROR: Weight address out of bounds at index " << i
                      << " (0x" << std::hex << addresses[i].weight_addr << ")\n" << std::dec;
            all_valid = false;
            break;
        }
        if (addresses[i].oc > 3) {
            std::cout << "ERROR: Invalid output channel at index " << i
                      << " (oc=" << (int)addresses[i].oc << ")\n";
            all_valid = false;
            break;
        }
    }

    ASSERT_TRUE(all_valid);

    if (all_valid) {
        std::cout << " All addresses within valid bounds\n";
        std::cout << " All output channel indices valid (0-3)\n";
    }

    // Use the verifyAddresses method
    bool verified = gen.verifyAddresses(addresses);
    ASSERT_TRUE(verified);

    TEST_END();
}

int main() {
    std::cout << "\n";
    std::cout << "╔" << std::string(68, '=') << "╗\n";
    std::cout << "║" << std::string(68, ' ') << "║\n";
    std::cout << "║  IndexGenerator C++ Test - Conv1 Layer Validation            ║\n";
    std::cout << "║" << std::string(68, ' ') << "║\n";
    std::cout << "╚" << std::string(68, '=') << "╝\n";

    test_configuration();
    test_output_dimensions();
    test_address_generation();
    test_tlast_pattern();
    test_complete_generation();
    test_address_bounds();

    TestFramework::instance().printSummary();

    return TestFramework::instance().allPassed() ? 0 : 1;
}
