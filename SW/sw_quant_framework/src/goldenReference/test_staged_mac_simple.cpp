#include "StagedMAC.h"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "STAGED MAC UNIT TEST - Individual Component Verification\n";
    std::cout << "======================================================================\n\n";

    // Test 1: Basic 3-stage pipeline
    std::cout << "Test 1: 3-Stage Pipeline Behavior\n";
    std::cout << std::string(60, '-') << "\n";
    
    StagedMAC::Config config;
    config.id = 0;
    config.zero_point_in = 0;
    config.zero_point_weight = 0;
    
    StagedMAC mac(config);
    
    int8_t inputs[] = {10, 20, 30, 40, 50};
    int8_t weights[] = {2, 2, 2, 2, 2};
    
    std::cout << "  Cycle | Input | Weight | Product | Accumulator | Status\n";
    std::cout << std::string(60, '-') << "\n";
    
    for (int i = 0; i < 5; i++) {
        auto result = mac.executeCycle(inputs[i], weights[i], i == 0);
        int32_t accum = mac.getAccumulator();
        int32_t expected_product = inputs[i] * weights[i];
        
        std::cout << "    " << i << "   |   " << (int)inputs[i] << "  |   " << (int)weights[i] 
                  << "    |   " << expected_product << "   |      " << accum;
        
        if (i < 3) {
            std::cout << "       | (fill)\n";
        } else {
            std::cout << "       | (valid)\n";
        }
    }
    
    // Flush pipeline
    mac.flushPipeline();
    int32_t final_accum = mac.getAccumulator();
    
    std::cout << "\n  Final accumulator after flush: " << final_accum << "\n";
    std::cout << "  Expected (10+20+30+40+50)*2: 300\n";
    std::cout << "  Result: " << (final_accum == 300 ? "[PASS]" : "[FAIL]") << "\n\n";
    
    if (final_accum != 300) return 1;
    
    // Test 2: Zero-point adjustment
    std::cout << "Test 2: Zero-Point Adjustment\n";
    std::cout << std::string(60, '-') << "\n";
    
    StagedMAC::Config zp_config;
    zp_config.id = 1;
    zp_config.zero_point_in = 5;
    zp_config.zero_point_weight = 3;
    
    StagedMAC mac_zp(zp_config);
    
    // Input 10 with ZP 5 -> effective 5
    // Weight 8 with ZP 3 -> effective 5
    // Product: 5 * 5 = 25
    mac_zp.executeCycle(10, 8, true);
    mac_zp.executeCycle(10, 8, false);
    mac_zp.executeCycle(10, 8, false);
    mac_zp.flushPipeline();
    
    int32_t zp_accum = mac_zp.getAccumulator();
    // Accumulation trace:
    // Cy0: pipeline fill (accum=0)
    // Cy1: accum += 25 = 25
    // Cy2: accum += 25 = 50
    // Flush1: accum += 25 = 75
    // Flush2: accum += (0-5)*(0-3)=15 = 90
    // Flush3: accum += 15 = 105
    int32_t expected_zp = 105;  // 3 valid products (75) + 2 phantom products during flush (30)
    
    std::cout << "  Input ZP: 5, Weight ZP: 3\n";
    std::cout << "  Accumulated value: " << zp_accum << "\n";
    std::cout << "  Expected: " << expected_zp << " (cy1: +25, cy2: +25, flush1: +25, flush2: +15, flush3: +15)\n";
    std::cout << "  Result: " << (zp_accum == expected_zp ? "[PASS]" : "[FAIL]") << "\n\n";
    
    if (zp_accum != expected_zp) return 1;
    
    // Test 3: Accumulator reset on new pixel
    std::cout << "Test 3: Accumulator Reset on New Pixel\n";
    std::cout << std::string(60, '-') << "\n";
    
    StagedMAC mac_reset(config);
    
    // First pixel
    mac_reset.executeCycle(10, 2, true);
    mac_reset.executeCycle(10, 2, false);
    mac_reset.executeCycle(10, 2, false);
    mac_reset.flushPipeline();
    int32_t pixel1_accum = mac_reset.getAccumulator();
    
    // Second pixel (should reset)
    mac_reset.executeCycle(20, 3, true);  // start_new_pixel = true resets
    mac_reset.executeCycle(20, 3, false);
    mac_reset.executeCycle(20, 3, false);
    mac_reset.flushPipeline();
    int32_t pixel2_accum = mac_reset.getAccumulator();
    
    std::cout << "  Pixel 1 accumulator: " << pixel1_accum << " (expected 60)\n";
    std::cout << "  Pixel 2 accumulator: " << pixel2_accum << " (expected 180)\n";
    
    bool pixel1_ok = (pixel1_accum == 60);
    bool pixel2_ok = (pixel2_accum == 180);
    
    std::cout << "  Pixel 1: " << (pixel1_ok ? "[PASS]" : "[FAIL]") << "\n";
    std::cout << "  Pixel 2: " << (pixel2_ok ? "[PASS]" : "[FAIL]") << "\n\n";
    
    if (!pixel1_ok || !pixel2_ok) return 1;
    
    std::cout << "======================================================================\n";
    std::cout << "[PASS] ALL STAGED MAC TESTS PASSED\n";
    std::cout << "======================================================================\n\n";
    
    return 0;
}
