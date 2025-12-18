#include "Model.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <limits>

namespace ML {

// Run inference on the entire model using the inData and outputting the outData
// infType can be used to determine the inference function to call
const LayerData& Model::inference(const LayerData& inData, const Layer::InfType infType) const {
    assert(layers.size() > 0 && "There must be at least 1 layer to perform inference");
    inferenceLayer(inData, 0, infType);

    for (std::size_t i = 1; i < layers.size(); i++) {
        inferenceLayer(layers[i - 1]->getOutputData(), i, infType);
    }

    return layers.back()->getOutputData();
}

// Run inference on a single layer of the model using the inData and outputting the outData
// infType can be used to determine the inference function to call
const LayerData& Model::inferenceLayer(const LayerData& inData, const int layerNum, const Layer::InfType infType) const {
    Layer& layer = *layers[layerNum];

    assert(layer.getInputParams().isCompatible(inData.getParams()) && "Input data is not compatible with layer");
    assert(layer.isOutputBufferAlloced() && "Output buffer must be allocated prior to inference");
    
    switch (infType) {
    case Layer::InfType::NAIVE:
        layer.computeNaive(inData);
        break;
    case Layer::InfType::THREADED:
        layer.computeThreaded(inData);
        break;
    case Layer::InfType::TILED:
        layer.computeTiled(inData);
        break;
    case Layer::InfType::SIMD:
        layer.computeSIMD(inData);
        break;
    case Layer::InfType::QUANTIZED:
        layer.computeQuantized(inData);
        break;
    default:
        assert(false && "Inference Type not implemented");
    }

    return layer.getOutputData();
}

// Helper to write JSON manually to avoid dependencies
void writeLayerStats(std::ofstream& outFile, const std::string& layerName, float minVal, float maxVal, float meanVal, float Si, int zi, bool isLast) {
    outFile << "  \"" << layerName << "\": {\n";
    outFile << "    \"min\": " << minVal << ",\n";
    outFile << "    \"max\": " << maxVal << ",\n";
    outFile << "    \"mean\": " << meanVal << ",\n";
    outFile << "    \"Si\": " << Si << ",\n";
    outFile << "    \"zi\": " << zi << "\n";
    outFile << "  }" << (isLast ? "" : ",") << "\n";
}

void Model::generateCalibration(const LayerData& inData, const std::string& outPath) const {
    std::cout << "--- Generating Calibration Statistics ---" << std::endl;
    std::ofstream outFile(outPath);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open output file " << outPath << std::endl;
        return;
    }

    outFile << "{\n";

    // We need to capture the input statistics for each relevant layer
    // The "input" to the network is handled as a special case or as the input to the first layer
    
    const LayerData* currentInput = &inData;

    // Helper lambda to process a layer's input
    auto processInputCtx = [&](const std::string& layerKey) {
        const LayerData& data = *currentInput;
        size_t count = data.getParams().flat_count();
        
        float minVal = std::numeric_limits<float>::max();
        float maxVal = std::numeric_limits<float>::lowest();
        double sum = 0;

        for(size_t k = 0; k < count; k++) {
            float val = data.get<float>(k);
            if(val < minVal) minVal = val;
            if(val > maxVal) maxVal = val;
            sum += val;
        }
        float meanVal = static_cast<float>(sum / count);

        // Calculate Quantization Parameters (Int8 symmetric/asymmetric logic)
        // Target range: [-128, 127] -> qmin=-128, qmax=127
        // Formula: Si = (real_max - real_min) / (qmax - qmin)
        //          zi = qmin - round(real_min / Si)
        // However, standard affine quantization often ensures 0 is representable exactly or uses symmetric logic.
        // Let's use the standard asymmetric affine quantization formula which is robust.
        
        const float qmin = -128.0f;
        const float qmax = 127.0f;
        
        // Avoid division by zero if input is constant
        float range = maxVal - minVal;
        if (range < 1e-6f) range = 1e-6f;

        float Si = range / (qmax - qmin);
        int zi = static_cast<int>(std::round(qmin - (minVal / Si)));

        // Clamp zi to int8 range just in case
        if (zi < -128) zi = -128;
        if (zi > 127) zi = 127;

        std::cout << "Layer Key: " << layerKey 
                  << " Min: " << minVal << " Max: " << maxVal 
                  << " Si: " << Si << " zi: " << zi << std::endl;

        // Write to file immediately (simplifies logic)
        // We will fix the comma format by buffering or logic logic, but simple bool flag works manually? 
        // Actually, let's just use a buffer of stats and write at the end to handle commas cleanly.
        return std::make_tuple(minVal, maxVal, meanVal, Si, zi);
    };

    struct Stats {
        std::string name;
        float min, max, mean, Si;
        int zi;
    };
    std::vector<Stats> allStats;

