/**
 * Complete Pipeline Test - C++ Implementation
 *
 * Tests complete hardware pipeline with cycle-by-cycle verbose logging
 * Output format matches FPGA verification requirements
 */

#include "../IndexGenerator.h"
#include "../StagedMAC.h"
#include "../Dequantization.h"
#include "../OutputStorage.h"
#include "test_framework.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

class VerboseLogger {
public:
    void logMACOperation(uint32_t cycle, uint8_t mac_id, uint8_t input_val,
                        uint8_t weight_val, int32_t accumulator) {
        std::ostringstream oss;
        oss << "[CYCLE " << std::setw(6) << std::setfill('0') << cycle << "] "
            << "MAC#" << (int)mac_id
            << " input=0x" << std::setw(2) << std::setfill('0') << std::hex << (int)input_val
            << " weight=0x" << std::setw(2) << (int)weight_val
            << " -> accum=0x" << std::setw(8) << accumulator << std::dec;
        log_.push_back(oss.str());
    }

    void logDequantOperation(uint32_t cycle, int32_t input_accum,
                            int32_t scale, uint8_t output_int8) {
        std::ostringstream oss;
        oss << "[CYCLE " << std::setw(6) << std::setfill('0') << cycle << "] "
            << "DEQUANT input=0x" << std::setw(8) << std::setfill('0') << std::hex << input_accum
            << " scale=0x" << std::setw(8) << scale
            << " -> output=0x" << std::setw(2) << (int)output_int8 << std::dec;
        log_.push_back(oss.str());
    }

    void logOutputStore(uint32_t cycle, uint32_t addr, uint8_t byte_sel, uint8_t value) {
        std::ostringstream oss;
        oss << "[CYCLE " << std::setw(6) << std::setfill('0') << cycle << "] "
            << "STORE addr=0x" << std::setw(6) << std::setfill('0') << std::hex << addr
            << " byte[" << std::dec << (int)byte_sel << "]=0x"
            << std::setw(2) << std::setfill('0') << std::hex << (int)value << std::dec;
        log_.push_back(oss.str());
    }

    void logPixelComplete(uint32_t cycle, uint16_t out_y, uint16_t out_x, uint16_t out_c) {
        std::ostringstream oss;
        oss << "[CYCLE " << std::setw(6) << std::setfill('0') << cycle << "] "
            << "PIXEL_COMPLETE y=" << std::setw(3) << out_y
            << " x=" << std::setw(3) << out_x
            << " c=" << std::setw(2) << out_c;
        log_.push_back(oss.str());
    }

    const std::vector<std::string>& getLog() const { return log_; }

    void printLog(size_t first_n = 50, size_t last_n = 10) const {
        if (log_.size() <= first_n + last_n) {
            for (const auto& entry : log_) {
                std::cout << entry << "\n";
            }
        } else {
            std::cout << "First " << first_n << " operations:\n";
            for (size_t i = 0; i < first_n; i++) {
                std::cout << log_[i] << "\n";
            }

            std::cout << "\n... (" << (log_.size() - first_n - last_n)
                      << " more operations) ...\n\n";

            std::cout << "Last " << last_n << " operations:\n";
            for (size_t i = log_.size() - last_n; i < log_.size(); i++) {
                std::cout << log_[i] << "\n";
            }
        }
    }

private:
    std::vector<std::string> log_;
};

void test_mac_unit_only() {
    TEST_BEGIN("Complete Pipeline - MAC Unit Only");

    StagedMAC::Config config = {
        0,  // id
        0,  // zero_point_in
        0   // zero_point_weight
    };

    StagedMAC mac(config);

    std::cout << "Testing 3-stage pipeline with 5 MAC operations\n";
    std::cout << "Expected: Pipeline fill (3 cycles), then accumulation\n\n";

    int8_t test_inputs[] = {10, 20, 30, 40, 50};
    int8_t test_weights[] = {2, 2, 2, 2, 2};

    std::cout << std::setw(5) << "Cycle" << " | "
              << std::setw(6) << "Input" << " | "
              << std::setw(6) << "Weight" << " | "
              << std::setw(10) << "Accum" << " | Status\n";
    std::cout << std::string(60, '-') << "\n";

    for (int i = 0; i < 5; i++) {
        mac.executeCycle(test_inputs[i], test_weights[i], i == 0);
        int32_t accum = mac.getAccumulator();

        std::cout << std::setw(5) << i << " | "
                  << std::setw(6) << (int)test_inputs[i] << " | "
                  << std::setw(6) << (int)test_weights[i] << " | "
                  << std::setw(10) << accum << " | ";

        if (i < 3) {
            std::cout << "(filling)\n";
        } else {
            std::cout << "(accumulating)\n";
        }
    }

    int32_t final_accum = mac.getAccumulator();
    std::cout << "\nFinal accumulator: " << final_accum << "\n";
    std::cout << "Expected: 300 (sum of 10+20+30+40+50 = 150, × 2)\n";

    ASSERT_EQ(300, final_accum);

    TEST_END();
}

