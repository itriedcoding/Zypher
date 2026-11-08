// Crash reporting.
//
// Writes a session marker on start and removes it on clean shutdown, so the
// next launch can detect an unclean exit and offer to attach the log. When
// ZYPHER_WITH_CRASHPAD is enabled the handler is registered with Crashpad;
// the built-in marker based detection works everywhere.
#pragma once

#include "zypher/core/Base.h"
#include "zypher/core/Result.h"

#include <string>

namespace zy {

class CrashReporter final : NonCopyable {
public:
    [[nodiscard]] static Status initialize(std::string_view applicationName, const std::string& reportDirectory);
    static void shutdown();

    [[nodiscard]] static bool hasPreviousUncleanExit();
    [[nodiscard]] static std::string reportDirectory();
    [[nodiscard]] static std::string lastCrashSummary();

    // Records a non-fatal diagnostic (also forwarded to the crash backend when
    // available). The engine calls this for fatal errors before shutting down.
    static void recordFatal(const std::string& message);
};

} // namespace zy
