/**
 * OutputStorage Test - C++ Implementation
 *
 * Tests BRAM read-modify-write operations and byte packing
 * Verifies correctness against Python reference implementation
 */

#include "../OutputStorage.h"
#include "test_framework.h"
#include <iostream>
#include <iomanip>

void test_basic_rmw() {
    TEST_BEGIN("OutputStorage - Basic Read-Modify-Write");

    OutputStorage::Config config = {
        8,      // output_height
        8,      // output_width
        4,      // output_channels
        false,  // enable_pooling
        0       // output_base_addr
    };

    OutputStorage storage(config);

    std::cout << "Configuration:\n";
    std::cout << "  Output: 8×8×4\n";
    std::cout << "  Total elements: " << (8 * 8 * 4) << "\n\n";

    // Test cases: (out_y, out_x, out_c, value)
    struct TestCase {
        uint16_t out_y;
        uint16_t out_x;
        uint16_t out_c;
        int8_t value;
    };

    TestCase test_cases[] = {
        {0, 0, 0, 10},   // First pixel, channel 0
        {0, 0, 1, 20},   // First pixel, channel 1
        {0, 0, 2, 30},   // First pixel, channel 2
        {0, 0, 3, 40},   // First pixel, channel 3 (all 4 channels done)
        {0, 1, 0, 50},   // Second pixel
        {7, 7, 3, 127},  // Last element
    };

    std::cout << std::setw(2) << "Y" << " | "
              << std::setw(2) << "X" << " | "
              << std::setw(2) << "C" << " | "
              << std::setw(6) << "Value" << " | "
              << std::setw(8) << "Addr" << " | "
              << std::setw(4) << "Byte" << " | "
              << std::setw(10) << "New Word\n";
    std::cout << std::string(70, '-') << "\n";

    for (const auto& tc : test_cases) {
        OutputStorage::OutputStats stats;
        uint32_t new_word = storage.storeOutput(tc.out_y, tc.out_x, tc.out_c,
                                                tc.value, 0, &stats);

        std::cout << std::setw(2) << tc.out_y << " | "
                  << std::setw(2) << tc.out_x << " | "
                  << std::setw(2) << tc.out_c << " | "
                  << std::setw(6) << (int)tc.value << " | "
                  << "0x" << std::setw(6) << std::setfill('0') << std::hex << stats.bram_addr << " | "
                  << std::setfill(' ') << std::dec
                  << std::setw(4) << (int)stats.byte_sel << " | "
                  << "0x" << std::setw(8) << std::setfill('0') << std::hex << new_word << std::dec << "\n";
    }

    std::cout << std::setfill(' ');

    TEST_END();
}

void test_byte_packing() {
    TEST_BEGIN("OutputStorage - 32-bit Word Byte Packing");

    OutputStorage::Config config = {
        16,     // output_height
        16,     // output_width
        64,     // output_channels
        false,  // enable_pooling
        0       // output_base_addr
    };

    OutputStorage storage(config);

    std::cout << "Test: Pack 4 int8 values into one 32-bit word\n\n";

    // Pack 4 values: 10, 20, 30, 40
    int8_t values[] = {10, 20, 30, 40};
    uint32_t word = 0;

    std::cout << std::setw(4) << "Byte" << " | "
              << std::setw(6) << "Value" << " | "
              << "Word After\n";
    std::cout << std::string(35, '-') << "\n";

    for (int i = 0; i < 4; i++) {
        // Simulate byte packing at position (0, 0, i)
        OutputStorage::OutputStats stats;
        word = storage.storeOutput(0, 0, i, values[i], word, &stats);

        std::cout << std::setw(4) << i << " | "
                  << std::setw(6) << (int)values[i] << " | "
                  << "0x" << std::setw(8) << std::setfill('0') << std::hex << word << std::dec << "\n";
    }

    std::cout << std::setfill(' ');

    // Expected: 0x280A140A (little-endian: byte0=10, byte1=20, byte2=30, byte3=40)
    // Actually: 0x281E140A (40 << 24 | 30 << 16 | 20 << 8 | 10)
    uint32_t expected = (40 << 24) | (30 << 16) | (20 << 8) | 10;

    std::cout << "\nFinal word: 0x" << std::hex << word << std::dec << "\n";
    std::cout << "Expected:   0x" << std::hex << expected << std::dec << "\n";

    ASSERT_EQ(expected, word);

    TEST_END();
}

