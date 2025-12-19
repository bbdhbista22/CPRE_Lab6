#include "OutputStorage.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

/**
 * Constructor
 */
OutputStorage::OutputStorage(const Config& config)
    : config_(config), pixel_count_(0) {
    
    if (config.output_height == 0 || config.output_width == 0 || config.output_channels == 0) {
        throw std::invalid_argument("Invalid output dimensions");
    }
}

/**
 * Calculate output address
 */
OutputStorage::AddressInfo OutputStorage::calcOutputAddr(uint16_t out_y, uint16_t out_x,
                                                         uint16_t out_c) {
    // output_addr = base + ((out_y * output_width + out_x) * output_channels + out_c) / 4
    uint32_t linear_addr = ((uint32_t)out_y * config_.output_width + out_x) 
                          * config_.output_channels + out_c;
    
    AddressInfo info;
    info.word_addr = config_.output_base_addr + (linear_addr / 4);
    info.byte_sel = linear_addr % 4;
    
    return info;
}

/**
 * Insert byte into 32-bit word (little-endian)
 */
uint32_t OutputStorage::insertByte(uint32_t old_word, uint8_t new_byte, uint8_t byte_sel) {
    uint32_t mask = 0xFFFFFFFF ^ (0xFF << (byte_sel * 8));
    uint32_t new_word = old_word & mask;
    new_word |= ((uint32_t)new_byte << (byte_sel * 8));
    return new_word;
}

/**
 * Extract byte from 32-bit word
 */
uint8_t OutputStorage::extractByte(uint32_t word, uint8_t byte_sel) {
    return (uint8_t)((word >> (byte_sel * 8)) & 0xFF);
}

/**
 * Store single output value
 */
uint32_t OutputStorage::storeOutput(uint16_t out_y, uint16_t out_x, uint16_t out_c,
                                    int8_t value, uint32_t bram_data,
                                    OutputStats* stats) {
    // Validate coordinates
    if (out_y >= config_.output_height || out_x >= config_.output_width ||
        out_c >= config_.output_channels) {
        throw std::out_of_range("Output coordinates out of bounds");
    }

    // Calculate address
    AddressInfo addr_info = calcOutputAddr(out_y, out_x, out_c);

    // Perform read-modify-write
    uint32_t old_word = bram_data;
    uint32_t new_word = insertByte(old_word, (uint8_t)value, addr_info.byte_sel);

    // Record statistics if requested
    if (stats != nullptr) {
        stats->out_y = out_y;
        stats->out_x = out_x;
        stats->out_c = out_c;
        stats->bram_addr = addr_info.word_addr;
        stats->byte_sel = addr_info.byte_sel;
        stats->value = value;
        stats->old_word = old_word;
        stats->new_word = new_word;
    }

    return new_word;
}

/**
 * Process AXI-Stream input
 */
OutputStorage::BRAMUpdate OutputStorage::processStream(int8_t tdata, uint8_t tid, bool tlast,
                                                       const std::vector<uint32_t>& bram_contents,
                                                       OutputStats* stats) {
    // Calculate output position from pixel counter and TID
    uint32_t pixel_idx = pixel_count_;
    uint16_t out_y = pixel_idx / config_.output_width;
    uint16_t out_x = pixel_idx % config_.output_width;
    
    // TID (0-3) maps to output channel offset
    // For 64 output channels with 4 parallel MACs:
    // cycle 0: channels 0,1,2,3 (tid=0,1,2,3)
    // cycle 1: channels 4,5,6,7 (tid=0,1,2,3)
    // etc.
    uint16_t out_c = tid;  // Simple mapping for now (can be extended)

    // Clamp to valid bounds
    if (out_y >= config_.output_height || out_x >= config_.output_width) {
        return {0, 0};  // Invalid output
    }

    // Calculate address
    AddressInfo addr_info = calcOutputAddr(out_y, out_x, out_c);

    // Get current BRAM contents
    uint32_t old_word = (addr_info.word_addr < bram_contents.size()) 
                        ? bram_contents[addr_info.word_addr] : 0;

    // Perform RMW
    uint32_t new_word = insertByte(old_word, (uint8_t)tdata, addr_info.byte_sel);

    // Update pixel counter on TLAST
    if (tlast) {
        pixel_count_++;
    }

    // Record statistics
    if (stats != nullptr) {
        stats->out_y = out_y;
        stats->out_x = out_x;
        stats->out_c = out_c;
        stats->bram_addr = addr_info.word_addr;
        stats->byte_sel = addr_info.byte_sel;
        stats->value = tdata;
        stats->old_word = old_word;
        stats->new_word = new_word;
    }

    return {addr_info.word_addr, new_word};
}

/**
 * 2X2 max pooling
 */
int8_t OutputStorage::poolMax2x2(const std::vector<int8_t>& values) {
    if (values.size() != 4) {
        throw std::invalid_argument("Pooling requires exactly 4 values");
    }
    
    int8_t max_val = values[0];
    for (size_t i = 1; i < values.size(); i++) {
        if (values[i] > max_val) {
            max_val = values[i];
        }
    }
    
    return max_val;
}

/**
 * Verify output address sequence
 */
bool OutputStorage::verifyAddresses(const std::vector<uint32_t>& addresses) {
    if (addresses.empty()) {
        std::cerr << "ERROR: Empty address vector\n";
        return false;
    }

    uint32_t num_outputs = config_.output_height * config_.output_width * config_.output_channels;

    // Check that addresses cover expected output range
    uint32_t max_addr = config_.output_base_addr + (num_outputs + 3) / 4;

    for (size_t i = 0; i < addresses.size(); i++) {
        if (addresses[i] >= max_addr) {
            std::cerr << "ERROR: Address out of bounds at index " << i 
                      << ". Address: 0x" << std::hex << addresses[i] 
                      << ", Max: 0x" << max_addr << std::dec << "\n";
            return false;
        }
    }

    std::cout << "  Output storage address verification PASSED\n";
    std::cout << "  Total outputs: " << num_outputs << "\n";
    std::cout << "  BRAM words needed: " << (num_outputs + 3) / 4 << "\n";

    return true;
}
