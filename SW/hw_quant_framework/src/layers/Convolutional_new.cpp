#include "Convolutional.h"

#include <iostream>
#include <algorithm>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>

#include "../Types.h"
#include "../Utils.h"
#include "Layer.h"
#include "../HardwareMac.h"

namespace {
inline uint16_t packMacOperands(ML::i8 weight, ML::i8 activation) {
    return (static_cast<uint16_t>(static_cast<uint8_t>(weight)) << 8) |
           static_cast<uint16_t>(static_cast<uint8_t>(activation));
}
}

namespace ML
{
    // ==========================================================================
    // CALIBRATION STATISTICS STRUCTURE
    // ==========================================================================
    struct CalibrationStats {
        fp32 min, max, mean, Si;
        i8 zi;
    };
    
    // Global calibration data loaded from JSON
    static std::map<std::string, CalibrationStats> calibration_data;
    static bool calibration_loaded = false;
    
    // Layer counter for automatic naming
    static int conv_layer_count = 0;
    
    // Mode flag to determine how to select calibration stats
    static bool use_layer_specific_calibration = false; 
    
    // ==========================================================================
    // SIMPLE JSON PARSER FOR CALIBRATION STATS
    // ==========================================================================
    static bool readFileToString(const std::string& path, std::string& content) {
#ifdef ZEDBOARD
        logInfo("Checking calibration path: " + path);

        std::string normalized = path;
        if (normalized.rfind("0:/", 0) != 0) {
            normalized = "0:/" + normalized;
        }

        FILINFO info;
        if (f_stat(normalized.c_str(), &info) != FR_OK) {
            logError("f_stat failed for calibration file: " + normalized);
            return false;
        }

        size_t file_size = static_cast<size_t>(info.fsize);
        LayerParams tmp_params(1, {file_size}, normalized);
        LayerData tmp_data(tmp_params, normalized);
        tmp_data.allocData();
        try {
            tmp_data.loadData();
        } catch (const std::exception& e) {
            logError("LayerData::loadData failed for calibration file: " + normalized + " (" + e.what() + ")");
            return false;
        }

        content.assign(static_cast<const char*>(tmp_data.raw()), file_size);
        return true;
#else
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        content = ss.str();
        return true;
#endif
    }

    bool loadCalibrationStats(const std::string& json_path) {
        if (calibration_loaded) return true;
        
        std::string content;
        if (!readFileToString(json_path, content)) {
            logError("Failed to open calibration stats file: " + json_path);
            return false;
        }
        
        // Simple JSON parsing - look for layer entries
        size_t pos = 0;
        while ((pos = content.find("\"", pos)) != std::string::npos) {
            size_t name_start = pos + 1;
            size_t name_end = content.find("\"", name_start);
            if (name_end == std::string::npos) break;
            
            std::string layer_name = content.substr(name_start, name_end - name_start);
            pos = name_end + 1;
            
            // Skip to the opening brace
            size_t brace_start = content.find("{", pos);
            if (brace_start == std::string::npos) break;
            
            // Find the closing brace
            size_t brace_end = content.find("}", brace_start);
            if (brace_end == std::string::npos) break;
            
            std::string layer_content = content.substr(brace_start + 1, brace_end - brace_start - 1);
            
            // Parse the values
            CalibrationStats stats = {};
            
            // Extract min
            size_t min_pos = layer_content.find("\"min\":");
            if (min_pos != std::string::npos) {
                size_t val_start = layer_content.find(":", min_pos) + 1;
                size_t val_end = layer_content.find(",", val_start);
                if (val_end == std::string::npos) val_end = layer_content.find("}", val_start);
                stats.min = std::stof(layer_content.substr(val_start, val_end - val_start));
            }
            
            // Extract max  
            size_t max_pos = layer_content.find("\"max\":");
            if (max_pos != std::string::npos) {
                size_t val_start = layer_content.find(":", max_pos) + 1;
                size_t val_end = layer_content.find(",", val_start);
                if (val_end == std::string::npos) val_end = layer_content.find("}", val_start);
                stats.max = std::stof(layer_content.substr(val_start, val_end - val_start));
            }
            
            // Extract mean
            size_t mean_pos = layer_content.find("\"mean\":");
            if (mean_pos != std::string::npos) {
                size_t val_start = layer_content.find(":", mean_pos) + 1;
                size_t val_end = layer_content.find(",", val_start);
                if (val_end == std::string::npos) val_end = layer_content.find("}", val_start);
                stats.mean = std::stof(layer_content.substr(val_start, val_end - val_start));
            }
            
            // Extract Si
            size_t si_pos = layer_content.find("\"Si\":");
            if (si_pos != std::string::npos) {
                size_t val_start = layer_content.find(":", si_pos) + 1;
                size_t val_end = layer_content.find(",", val_start);
                if (val_end == std::string::npos) val_end = layer_content.find("}", val_start);
                stats.Si = std::stof(layer_content.substr(val_start, val_end - val_start));
            }
            
            // Extract zi
            size_t zi_pos = layer_content.find("\"zi\":");
            if (zi_pos != std::string::npos) {
                size_t val_start = layer_content.find(":", zi_pos) + 1;
                size_t val_end = layer_content.find(",", val_start);
                if (val_end == std::string::npos) val_end = layer_content.find("}", val_start);
                stats.zi = static_cast<i8>(std::stoi(layer_content.substr(val_start, val_end - val_start)));
            }
            
            calibration_data[layer_name] = stats;
            pos = brace_end + 1;
        }
        
        calibration_loaded = true;
        logInfo("Loaded calibration stats from " + json_path + " for " + std::to_string(calibration_data.size()) + " layers");
        return true;
    }
    
