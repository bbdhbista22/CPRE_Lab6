/**
 * StagedMAC Test - C++ Implementation
 *
 * Tests 3-stage pipelined MAC unit and 4-unit cluster
 * Verifies pipeline behavior and zero-point adjustment
 */

#include "../StagedMAC.h"
#include "test_framework.h"
#include <iostream>
#include <iomanip>

void test_single_mac_pipeline() {
    TEST_BEGIN("StagedMAC - 3-Stage Pipeline Verification");

    StagedMAC::Config config = {
        0,  // id
        0,  // zero_point_in
        0   // zero_point_weight
    };

    StagedMAC mac(config);

    std::cout << "Testing 3-stage pipeline:\n";
    std::cout << "  Input: 5 multiply-accumulate operations\n";
    std::cout << "  Expected: Pipeline fills (3 cycles latency), then 1 result/cycle\n\n";

    // Test inputs: all multiply by 2
    int8_t test_inputs[] = {10, 20, 30, 40, 50};
    int8_t test_weights[] = {2, 2, 2, 2, 2};

    std::cout << std::setw(5) << "Cycle" << " | "
              << std::setw(6) << "Input" << " | "
              << std::setw(6) << "Weight" << " | "
              << std::setw(8) << "Product" << " | "
              << std::setw(10) << "Accum" << " | Status\n";
    std::cout << std::string(80, '-') << "\n";

    for (int cycle = 0; cycle < 5; cycle++) {
        bool start_new = (cycle == 0);
        auto result = mac.executeCycle(test_inputs[cycle], test_weights[cycle], start_new);

        int32_t accum = mac.getAccumulator();
        int32_t product = (test_inputs[cycle] - 0) * (test_weights[cycle] - 0);

        std::cout << std::setw(5) << cycle << " | "
                  << std::setw(6) << (int)test_inputs[cycle] << " | "
                  << std::setw(6) << (int)test_weights[cycle] << " | "
                  << std::setw(8) << product << " | "
                  << std::setw(10) << accum << " | ";

        if (cycle < 3) {
            std::cout << "(pipeline fill)\n";
        } else {
            std::cout << "(result valid)\n";
        }
    }

    // Expected: (10 + 20 + 30 + 40 + 50) * 2 = 300
    int32_t final_accum = mac.getAccumulator();
    std::cout << "\nFinal accumulator: " << final_accum << "\n";
    std::cout << "Expected: 300\n";

    ASSERT_EQ(300, final_accum);

    TEST_END();
}

void test_zero_point_adjustment() {
    TEST_BEGIN("StagedMAC - Zero-Point Adjustment");

    StagedMAC::Config config = {
        0,   // id
        10,  // zero_point_in (inputs offset by 10)
        5    // zero_point_weight (weights offset by 5)
    };

    StagedMAC mac(config);

    std::cout << "Configuration:\n";
    std::cout << "  Zero-point in:     " << config.zero_point_in << "\n";
    std::cout << "  Zero-point weight: " << config.zero_point_weight << "\n\n";

    // With zero-point adjustment:
    // adjusted_input = input - zero_point_in
    // adjusted_weight = weight - zero_point_weight
    // product = adjusted_input × adjusted_weight

    int8_t input = 20;    // Adjusted: 20 - 10 = 10
    int8_t weight = 8;    // Adjusted: 8 - 5 = 3
    // Expected product: 10 × 3 = 30

    std::cout << "Test case:\n";
    std::cout << "  Input:  " << (int)input << " (adjusted: " << (input - 10) << ")\n";
    std::cout << "  Weight: " << (int)weight << " (adjusted: " << (weight - 5) << ")\n";
    std::cout << "  Expected product: " << ((input - 10) * (weight - 5)) << "\n\n";

    // Execute 4 cycles to get through pipeline
    for (int i = 0; i < 4; i++) {
        mac.executeCycle(input, weight, i == 0);
    }

    int32_t accum = mac.getAccumulator();
    std::cout << "Accumulator after 4 operations: " << accum << "\n";
    std::cout << "Expected: " << (4 * 30) << " (4 × 30)\n";

    ASSERT_EQ(4 * 30, accum);

    TEST_END();
}

void test_accumulator_reset() {
    TEST_BEGIN("StagedMAC - Accumulator Reset for New Pixel");

    StagedMAC::Config config = {
        0,  // id
        0,  // zero_point_in
        0   // zero_point_weight
    };

    StagedMAC mac(config);

    std::cout << "Testing accumulator reset between pixels:\n\n";

    // First pixel: 3 MACs
    std::cout << "Pixel 1: 3 MAC operations\n";
    for (int i = 0; i < 3; i++) {
        mac.executeCycle(10, 2, i == 0);
    }

    int32_t accum1 = mac.getAccumulator();
    std::cout << "  Accumulator: " << accum1 << " (expected 60)\n";
    ASSERT_EQ(60, accum1);

    // Second pixel: reset and do 2 MACs
    std::cout << "\nPixel 2: Reset and 2 MAC operations\n";
    mac.resetAccumulator();

    for (int i = 0; i < 2; i++) {
        mac.executeCycle(5, 3, i == 0);
    }

    int32_t accum2 = mac.getAccumulator();
    std::cout << "  Accumulator: " << accum2 << " (expected 30, NOT 90)\n";
    ASSERT_EQ(30, accum2);

    TEST_END();
}

