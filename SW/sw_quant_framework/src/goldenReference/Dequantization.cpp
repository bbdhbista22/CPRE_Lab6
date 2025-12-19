#include "Dequantization.h"
#include <algorithm>
#include <cmath>

/**
 * Constructor
 */
Dequantization::Dequantization(const Config& config) : config_(config) {
}

/**
 * Set quantization parameters
 */
void Dequantization::setQuantParams(int32_t zero_point_in, 
                                    int32_t zero_point_out, 
                                    int32_t scale_factor) {
    config_.zero_point_in = zero_point_in;
    config_.zero_point_out = zero_point_out;
    config_.scale_factor = scale_factor;
}

/**
 * Fixed-point multiply: (value X scale_factor) >> 24
 * 
 * scale_factor is in Q8.24 format (8 integer bits + 24 fractional bits)
 * Example: 0x01000000 = 1.0 in Q8.24
 *          0x00800000 = 0.5 in Q8.24
 */
int32_t Dequantization::fixedPointMultiply(int32_t value, int32_t scale) {
    // Perform 64-bit multiplication to avoid overflow
    int64_t temp = (int64_t)value * scale;
    
    // Shift right by 24 bits to get back to integer + rounding
    // Add 0x00800000 (0.5 in Q8.24) before shifting to round
    int32_t result = (int32_t)((temp + 0x00800000) >> 24);
    
    return result;
}

/**
 * Saturate to int8 range [-128, 127]
 */
int8_t Dequantization::saturateToInt8(int32_t value) {
    if (value > 127) return 127;
    if (value < -128) return -128;
    return (int8_t)value;
}

/**
 * Dequantize with pipelined stages separated
 */
Dequantization::OutputStats Dequantization::dequantizePipelined(int32_t accumulator) {
    OutputStats stats = {};
    
    // Stage 0: Input accumulator
    stats.accum_before = accumulator;
    
    // Stage 1: Subtract zero-point
    stats.accum_after_zp = accumulator - config_.zero_point_in;
    
    // Stage 2: Multiply by scale factor (fixed-point Q8.24)
    // Note: scale_factor is Q8.24, so result is (value * scale) >> 24
    // Add 0x00800000 (0.5 in Q8.24) before shifting to round
    int64_t temp = (int64_t)stats.accum_after_zp * config_.scale_factor;
    stats.product = (int32_t)((temp + 0x00800000) >> 24);
    
    // Stage 3: Round (already done in fixed-point multiply above)
    // But we can separate it for clarity:
    // rounded = (product + 0.5) >> 24 - already done in multiply
    stats.rounded = stats.product;
    
    // Stage 4: Apply ReLU activation
    if (config_.enable_relu && stats.rounded < 0) {
        stats.rounded = 0;
    }
    stats.after_relu = stats.rounded;
    
    // Stage 5: Saturate to int8
    stats.final = saturateToInt8(stats.after_relu + config_.zero_point_out);
    
    return stats;
}

/**
 * Dequantize single scalar value
 */
int8_t Dequantization::dequantizeScalar(int32_t accumulator, OutputStats* stats) {
    OutputStats local_stats = dequantizePipelined(accumulator);
    
    if (stats != nullptr) {
        *stats = local_stats;
    }
    
    return local_stats.final;
}

/**
 * Dequantize vector of values
 */
std::vector<int8_t> Dequantization::dequantizeVector(const std::vector<int32_t>& accumulators,
                                                    std::vector<OutputStats>* stats) {
    std::vector<int8_t> results;
    results.reserve(accumulators.size());
    
    if (stats != nullptr) {
        stats->clear();
        stats->reserve(accumulators.size());
    }
    
    for (int32_t accum : accumulators) {
        OutputStats local_stats;
        int8_t result = dequantizeScalar(accum, &local_stats);
        results.push_back(result);
        
        if (stats != nullptr) {
            stats->push_back(local_stats);
        }
    }
    
    return results;
}
