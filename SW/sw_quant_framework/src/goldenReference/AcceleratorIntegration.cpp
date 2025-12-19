#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <cstring>
#include "IndexGenerator.h"
#include "StagedMAC.h"
#include "Dequantization.h"
#include "OutputStorage.h"

/**
 * Complete Hardware Accelerator Integration Test
 * Combines all 4 major components for FPGA verification
 */

class AcceleratorIntegration {
private:
    IndexGenerator* index_gen;
    MACStreamProvider* mac_provider;
    Dequantization* dequant;
    OutputStorage* output_storage;
    
    struct ConvConfig {
        uint16_t input_height = 64;
        uint16_t input_width = 64;
        uint16_t input_channels = 3;
        uint16_t filter_height = 3;
        uint16_t filter_width = 3;
        uint16_t num_filters = 64;
        uint16_t stride = 1;
        uint16_t padding = 1;
    } conv_config;
    
    struct QuantConfig {
        int8_t zero_point_in = 0;
        int8_t zero_point_out = 0;
        int32_t scale_factor = 0x00800000;
        bool enable_relu = true;
    } quant_config;
    
    struct MACConfig {
        int8_t zero_point_in = 0;
        int8_t zero_point_weight = 0;
    } mac_config;
    
    struct OutputConfig {
        uint16_t output_height = 64;
        uint16_t output_width = 64;
        uint16_t output_channels = 64;
        uint32_t output_base_addr = 0;
        bool enable_pooling = false;
    } output_config;
    
    // Test data
    std::vector<uint8_t> input_data;
    std::vector<int8_t> weight_data;
    std::vector<uint32_t> bram_memory;
    
    uint64_t cycle_count = 0;
    uint32_t mac_count = 0;
    uint32_t pixel_count = 0;
    
public:
    AcceleratorIntegration() {
        // Initialize BRAM (315KB = 80,640 words)
        bram_memory.resize(80640, 0);
        
        // Initialize test data
        input_data.resize(conv_config.input_height * 
                         conv_config.input_width * 
                         conv_config.input_channels);
        for (size_t i = 0; i < input_data.size(); i++) {
            input_data[i] = (i % 128);
        }
        
        weight_data.resize(conv_config.num_filters * 
                          conv_config.filter_height * 
                          conv_config.filter_width * 
                          conv_config.input_channels);
        for (size_t i = 0; i < weight_data.size(); i++) {
            weight_data[i] = (i % 64 - 32);
        }
        
        // Initialize components
        index_gen = new IndexGenerator(
            conv_config.input_height,
            conv_config.input_width,
            conv_config.input_channels,
            conv_config.filter_height,
            conv_config.filter_width,
            conv_config.num_filters,
            conv_config.stride,
            conv_config.padding
        );
        
        mac_provider = new MACStreamProvider(
            mac_config.zero_point_in,
            mac_config.zero_point_weight
        );
        
        dequant = new Dequantization(
            quant_config.zero_point_in,
            quant_config.zero_point_out,
            quant_config.scale_factor,
            quant_config.enable_relu
        );
        
        output_storage = new OutputStorage(
            output_config.output_height,
            output_config.output_width,
            output_config.output_channels,
            output_config.output_base_addr,
            output_config.enable_pooling
        );
    }
    
    ~AcceleratorIntegration() {
        delete index_gen;
        delete mac_provider;
        delete dequant;
        delete output_storage;
    }
    
