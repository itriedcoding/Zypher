// Runtime assertions. These stay enabled in release builds: the engine treats a
// broken invariant as a hard error rather than undefined behaviour.
#pragma once

#include "zypher/core/Base.h"

namespace zy {

using AssertHandler = void (*)(const char* expression, const char* message, const char* file, i32 line);

// Installs a custom handler (for example one that shows a native message box).
// Passing nullptr restores the default handler.
void setAssertHandler(AssertHandler handler);

[[noreturn]] void assertFailed(const char* expression, const char* message, const char* file, i32 line);

} // namespace zy

#define ZY_ASSERT(expression, message)                                             \
    do {                                                                           \
        if (!(expression)) {                                                       \
            ::zy::assertFailed(#expression, message, __FILE__, __LINE__);          \
        }                                                                          \
    } while (false)

#define ZY_VERIFY(expression) ZY_ASSERT(expression, "verification failed")
