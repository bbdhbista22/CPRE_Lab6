#include "Dequantization.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "DEQUANTIZATION UNIT TEST - Individual Component Verification\n";
    std::cout << "======================================================================\n\n";

    // Test 1: Basic Q8.24 fixed-point multiplication and rounding
    std::cout << "Test 1: Q8.24 Fixed-Point Multiply with Rounding\n";
    std::cout << std::string(60, '-') << "\n";
    
    Dequantization::Config config;
    config.zero_point_in = 0;
    config.zero_point_out = 0;
    config.scale_factor = 0x00800000;  // 0.5 in Q8.24
    config.enable_relu = false;
    config.enable_batch_norm = false;
    
    Dequantization dequant(config);
    
    // Test cases with known expected outputs
    struct TestCase {
        int32_t accumulator;
        int8_t expected_output;
        const char* description;
    };
    
    TestCase tests[] = {
        {0x00000000, 0, "Zero accumulator"},
        {0x00800000, 1, "Accumulator = 0.5 (should round to 1)"},
        {0x01000000, 2, "Accumulator = 1.0 (scale=0.5, so 2*0.5=1)"},
        {0x7FFFFFFF, 127, "Max positive (saturated to 127)"},
        {0x80000000, -128, "Min negative (saturated to -128)"},
    };
    
    bool all_pass = true;
    
    for (int i = 0; i < 5; i++) {
        dequant.resetPipeline();
        
        // Process through 5-stage pipeline
        for (int stage = 0; stage < 5; stage++) {
            if (stage == 0) {
                dequant.executeCycle(tests[i].accumulator);
            } else {
                dequant.executeCycle(0);
            }
        }
        
        int8_t output = dequant.getLatestOutput();
        bool pass = (output == tests[i].expected_output);
        all_pass = all_pass && pass;
        
        std::cout << "  " << tests[i].description << "\n";
        std::cout << "    Input (hex): 0x" << std::hex << std::setfill('0') << std::setw(8) 
                  << (tests[i].accumulator & 0xFFFFFFFF) << std::dec << "\n";
        std::cout << "    Expected: " << (int)tests[i].expected_output 
                  << ", Got: " << (int)output << " ";
        std::cout << (pass ? "[PASS]" : "[FAIL]") << "\n\n";
    }
    
    // Test 2: ReLU activation
    std::cout << "Test 2: ReLU Activation\n";
    std::cout << std::string(60, '-') << "\n";
    
    Dequantization::Config relu_config;
    relu_config.zero_point_in = 0;
    relu_config.zero_point_out = 0;
    relu_config.scale_factor = 0x00800000;
    relu_config.enable_relu = true;
    relu_config.enable_batch_norm = false;
    
    Dequantization dequant_relu(relu_config);
    
    // Negative accumulator should become 0 with ReLU
    dequant_relu.resetPipeline();
    for (int stage = 0; stage < 5; stage++) {
        if (stage == 0) {
            dequant_relu.executeCycle(0xFFFFFFFF);  // Negative value
        } else {
            dequant_relu.executeCycle(0);
        }
    }
    
    int8_t relu_output = dequant_relu.getLatestOutput();
    bool relu_pass = (relu_output == 0);  // ReLU should clamp to 0
    all_pass = all_pass && relu_pass;
    
    std::cout << "  Negative accumulator with ReLU enabled\n";
    std::cout << "    Input: 0xFFFFFFFF (negative)\n";
    std::cout << "    Expected: 0 (ReLU clamps negative to 0)\n";
    std::cout << "    Got: " << (int)relu_output << " " << (relu_pass ? "[PASS]" : "[FAIL]") << "\n\n";
    
    // Test 3: Pipeline latency
    std::cout << "Test 3: 5-Stage Pipeline Latency\n";
    std::cout << std::string(60, '-') << "\n";
    
    Dequantization dequant_latency(config);
    
    // Insert value at stage 0, should appear at output after 5 cycles
    dequant_latency.resetPipeline();
    dequant_latency.executeCycle(0x00800000);  // Cycle 0: input
    
    std::cout << "  After cycle 0 (input): output = " << (int)dequant_latency.getLatestOutput() << "\n";
    
    int8_t output_after_5 = 0;
    for (int i = 1; i <= 5; i++) {
        dequant_latency.executeCycle(0);
        output_after_5 = dequant_latency.getLatestOutput();
        std::cout << "  After cycle " << i << ": output = " << (int)output_after_5 << "\n";
    }
    
    bool latency_pass = (output_after_5 == 1);  // 0x00800000 * 0.5 = 0.5 -> rounds to 1
    all_pass = all_pass && latency_pass;
    std::cout << "  Expected output at cycle 5: 1, Got: " << (int)output_after_5 
              << " " << (latency_pass ? "[PASS]" : "[FAIL]") << "\n\n";
    
    if (!all_pass) {
        std::cout << "======================================================================\n";
        std::cout << "[FAIL] SOME DEQUANTIZATION TESTS FAILED\n";
        std::cout << "======================================================================\n\n";
        return 1;
    }
    
    std::cout << "======================================================================\n";
    std::cout << "[PASS] ALL DEQUANTIZATION TESTS PASSED\n";
    std::cout << "======================================================================\n\n";
    
    return 0;
}
