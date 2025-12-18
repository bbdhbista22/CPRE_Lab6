/**
 * Dequantization Test - C++ Implementation
 *
 * Tests Q8.24 fixed-point dequantization, saturation, and ReLU
 * Verifies correctness against Python reference implementation
 */

#include "../Dequantization.h"
#include "test_framework.h"
#include <iostream>
#include <iomanip>

void test_basic_dequantization() {
    TEST_BEGIN("Dequantization - Basic Functionality");

    // Scale factor = 0.5 in Q8.24
    Dequantization::Config config = {
        0,           // zero_point_in
        0,           // zero_point_out
        0x00800000,  // scale_factor (0.5 in Q8.24)
        true,        // enable_relu
        false        // enable_batch_norm
    };

    Dequantization dequant(config);

    std::cout << "Configuration:\n";
    std::cout << "  Zero-point in:  " << config.zero_point_in << "\n";
    std::cout << "  Zero-point out: " << config.zero_point_out << "\n";
    std::cout << "  Scale factor:   0x" << std::hex << config.scale_factor << " (Q8.24 = 0.5)" << std::dec << "\n";
    std::cout << "  Enable ReLU:    " << (config.enable_relu ? "true" : "false") << "\n\n";

    // Test cases: (input, expected_output, description)
    struct TestCase {
        int32_t input;
        int8_t expected;
        const char* description;
    };

    TestCase test_cases[] = {
        {0,      0,      "Zero input"},
        {100,    50,     "Positive (100 * 0.5 = 50)"},
        {200,    100,    "Large positive (200 * 0.5 = 100)"},
        {512,    127,    "Overflow (512 * 0.5 = 256, saturates to 127)"},
        {-100,   0,      "Negative with ReLU (-50 clipped to 0)"},
        {-50,    0,      "Negative with ReLU (-25 clipped to 0)"},
    };

    std::cout << std::setw(8) << "Input" << " | "
              << std::setw(8) << "Expected" << " | "
              << std::setw(8) << "Output" << " | "
              << std::setw(6) << "Status" << " | Description\n";
    std::cout << std::string(70, '-') << "\n";

    int passed = 0;
    int failed = 0;

    for (const auto& tc : test_cases) {
        Dequantization::OutputStats stats;
        int8_t result = dequant.dequantizeScalar(tc.input, &stats);

        bool pass = (result == tc.expected);
        if (pass) {
            passed++;
        } else {
            failed++;
        }

        std::cout << std::setw(8) << tc.input << " | "
                  << std::setw(8) << (int)tc.expected << " | "
                  << std::setw(8) << (int)result << " | "
                  << (pass ? " PASS" : " FAIL") << " | "
                  << tc.description << "\n";

        ASSERT_EQ(tc.expected, result);
    }

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";

    TEST_END();
}

void test_saturation() {
    TEST_BEGIN("Dequantization - Saturation to int8 Range");

    // Scale factor = 1.0 (no scaling) to test saturation directly
    Dequantization::Config config = {
        0,           // zero_point_in
        0,           // zero_point_out
        0x01000000,  // scale_factor (1.0 in Q8.24)
        false,       // enable_relu (disabled for saturation test)
        false        // enable_batch_norm
    };

    Dequantization dequant(config);

    std::cout << "Configuration:\n";
    std::cout << "  Scale factor:   0x" << std::hex << config.scale_factor << " (Q8.24 = 1.0)" << std::dec << "\n";
    std::cout << "  Enable ReLU:    false (testing saturation boundaries)\n\n";

    struct TestCase {
        int32_t input;
        int8_t expected;
        const char* description;
    };

    TestCase test_cases[] = {
        {0,     0,      "Zero"},
        {127,   127,    "Max positive"},
        {128,   127,    "Overflow +1"},
        {255,   127,    "Large overflow"},
        {-128,  -128,   "Min negative"},
        {-129,  -128,   "Underflow -1"},
        {-200,  -128,   "Large underflow"},
    };

    std::cout << std::setw(8) << "Input" << " | "
              << std::setw(8) << "Expected" << " | "
              << std::setw(8) << "Output" << " | "
              << std::setw(6) << "Status" << " | Description\n";
    std::cout << std::string(70, '-') << "\n";

    for (const auto& tc : test_cases) {
        int8_t result = dequant.dequantizeScalar(tc.input);
        bool pass = (result == tc.expected);

        std::cout << std::setw(8) << tc.input << " | "
                  << std::setw(8) << (int)tc.expected << " | "
                  << std::setw(8) << (int)result << " | "
                  << (pass ? " PASS" : " FAIL") << " | "
                  << tc.description << "\n";

        ASSERT_EQ(tc.expected, result);
    }

    TEST_END();
}