void test_complete_pipeline() {
    TEST_BEGIN("Complete Pipeline - Verbose Hardware Simulation");

    // Conv1 configuration (using first 108 MACs for detailed logging)
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
    std::cout << "  Input:        64×64×3\n";
    std::cout << "  Filters:      64×3×3\n";
    std::cout << "  Output:       64×64×64\n";
    std::cout << "  Scale factor: 0x" << std::hex << dequant_config.scale_factor << " (Q8.24 = 0.5)" << std::dec << "\n";
    std::cout << "  ReLU:         enabled\n\n";

    // Initialize components
    IndexGenerator index_gen(conv_config);
    MACStreamProvider macs(mac_config);
    Dequantization dequant(dequant_config);
    OutputStorage storage(output_config);
    VerboseLogger logger;

    // Generate first 108 MACs (4 complete pixels)
    std::cout << "Simulating first 108 MACs (4 pixels × 27 MACs/pixel)...\n";
    auto addresses = index_gen.generateFirstN(108);

    // Create test data
    std::vector<int8_t> input_data(64 * 64 * 3);
    std::vector<int8_t> weight_data(64 * 3 * 3 * 3);

    for (size_t i = 0; i < input_data.size(); i++) {
        input_data[i] = (i % 128);
    }
    for (size_t i = 0; i < weight_data.size(); i++) {
        weight_data[i] = ((i % 64) - 32);
    }

    // Simulate pipeline
    uint32_t cycle = 0;
    int pixel_count = 0;
    int outputs_generated = 0;

    std::cout << "\nDETAILED PIPELINE LOG:\n";
    std::cout << std::string(90, '-') << "\n\n";

    for (const auto& addr : addresses) {
        // Get input and weight values
        uint8_t input_val = input_data[addr.input_addr % input_data.size()] & 0xFF;
        uint8_t weight_val = weight_data[addr.weight_addr % weight_data.size()] & 0xFF;

        // Feed to 4 parallel MACs
        int8_t inputs[4] = {(int8_t)input_val, (int8_t)input_val,
                           (int8_t)input_val, (int8_t)input_val};
        int8_t weights[4];
        for (int i = 0; i < 4; i++) {
            weights[i] = weight_data[(addr.weight_addr + i) % weight_data.size()];
        }

        // Execute MACs
        auto mac_output = macs.executeCluster(inputs, weights, addr.tlast);

        // Log MAC operations
        for (int mac_id = 0; mac_id < 4; mac_id++) {
            logger.logMACOperation(cycle, mac_id, inputs[mac_id] & 0xFF,
                                  weights[mac_id] & 0xFF, mac_output.accum[mac_id]);
        }

        // If TLAST, process outputs
        if (addr.tlast) {
            pixel_count++;

            // Calculate output position
            uint16_t out_y = pixel_count / 64;
            uint16_t out_x = pixel_count % 64;

            // Process 4 output channels
            for (int oc = 0; oc < 4; oc++) {
                // Dequantize
                int32_t accum = mac_output.accum[oc];
                Dequantization::OutputStats dequant_stats;
                int8_t output_int8 = dequant.dequantizeScalar(accum, &dequant_stats);

                logger.logDequantOperation(cycle, accum, dequant_config.scale_factor,
                                          output_int8 & 0xFF);

                // Store output
                OutputStorage::OutputStats store_stats;
                storage.storeOutput(out_y, out_x, oc, output_int8, 0, &store_stats);

                logger.logOutputStore(cycle, store_stats.bram_addr,
                                     store_stats.byte_sel, output_int8 & 0xFF);

                outputs_generated++;
            }

            logger.logPixelComplete(cycle, out_y, out_x, pixel_count % 4);
        }

        cycle++;
    }

    // Print log (first 50 and last 10 entries)
    std::cout << "\n";
    logger.printLog(50, 10);

    std::cout << "\n";
    std::cout << std::string(90, '=') << "\n";
    std::cout << "PIPELINE SIMULATION SUMMARY\n";
    std::cout << std::string(90, '=') << "\n";
    std::cout << "Total cycles executed:      " << cycle << "\n";
    std::cout << "Total MACs processed:       " << addresses.size() << "\n";
    std::cout << "Pixels completed:           " << pixel_count << "\n";
    std::cout << "Outputs generated:          " << outputs_generated << "\n";
    std::cout << "Accumulators created:       " << (pixel_count * 4) << "\n";
    std::cout << "Log entries:                " << logger.getLog().size() << "\n";

    ASSERT_EQ(108U, addresses.size());
    ASSERT_EQ(4, pixel_count);
    ASSERT_EQ(16, outputs_generated);

    std::cout << "\n Complete pipeline test PASSED\n";

    TEST_END();
}

