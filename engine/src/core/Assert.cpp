#include "zypher/core/Assert.h"

#include "zypher/core/Log.h"

#include <cstdio>
#include <cstdlib>

namespace zy {
namespace {

AssertHandler g_handler = nullptr;

void defaultHandler(const char* expression, const char* message, const char* file, i32 line) {
    std::fprintf(stderr, "\n[zypher] assertion failed: %s\n  message : %s\n  location: %s:%d\n", expression,
                 message != nullptr ? message : "(none)", file, line);
    std::fflush(stderr);
}

} // namespace

void setAssertHandler(AssertHandler handler) {
    g_handler = handler;
}

void assertFailed(const char* expression, const char* message, const char* file, i32 line) {
    if (g_handler != nullptr) {
        g_handler(expression, message, file, line);
    } else {
        defaultHandler(expression, message, file, line);
    }
    Log::write(LogLevel::Error, "assert",
               detail::concat("assertion failed: ", expression, " (", message, ") at ", file, ":", line));
#if defined(ZYPHER_PLATFORM_WINDOWS)
    __debugbreak();
#else
    std::abort();
#endif
}

} // namespace zy
