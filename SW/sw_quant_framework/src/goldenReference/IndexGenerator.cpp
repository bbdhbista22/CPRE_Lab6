#include "IndexGenerator.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <algorithm>

/**
 * Constructor - Initialize configuration and compute derived values
 */
IndexGenerator::IndexGenerator(const ConvConfig& config,
                               uint32_t input_base_addr,
                               uint32_t weight_base_addr,
                               uint16_t tile_size)
    : conv_config_(config),
      input_base_addr_(input_base_addr),
      weight_base_addr_(weight_base_addr) {
    
    // Validate configuration
    if (config.input_height == 0 || config.input_width == 0 || config.input_channels == 0) {
        throw std::invalid_argument("Invalid input dimensions");
    }
    if (config.filter_height == 0 || config.filter_width == 0 || config.num_filters == 0) {
        throw std::invalid_argument("Invalid filter dimensions");
    }

    // Compute output dimensions
    // output_h = floor((input_h - filter_h + 2*padding) / stride) + 1
    conv_config_.output_height = (config.input_height - config.filter_height + 2 * config.padding) 
                                 / config.stride + 1;
    conv_config_.output_width = (config.input_width - config.filter_width + 2 * config.padding) 
                                / config.stride + 1;

    // Compute MACs per pixel = filter_h * filter_w * input_channels
    conv_config_.macs_per_pixel = (uint32_t)config.filter_height * config.filter_width * config.input_channels;

    // Compute tiling configuration
    tile_config_.tile_size = tile_size;
    tile_config_.tiles_per_row = (conv_config_.output_width + tile_size - 1) / tile_size;  // ceil division
    tile_config_.tiles_per_col = (conv_config_.output_height + tile_size - 1) / tile_size;
    tile_config_.total_tiles = tile_config_.tiles_per_row * tile_config_.tiles_per_col;
}

/**
 * Calculate input BRAM address
 * 
 * input_addr = input_base + (in_y * input_width + in_x) * input_channels + ic
 */
uint32_t IndexGenerator::calcInputAddr(uint16_t in_y, uint16_t in_x, uint8_t ic) {
    uint32_t offset = ((uint32_t)in_y * conv_config_.input_width + in_x) 
                      * conv_config_.input_channels + ic;
    return input_base_addr_ + offset;
}

/**
 * Calculate weight BRAM address
 * 
 * Weights are stored per-output-channel:
 * weight_addr = weight_base + (oc * filter_h * filter_w * input_c +
 *                              fy * filter_w * input_c +
 *                              fx * input_c + ic)
 */
uint32_t IndexGenerator::calcWeightAddr(uint8_t oc, uint8_t fy, uint8_t fx, uint8_t ic) {
    uint32_t offset = ((uint32_t)oc * conv_config_.filter_height * conv_config_.filter_width * conv_config_.input_channels
                      + (uint32_t)fy * conv_config_.filter_width * conv_config_.input_channels
                      + (uint32_t)fx * conv_config_.input_channels
                      + ic);
    return weight_base_addr_ + offset;
}

/**
 * Calculate input position from output position and filter offset
 * Handles padding by returning false for positions outside actual input
 */
bool IndexGenerator::calcInputPosition(uint16_t out_y, uint16_t out_x,
                                       uint8_t fy, uint8_t fx,
                                       uint16_t& in_y, uint16_t& in_x) {
    // Calculate top-left corner of input window for this output position
    // in_y_start = out_y * stride - padding
    // Then add filter offset: in_y = in_y_start + fy
    
    int32_t temp_in_y = (int32_t)out_y * conv_config_.stride - (int32_t)conv_config_.padding + fy;
    int32_t temp_in_x = (int32_t)out_x * conv_config_.stride - (int32_t)conv_config_.padding + fx;

    // Check if within valid input bounds
    if (temp_in_y < 0 || temp_in_y >= (int32_t)conv_config_.input_height ||
        temp_in_x < 0 || temp_in_x >= (int32_t)conv_config_.input_width) {
        return false;  // This is a padding position
    }

    in_y = (uint16_t)temp_in_y;
    in_x = (uint16_t)temp_in_x;
    return true;
}

