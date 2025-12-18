#ifndef INDEX_GENERATOR_H
#define INDEX_GENERATOR_H

#include <cstdint>
#include <vector>

/**
 * IndexGenerator - C++ Reference Implementation
 * 
 * Generates address pairs (input_addr, weight_addr) for convolution operations.
 * This class serves as a golden reference for VHDL hardware validation.
 * 
 * Implements row-stationary dataflow with nested loop structure:
 * for output_channels:
 *   for output_pixels (out_y, out_x):
 *     for filter_positions (fy, fx):
 *       for input_channels (ic):
 *         generate (input_addr, weight_addr, TLAST)
 */
class IndexGenerator {
public:
    struct Address {
        uint32_t input_addr;      ///< BRAM address for input activation
        uint32_t weight_addr;     ///< BRAM address for weight
        bool tlast;               ///< Assert on last MAC of output pixel (every 27 MACs for 3x3x3)
        uint8_t oc;               ///< Output channel index (0-3 for 4 PEs)
    };

    /**
     * Configuration for convolution layer
     */
    struct ConvConfig {
        uint16_t input_height;    ///< Input height (pixels)
        uint16_t input_width;     ///< Input width (pixels)
        uint16_t input_channels;  ///< Input channels (depth)
        
        uint8_t filter_height;    ///< Filter height (pixels)
        uint8_t filter_width;     ///< Filter width (pixels)
        uint8_t num_filters;      ///< Number of output channels (filters)
        
        uint8_t stride;           ///< Convolution stride
        uint8_t padding;          ///< Zero-padding on all sides
        
        // Derived values (computed in constructor)
        uint16_t output_height;   ///< Output height = (input_height - filter_height + 2*padding) / stride + 1
        uint16_t output_width;    ///< Output width = (input_width - filter_width + 2*padding) / stride + 1
        uint32_t macs_per_pixel;  ///< MACs per output pixel = filter_height * filter_width * input_channels
    };

    /**
     * Tiling configuration
     */
    struct TileConfig {
        uint16_t tile_size;       ///< Tile dimension (16x16 pixels)
        uint16_t tiles_per_row;   ///< Number of tiles per row = ceil(output_width / tile_size)
        uint16_t tiles_per_col;   ///< Number of tiles per column = ceil(output_height / tile_size)
        uint16_t total_tiles;     ///< Total tiles = tiles_per_row * tiles_per_col
    };

    /**
     * Constructor
     * 
     * @param config Convolution configuration (input/filter sizes, stride, padding)
     * @param input_base_addr Base address in BRAM for input activations
     * @param weight_base_addr Base address in BRAM for weights
     * @param tile_size Tile size in pixels (default 16 for 16x16 tiles)
     */
    IndexGenerator(const ConvConfig& config, 
                   uint32_t input_base_addr = 0,
                   uint32_t weight_base_addr = 0,
                   uint16_t tile_size = 16);

    /**
     * Generate all address pairs for a complete layer
     * 
     * Returns vector of Address structs in hardware execution order:
     * - Iterate output_channels (oc): 0..num_filters-1, in groups of 4
     * - Iterate output_y, output_x within each tile
     * - Iterate filter positions (fy, fx)
     * - Iterate input_channels (ic)
     * 
     * Each Address represents one multiply-accumulate (MAC) operation.
     * TLAST is asserted when ic == input_channels-1 AND fx == filter_width-1 AND fy == filter_height-1
     * (on the LAST MAC of each output pixel), which occurs every (filter_height × filter_width × input_channels) MACs
     * 
     * @return Vector of addresses in execution order
     */
    std::vector<Address> generateAllAddresses();

    /**
     * Generate addresses for first N operations
     * 
     * Useful for quick validation without generating entire layer
     * 
     * @param n Number of operations to generate
     * @return Vector of first n addresses
     */
    std::vector<Address> generateFirstN(uint32_t n);

    /**
     * Get current configuration
     */
    const ConvConfig& getConvConfig() const { return conv_config_; }
    const TileConfig& getTileConfig() const { return tile_config_; }

    /**
     * Verify address sequence
     * 
     * Checks correctness of generated addresses:
     * - All addresses within valid BRAM bounds
     * - Address sequences are monotonic (mostly increasing with resets)
     * - TLAST flags placed correctly (every macs_per_pixel operations)
     * 
     * @param addresses Vector of addresses to verify
     * @return True if all checks pass
     */
    bool verifyAddresses(const std::vector<Address>& addresses);

private:
    ConvConfig conv_config_;
    TileConfig tile_config_;
    uint32_t input_base_addr_;
    uint32_t weight_base_addr_;

    // Internal helper methods
    /**
     * Calculate input address for given position
     * 
     * input_addr = input_base + (in_y * input_width + in_x) * input_channels + ic
     * 
     * @param in_y Input row coordinate
     * @param in_x Input column coordinate
     * @param ic Input channel
     * @return BRAM address for this element
     */
    uint32_t calcInputAddr(uint16_t in_y, uint16_t in_x, uint8_t ic);

    /**
     * Calculate weight address for given filter and input channel
     * 
     * weight_addr = weight_base + (oc * filter_height * filter_width * input_channels + 
     *                              fy * filter_width * input_channels +
     *                              fx * input_channels + ic)
     * 
     * @param oc Output channel
     * @param fy Filter row
     * @param fx Filter column
     * @param ic Input channel
     * @return BRAM address for this weight
     */
    uint32_t calcWeightAddr(uint8_t oc, uint8_t fy, uint8_t fx, uint8_t ic);

    /**
     * Calculate input row/column from output position and filter offset
     * 
     * in_y = out_y * stride - padding + fy
     * in_x = out_x * stride - padding + fx
     * 
     * Clips to input bounds (0 to input_height-1, 0 to input_width-1)
     * Returns true if position is valid (within input), false if padding zone
     * 
     * @param out_y Output row
     * @param out_x Output column
     * @param fy Filter row offset (0 to filter_height-1)
     * @param fx Filter column offset (0 to filter_width-1)
     * @param in_y [out] Calculated input row
     * @param in_x [out] Calculated input column
     * @return True if position is within valid input bounds
     */
    bool calcInputPosition(uint16_t out_y, uint16_t out_x, 
                          uint8_t fy, uint8_t fx,
                          uint16_t& in_y, uint16_t& in_x);
};

#endif // INDEX_GENERATOR_H