void test_pipeline_performance_metrics() {
    TEST_BEGIN("Complete Pipeline - Performance Metrics");

    IndexGenerator::ConvConfig conv_config = {
        64, 64, 3,  // input
        3, 3, 64,   // filter
        1, 1        // stride, padding
    };

    IndexGenerator index_gen(conv_config);

    uint32_t total_macs = 64 * 64 * 64 * 27;
    uint32_t total_pixels = 64 * 64 * 64;

    std::cout << "Conv1 Layer Performance:\n";
    std::cout << "  Total MACs:           " << total_macs << "\n";
    std::cout << "  Total output pixels:  " << total_pixels << "\n";
    std::cout << "  MACs per pixel:       27 (3×3×3)\n";
    std::cout << "  Parallel MACs:        4\n\n";

    std::cout << "Hardware Metrics:\n";
    std::cout << "  Clock frequency:      112 MHz\n";
    std::cout << "  MAC throughput:       4 MACs/cycle\n";
    std::cout << "  Peak throughput:      448 MACs/ns\n\n";

    // Calculate execution time
    uint32_t cycles_needed = total_macs / 4;  // 4 parallel MACs
    double exec_time_ms = (cycles_needed / 112.0e6) * 1000.0;

    std::cout << "Estimated Execution:\n";
    std::cout << "  Cycles needed:        " << cycles_needed << "\n";
    std::cout << "  Execution time:       " << std::fixed << std::setprecision(2)
              << exec_time_ms << " ms\n";
    std::cout << "  Throughput:           " << std::fixed << std::setprecision(2)
              << (total_macs / (exec_time_ms / 1000.0) / 1e9) << " GMAC/s\n";

    std::cout << "\nMemory Requirements:\n";
    std::cout << "  Input BRAM:           " << (64 * 64 * 3) << " bytes\n";
    std::cout << "  Weight BRAM:          " << (64 * 3 * 3 * 3) << " bytes\n";
    std::cout << "  Output BRAM:          " << (64 * 64 * 64) << " bytes = "
              << ((64 * 64 * 64 + 3) / 4) << " words\n";

    TEST_END();
}

int main() {
    std::cout << "\n";
    std::cout << "╔" << std::string(88, '=') << "╗\n";
    std::cout << "║" << std::string(88, ' ') << "║\n";
    std::cout << "║  Complete Pipeline C++ Test - Hardware-Comparable Verbose Output            ║\n";
    std::cout << "║  Includes: MAC Units, Dequantization, Output Storage                        ║\n";
    std::cout << "║" << std::string(88, ' ') << "║\n";
    std::cout << "╚" << std::string(88, '=') << "╝\n";

    test_mac_unit_only();
    test_complete_pipeline();
    test_pipeline_performance_metrics();

    TestFramework::instance().printSummary();

    return TestFramework::instance().allPassed() ? 0 : 1;
}