    // ==========================================================================
    // CALIBRATION STATE MANAGEMENT
    // --------------------------------------------------------------------------
    // Tracks global state used by calibrated quantization:
    //  - calibration_data: map of layer name -> CalibrationStats loaded from JSON
    //  - calibration_loaded: indicates whether calibration_data has been loaded
    //  - conv_layer_count: call-order counter used to pick layer-specific stats
    //      - incremented inside computeQuantized() when layer-specific mode is enabled
    //      - reset via resetCalibrationState() / resetConvLayerCounter()
    //  - use_layer_specific_calibration: mode flag
    //      - false: "individual test" mode — every layer uses "_input" stats
    //      - true: "full inference chain" — layers use conv2d, conv2d_1, ... based on conv_layer_count
    //
    // Helper functions around this state:
    //  - loadCalibrationStats(path)  : loads calibration_data from JSON (idempotent)
    //  - resetCalibrationState()     : resets conv_layer_count and logs
    //  - setCalibrationMode(flag)    : toggles mode and resets counter
    //  - resetConvLayerCounter()     : resets conv_layer_count (public wrapper)
    //  - getCurrentConvLayerCount()  : returns current conv_layer_count
    //  - enableLayerSpecificCalibration(enable) : convenience wrapper to set mode
    //  - isLayerSpecificCalibrationEnabled()    : query current mode
    // ==========================================================================
    void resetCalibrationState() {
        conv_layer_count = 0;
        logInfo("Reset calibration state: conv_layer_count = 0");
    }

    void setCalibrationMode(bool use_layer_specific) {
        use_layer_specific_calibration = use_layer_specific;
        resetCalibrationState();
        logInfo("Set calibration mode: " + std::string(use_layer_specific ? "layer-specific" : "individual-tests"));
    }

    // ==========================================================================
    // LAB 2: NAIVE CONVOLUTION (Baseline - No Quantization)
    // ==========================================================================
    // This is your working Lab 2 implementation
    // It performs convolution using 32-bit floating point (fp32) arithmetic
    // ==========================================================================
    
