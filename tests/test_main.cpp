#include "TestFramework.h"

#include "zypher/core/Log.h"

#include <cstdio>
#include <string>
#include <vector>

namespace zytest {
namespace {

std::vector<TestCase> g_tests;
int g_failures = 0;
int g_checks = 0;
std::string g_currentTest;

} // namespace

std::vector<TestCase>& allTests() { return g_tests; }
int failureCount() { return g_failures; }
int checkCount() { return g_checks; }
const char* currentTest() { return g_currentTest.c_str(); }

void beginTest(const std::string& name) { g_currentTest = name; }

Registrar::Registrar(std::string name, TestFn function) {
    g_tests.push_back(TestCase{std::move(name), function});
}

void expect(bool condition, const char* expression, const char* file, int line) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::fprintf(stderr, "  FAIL [%s] %s\n        at %s:%d\n", g_currentTest.c_str(), expression, file, line);
    }
}

void expectNear(double actual, double expected, double tolerance, const char* expression, const char* file,
                int line) {
    ++g_checks;
    const double difference = actual > expected ? actual - expected : expected - actual;
    if (difference > tolerance) {
        ++g_failures;
        std::fprintf(stderr, "  FAIL [%s] %s (actual %.9g, expected %.9g, diff %.9g)\n        at %s:%d\n",
                     g_currentTest.c_str(), expression, actual, expected, difference, file, line);
    }
}

void expectStringEqual(const std::string& actual, const std::string& expected, const char* expression,
                       const char* file, int line) {
    ++g_checks;
    if (actual != expected) {
        ++g_failures;
        std::fprintf(stderr, "  FAIL [%s] %s\n        actual  : '%s'\n        expected: '%s'\n        at %s:%d\n",
                     g_currentTest.c_str(), expression, actual.c_str(), expected.c_str(), file, line);
    }
}

void fail(const char* message, const char* file, int line) {
    ++g_checks;
    ++g_failures;
    std::fprintf(stderr, "  FAIL [%s] %s\n        at %s:%d\n", g_currentTest.c_str(), message, file, line);
}

} // namespace zytest

int main(int argc, const char* const* argv) {
    zy::Log::initialize("Zypher Tests");
    zy::Log::setLevel(zy::LogLevel::Warn);

    std::string filter;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--filter" && index + 1 < argc) {
            filter = argv[++index];
        } else if (argument == "--list") {
            for (const auto& test : zytest::allTests()) {
                std::printf("%s\n", test.name.c_str());
            }
            zy::Log::shutdown();
            return 0;
        }
    }

    int run = 0;
    int failedTests = 0;
    for (const auto& test : zytest::allTests()) {
        if (!filter.empty() && test.name.find(filter) == std::string::npos) {
            continue;
        }
        ++run;
        const int failuresBefore = zytest::failureCount();
        zytest::beginTest(test.name);
        test.function();
        if (zytest::failureCount() > failuresBefore) {
            ++failedTests;
        }
    }

    std::printf("\n%zu test(s) discovered, %d run, %d failed, %d check(s)\n", zytest::allTests().size(), run,
                failedTests, zytest::checkCount());
    zy::Log::shutdown();
    return failedTests == 0 ? 0 : 1;
}
