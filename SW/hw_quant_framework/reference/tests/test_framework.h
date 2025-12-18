#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <iomanip>

/**
 * Simple test framework for C++ accelerator tests
 * No external dependencies - just standard library
 */

class TestFramework {
public:
    static TestFramework& instance() {
        static TestFramework inst;
        return inst;
    }

    void beginTest(const std::string& name) {
        current_test_ = name;
        test_passed_ = true;
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << name << "\n";
        std::cout << std::string(70, '=') << "\n\n";
    }

    void endTest() {
        if (test_passed_) {
            passed_tests_++;
            std::cout << "\n " << current_test_ << " PASSED\n";
        } else {
            failed_tests_++;
            std::cout << "\n " << current_test_ << " FAILED\n";
        }
    }

    template<typename T>
    void assertEqual(T expected, T actual, const std::string& msg = "") {
        if (expected != actual) {
            std::cout << "FAIL: " << msg << "\n";
            std::cout << "  Expected: " << expected << "\n";
            std::cout << "  Actual:   " << actual << "\n";
            test_passed_ = false;
        }
    }

    void assertTrue(bool condition, const std::string& msg = "") {
        if (!condition) {
            std::cout << "FAIL: " << msg << "\n";
            test_passed_ = false;
        }
    }

    void assertFalse(bool condition, const std::string& msg = "") {
        if (condition) {
            std::cout << "FAIL: " << msg << "\n";
            test_passed_ = false;
        }
    }

    template<typename T>
    void assertLess(T value, T limit, const std::string& msg = "") {
        if (!(value < limit)) {
            std::cout << "FAIL: " << msg << "\n";
            std::cout << "  Value: " << value << " should be < " << limit << "\n";
            test_passed_ = false;
        }
    }

    template<typename T>
    void assertGreaterEqual(T value, T limit, const std::string& msg = "") {
        if (!(value >= limit)) {
            std::cout << "FAIL: " << msg << "\n";
            std::cout << "  Value: " << value << " should be >= " << limit << "\n";
            test_passed_ = false;
        }
    }

    void printSummary() {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "TEST SUMMARY\n";
        std::cout << std::string(70, '=') << "\n";
        std::cout << "Passed: " << passed_tests_ << "\n";
        std::cout << "Failed: " << failed_tests_ << "\n";
        std::cout << "Total:  " << (passed_tests_ + failed_tests_) << "\n";

        if (failed_tests_ == 0) {
            std::cout << "\n ALL TESTS PASSED!\n\n";
        } else {
            std::cout << "\n SOME TESTS FAILED\n\n";
        }
    }

    bool allPassed() const {
        return failed_tests_ == 0;
    }

private:
    TestFramework() : passed_tests_(0), failed_tests_(0), test_passed_(true) {}

    int passed_tests_;
    int failed_tests_;
    bool test_passed_;
    std::string current_test_;
};

// Macro helpers
#define TEST_BEGIN(name) TestFramework::instance().beginTest(name)
#define TEST_END() TestFramework::instance().endTest()
#define ASSERT_EQ(expected, actual) TestFramework::instance().assertEqual(expected, actual, #expected " == " #actual)
#define ASSERT_TRUE(cond) TestFramework::instance().assertTrue(cond, #cond)
#define ASSERT_FALSE(cond) TestFramework::instance().assertFalse(cond, #cond)
#define ASSERT_LT(val, limit) TestFramework::instance().assertLess(val, limit, #val " < " #limit)
#define ASSERT_GE(val, limit) TestFramework::instance().assertGreaterEqual(val, limit, #val " >= " #limit)

#endif // TEST_FRAMEWORK_H