void test_relu() {
    TEST_BEGIN("Dequantization - ReLU Activation");

    // Scale factor = 1.0 for simplicity
    Dequantization::Config config = {
        0,           // zero_point_in
        0,           // zero_point_out
        0x01000000,  // scale_factor (1.0 in Q8.24)
        true,        // enable_relu
        false        // enable_batch_norm
    };

    Dequantization dequant(config);

    std::cout << "Configuration:\n";
    std::cout << "  Enable ReLU:    true\n";
    std::cout << "  (Testing ReLU behavior)\n\n";

    struct TestCase {
        int32_t input;
        int8_t expected;
        const char* description;
    };

    TestCase test_cases[] = {
        {100,   100,    "Positive value passed through"},
        {50,    50,     "Positive value passed through"},
        {0,     0,      "Zero boundary"},
        {-1,    0,      "Negative clipped to 0"},
        {-50,   0,      "Large negative clipped to 0"},
        {-128,  0,      "Min negative clipped to 0"},
    };

    std::cout << std::setw(8) << "Input" << " | "
              << std::setw(8) << "Expected" << " | "
              << std::setw(8) << "Output" << " | "
              << std::setw(6) << "Status" << " | Description\n";
    std::cout << std::string(70, '-') << "\n";

    for (const auto& tc : test_cases) {
        int8_t result = dequant.dequantizeScalar(tc.input);
        bool pass = (result == tc.expected);

        std::cout << std::setw(8) << tc.input << " | "
                  << std::setw(8) << (int)tc.expected << " | "
                  << std::setw(8) << (int)result << " | "
                  << (pass ? " PASS" : " FAIL") << " | "
                  << tc.description << "\n";

        ASSERT_EQ(tc.expected, result);
    }

    TEST_END();
}

void test_vector_operations() {
    TEST_BEGIN("Dequantization - Vector Operations");

    Dequantization::Config config = {
        0,           // zero_point_in
        0,           // zero_point_out
        0x00800000,  // scale_factor (0.5 in Q8.24)
        true,        // enable_relu
        false        // enable_batch_norm
    };

    Dequantization dequant(config);

    std::cout << "Configuration:\n";
    std::cout << "  Scale factor:   0x" << std::hex << config.scale_factor << " (Q8.24 = 0.5)" << std::dec << "\n";
    std::cout << "  Enable ReLU:    true\n\n";

    // Test vector
    std::vector<int32_t> accums = {0, 100, 200, -100, -50, 300};
    std::vector<int8_t> expected = {0, 50, 100, 0, 0, 127};  // 300*0.5=150, saturates to 127

    auto results = dequant.dequantizeVector(accums);

    ASSERT_EQ(expected.size(), results.size());

    std::cout << "Vector dequantization test:\n";
    std::cout << std::setw(5) << "Index" << " | "
              << std::setw(8) << "Input" << " | "
              << std::setw(8) << "Expected" << " | "
              << std::setw(8) << "Output" << " | "
              << std::setw(6) << "Status\n";
    std::cout << std::string(55, '-') << "\n";

    for (size_t i = 0; i < accums.size(); i++) {
        bool pass = (results[i] == expected[i]);

        std::cout << std::setw(5) << i << " | "
                  << std::setw(8) << accums[i] << " | "
                  << std::setw(8) << (int)expected[i] << " | "
                  << std::setw(8) << (int)results[i] << " | "
                  << (pass ? " PASS" : " FAIL") << "\n";

        ASSERT_EQ(expected[i], results[i]);
    }

    std::cout << "\nVector length: " << accums.size() << "\n";

    TEST_END();
}

int main() {
    std::cout << "\n";
    std::cout << "╔" << std::string(68, '=') << "╗\n";
    std::cout << "║" << std::string(68, ' ') << "║\n";
    std::cout << "║  Dequantization C++ Test - Q8.24 Fixed-Point Validation      ║\n";
    std::cout << "║" << std::string(68, ' ') << "║\n";
    std::cout << "╚" << std::string(68, '=') << "╝\n";

    test_basic_dequantization();
    test_saturation();
    test_relu();
    test_vector_operations();

    TestFramework::instance().printSummary();

    return TestFramework::instance().allPassed() ? 0 : 1;
}