void test_mac_cluster() {
    TEST_BEGIN("MACStreamProvider - 4 Parallel MACs");

    MACStreamProvider::Config config = {
        4,  // num_macs
        0,  // zero_point_in
        0   // zero_point_weight
    };

    MACStreamProvider provider(config);

    std::cout << "Testing 4 parallel MACs with different weights:\n\n";

    // All MACs get same input, different weights
    int8_t inputs[4] = {10, 10, 10, 10};
    int8_t weights[4] = {1, 2, 3, 4};

    std::cout << "Executing 3 cycles (27 MACs for 3×3×3 filter):\n";
    std::cout << std::setw(5) << "Cycle" << " | MAC0 | MAC1 | MAC2 | MAC3\n";
    std::cout << std::string(40, '-') << "\n";

    // Execute 3 cycles (not TLAST yet)
    for (int cycle = 0; cycle < 3; cycle++) {
        auto output = provider.executeCluster(inputs, weights, false);

        std::cout << std::setw(5) << cycle << " | "
                  << std::setw(4) << output.accum[0] << " | "
                  << std::setw(4) << output.accum[1] << " | "
                  << std::setw(4) << output.accum[2] << " | "
                  << std::setw(4) << output.accum[3] << "\n";

        ASSERT_FALSE(output.valid);  // Not valid until TLAST
    }

    // Final cycle with TLAST
    auto final_output = provider.executeCluster(inputs, weights, true);

    std::cout << std::setw(5) << "TLAST" << " | "
              << std::setw(4) << final_output.accum[0] << " | "
              << std::setw(4) << final_output.accum[1] << " | "
              << std::setw(4) << final_output.accum[2] << " | "
              << std::setw(4) << final_output.accum[3] << " (valid)\n";

    ASSERT_TRUE(final_output.valid);

    // Expected: 4 operations × (10 × weight)
    ASSERT_EQ(4 * 10 * 1, final_output.accum[0]);
    ASSERT_EQ(4 * 10 * 2, final_output.accum[1]);
    ASSERT_EQ(4 * 10 * 3, final_output.accum[2]);
    ASSERT_EQ(4 * 10 * 4, final_output.accum[3]);

    std::cout << "\nExpected values: [40, 80, 120, 160]\n";

    TEST_END();
}

void test_mac_cluster_reset() {
    TEST_BEGIN("MACStreamProvider - Cluster Reset After TLAST");

    MACStreamProvider::Config config = {
        4,  // num_macs
        0,  // zero_point_in
        0   // zero_point_weight
    };

    MACStreamProvider provider(config);

    int8_t inputs[4] = {5, 5, 5, 5};
    int8_t weights[4] = {2, 2, 2, 2};

    std::cout << "Pixel 1: 2 cycles with TLAST\n";

    // First pixel: 2 cycles
    provider.executeCluster(inputs, weights, false);
    auto output1 = provider.executeCluster(inputs, weights, true);

    std::cout << "  Accumulators: ["
              << output1.accum[0] << ", "
              << output1.accum[1] << ", "
              << output1.accum[2] << ", "
              << output1.accum[3] << "]\n";

    // All should be 2 × 5 × 2 = 20
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(20, output1.accum[i]);
    }

    std::cout << "\nPixel 2: 3 cycles with TLAST (accumulators should reset)\n";

    // Second pixel: 3 cycles (should start from 0)
    provider.executeCluster(inputs, weights, false);
    provider.executeCluster(inputs, weights, false);
    auto output2 = provider.executeCluster(inputs, weights, true);

    std::cout << "  Accumulators: ["
              << output2.accum[0] << ", "
              << output2.accum[1] << ", "
              << output2.accum[2] << ", "
              << output2.accum[3] << "]\n";

    // All should be 3 × 5 × 2 = 30 (NOT 20 + 30 = 50)
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(30, output2.accum[i]);
    }

    std::cout << "\n Accumulators correctly reset after TLAST\n";

    TEST_END();
}

int main() {
    std::cout << "\n";
    std::cout << "╔" << std::string(68, '=') << "╗\n";
    std::cout << "║" << std::string(68, ' ') << "║\n";
    std::cout << "║  StagedMAC C++ Test - Pipeline & Cluster Validation          ║\n";
    std::cout << "║" << std::string(68, ' ') << "║\n";
    std::cout << "╚" << std::string(68, '=') << "╝\n";

    test_single_mac_pipeline();
    test_zero_point_adjustment();
    test_accumulator_reset();
    test_mac_cluster();
    test_mac_cluster_reset();

    TestFramework::instance().printSummary();

    return TestFramework::instance().allPassed() ? 0 : 1;
}
