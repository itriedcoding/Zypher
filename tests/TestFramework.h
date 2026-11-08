// Minimal dependency free test framework. Tests are registered at static
// initialisation time and run from test_main.cpp.
#pragma once

#include <string>
#include <vector>

namespace zytest {

using TestFn = void (*)();

struct TestCase {
    std::string name;
    TestFn function = nullptr;
};

[[nodiscard]] std::vector<TestCase>& allTests();
[[nodiscard]] int failureCount();
[[nodiscard]] int checkCount();
[[nodiscard]] const char* currentTest();

void beginTest(const std::string& name);
void expect(bool condition, const char* expression, const char* file, int line);
void expectNear(double actual, double expected, double tolerance, const char* expression, const char* file,
                int line);
void expectStringEqual(const std::string& actual, const std::string& expected, const char* expression,
                       const char* file, int line);
void fail(const char* message, const char* file, int line);

struct Registrar {
    Registrar(std::string name, TestFn function);
};

} // namespace zytest

#define ZY_TEST(testName)                                                                    \
    static void testName();                                                                  \
    static const ::zytest::Registrar zytest_registrar_##testName(#testName, testName);       \
    static void testName()

#define EXPECT_TRUE(condition) ::zytest::expect((condition), #condition, __FILE__, __LINE__)
#define EXPECT_FALSE(condition) ::zytest::expect(!(condition), "!(" #condition ")", __FILE__, __LINE__)
#define EXPECT_EQ(a, b) ::zytest::expect((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define EXPECT_NE(a, b) ::zytest::expect((a) != (b), #a " != " #b, __FILE__, __LINE__)
#define EXPECT_NEAR(a, b, tolerance) \
    ::zytest::expectNear(static_cast<double>(a), static_cast<double>(b), (tolerance), #a " ~= " #b, __FILE__, __LINE__)
#define EXPECT_STR_EQ(a, b) ::zytest::expectStringEqual((a), (b), #a " == " #b, __FILE__, __LINE__)
#define TEST_FAIL(message) ::zytest::fail((message), __FILE__, __LINE__)
#define ASSERT_TRUE(condition)                 \
    do {                                       \
        ::zytest::expect((condition), #condition, __FILE__, __LINE__); \
        if (!(condition)) {                    \
            return;                            \
        }                                      \
    } while (false)
