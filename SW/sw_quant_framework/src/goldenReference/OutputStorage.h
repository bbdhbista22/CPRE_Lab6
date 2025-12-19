#ifndef OUTPUT_STORAGE_H
#define OUTPUT_STORAGE_H

#include <cstdint>
#include <vector>
#include <queue>

/**
 * OutputStorage - C++ Reference Implementation
 * 
 * Handles writing quantized int8 outputs to BRAM with read-modify-write semantics.
 * Implements 32-bit word packing (4 int8 values per word) for efficient BRAM usage.
 * Optional 2X2 max pooling support.
 * 
 * This class serves as a golden reference for VHDL hardware validation.
 */
class OutputStorage {
public:
    /**
     * Configuration for output storage
     */
    struct Config {
        uint16_t output_height;   ///< Output tensor height (pixels)
        uint16_t output_width;    ///< Output tensor width (pixels)
        uint16_t output_channels; ///< Output tensor channels (depth)
        
        bool enable_pooling;      ///< Enable 2X2 max pooling
        uint32_t output_base_addr; ///< Base address in BRAM for output buffer
    };

    /**
     * Per-output statistics for debugging
     */
    struct OutputStats {
        uint16_t out_y;           ///< Output row coordinate
        uint16_t out_x;           ///< Output column coordinate
        uint16_t out_c;           ///< Output channel
        uint32_t bram_addr;       ///< BRAM word address
        uint8_t byte_sel;         ///< Which byte in 32-bit word (0-3)
        int8_t value;             ///< Output value
        uint32_t old_word;        ///< Original BRAM word (before RMW)
        uint32_t new_word;        ///< Updated BRAM word (after RMW)
    };

    /**
     * Constructor
     * 
     * @param config Output storage configuration
     */
    explicit OutputStorage(const Config& config);

    /**
     * Store single output value
     * 
     * Performs read-modify-write:
     * 1. Calculate BRAM address for output position
     * 2. Read existing 32-bit word from BRAM
     * 3. Insert new byte at correct position
     * 4. Write modified word back
     * 
     * @param out_y Output row (0 to output_height-1)
     * @param out_x Output column (0 to output_width-1)
     * @param out_c Output channel (0 to output_channels-1)
     * @param value int8 value to store
     * @param bram_data Current BRAM contents (for simulation)
     * @param stats [optional] Output statistics
     * @return Modified BRAM word to write back
     */
    uint32_t storeOutput(uint16_t out_y, uint16_t out_x, uint16_t out_c,
                        int8_t value, uint32_t bram_data = 0,
                        OutputStats* stats = nullptr);

    /**
     * Process AXI-Stream input (from Dequantization pipeline)
     * 
     * Simulates receiving quantized values with timing info:
     * - TDATA: int8 value
     * - TID: MAC unit ID (0-3, maps to output channel offset)
     * - TLAST: Signals end of output pixel
     * 
     * @param tdata int8 value
     * @param tid MAC unit ID (0-3)
     * @param tlast True if this is last MAC for current pixel
     * @param bram_contents Current BRAM state
     * @param stats [optional] Statistics for this output
     * @return BRAM update (address, new_value) for RMW
     */
    struct BRAMUpdate {
        uint32_t addr;
        uint32_t data;
    };
    
    BRAMUpdate processStream(int8_t tdata, uint8_t tid, bool tlast,
                            const std::vector<uint32_t>& bram_contents,
                            OutputStats* stats = nullptr);

    /**
     * Optional: Apply 2X2 max pooling
     * 
     * Reduces output spatial dimensions by 2X:
     * - 4 input pixels (2X2 neighborhood) → 1 output pixel
     * - Only writes output when all 4 inputs received
     * 
     * @param values Vector of 4 int8 values
     * @return Single pooled value (max of 4 inputs)
     */
    int8_t poolMax2x2(const std::vector<int8_t>& values);

    /**
     * Get current configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * Verify output address sequence
     * 
     * @param addresses Vector of addresses generated
     * @return True if all checks pass
     */
    bool verifyAddresses(const std::vector<uint32_t>& addresses);

private:
    Config config_;
    uint32_t pixel_count_;  ///< Internal pixel counter for positional calculation

    /**
     * Calculate BRAM address for output position
     * 
     * output_addr = base + ((out_y * output_width + out_x) * output_channels + out_c) / 4
     * 
     * @param out_y Output row
     * @param out_x Output column
     * @param out_c Output channel
     * @return BRAM word address and byte position
     */
    struct AddressInfo {
        uint32_t word_addr;  ///< 32-bit word address
        uint8_t byte_sel;    ///< Byte position within word (0-3)
    };
    
    AddressInfo calcOutputAddr(uint16_t out_y, uint16_t out_x, uint16_t out_c);

    /**
     * Perform byte insertion into 32-bit word
     * 
     * Byte order (little-endian):
     * - byte_sel=0: word[7:0]
     * - byte_sel=1: word[15:8]
     * - byte_sel=2: word[23:16]
     * - byte_sel=3: word[31:24]
     * 
     * @param old_word Original 32-bit value
     * @param new_byte Value to insert
     * @param byte_sel Position in word
     * @return Modified word
     */
    uint32_t insertByte(uint32_t old_word, uint8_t new_byte, uint8_t byte_sel);

    /**
     * Extract byte from 32-bit word
     */
    uint8_t extractByte(uint32_t word, uint8_t byte_sel);
};

#endif // OUTPUT_STORAGE_H