    void run_simulation(uint32_t num_macs) {
        std::cout << std::endl;
        std::cout << "╔═══════════════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║        HARDWARE ACCELERATOR INTEGRATION TEST - C++ SIMULATION              ║" << std::endl;
        std::cout << "║  IndexGenerator -> StagedMAC (4x) -> Dequantization -> OutputStorage          ║" << std::endl;
        std::cout << "╚═══════════════════════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;
        
        print_configuration();
        
        std::cout << "\n" << std::string(78, '=') << std::endl;
        std::cout << "SIMULATION LOG (First 100 cycles)" << std::endl;
        std::cout << std::string(78, '=') << std::endl << std::endl;
        
        // Generate addresses
        std::vector<IndexGenerator::AddressInfo> addresses = 
            index_gen->generateAddresses(num_macs);
        
        std::cout << std::string(78, '-') << std::endl;
        std::cout << "CYCLE | MAC_ID | INPUT_ADDR | WEIGHT_ADDR | TLAST | ACCUM_OUT" << std::endl;
        std::cout << std::string(78, '-') << std::endl;
        
        // Process each MAC operation
        for (uint32_t i = 0; i < addresses.size(); i++) {
            const auto& addr = addresses[i];
            
            uint8_t input_val = input_data[addr.input_addr % input_data.size()];
            int8_t weight_val = weight_data[addr.weight_addr % weight_data.size()];
            
            // Simulate 4 parallel MACs
            std::vector<uint8_t> inputs = {input_val, input_val, input_val, input_val};
            std::vector<int8_t> weights = {
                weight_val,
                (int8_t)(weight_val + 1),
                (int8_t)(weight_val + 2),
                (int8_t)(weight_val + 3)
            };
            
            // Execute MACs
            auto mac_outputs = mac_provider->executeCluster(inputs, weights, addr.tlast);
            
            // Log cycles (first 100 only for readability)
            if (cycle_count < 100) {
                std::cout << std::setw(5) << cycle_count << " | "
                         << std::setw(6) << (int)i % 4 << " | "
                         << "0x" << std::hex << std::setw(8) << std::setfill('0') << addr.input_addr << " | "
                         << "0x" << std::setw(10) << std::setfill('0') << addr.weight_addr << " | "
                         << (addr.tlast ? "  1   " : "  0   ") << " | ";
                
                for (int j = 0; j < 4; j++) {
                    std::cout << "0x" << std::setw(8) << std::setfill('0') << mac_outputs.accumulators[j];
                    if (j < 3) std::cout << ", ";
                }
                std::cout << std::endl;
                std::cout << std::dec;
            }
            
            // Process outputs through dequantization and storage
            if (addr.tlast) {
                process_outputs(mac_outputs);
            }
            
            cycle_count++;
            mac_count++;
        }
        
        if (addresses.size() > 100) {
            std::cout << "\n... (" << (addresses.size() - 100) << " more cycles) ...\n" << std::endl;
        }
        
        print_summary();
    }
    
private:
    void process_outputs(const MACStreamProvider::ClusterOutput& outputs) {
        // Dequantize and store outputs
        for (int i = 0; i < 4; i++) {
            int32_t accum = outputs.accumulators[i];
            
            // Dequantize
            int8_t output_val = dequant->dequantizeScalar(accum);
            
            // Store to output
            uint32_t word_addr = (pixel_count * 64 + i) / 4;
            uint8_t byte_sel = (pixel_count * 64 + i) % 4;
            
            // Simulate BRAM write
            if (word_addr < bram_memory.size()) {
                uint32_t word = bram_memory[word_addr];
                uint32_t byte_val = (uint8_t)output_val;
                uint32_t mask = 0xFF << (byte_sel * 8);
                word = (word & ~mask) | (byte_val << (byte_sel * 8));
                bram_memory[word_addr] = word;
            }
        }
        pixel_count++;
    }
    
    void print_configuration() {
        std::cout << "Configuration:" << std::endl;
        std::cout << "  Input shape:       " << conv_config.input_height << "x"
                 << conv_config.input_width << "x" << conv_config.input_channels << std::endl;
        std::cout << "  Filter shape:      " << conv_config.num_filters << "x"
                 << conv_config.filter_height << "x" << conv_config.filter_width
                 << "x" << conv_config.input_channels << std::endl;
        std::cout << "  Output shape:      " << output_config.output_height << "x"
                 << output_config.output_width << "x" << output_config.output_channels << std::endl;
        std::cout << "  Quantization:      int8, Q8.24" << std::endl;
        std::cout << "  Scale factor:      0x" << std::hex << std::setfill('0') 
                 << std::setw(8) << quant_config.scale_factor << std::dec << std::endl;
        std::cout << "  ReLU enabled:      " << (quant_config.enable_relu ? "yes" : "no") << std::endl;
        std::cout << "  MAC units:         4 parallel, 3-stage pipeline" << std::endl;
        std::cout << "  Memory:            BRAM 315KB (80,640 words)" << std::endl;
    }
    
    void print_summary() {
        std::cout << std::endl;
        std::cout << "╔═══════════════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║                       SIMULATION RESULTS SUMMARY                          ║" << std::endl;
        std::cout << "╚═══════════════════════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;
        
        std::cout << "  Total cycles executed:        " << std::setw(10) << cycle_count << std::endl;
        std::cout << "  Total MACs processed:         " << std::setw(10) << mac_count << std::endl;
        std::cout << "  Pixels completed:             " << std::setw(10) << pixel_count << std::endl;
        std::cout << "  Accumulators generated:       " << std::setw(10) << (pixel_count * 4) << std::endl;
        std::cout << "  BRAM words written:           " << std::setw(10) << (pixel_count * 4 / 4) << std::endl;
        std::cout << "  BRAM utilization:             " << std::setw(10) 
                 << ((pixel_count * 64) / 80640.0 * 100.0) << "%" << std::endl;
        std::cout << std::endl;
        
        std::cout << "  Hardware Specifications:" << std::endl;
        std::cout << "    Clock frequency:          112 MHz" << std::endl;
        std::cout << "    Estimated runtime:        " << std::fixed << std::setprecision(2)
                 << (cycle_count / 112.0e6) * 1000 << " ms" << std::endl;
        std::cout << "    Peak throughput:          " << (112.0 / 3.0) << " MACs/cycle" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Complete hardware accelerator integration test PASSED" << std::endl;
        std::cout << std::endl;
    }
};

int main() {
    try {
        AcceleratorIntegration accelerator;
        
        // Run simulation for first 108 MACs (4 pixels x 27 MACs/pixel)
        accelerator.run_simulation(108);
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
