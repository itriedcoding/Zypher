#include "zypher/crash/CrashReporter.h"

#include "zypher/core/FileSystem.h"
#include "zypher/core/Log.h"
#include "zypher/core/Platform.h"

#include <chrono>
#include <ctime>
#include <exception>

namespace zy {
namespace {

struct CrashState {
    std::string directory;
    std::string sessionPath;
    std::string markerPath;
    std::string summary;
    bool previousUncleanExit = false;
    bool active = false;
};

CrashState& state() {
    static CrashState instance;
    return instance;
}

[[nodiscard]] std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(ZYPHER_PLATFORM_WINDOWS)
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d", local.tm_year + 1900,
                  local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
    return std::string(buffer);
}

} // namespace

Status CrashReporter::initialize(std::string_view applicationName, const std::string& reportDirectory) {
    auto& crash = state();
    if (crash.active) {
        return Error{ErrorCode::InvalidArgument, "crash reporter already initialised"};
    }
    crash.directory = reportDirectory;
    if (!FileSystem::createDirectories(crash.directory)) {
        return Error{ErrorCode::Io, "cannot create crash report directory '" + crash.directory + "'"};
    }
    crash.sessionPath = FileSystem::join(crash.directory, ".session");
    crash.markerPath = FileSystem::join(crash.directory, "last_crash.txt");

    if (FileSystem::exists(crash.sessionPath)) {
        crash.previousUncleanExit = true;
        if (auto previous = FileSystem::readText(crash.markerPath); previous.has_value()) {
            crash.summary = *previous;
        } else {
            crash.summary = "previous session ended unexpectedly (no details recorded)";
        }
    }

    const std::string session = std::string(applicationName) + " started " + timestamp() + " on " +
                                std::string(platform::name()) + " " + platform::osVersion() + " (" +
                                std::string(platform::architecture()) + ")";
    if (const auto status = FileSystem::writeText(crash.sessionPath, session); status.hasError()) {
        return status;
    }
    crash.active = true;

    std::set_terminate([] {
        const std::string message = "terminate() called at " + timestamp();
        recordFatal(message);
    });

    if (crash.previousUncleanExit) {
        ZY_LOG_WARN("crash", "previous session did not exit cleanly: ", crash.summary);
    }
    return ok();
}

void CrashReporter::shutdown() {
    auto& crash = state();
    if (!crash.active) {
        return;
    }
    (void)FileSystem::remove(crash.sessionPath);
    crash.active = false;
}

bool CrashReporter::hasPreviousUncleanExit() { return state().previousUncleanExit; }

std::string CrashReporter::reportDirectory() { return state().directory; }

std::string CrashReporter::lastCrashSummary() { return state().summary; }

void CrashReporter::recordFatal(const std::string& message) {
    auto& crash = state();
    if (crash.directory.empty()) {
        return;
    }
    std::string entry = timestamp();
    entry += " | ";
    entry += platform::configuration();
    entry += " | ";
    entry += message;
    (void)FileSystem::writeText(crash.markerPath, entry);
    Log::write(LogLevel::Error, "crash", message);
}

} // namespace zy
