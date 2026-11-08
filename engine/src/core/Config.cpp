#include "zypher/core/Config.h"

#include "zypher/core/FileSystem.h"
#include "zypher/core/Log.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>

namespace zy {
namespace {

[[nodiscard]] std::string lowerCopy(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
}

[[nodiscard]] std::string trim(std::string_view text) {
    usize begin = 0;
    usize end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

[[nodiscard]] std::string stripQuotes(std::string value) {
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

// Flags that never take a value. Without this, `--dump-frame positional` would
// swallow the positional argument as the flag's value.
[[nodiscard]] bool isBooleanFlag(std::string_view key) {
    static constexpr std::string_view kBooleanFlags[] = {
        "help", "version", "verbose", "quiet", "list-assets", "dump-frame", "fullscreen",
        "vsync", "no-window", "gpu-validation", "validate-only", "headless",
    };
    for (const std::string_view flag : kBooleanFlags) {
        if (key == flag) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool parseBool(std::string_view text, bool fallback) {
    const auto lowered = lowerCopy(trim(text));
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off") {
        return false;
    }
    return fallback;
}

} // namespace

void CommandLine::parse(int argc, const char* const* argv) {
    m_options.clear();
    m_positional.clear();
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--") {
            for (int rest = index + 1; rest < argc; ++rest) {
                m_positional.emplace_back(argv[rest]);
            }
            break;
        }
        if (argument.size() > 2 && argument.substr(0, 2) == "--") {
            std::string_view body = argument.substr(2);
            const auto equals = body.find('=');
            if (equals != std::string_view::npos) {
                m_options[std::string(body.substr(0, equals))] = std::string(body.substr(equals + 1));
                continue;
            }
            const std::string key(body);
            if (index + 1 < argc && !isBooleanFlag(key)) {
                const std::string_view next(argv[index + 1]);
                if (!(next.size() > 2 && next.substr(0, 2) == "--")) {
                    m_options[key] = std::string(next);
                    ++index;
                    continue;
                }
            }
            m_options[key] = "1";
            continue;
        }
        if (argument.size() > 1 && argument.front() == '-' && argument.size() < 3) {
            m_options[std::string(argument.substr(1))] = "1";
            continue;
        }
        m_positional.emplace_back(argument);
    }
}

bool CommandLine::has(std::string_view flag) const {
    return m_options.find(std::string(flag)) != m_options.end();
}

std::optional<std::string> CommandLine::get(std::string_view key) const {
    const auto iterator = m_options.find(std::string(key));
    if (iterator == m_options.end()) {
        return std::nullopt;
    }
    return iterator->second;
}

std::string CommandLine::getString(std::string_view key, std::string_view fallback) const {
    const auto iterator = m_options.find(std::string(key));
    return iterator == m_options.end() ? std::string(fallback) : iterator->second;
}

i64 CommandLine::getInt(std::string_view key, i64 fallback) const {
    const auto iterator = m_options.find(std::string(key));
    if (iterator == m_options.end()) {
        return fallback;
    }
    errno = 0;
    char* end = nullptr;
    const long long parsed = std::strtoll(iterator->second.c_str(), &end, 10);
    if (errno != 0 || end == iterator->second.c_str()) {
        return fallback;
    }
    return static_cast<i64>(parsed);
}

f64 CommandLine::getFloat(std::string_view key, f64 fallback) const {
    const auto iterator = m_options.find(std::string(key));
    if (iterator == m_options.end()) {
        return fallback;
    }
    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(iterator->second.c_str(), &end);
    if (errno != 0 || end == iterator->second.c_str()) {
        return fallback;
    }
    return parsed;
}

bool CommandLine::getBool(std::string_view key, bool fallback) const {
    const auto iterator = m_options.find(std::string(key));
    if (iterator == m_options.end()) {
        return fallback;
    }
    return parseBool(iterator->second, fallback);
}

IniFile IniFile::parse(std::string_view text) {
    IniFile file;
    std::string section;
    usize position = 0;
    while (position < text.size()) {
        const auto lineEnd = text.find_first_of("\r\n", position);
        const auto line = trim(text.substr(position, lineEnd == std::string_view::npos ? text.size() - position
                                                                                       : lineEnd - position));
        position = (lineEnd == std::string_view::npos) ? text.size() : lineEnd + 1;
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = trim(std::string_view(line).substr(1, line.size() - 2));
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const auto key = trim(std::string_view(line).substr(0, equals));
        const auto value = stripQuotes(trim(std::string_view(line).substr(equals + 1)));
        file.m_entries[section.empty() ? key : section + "." + key] = value;
    }
    return file;
}

std::optional<IniFile> IniFile::load(const std::string& path) {
    const auto text = FileSystem::readText(path);
    if (!text.has_value()) {
        return std::nullopt;
    }
    return parse(*text);
}

bool IniFile::contains(std::string_view key) const {
    return m_entries.find(std::string(key)) != m_entries.end();
}

std::string IniFile::getString(std::string_view key, std::string_view fallback) const {
    const auto iterator = m_entries.find(std::string(key));
    return iterator == m_entries.end() ? std::string(fallback) : iterator->second;
}

i64 IniFile::getInt(std::string_view key, i64 fallback) const {
    const auto iterator = m_entries.find(std::string(key));
    if (iterator == m_entries.end()) {
        return fallback;
    }
    errno = 0;
    char* end = nullptr;
    const long long parsed = std::strtoll(iterator->second.c_str(), &end, 10);
    return (errno != 0 || end == iterator->second.c_str()) ? fallback : static_cast<i64>(parsed);
}

f64 IniFile::getFloat(std::string_view key, f64 fallback) const {
    const auto iterator = m_entries.find(std::string(key));
    if (iterator == m_entries.end()) {
        return fallback;
    }
    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(iterator->second.c_str(), &end);
    return (errno != 0 || end == iterator->second.c_str()) ? fallback : parsed;
}

bool IniFile::getBool(std::string_view key, bool fallback) const {
    const auto iterator = m_entries.find(std::string(key));
    return iterator == m_entries.end() ? fallback : parseBool(iterator->second, fallback);
}

} // namespace zy