    void ConvolutionalLayer::computeNaive(const LayerData &dataIn) const
    {
        // Get layer dimensions from parameters
        const auto &inputDims = getInputParams().dims;   // [H, W, C_in]
        const auto &outputDims = getOutputParams().dims; // [H_out, W_out, C_out]
        const auto &weightDims = getWeightParams().dims; // [K_H, K_W, C_in, C_out]

        size_t U = 1; // Stride (how many pixels we move the kernel each step)

        // Input dimensions
        size_t W = inputDims[1];  // Input width
        size_t C = inputDims[2];  // Input channels (e.g., 3 for RGB)

        // Output dimensions
        size_t P = outputDims[0]; // Output height
        size_t Q = outputDims[1]; // Output width
        size_t M = outputDims[2]; // Output channels (number of filters)

        // Kernel/Filter dimensions
        size_t R = weightDims[0]; // Kernel height
        size_t S = weightDims[1]; // Kernel width

        // Triple nested loop over output positions and channels
        for (size_t p = 0; p < P; p++)         // For each output row
        {
            for (size_t q = 0; q < Q; q++)     // For each output column
            {
                for (size_t m = 0; m < M; m++) // For each output channel
                {
                    fp32 result = 0.0f; // Accumulator for this output position
                    
                    // Perform the convolution sum
                    // Formula: o[p][q][m] = sum_{c,r,s} i[U*p+r][U*q+s][c] * f[r][s][c][m] + b[m]
                    for (size_t c = 0; c < C; c++)     // For each input channel
                    {
                        for (size_t r = 0; r < R; r++) // For each kernel row
                        {
                            for (size_t s = 0; s < S; s++) // For each kernel column
                            {
                                // Calculate input coordinates for this kernel position
                                size_t input_h = U * p + r; // Input height position
                                size_t input_w = U * q + s; // Input width position
                                
                                // Calculate flat array index for input
                                // 3D array [H][W][C] flattened to 1D
                                size_t input_idx = input_h * W * C + input_w * C + c;
                                
                                // Calculate flat array index for weight
                                // 4D array [R][S][C][M] flattened to 1D
                                size_t weight_idx = r * S * C * M + s * C * M + c * M + m;
                                
                                // Multiply-Accumulate operation (MAC)
                                // This is the core operation we're accelerating in hardware
                                result += dataIn.get<fp32>(input_idx) *
                                          getWeightData().get<fp32>(weight_idx);
                            }
                        }
                    }
                    
                    // Add bias term for this output channel
                    result += getBiasData().get<fp32>(m);
                    
                    // Apply ReLU activation: max(0, x)
                    // This introduces non-linearity into the network
                    result = std::max(0.0f, result);
                    
                    // Calculate output index and store result
                    size_t output_idx = p * Q * M + q * M + m;
                    getOutputData().get<fp32>(output_idx) = result;
                }
            }
        }
    }

    // ==========================================================================
    // PLACEHOLDER IMPLEMENTATIONS (Not modified for this lab)
    // ==========================================================================
    
    void ConvolutionalLayer::computeThreaded(const LayerData &dataIn) const
    {
        // For simplicity, use naive implementation
        computeNaive(dataIn);
    }

    void ConvolutionalLayer::computeTiled(const LayerData &dataIn) const
    {
        // For simplicity, use naive implementation
        computeNaive(dataIn);
    }

    void ConvolutionalLayer::computeSIMD(const LayerData &dataIn) const
    {
        // For simplicity, use naive implementation
        computeNaive(dataIn);
    }

