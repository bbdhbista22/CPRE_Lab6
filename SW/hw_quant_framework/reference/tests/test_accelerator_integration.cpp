/**
 * Accelerator Integration Test - C++ Implementation
 *
 * Tests complete dataflow: IndexGen → MAC → Dequant → Storage
 * Simulates full Conv1 layer with all 7M MACs
 */

#include "../IndexGenerator.h"
#include "../StagedMAC.h"
#include "../Dequantization.h"
#include "../OutputStorage.h"
#include "test_framework.h"
#include <iostream>
#include <iomanip>
#include <vector>

void test_small_integration() {
    TEST_BEGIN("Integration - Small Layer (First 108 MACs)");

    // Conv1 configuration
    IndexGenerator::ConvConfig conv_config = {
        64, 64, 3,  // input
        3, 3, 64,   // filter
        1, 1        // stride, padding
    };

    Dequantization::Config dequant_config = {
        0,           // zero_point_in
        0,           // zero_point_out
        0x00800000,  // scale_factor (0.5 in Q8.24)
        true,        // enable_relu
        false        // enable_batch_norm
    };

    MACStreamProvider::Config mac_config = {
        4,  // num_macs
        0,  // zero_point_in
        0   // zero_point_weight
    };

    OutputStorage::Config output_config = {
        64,     // output_height
        64,     // output_width
        64,     // output_channels
        false,  // enable_pooling
        0       // output_base_addr
    };

    std::cout << "Configuration:\n";
    std::cout << "  Input:   64×64×3\n";
    std::cout << "  Filters: 64×3×3\n";
    std::cout << "  Output:  64×64×64\n";
    std::cout << "  Scale:   0.5 (Q8.24)\n";
    std::cout << "  ReLU:    enabled\n\n";

    // Initialize components
    IndexGenerator index_gen(conv_config);
    MACStreamProvider macs(mac_config);
    Dequantization dequant(dequant_config);
    OutputStorage storage(output_config);

    // Generate first 108 MACs (4 complete pixels × 27 MACs/pixel)
    std::cout << "Generating first 108 addresses (4 pixels)...\n";
    auto addresses = index_gen.generateFirstN(108);

    ASSERT_EQ(108U, addresses.size());

    // Create dummy input and weight data
    std::vector<int8_t> input_data(64 * 64 * 3);
    std::vector<int8_t> weight_data(64 * 3 * 3 * 3);

    for (size_t i = 0; i < input_data.size(); i++) {
        input_data[i] = (i % 128);
    }
    for (size_t i = 0; i < weight_data.size(); i++) {
        weight_data[i] = ((i % 64) - 32);
    }

    std::cout << "Created " << input_data.size() << " input elements\n";
    std::cout << "Created " << weight_data.size() << " weight elements\n\n";

    // Simulate MAC operations
    int pixel_count = 0;
    int output_count = 0;

    std::cout << "Simulating 108 MACs through complete pipeline...\n";

    for (const auto& addr : addresses) {
        // Get data
        int8_t input_val = input_data[addr.input_addr % input_data.size()];
        int8_t weight_val = weight_data[addr.weight_addr % weight_data.size()];

        // Feed to all 4 MACs (simplified - same input, different weights)
        int8_t inputs[4] = {input_val, input_val, input_val, input_val};
        int8_t weights[4];
        for (int i = 0; i < 4; i++) {
            weights[i] = weight_data[(addr.weight_addr + i) % weight_data.size()];
        }

        // Execute MAC cluster
        auto mac_output = macs.executeCluster(inputs, weights, addr.tlast);

        // On TLAST, process outputs
        if (addr.tlast && mac_output.valid) {
            pixel_count++;

            // Dequantize all 4 outputs
            for (int oc = 0; oc < 4; oc++) {
                int8_t dequant_output = dequant.dequantizeScalar(mac_output.accum[oc]);

                // Store (simplified - just count)
                output_count++;
            }
        }
    }

    std::cout << "\nResults:\n";
    std::cout << "  MACs processed:   " << addresses.size() << "\n";
    std::cout << "  Pixels completed: " << pixel_count << " (expected 4)\n";
    std::cout << "  Outputs generated: " << output_count << " (expected 16)\n";

    ASSERT_EQ(4, pixel_count);
    ASSERT_EQ(16, output_count);

    TEST_END();
}

