#ifndef DEQUANTIZATION_H
#define DEQUANTIZATION_H

#include <cstdint>
#include <vector>

/**
 * Dequantization - C++ Reference Implementation
 * 
 * Converts accumulated int32 multiply-accumulate results to int8 quantized outputs.
 * This is a 4-stage pipeline:
 * 1. Multiply by scale factor (fixed-point Q8.24)
 * 2. Round to nearest integer (add 0.5)
 * 3. Apply ReLU activation
 * 4. Saturate to int8 range [-128, 127]
 * 
 * This class serves as a golden reference for VHDL hardware validation.
 */
class Dequantization {
public:
    /**
     * Configuration for dequantization
     */
    struct Config {
        // Quantization parameters
        int32_t zero_point_in;    ///< Zero-point offset for input accumulator
        int32_t zero_point_out;   ///< Zero-point offset for output (typically 0 or -128)
        int32_t scale_factor;     ///< Scale factor in Q8.24 fixed-point format
        bool enable_relu;         ///< Enable ReLU activation (zero negative values)
        bool enable_batch_norm;   ///< Enable batch normalization (currently unused)
    };

    /**
     * Per-output statistics for debugging/validation
     */
    struct OutputStats {
        int32_t accum_before;     ///< Accumulator value before dequantization
        int32_t accum_after_zp;   ///< After zero-point subtraction
        int32_t product;          ///< Multiply result (before rounding)
        int32_t rounded;          ///< After rounding
        int32_t after_relu;       ///< After ReLU
        int8_t  final;            ///< Final saturated int8 output
    };

    /**
     * Constructor
     * 
     * @param config Dequantization configuration
     */
    explicit Dequantization(const Config& config);

    /**
     * Dequantize single accumulator value
     * 
     * Pipeline stages:
     * 1. Subtract zero-point: accum' = accum - zero_point_in
     * 2. Multiply by scale: product = accum' X scale_factor (Q8.24 fixed-point)
     *    Note: scale_factor is in Q8.24, so result is (value << 24) / scale_factor
     * 3. Round to nearest: rounded = (product + 0.5) >> 24
     * 4. Apply ReLU: if enable_relu && rounded < 0: rounded = 0
     * 5. Saturate to int8: result = clamp(rounded, -128, 127)
     * 
     * @param accumulator int32 accumulator value
     * @param stats [optional] Output statistics for verification
     * @return int8 dequantized and saturated value
     */
    int8_t dequantizeScalar(int32_t accumulator, OutputStats* stats = nullptr);

    /**
     * Dequantize vector of accumulators
     * 
     * @param accumulators Vector of int32 accumulator values
     * @param stats [optional] Vector to store per-value statistics
     * @return Vector of int8 outputs
     */
    std::vector<int8_t> dequantizeVector(const std::vector<int32_t>& accumulators,
                                        std::vector<OutputStats>* stats = nullptr);

    /**
     * Dequantize with pipeline stages separated (for cycle-by-cycle comparison)
     * 
     * Useful for verifying multi-stage pipeline behavior
     * 
     * @param accumulator Input value
     * @return OutputStats with all intermediate values
     */
    OutputStats dequantizePipelined(int32_t accumulator);

    /**
     * Get current configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * Set quantization parameters (for batch normalization fusion)
     * 
     * @param zero_point_in New input zero-point
     * @param zero_point_out New output zero-point
     * @param scale_factor New scale factor (Q8.24 format)
     */
    void setQuantParams(int32_t zero_point_in, int32_t zero_point_out, int32_t scale_factor);

private:
    Config config_;

    /**
     * Fixed-point multiply: (value X scale_factor) >> 24
     * 
     * scale_factor is in Q8.24 format (signed 32-bit with 24 fractional bits)
     * Example: scale_factor = 0x0100000 means 1.0 in Q8.24
     * 
     * @param value Input value
     * @param scale Scale factor in Q8.24
     * @return Scaled value (rounded)
     */
    int32_t fixedPointMultiply(int32_t value, int32_t scale);

    /**
     * Saturate value to int8 range
     */
    int8_t saturateToInt8(int32_t value);
};

#endif // DEQUANTIZATION_H
