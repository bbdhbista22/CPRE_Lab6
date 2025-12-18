#ifndef STAGED_MAC_H
#define STAGED_MAC_H

#include <cstdint>
#include <vector>

/**
 * StagedMAC - C++ Reference Implementation
 * 
 * 3-stage pipelined multiply-accumulate unit:
 * Stage 1: Multiply (input × weight)
 * Stage 2: Accumulate (partial_sum + product)
 * Stage 3: Register (output to next stage)
 * 
 * Throughput: 1 MAC per cycle (after pipeline fill)
 * Latency: 3 cycles (multiply → accumulate → register)
 * 
 * This matches the Lab 3 staged_mac FPGA implementation.
 */
class StagedMAC {
public:
    /**
     * Configuration
     */
    struct Config {
        uint32_t id;              ///< MAC unit ID (0-3)
        int32_t zero_point_in;    ///< Zero-point for inputs
        int32_t zero_point_weight; ///< Zero-point for weights
    };

    /**
     * Pipeline stage data
     */
    struct PipelineStage {
        int32_t partial_sum;      ///< Running accumulator
        int8_t input;             ///< Input activation
        int8_t weight;            ///< Weight value
        int32_t product;          ///< Multiply result
        bool valid;               ///< Stage contains valid data
    };

    /**
     * MAC operation result
     */
    struct MACResult {
        uint32_t cycle;           ///< Cycle number
        int32_t accumulator;      ///< Output accumulator value
        bool valid;               ///< Result is valid
    };

    /**
     * Constructor
     * 
     * @param config MAC configuration
     */
    explicit StagedMAC(const Config& config);

    /**
     * Execute single MAC operation with pipelining
     * 
     * Pipeline stages:
     * 1. MULTIPLY: partial_sum_in = previous_sum; product = (input - zp_in) × (weight - zp_w)
     * 2. ACCUMULATE: accum = product + partial_sum_in
     * 3. REGISTER: hold accumulator for output
     * 
     * @param input int8 input activation
     * @param weight int8 weight value
     * @param start_new_pixel If true, reset accumulator for new output pixel
     * @return Result with accumulator (may be from previous operation)
     */
    MACResult executeCycle(int8_t input, int8_t weight, bool start_new_pixel = false);

    /**
     * Flush pipeline and get final result
     * 
     * Returns the accumulated value after all stages complete
     * 
     * @return Final accumulator value
     */
    int32_t flushPipeline();

    /**
     * Reset accumulator (for new output pixel)
     */
    void resetAccumulator();

    /**
     * Get current pipeline state (for debugging)
     */
    const std::vector<PipelineStage>& getPipelineState() const {
        return pipeline_;
    }

    /**
     * Get configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * Get current accumulator value
     */
    int32_t getAccumulator() const { return current_accumulator_; }

private:
    Config config_;
    std::vector<PipelineStage> pipeline_;  ///< 3 pipeline stages
    int32_t current_accumulator_;
    uint32_t cycle_count_;
};

/**
 * MACStreamProvider - Orchestrates 4 parallel StagedMAC units
 * 
 * Manages 4 independent MAC pipelines, each computing one output channel
 * from the same input activation and different weight values.
 * 
 * When all 4 MACs complete (TLAST asserted), outputs are fed to Dequantization.
 */
class MACStreamProvider {
public:
    /**
     * Configuration
     */
    struct Config {
        uint8_t num_macs;         ///< Number of parallel MACs (typically 4)
        int32_t zero_point_in;
        int32_t zero_point_weight;
    };

    /**
     * Output from MAC cluster
     */
    struct Output {
        int32_t accum[4];         ///< 4 accumulator values
        bool valid;               ///< All 4 are valid
        uint8_t mac_id;           ///< Which set of 4 (0-15 for 64 channels)
    };

    /**
     * Constructor
     */
    explicit MACStreamProvider(const Config& config);

    /**
     * Execute one cycle across all 4 MACs
     * 
     * @param inputs Array of 4 input values (one per MAC)
     * @param weights Array of 4 weight values (one per MAC)
     * @param tlast If true, complete this pixel and output accumulators
     * @return Output from 4 MACs (valid if tlast was set)
     */
    Output executeCluster(const int8_t inputs[4], const int8_t weights[4], bool tlast);

    /**
     * Reset all accumulators for new pixel
     */
    void resetAllAccumulators();

    /**
     * Get MAC by ID
     */
    const StagedMAC& getMAC(uint8_t id) const { return macs_[id]; }

private:
    Config config_;
    std::vector<StagedMAC> macs_;
};

#endif // STAGED_MAC_H
