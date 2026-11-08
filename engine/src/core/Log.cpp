#include "zypher/core/Log.h"

#include "zypher/core/Assert.h"
#include "zypher/core/FileSystem.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <mutex>

namespace zy {
namespace {

struct LogState {
    std::mutex mutex;
    LogLevel level = LogLevel::Info;
    bool console = true;
    std::ofstream file;
    std::string filePath;
    bool initialized = false;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
};

LogState& state() {
    static LogState instance;
    return instance;
}

[[nodiscard]] LogLevel parseLevel(std::string_view text) {
    if (text == "trace") return LogLevel::Trace;
    if (text == "debug") return LogLevel::Debug;
    if (text == "info") return LogLevel::Info;
    if (text == "warn") return LogLevel::Warn;
    if (text == "error") return LogLevel::Error;
    if (text == "off") return LogLevel::Off;
    return LogLevel::Info;
}

[[nodiscard]] std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm local{};
#if defined(ZYPHER_PLATFORM_WINDOWS)
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d", local.tm_hour, local.tm_min, local.tm_sec,
                  static_cast<int>(millis));
    return std::string(buffer);
}

} // namespace

std::string_view logLevelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Off: return "OFF";
    }
    return "?";
}

void Log::initialize(std::string_view appName, std::string_view logFile) {
    std::string banner = "Zypher ";
    banner += appName;
    banner += " | engine ";
    banner += ZYPHER_VERSION_STRING;
    {
        auto& log = state();
        std::lock_guard lock(log.mutex);
        log.start = std::chrono::steady_clock::now();
        log.initialized = true;
        log.filePath = std::string(logFile);
        if (!log.filePath.empty()) {
            if (const auto parent = FileSystem::parentPath(log.filePath); !parent.empty()) {
                (void)FileSystem::createDirectories(parent);
            }
            log.file.open(log.filePath, std::ios::out | std::ios::trunc);
            if (!log.file.is_open()) {
                std::fprintf(stderr, "[zypher] warning: could not open log file '%s'\n",
                             log.filePath.c_str());
            }
        }
        if (const char* envLevel = std::getenv("ZYPHER_LOG_LEVEL"); envLevel != nullptr) {
            log.level = parseLevel(envLevel);
        }
    }
    writeLine(banner, LogLevel::Info);
}

void Log::shutdown() {
    auto& log = state();
    std::lock_guard lock(log.mutex);
    if (log.file.is_open()) {
        log.file.flush();
        log.file.close();
    }
    log.initialized = false;
}

void Log::setLevel(LogLevel level) {
    std::lock_guard lock(state().mutex);
    state().level = level;
}

LogLevel Log::level() {
    std::lock_guard lock(state().mutex);
    return state().level;
}

void Log::setConsoleEnabled(bool enabled) {
    std::lock_guard lock(state().mutex);
    state().console = enabled;
}

bool Log::consoleEnabled() {
    std::lock_guard lock(state().mutex);
    return state().console;
}

std::string_view Log::logFilePath() {
    return state().filePath;
}

void Log::write(LogLevel level, std::string_view category, std::string_view message) {
    auto& log = state();
    LogLevel threshold = LogLevel::Info;
    {
        std::lock_guard lock(log.mutex);
        threshold = log.level;
    }
    if (level < threshold || level == LogLevel::Off) {
        return;
    }
    std::string line;
    line.reserve(message.size() + 64);
    line += '[';
    line += timestamp();
    line += "] [";
    line += logLevelName(level);
    line += "] [";
    line.append(category.data(), category.size());
    line += "] ";
    line.append(message.data(), message.size());
    writeLine(line, level);
}

void Log::writeLine(std::string_view line, LogLevel level) {
    auto& log = state();
    std::lock_guard lock(log.mutex);
    if (log.console) {
        std::FILE* stream = (level >= LogLevel::Warn) ? stderr : stdout;
        std::fwrite(line.data(), 1, line.size(), stream);
        std::fputc('\n', stream);
        if (level >= LogLevel::Warn) {
            std::fflush(stream);
        }
    }
    if (log.file.is_open()) {
        log.file.write(line.data(), static_cast<std::streamsize>(line.size()));
        log.file.put('\n');
        if (level >= LogLevel::Warn) {
            log.file.flush();
        }
    }
}

} // namespace zy