/**
 * Generate all addresses for complete layer
 * 
 * Loop nesting (row-stationary dataflow):
 *   for oc_batch = 0 to (num_filters / 4 - 1):  # Process 4 output channels at a time
 *     for tile_id = 0 to total_tiles-1:
 *       for out_y_in_tile = 0 to tile_size-1:
 *         for out_x_in_tile = 0 to tile_size-1:
 *           actual_out_y = tile_row * tile_size + out_y_in_tile
 *           actual_out_x = tile_col * tile_size + out_x_in_tile
 *           if (actual_out_y < output_height AND actual_out_x < output_width):
 *             for oc_offset = 0 to 3:  # 4 parallel MACs
 *               oc = oc_batch * 4 + oc_offset
 *               if (oc < num_filters):
 *                 for fy = 0 to filter_height-1:
 *                   for fx = 0 to filter_width-1:
 *                     for ic = 0 to input_channels-1:
 *                       emit (input_addr, weight_addr, tlast=(ic==input_channels-1), oc=oc%4)
 */
std::vector<IndexGenerator::Address> IndexGenerator::generateAllAddresses() {
    std::vector<Address> addresses;

    uint32_t num_output_channels = conv_config_.num_filters;
    uint16_t output_height = conv_config_.output_height;
    uint16_t output_width = conv_config_.output_width;
    uint8_t filter_height = conv_config_.filter_height;
    uint8_t filter_width = conv_config_.filter_width;
    uint8_t input_channels = conv_config_.input_channels;

    // Process output channels in groups of 4 (4 parallel MACs)
    for (uint8_t oc_batch = 0; oc_batch < (num_output_channels + 3) / 4; oc_batch++) {
        
        // Process each tile
        for (uint16_t tile_id = 0; tile_id < tile_config_.total_tiles; tile_id++) {
            uint16_t tile_row = tile_id / tile_config_.tiles_per_row;
            uint16_t tile_col = tile_id % tile_config_.tiles_per_row;
            
            // Process each pixel within tile
            for (uint16_t out_y_in_tile = 0; out_y_in_tile < tile_config_.tile_size; out_y_in_tile++) {
                for (uint16_t out_x_in_tile = 0; out_x_in_tile < tile_config_.tile_size; out_x_in_tile++) {
                    
                    uint16_t actual_out_y = tile_row * tile_config_.tile_size + out_y_in_tile;
                    uint16_t actual_out_x = tile_col * tile_config_.tile_size + out_x_in_tile;
                    
                    // Skip if outside output bounds
                    if (actual_out_y >= output_height || actual_out_x >= output_width) {
                        continue;
                    }

                    // Process 4 output channels in parallel (4 MACs)
                    for (uint8_t oc_offset = 0; oc_offset < 4; oc_offset++) {
                        uint8_t oc = oc_batch * 4 + oc_offset;
                        
                        // Skip if this output channel doesn't exist
                        if (oc >= num_output_channels) {
                            continue;
                        }

                        // Generate MAC operations for this output pixel and channel
                        // Iterate filter positions and input channels
                        for (uint8_t fy = 0; fy < filter_height; fy++) {
                            for (uint8_t fx = 0; fx < filter_width; fx++) {
                                for (uint8_t ic = 0; ic < input_channels; ic++) {
                                    
                                    // Calculate input position (handles padding)
                                    uint16_t in_y = 0, in_x = 0;
                                    calcInputPosition(actual_out_y, actual_out_x, fy, fx, in_y, in_x);

                                    // Always generate address (for padding regions, input reads zero from BRAM initialization)
                                    uint32_t input_addr = calcInputAddr(in_y, in_x, ic);
                                    uint32_t weight_addr = calcWeightAddr(oc, fy, fx, ic);

                                    Address addr;
                                    addr.input_addr = input_addr;
                                    addr.weight_addr = weight_addr;
                                    // TLAST asserted only on LAST MAC of this output pixel
                                    // = last input channel (ic == input_channels-1) AND
                                    //   last filter position (fy == filter_height-1 and fx == filter_width-1)
                                    addr.tlast = (ic == input_channels - 1 && 
                                                  fy == filter_height - 1 && 
                                                  fx == filter_width - 1);
                                    addr.oc = oc_offset;  // Which of 4 parallel MACs (0-3)

                                    addresses.push_back(addr);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return addresses;
}

/**
 * Generate first N addresses (for quick validation)
 */
std::vector<IndexGenerator::Address> IndexGenerator::generateFirstN(uint32_t n) {
    std::vector<Address> all_addresses = generateAllAddresses();
    
    if (n >= all_addresses.size()) {
        return all_addresses;
    }
    
    return std::vector<Address>(all_addresses.begin(), all_addresses.begin() + n);
}

/**
 * Verify address sequence correctness
 */
bool IndexGenerator::verifyAddresses(const std::vector<Address>& addresses) {
    if (addresses.empty()) {
        std::cerr << "ERROR: Empty address vector\n";
        return false;
    }

    uint32_t expected_total_macs = (uint32_t)conv_config_.output_height * conv_config_.output_width 
                                   * conv_config_.num_filters * conv_config_.macs_per_pixel;

    // Check total count
    if (addresses.size() != expected_total_macs) {
        std::cerr << "ERROR: Total MACs mismatch. Expected: " << expected_total_macs 
                  << ", Got: " << addresses.size() << "\n";
        return false;
    }

    // Check TLAST placement (every macs_per_pixel operations)
    uint32_t mac_count = 0;
    for (size_t i = 0; i < addresses.size(); i++) {
        mac_count++;
        bool expected_tlast = (mac_count % conv_config_.macs_per_pixel) == 0;
        
        if (addresses[i].tlast != expected_tlast) {
            std::cerr << "ERROR: TLAST mismatch at address " << i 
                      << ". Expected: " << expected_tlast 
                      << ", Got: " << addresses[i].tlast << "\n";
            return false;
        }
    }

    // Check address bounds
    uint32_t max_input_addr = input_base_addr_ + conv_config_.input_height * conv_config_.input_width 
                              * conv_config_.input_channels;
    uint32_t max_weight_addr = weight_base_addr_ + conv_config_.num_filters * conv_config_.filter_height 
                               * conv_config_.filter_width * conv_config_.input_channels;

    for (size_t i = 0; i < addresses.size(); i++) {
        if (addresses[i].input_addr >= max_input_addr) {
            std::cerr << "ERROR: Input address out of bounds at index " << i 
                      << ". Address: 0x" << std::hex << addresses[i].input_addr 
                      << ", Max: 0x" << max_input_addr << std::dec << "\n";
            return false;
        }
        if (addresses[i].weight_addr >= max_weight_addr) {
            std::cerr << "ERROR: Weight address out of bounds at index " << i 
                      << ". Address: 0x" << std::hex << addresses[i].weight_addr 
                      << ", Max: 0x" << max_weight_addr << std::dec << "\n";
            return false;
        }
    }

    // Check output channel index (0-3)
    for (size_t i = 0; i < addresses.size(); i++) {
        if (addresses[i].oc > 3) {
            std::cerr << "ERROR: Invalid output channel at index " << i 
                      << ". OC: " << (int)addresses[i].oc << "\n";
            return false;
        }
    }

    std::cout << "  Address verification PASSED\n";
    std::cout << "  Total MACs: " << addresses.size() << "\n";
    std::cout << "  Expected: " << expected_total_macs << "\n";
    std::cout << "  TLAST placement: CORRECT (every " << conv_config_.macs_per_pixel << " MACs)\n";
    std::cout << "  Address bounds: OK\n";

    return true;
}