    // 1. Process Network Input ("_input")
    // This corresponds to the raw image input
    {
        auto res = processInputCtx("_input");
        allStats.push_back({"_input", std::get<0>(res), std::get<1>(res), std::get<2>(res), std::get<3>(res), std::get<4>(res)});
    }

    // Loop through layers
    // Note: The logic in Convolutional_new.cpp uses "conv2d" stats for the input of the 2nd convolution (which is the output of the 1st).
    // wait, let's re-read carefully:
    // conv_layer_count == 0 -> uses "_input" (Raw Image)
    // conv_layer_count == 1 -> uses "conv2d" (Output of Conv 0 / Input to Conv 1)
    // So "conv2d" key stores stats of the DATA flowing INTO the 2nd conv layer (index 1).
    
    // We already processed _input (Input to Layer 0).
    // Now run Layer 0. Its output is the input to Layer 1. Use that for "conv2d".
    
    for (std::size_t i = 0; i < layers.size(); i++) {
        // Run Naive Inference
        inferenceLayer(*currentInput, i, Layer::InfType::NAIVE);
        
        // The output of this layer is the input to the next layer
        const LayerData* nextInput = &layers[i]->getOutputData();
        
        // Determine if we need to capture stats for the NEXT layer's consumption
        // We check the NEXT layer type to decide?
        // Actually, the keys are "conv2d", "conv2d_1", etc.
        // These keys are used when the *Convolutional* layer is computed.
        // So if Layer[i+1] is a Conv layer, we need to save the stats of its input (which is Layer[i]'s output).
        
        if (i + 1 < layers.size()) {
            Layer::LayerType nextType = layers[i+1]->getLType();
            
            if (nextType == Layer::LayerType::CONVOLUTIONAL) {
                // Determine naming convention based on conv layer count
                // We just finished a layer. If the NEXT one is Conv, increment counter?
                // The logic in Convolutional_new counts every time computeQuantized is called.
                // It starts at 0.
                
                // Let's count how many Conv layers we have processed so far.
                // But wait, the key "conv2d" is used when `conv_layer_count == 1`.
                // This happens when the SECOND Convolutional layer is being executed.
                // So "conv2d" must characterize the Input to the 2nd Conv Layer.
                
                // Let's identify the conv index of the NEXT layer.
                int nextConvIndex = 0;
                for(size_t k=0; k<=i; k++) {
                    if(layers[k]->getLType() == Layer::LayerType::CONVOLUTIONAL) nextConvIndex++;
                }
                
                // If next layer is Conv (which it is, per the check above), its 'conv_layer_count' will be 'nextConvIndex'.
                // If nextConvIndex == 1, key is "conv2d".
                // If nextConvIndex == 2, key is "conv2d_1".
                // If nextConvIndex == 3, key is "conv2d_2".
                
                std::string keyName;
                if (nextConvIndex == 1) keyName = "conv2d";
                else if (nextConvIndex > 1) keyName = "conv2d_" + std::to_string(nextConvIndex - 1);
                
                if (!keyName.empty()) {
                    currentInput = nextInput; // Point to the output we just computed
                    auto res = processInputCtx(keyName);
                    allStats.push_back({keyName, std::get<0>(res), std::get<1>(res), std::get<2>(res), std::get<3>(res), std::get<4>(res)});
                }
            } else if (nextType == Layer::LayerType::DENSE) {
                // The dense layer also needs calibration if 'setDenseCalibrationMode' is true.
                // It likely uses a key like "dense" or falls back to "conv2d_5" (last output).
                // Let's assume there is a key "dense" based on the viewed json file.
                // "dense" key exists in the file.
                
                // Only generate if it's the first dense layer? Or all?
                // The JSON only had one "dense" key.
                // Let's generate "dense" stats for the input to the Dense layer.
                
                bool alreadyHasDense = false;
                for(const auto& s : allStats) if(s.name == "dense") alreadyHasDense = true;
                
                if (!alreadyHasDense) {
                    currentInput = nextInput;
                    auto res = processInputCtx("dense");
                    allStats.push_back({"dense", std::get<0>(res), std::get<1>(res), std::get<2>(res), std::get<3>(res), std::get<4>(res)});
                }
            }
        }
        
        // Update currentInput pointer for the next iteration of the loop (standard inference flow)
        currentInput = nextInput; 
    }

    // Write all collected stats to file
    for (size_t i = 0; i < allStats.size(); i++) {
        writeLayerStats(outFile, allStats[i].name, allStats[i].min, allStats[i].max, allStats[i].mean, allStats[i].Si, allStats[i].zi, (i == allStats.size() - 1));
    }

    outFile << "}\n";
    outFile.close();
    std::cout << "--- Calibration Statistics Generated: " << outPath << " ---" << std::endl;
}

}  // namespace ML
