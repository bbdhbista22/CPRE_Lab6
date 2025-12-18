#include "StagedMAC.h"
#include <stdexcept>

/**
 * StagedMAC Constructor
 */
StagedMAC::StagedMAC(const Config& config)
    : config_(config), current_accumulator_(0), cycle_count_(0) {
    
    // Initialize 3-stage pipeline
    pipeline_.resize(3);
    for (auto& stage : pipeline_) {
        stage.valid = false;
        stage.partial_sum = 0;
        stage.input = 0;
        stage.weight = 0;
        stage.product = 0;
    }
}

/**
 * Execute one cycle of the 3-stage pipeline
 */
StagedMAC::MACResult StagedMAC::executeCycle(int8_t input, int8_t weight, bool start_new_pixel) {
    if (start_new_pixel) {
        current_accumulator_ = 0;
    }

    // Shift pipeline stages and get output
    // Stage 2 (register) output
    MACResult result;
    result.cycle = cycle_count_;
    result.valid = pipeline_[2].valid;
    result.accumulator = current_accumulator_;

    // Move Stage 1 → Stage 2
    pipeline_[2] = pipeline_[1];

    // Accumulate at Stage 1: if Stage 0 has valid data, accumulate its product
    if (pipeline_[0].valid) {
        pipeline_[1].valid = true;
        current_accumulator_ += pipeline_[0].product;
    }

    // New input to Stage 0 (multiply)
    // Multiply (with zero-point adjustment)
    int32_t adj_input = (int32_t)input - config_.zero_point_in;
    int32_t adj_weight = (int32_t)weight - config_.zero_point_weight;
    int32_t product = adj_input * adj_weight;

    pipeline_[0].input = input;
    pipeline_[0].weight = weight;
    pipeline_[0].valid = true;
    pipeline_[0].product = product;
    pipeline_[0].partial_sum = current_accumulator_;

    cycle_count_++;
    return result;
}

/**
 * Flush pipeline
 */
int32_t StagedMAC::flushPipeline() {
    // Execute empty cycles to push data through pipeline
    executeCycle(0, 0, false);
    executeCycle(0, 0, false);
    executeCycle(0, 0, false);
    return current_accumulator_;
}

/**
 * Reset accumulator
 */
void StagedMAC::resetAccumulator() {
    current_accumulator_ = 0;
}

/**
 * MACStreamProvider Constructor
 */
MACStreamProvider::MACStreamProvider(const Config& config) : config_(config) {
    // Create 4 staged MAC units
    for (uint8_t i = 0; i < config.num_macs; i++) {
        StagedMAC::Config mac_config;
        mac_config.id = i;
        mac_config.zero_point_in = config.zero_point_in;
        mac_config.zero_point_weight = config.zero_point_weight;
        macs_.emplace_back(mac_config);
    }
}

/**
 * Execute one cycle across all 4 MACs
 */
MACStreamProvider::Output MACStreamProvider::executeCluster(
    const int8_t inputs[4], const int8_t weights[4], bool tlast) {
    
    Output output = {};
    output.valid = false;

    // Execute all 4 MACs in parallel
    for (uint8_t i = 0; i < config_.num_macs; i++) {
        StagedMAC::MACResult result = macs_[i].executeCycle(inputs[i], weights[i], false);
        if (tlast) {
            output.accum[i] = macs_[i].getAccumulator();
            output.valid = true;
            macs_[i].resetAccumulator();
        } else {
            output.accum[i] = result.accumulator;
        }
    }

    return output;
}

/**
 * Reset all accumulators
 */
void MACStreamProvider::resetAllAccumulators() {
    for (auto& mac : macs_) {
        mac.resetAccumulator();
    }
}