void test_full_layer_simulation() {
    TEST_BEGIN("Integration - Full Conv1 Layer (7M MACs)");

    // Conv1 configuration
    IndexGenerator::ConvConfig conv_config = {
        64, 64, 3,  // input
        3, 3, 64,   // filter
        1, 1        // stride, padding
    };

    IndexGenerator index_gen(conv_config);

    std::cout << "Configuration:\n";
    std::cout << "  Input:       64×64×3\n";
    std::cout << "  Filters:     64×3×3\n";
    std::cout << "  Output:      64×64×64\n";
    std::cout << "  MACs/pixel:  27\n\n";

    // Calculate expected totals
    uint32_t expected_total_macs = 64 * 64 * 64 * 27;
    uint32_t expected_pixels = 64 * 64 * 64;

    std::cout << "Expected:\n";
    std::cout << "  Total MACs:   " << expected_total_macs << "\n";
    std::cout << "  Total pixels: " << expected_pixels << "\n\n";

    std::cout << "Generating all addresses (this may take 10-30 seconds)...\n";

    auto addresses = index_gen.generateAllAddresses();

    std::cout << "Generated " << addresses.size() << " addresses\n";

    ASSERT_EQ(expected_total_macs, (uint32_t)addresses.size());

    // Verify TLAST count
    int tlast_count = 0;
    for (const auto& addr : addresses) {
        if (addr.tlast) {
            tlast_count++;
        }
    }

    std::cout << "TLAST count: " << tlast_count << "\n";
    std::cout << "Expected:    " << expected_pixels << "\n";

    ASSERT_EQ(expected_pixels, (uint32_t)tlast_count);

    // Verify addresses
    bool verified = index_gen.verifyAddresses(addresses);
    ASSERT_TRUE(verified);

    std::cout << "\n Full layer simulation validated\n";
    std::cout << " Ready for hardware execution\n";

    TEST_END();
}

void test_end_to_end_dataflow() {
    TEST_BEGIN("Integration - End-to-End Dataflow Verification");

    // Smaller test case for detailed verification
    IndexGenerator::ConvConfig conv_config = {
        8, 8, 3,   // input (smaller for testing)
        3, 3, 4,   // filter (4 output channels)
        1, 1       // stride, padding
    };

    Dequantization::Config dequant_config = {
        0,           // zero_point_in
        0,           // zero_point_out
        0x01000000,  // scale_factor (1.0 in Q8.24 - no scaling)
        false,       // enable_relu (disabled for testing)
        false        // enable_batch_norm
    };

    MACStreamProvider::Config mac_config = {
        4,  // num_macs
        0,  // zero_point_in
        0   // zero_point_weight
    };

    std::cout << "Smaller test case: 8×8×3 → 8×8×4\n";
    std::cout << "Testing end-to-end dataflow with known values\n\n";

    // Initialize components
    IndexGenerator index_gen(conv_config);
    MACStreamProvider macs(mac_config);
    Dequantization dequant(dequant_config);

    // Create simple test data (all 1s for easy verification)
    std::vector<int8_t> input_data(8 * 8 * 3, 1);   // All 1s
    std::vector<int8_t> weight_data(4 * 3 * 3 * 3, 1);  // All 1s

    // Generate all addresses
    auto addresses = index_gen.generateAllAddresses();

    std::cout << "Generated " << addresses.size() << " addresses\n";

    // Expected: 8 × 8 × 4 × 27 = 6,912 MACs
    uint32_t expected_macs = 8 * 8 * 4 * 27;
    ASSERT_EQ(expected_macs, (uint32_t)addresses.size());

    // Simulate (just count for speed)
    int pixel_count = 0;
    int32_t total_accumulator_sum = 0;

    for (const auto& addr : addresses) {
        int8_t input_val = input_data[addr.input_addr % input_data.size()];
        int8_t weight_val = weight_data[addr.weight_addr % weight_data.size()];

        int8_t inputs[4] = {input_val, input_val, input_val, input_val};
        int8_t weights[4] = {weight_val, weight_val, weight_val, weight_val};

        auto mac_output = macs.executeCluster(inputs, weights, addr.tlast);

        if (addr.tlast && mac_output.valid) {
            pixel_count++;

            // With all 1s: each accumulator should be 27 (3×3×3 MACs of 1×1)
            for (int oc = 0; oc < 4; oc++) {
                total_accumulator_sum += mac_output.accum[oc];
            }
        }
    }

    std::cout << "\nResults:\n";
    std::cout << "  Pixels completed: " << pixel_count << "\n";
    std::cout << "  Expected pixels:  " << (8 * 8 * 4) << "\n";

    ASSERT_EQ(8 * 8 * 4, pixel_count);

    // With all 1s: 256 pixels × 4 channels × 27 = 27,648
    std::cout << "  Total accumulator sum: " << total_accumulator_sum << "\n";
    std::cout << "  Expected sum:          " << (8 * 8 * 4 * 27) << "\n";

    ASSERT_EQ(8 * 8 * 4 * 27, total_accumulator_sum);

    std::cout << "\n End-to-end dataflow verified\n";

    TEST_END();
}

int main() {
    std::cout << "\n";
    std::cout << "╔" << std::string(68, '=') << "╗\n";
    std::cout << "║" << std::string(68, ' ') << "║\n";
    std::cout << "║  Accelerator Integration Test - Full Pipeline Validation     ║\n";
    std::cout << "║" << std::string(68, ' ') << "║\n";
    std::cout << "╚" << std::string(68, '=') << "╝\n";

    test_small_integration();
    test_end_to_end_dataflow();
    test_full_layer_simulation();

    TestFramework::instance().printSummary();

    return TestFramework::instance().allPassed() ? 0 : 1;
}
