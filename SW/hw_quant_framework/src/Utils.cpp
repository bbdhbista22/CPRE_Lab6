#include "Utils.h"

namespace ML {
// #void exportQuantizedWeights(const std::string& filename, 
//                            const std::vector<int8_t>& weights,
//                            const std::vector<int32_t>& biases) {
//     std::ofstream file(filename, std::ios::binary);
    
//     // Write header with sizes
//     size_t weight_size = weights.size();
//     size_t bias_size = biases.size();
//     file.write(reinterpret_cast<const char*>(&weight_size), sizeof(size_t));
//     file.write(reinterpret_cast<const char*>(&bias_size), sizeof(size_t));
    
//     // Write weights
//     file.write(reinterpret_cast<const char*>(weights.data()), 
//                weights.size() * sizeof(int8_t));
    
//     // Write biases
//     file.write(reinterpret_cast<const char*>(biases.data()),
//                biases.size() * sizeof(int32_t));
    
//     file.close();
// }


}  // namespace ML