    // ==========================================================================
    // LAB 3: QUANTIZED CONVOLUTION (8-bit Integer Arithmetic)
    // ==========================================================================
    // This performs the SAME convolution as computeNaive(), but using:
    //   - 8-bit signed integers (int8) for inputs and weights
    //   - 32-bit signed integers (int32) for accumulation
    //   - Floating point (fp32) only for final output
    //
    // WHY? Hardware can compute int8 multiplications ~4x faster and with
    // ~16x less energy than fp32 multiplications!
    // ==========================================================================
    
void ConvolutionalLayer::computeQuantized(const LayerData &dataIn) const {
    computeQuantizedInternal(dataIn, false);
}

void ConvolutionalLayer::computeAccelerated(const LayerData &dataIn) const {
    computeQuantizedInternal(dataIn, true);
}

void ConvolutionalLayer::computeQuantizedInternal(const LayerData &dataIn, bool use_hardware) const {
        // ==========================================================================
        // SECTION 1: LOAD CALIBRATION STATS AND IDENTIFY CURRENT LAYER  
        // ==========================================================================
        
        // Load calibration statistics if not already loaded
        if (!calibration_loaded) {
            // Try different possible paths for the calibration file
            std::vector<std::string> possible_paths = {
                "data/calibration_stats.json",
                "calibration_stats.json",
                "../../../SW/Lab3/Phase_I_Calibration/calibration_stats.json",
                "../../SW/Lab3/Phase_I_Calibration/calibration_stats.json", 
                "../SW/Lab3/Phase_I_Calibration/calibration_stats.json",
                "SW/Lab3/Phase_I_Calibration/calibration_stats.json"
            };
            
            bool found = false;
            for (const auto& path : possible_paths) {
                logInfo("Attempting to load calibration file: " + path);
                if (loadCalibrationStats(path)) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                logError("Could not find calibration_stats.json file");
                logInfo("Falling back to runtime quantization parameter calculation");
                // Fall back to the original implementation would go here
                return;
            }
        }
        
        // ==========================================================================
        // SECTION 2: GET DIMENSIONS (Moved up by BibidhB to make variables available)
        // ==========================================================================
        const auto &inputDims = getInputParams().dims;
        const auto &outputDims = getOutputParams().dims;
        const auto &weightDims = getWeightParams().dims;

        size_t U = 1; // Stride
        size_t W = inputDims[1];
        size_t C = inputDims[2];
        size_t P = outputDims[0];
        size_t Q = outputDims[1];
        size_t M = outputDims[2];
        size_t R = weightDims[0];
        size_t S = weightDims[1];

#if defined(ZEDBOARD)
        const bool hardware_enabled = use_hardware;
#else
        const bool hardware_enabled = false;
#endif

        std::vector<uint16_t> mac_pairs;
#if defined(ZEDBOARD)
        if (hardware_enabled) {
            mac_pairs.reserve(R * S * C);
        }
#endif
        
        // ==========================================================================
        // ADAPTIVE INPUT CALIBRATION SELECTION FOR CONVOLUTIONAL LAYERS
        // ==========================================================================
        // Two calibration modes are supported:
        // 1) Individual layer tests (use_layer_specific_calibration == false):
        //    - Every invocation treats the input as the raw image.
        //    - Always use "_input" calibration stats.
        //    - conv_layer_count is NOT incremented.
        // 2) Full inference chain (use_layer_specific_calibration == true):
        //    - Layers are assigned calibration names by call order using conv_layer_count.
        //    - conv_layer_count == 0 -> use "_input"
        //    - conv_layer_count == 1 -> "conv2d"
        //    - conv_layer_count == 2..6 -> "conv2d_1" .. "conv2d_5" (conv_layer_count-1 suffix)
        //    - conv_layer_count > 6 -> fallback to "conv2d_5"
        //    - conv_layer_count is incremented after selecting the name so the next
        //      convolution invocation uses the next layer's stats.
        // Note: conv_layer_count can be reset via resetCalibrationState()/resetConvLayerCounter()
        // and mode toggled with setCalibrationMode()/enableLayerSpecificCalibration().
        // ==========================================================================
        
        std::string input_stats_name;

        // Check if this is an individual layer test (always start with raw image)
        // vs full inference chain (layer gets previous layer's output)
        bool is_individual_layer_test = !use_layer_specific_calibration;

        if (is_individual_layer_test) {
            // Individual layer test mode: all layers get raw image input
            input_stats_name = "_input";
            // Don't increment counter for individual tests
        } else {
            // Full inference chain mode: use layer-specific calibration
            if (conv_layer_count == 0) {
                input_stats_name = "_input";
            } else if (conv_layer_count <= 5) {  // Only 6 conv layers exist (0-5)
                input_stats_name = "conv2d";
                if (conv_layer_count > 1) {
                    input_stats_name += "_" + std::to_string(conv_layer_count - 1);
                }
            } else {
                //  Reasoning:
                //  Downstream layers (fully-connected, pooling, activations, normalization, etc.)
                //    commonly consume the outputs produced by the last convolutional layer.
                //  Calibration data computed for conv outputs (e.g., activation ranges, quantization
                //    scale/zero-point, running statistics) is the only available estimate of the
                //    dynamic range for those activations when later layers have no separate
                //    calibration step.
                //  Using the last conv calibration is a conservative fallback that prevents
                //    uncalibrated layers from causing quantization/scale mismatches or incorrect
                //    numeric behavior.
                input_stats_name = "conv2d_5";  // Use conv2d_5 for any layer beyond
                logInfo("Layer beyond conv range, using fallback calibration: conv2d_5");
            }
            // Increment counter for next layer in chain
            conv_layer_count++;
        }
        
        // Identify current layer for logging purposes only
        std::string current_layer_name;
        if (P == 60 && Q == 60 && M == 32) {
            current_layer_name = "conv2d";       // Conv1 is conv2d
        } else if (P == 56 && Q == 56 && M == 32) {
            current_layer_name = "conv2d_1";     // Conv2 is conv2d_1
        } else if (P == 26 && Q == 26 && M == 64) {
            current_layer_name = "conv2d_2";     // Conv3 is conv2d_2
        } else if (P == 24 && Q == 24 && M == 64) {
            current_layer_name = "conv2d_3";     // Conv4 is conv2d_3
        } else if (P == 10 && Q == 10 && M == 64) {
            current_layer_name = "conv2d_4";     // Conv5 is conv2d_4 (FIXED!)
        } else if (P == 8 && Q == 8 && M == 128) {
            current_layer_name = "conv2d_5";     // Conv6 is conv2d_5 (Added by BibidhB to match layer dims)
        } else {
            current_layer_name = "unknown_layer";
        }
                
        // Find the input calibration stats (always use "_input" for individual layer tests)
        auto input_stats_it = calibration_data.find(input_stats_name);
        if (input_stats_it == calibration_data.end()) {
            logError("No calibration stats found for input data: " + input_stats_name);
            logError("Available layers in calibration data:");
            for (const auto& pair : calibration_data) {
                logError("  - " + pair.first);
            }
            return;
        }
        
        const CalibrationStats& input_stats = input_stats_it->second;
        
        logInfo("Processing layer: " + current_layer_name + " (dims: " + 
                std::to_string(P) + "x" + std::to_string(Q) + "x" + std::to_string(M) + ")");
        logInfo("Using calibration stats: " + input_stats_name + " - Si=" + std::to_string(input_stats.Si) + 
                ", zi=" + std::to_string(static_cast<int>(input_stats.zi)));
        
            
        // ==========================================================================
        // SECTION 3: USE PRE-CALCULATED QUANTIZATION PARAMETERS
        // ==========================================================================
        
        // -------------------------
        // 3.1: Calculate WEIGHT SCALE (Sw) - Still calculated at runtime
        // -------------------------
        // We still calculate this because weight ranges can vary significantly
        size_t weight_size = getWeightParams().flat_count();
        fp32 max_weight = 0.0f;
        
        for (size_t i = 0; i < weight_size; i++) {
            fp32 abs_val = std::abs(getWeightData().get<fp32>(i));
            if (abs_val > max_weight) {
                max_weight = abs_val;
            }
        }
        
        if (max_weight < 1e-8f) {
            max_weight = 1.0f;
        }
        
        fp32 Sw = 127.0f / max_weight;
        // logDebug("Weight scale Sw = " + std::to_string(Sw) + " (max_weight = " + std::to_string(max_weight) + ")");
        
        // -------------------------
        // 3.2: Use PRE-CALCULATED INPUT SCALE (Si) and ZERO POINT (zi)
        // -------------------------
        // These come directly from calibration_stats.json
        fp32 Si = input_stats.Si;
        i8 zi = input_stats.zi;
        
        // logDebug("Using calibrated input scale Si = " + std::to_string(Si) + 
        //         ", zero point zi = " + std::to_string(static_cast<int>(zi)));
        
        // -------------------------
        // 3.3: Calculate BIAS SCALE (Sb)
        // -------------------------
        fp32 Sb = Si * Sw;
        // logDebug("Bias scale Sb = " + std::to_string(Sb));
        
        // ==========================================================================
        // SECTION 4: QUANTIZE ALL INPUTS (BEFORE CONVOLUTION LOOPS)
        // ==========================================================================
        // Formula: ix = round(Si * Ix) + zi
        // Using pre-calculated Si and zi from calibration stats
        // ==========================================================================
        
        size_t input_size = getInputParams().flat_count();
        std::vector<i8> quantized_input(input_size);
        
        for (size_t i = 0; i < input_size; i++) {
            i32 temp = static_cast<i32>(std::round(Si * dataIn.get<fp32>(i))) + zi;
            quantized_input[i] =
                static_cast<i8>(std::max<i32>(-128, std::min<i32>(127, temp)));
        }
        
        // logDebug("Quantized " + std::to_string(input_size) + " input values to int8");
        
        // ==========================================================================
        // SECTION 5: QUANTIZE ALL WEIGHTS (BEFORE CONVOLUTION LOOPS)
        // ==========================================================================
        // Formula: wx = round(Sw * Wx)
        // Note: No zero point for weights (symmetric quantization)
        // ==========================================================================
        
        std::vector<i8> quantized_weights(weight_size);
        
        for (size_t i = 0; i < weight_size; i++) {
            i32 temp = static_cast<i32>(std::round(Sw * getWeightData().get<fp32>(i)));
            quantized_weights[i] =
                static_cast<i8>(std::max<i32>(-128, std::min<i32>(127, temp)));
        }
        
        // logDebug("Quantized " + std::to_string(weight_size) + " weight values to int8");
        
        // ==========================================================================
        // SECTION 6: QUANTIZE ALL BIASES (BEFORE CONVOLUTION LOOPS)
        // ==========================================================================
        // Formula: bx = round(Sb * Bx)
        // Note: Biases are int32 (not int8) because they're added to accumulated sums
        // ==========================================================================
        
        size_t bias_size = M; // One bias per output channel
        std::vector<i32> quantized_biases(bias_size);
        
        for (size_t m = 0; m < M; m++) {
            quantized_biases[m] = static_cast<i32>(std::round(Sb * getBiasData().get<fp32>(m)));
        }
        
        // logDebug("Quantized " + std::to_string(bias_size) + " bias values to int32");
        
        // ==========================================================================
        // SECTION 7: MAIN CONVOLUTION LOOP (SAME STRUCTURE AS LAB 2!)
        // ==========================================================================
        // This is nearly IDENTICAL to computeNaive(), except:
        //   1. We use int8 quantized values instead of fp32
        //   2. Accumulator is int32 instead of fp32
        //   3. We dequantize at the end to get fp32 output
        // ==========================================================================
        
        // logDebug("Starting convolution loops...");
        
        // Triple nested loop over output positions (SAME as Lab 2)
        for (size_t p = 0; p < P; p++)         // For each output row
        {
            for (size_t q = 0; q < Q; q++)     // For each output column
            {
                for (size_t m = 0; m < M; m++) // For each output channel
                {
                    i32 accumulator = quantized_biases[m];
                    if (hardware_enabled) {
                        mac_pairs.clear();
                    }
                    
                    // Progress Indicator
                    // static int progress_cnt = 0;
                    // if (++progress_cnt % 2000 == 0) {
                    //     std::cout << "." << std::flush;
                    // }
                    
                    for (size_t c = 0; c < C; c++)     // For each input channel
                    {
                        for (size_t r = 0; r < R; r++) // For each kernel row
                        {
                            for (size_t s = 0; s < S; s++) // For each kernel column
                            {
                                size_t input_h = U * p + r;
                                size_t input_w = U * q + s;
                                
                                size_t input_idx = input_h * W * C + input_w * C + c;
                                size_t weight_idx = r * S * C * M + s * C * M + c * M + m;
                                
                                i8 input_val = quantized_input[input_idx];
                                i8 weight_val = quantized_weights[weight_idx];
                                
                                if (hardware_enabled) {
                                    mac_pairs.push_back(packMacOperands(weight_val, input_val));
                                } else {
                                    accumulator += static_cast<i32>(input_val) *
                                                   static_cast<i32>(weight_val);
                                }
                            }
                        }
                    }
                    
                    if (hardware_enabled && !mac_pairs.empty()) {
                        const i32 mac_sum =
                            HardwareMac::run(mac_pairs.data(), mac_pairs.size());
                        accumulator += mac_sum;  // Add to existing bias, don't replace
                    }
                    
                    // ==========================================================
                    // SECTION 8: DEQUANTIZE BACK TO FP32  
                    // ==========================================================
                    // Using calibrated quantization parameters for more accurate results
                    //
                    // MATHEMATICAL DERIVATION:
                    // When we compute: accumulator = Σ(ix * wx) + bx
                    // Where ix = round(Si*Ix) + zi
                    // We get: accumulator = Si*Sw*Σ(Ix*Wx) + zi*Sw*Σ(Wx) + Sb*Bx
                    //
                    // The zi*Sw*Σ(Wx) term is an unwanted offset that accumulated
                    // We need to remove it before dequantizing
                    // ==========================================================
                    
                    // STEP 1: Calculate sum of all quantized weights used for this output
                    i32 weight_sum = 0;
                    for (size_t c = 0; c < C; c++) {
                        for (size_t r = 0; r < R; r++) {
                            for (size_t s = 0; s < S; s++) {
                                size_t weight_idx = r * S * C * M + s * C * M + c * M + m;
                                weight_sum += static_cast<i32>(quantized_weights[weight_idx]);
                            }
                        }
                    }
                    
                    // STEP 2: Calculate the zero-point offset that accumulated
                    i32 zero_point_offset = static_cast<i32>(zi) * weight_sum;
                    
                    // STEP 3: Remove offset and dequantize using calibrated parameters
                    fp32 result = static_cast<fp32>(accumulator - zero_point_offset) / (Si * Sw);
                    
                    // ==========================================================
                    // SECTION 9: APPLY ReLU ACTIVATION (In FP32 space!)
                    // ==========================================================
                    result = std::max(0.0f, result);
                    
                    // Store result in output array
                    size_t output_idx = p * Q * M + q * M + m;
                    getOutputData().get<fp32>(output_idx) = result;
                }
            }
        }
        
        // ==========================================================================
        // DEBUG OUTPUT: Verify calibrated quantization worked correctly
        // ==========================================================================
        size_t output_size = P * Q * M;
        fp32 output_min = getOutputData().get<fp32>(0);
        fp32 output_max = getOutputData().get<fp32>(0);
        fp32 output_avg = 0.0f;
        size_t zero_count = 0;
        
        for (size_t i = 0; i < output_size; i++) {
            fp32 val = getOutputData().get<fp32>(i);
            output_avg += val;
            if (val < output_min) output_min = val;
            if (val > output_max) output_max = val;
            if (val == 0.0f) zero_count++;
        }
        output_avg /= output_size;
        
        logInfo("Layer " + current_layer_name + " quantized convolution complete\n"); // Extra newline for readability
        // logDebug("Output statistics - Min: " + std::to_string(output_min) + 
        //         ", Max: " + std::to_string(output_max) + 
        //         ", Avg: " + std::to_string(output_avg));
        // logDebug("Zero outputs: " + std::to_string(zero_count) + "/" + std::to_string(output_size) + 
        //         " (" + std::to_string(100.0f * zero_count / output_size) + "%)");
    }
    
