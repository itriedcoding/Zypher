// Logging: levelled, timestamped, thread safe, console + file sinks.
#pragma once

#include "zypher/core/Base.h"

#include <sstream>
#include <string>
#include <string_view>

namespace zy {

enum class LogLevel : u8 { Trace = 0, Debug, Info, Warn, Error, Off };

[[nodiscard]] std::string_view logLevelName(LogLevel level);

class Log {
public:
    // `logFile` may be empty to disable file logging. The ZYPHER_LOG_LEVEL
    // environment variable (trace|debug|info|warn|error|off) overrides the level.
    static void initialize(std::string_view appName, std::string_view logFile = {});
    static void shutdown();

    static void setLevel(LogLevel level);
    [[nodiscard]] static LogLevel level();
    static void setConsoleEnabled(bool enabled);
    [[nodiscard]] static bool consoleEnabled();
    [[nodiscard]] static std::string_view logFilePath();

    static void write(LogLevel level, std::string_view category, std::string_view message);

private:
    static void writeLine(std::string_view line, LogLevel level);
};

namespace detail {

template <class... Args>
[[nodiscard]] std::string concat(Args&&... args) {
    std::ostringstream stream;
    (stream << ... << std::forward<Args>(args));
    return stream.str();
}

} // namespace detail

} // namespace zy

#define ZY_LOG_TRACE(category, ...) \
    ::zy::Log::write(::zy::LogLevel::Trace, category, ::zy::detail::concat(__VA_ARGS__))
#define ZY_LOG_DEBUG(category, ...) \
    ::zy::Log::write(::zy::LogLevel::Debug, category, ::zy::detail::concat(__VA_ARGS__))
#define ZY_LOG_INFO(category, ...) \
    ::zy::Log::write(::zy::LogLevel::Info, category, ::zy::detail::concat(__VA_ARGS__))
#define ZY_LOG_WARN(category, ...) \
    ::zy::Log::write(::zy::LogLevel::Warn, category, ::zy::detail::concat(__VA_ARGS__))
#define ZY_LOG_ERROR(category, ...) \
    ::zy::Log::write(::zy::LogLevel::Error, category, ::zy::detail::concat(__VA_ARGS__))