void test_address_calculation() {
    TEST_BEGIN("OutputStorage - Address Calculation");

    OutputStorage::Config config = {
        64,     // output_height
        64,     // output_width
        64,     // output_channels
        false,  // enable_pooling
        0       // output_base_addr
    };

    OutputStorage storage(config);

    // Total outputs: 64 × 64 × 64 = 262,144
    // BRAM words needed: 262,144 / 4 = 65,536

    std::cout << "Output dimensions: 64×64×64\n";
    std::cout << "Total elements: " << (64 * 64 * 64) << "\n";
    std::cout << "BRAM words: " << ((64 * 64 * 64 + 3) / 4) << "\n\n";

    // Test specific positions
    struct TestCase {
        uint16_t out_y;
        uint16_t out_x;
        uint16_t out_c;
        uint32_t expected_addr;
        uint8_t expected_byte;
    };

    TestCase test_cases[] = {
        {0, 0, 0,      0x000000, 0},  // First element
        {0, 0, 63,     0x00000F, 3},  // Last channel of first pixel (63/4 = 15.75, so word 15, byte 3)
        {0, 1, 0,      0x000010, 0},  // Second pixel (64/4 = 16)
        {63, 63, 63,   0x00FFFF, 3},  // Last element
    };

    std::cout << std::setw(3) << "Y" << " | "
              << std::setw(3) << "X" << " | "
              << std::setw(3) << "C" << " | "
              << std::setw(12) << "Linear Addr" << " | "
              << std::setw(10) << "Word Addr" << " | "
              << std::setw(4) << "Byte\n";
    std::cout << std::string(65, '-') << "\n";

    for (const auto& tc : test_cases) {
        OutputStorage::OutputStats stats;
        storage.storeOutput(tc.out_y, tc.out_x, tc.out_c, 0, 0, &stats);

        uint32_t linear_addr = (tc.out_y * config.output_width + tc.out_x) * config.output_channels + tc.out_c;

        std::cout << std::setw(3) << tc.out_y << " | "
                  << std::setw(3) << tc.out_x << " | "
                  << std::setw(3) << tc.out_c << " | "
                  << std::setw(12) << linear_addr << " | "
                  << "0x" << std::setw(8) << std::setfill('0') << std::hex << stats.bram_addr << " | "
                  << std::setfill(' ') << std::dec
                  << std::setw(4) << (int)stats.byte_sel << "\n";

        ASSERT_EQ(tc.expected_addr, stats.bram_addr);
        ASSERT_EQ(tc.expected_byte, stats.byte_sel);
    }

    std::cout << std::setfill(' ');

    TEST_END();
}

void test_streaming() {
    TEST_BEGIN("OutputStorage - AXI-Stream Processing");

    OutputStorage::Config config = {
        4,      // output_height
        4,      // output_width
        4,      // output_channels
        false,  // enable_pooling
        0       // output_base_addr
    };

    OutputStorage storage(config);

    std::cout << "Simulating AXI-Stream data (1 pixel = 4 channels)\n\n";

    // Simulate one pixel's worth of outputs (4 values with tid 0-3)
    struct StreamData {
        int8_t tdata;
        uint8_t tid;
        bool tlast;
    };

    StreamData stream_data[] = {
        {10, 0, false},  // MAC 0 output
        {20, 1, false},  // MAC 1 output
        {30, 2, false},  // MAC 2 output
        {40, 3, true},   // MAC 3 output (TLAST)
    };

    std::vector<uint32_t> bram(256, 0);  // Simulated BRAM

    std::cout << std::setw(6) << "tdata" << " | "
              << std::setw(3) << "tid" << " | "
              << std::setw(5) << "tlast" << " | "
              << std::setw(8) << "Addr\n";
    std::cout << std::string(40, '-') << "\n";

    for (const auto& data : stream_data) {
        OutputStorage::OutputStats stats;
        auto update = storage.processStream(data.tdata, data.tid, data.tlast, bram, &stats);

        // Update simulated BRAM
        bram[update.addr] = update.data;

        std::cout << std::setw(6) << (int)data.tdata << " | "
                  << std::setw(3) << (int)data.tid << " | "
                  << std::setw(5) << (data.tlast ? "true" : "false") << " | "
                  << "0x" << std::setw(6) << std::setfill('0') << std::hex << update.addr << std::dec << "\n";
    }

    std::cout << std::setfill(' ');

    TEST_END();
}

void test_max_pooling() {
    TEST_BEGIN("OutputStorage - 2×2 Max Pooling");

    OutputStorage::Config config = {
        32,     // output_height
        32,     // output_width
        64,     // output_channels
        true,   // enable_pooling
        0       // output_base_addr
    };

    OutputStorage storage(config);

    struct TestCase {
        std::vector<int8_t> values;
        int8_t expected;
    };

    TestCase test_cases[] = {
        {{10, 20, 30, 40},         40},
        {{100, 50, 75, 25},        100},
        {{-50, -10, -30, -20},     -10},
        {{127, 127, 127, 127},     127},
    };

    std::cout << std::setw(20) << "Input Values" << " | "
              << std::setw(13) << "Expected Max" << " | "
              << std::setw(6) << "Result" << " | Status\n";
    std::cout << std::string(65, '-') << "\n";

    for (const auto& tc : test_cases) {
        int8_t result = storage.poolMax2x2(tc.values);
        bool pass = (result == tc.expected);

        char buf[32];
        snprintf(buf, sizeof(buf), "[%3d, %3d, %3d, %3d]",
                 tc.values[0], tc.values[1], tc.values[2], tc.values[3]);

        std::cout << std::setw(20) << buf << " | "
                  << std::setw(13) << (int)tc.expected << " | "
                  << std::setw(6) << (int)result << " | "
                  << (pass ? " PASS" : " FAIL") << "\n";

        ASSERT_EQ(tc.expected, result);
    }

    TEST_END();
}

int main() {
    std::cout << "\n";
    std::cout << "╔" << std::string(68, '=') << "╗\n";
    std::cout << "║" << std::string(68, ' ') << "║\n";
    std::cout << "║  OutputStorage C++ Test - BRAM RMW & Pooling Validation      ║\n";
    std::cout << "║" << std::string(68, ' ') << "║\n";
    std::cout << "╚" << std::string(68, '=') << "╝\n";

    test_basic_rmw();
    test_byte_packing();
    test_address_calculation();
    test_streaming();
    test_max_pooling();

    TestFramework::instance().printSummary();

    return TestFramework::instance().allPassed() ? 0 : 1;
}