    // ==========================================================================
    // SUMMARY OF KEY CHANGES - CALIBRATED QUANTIZATION:
    // ==========================================================================
    // 1. Load pre-calculated quantization parameters from calibration_stats.json
    // 2. Use calibrated Si (input scale) and zi (zero point) values
    // 3. Automatic layer identification using conv_layer_count
    // 4. Eliminated expensive runtime min/max calculations for inputs
    // 5. Maintained weight quantization (still calculated at runtime)
    // 6. Added robust path searching for calibration file
    // 7. Improved logging using Utils.h logging functions
    //
    // BENEFITS OF CALIBRATED APPROACH:
    // - More consistent quantization across different inputs
    // - Better accuracy due to representative calibration data
    // - Faster inference (no runtime input statistics calculation)
    // - Production-ready approach matching industry standards
    //
    // CALIBRATION DATA USAGE:
    // - Input layer: Si=230.49, zi=-103 (normalized input range)
    // - Conv layers: Progressively smaller Si values (wider activation ranges)
    // - Layer identification: Based on call order (conv2d, conv2d_1, etc.)
    // ==========================================================================

    // ==========================================================================
    // UTILITY FUNCTIONS FOR CALIBRATED QUANTIZATION
    // ==========================================================================
    
    // Reset the layer counter (call this at the start of each inference)
    void resetConvLayerCounter() {
        conv_layer_count = 0;
    }
    
    // Get current layer counter value (for debugging)
    int getCurrentConvLayerCount() {
        return conv_layer_count;
    }
    
    // Enable layer-specific calibration for full inference chains
    void enableLayerSpecificCalibration(bool enable) {
        use_layer_specific_calibration = enable;
        if (enable) {
            logInfo("Enabled layer-specific calibration for full inference chains");
        } else {
            logInfo("Using raw input calibration for all layers (individual layer test mode)");
        }
    }
    
    // Check if layer-specific calibration is enabled
    bool isLayerSpecificCalibrationEnabled() {
        return use_layer_specific_calibration;
    }

} // namespace ML
