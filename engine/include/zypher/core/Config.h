// Command line and INI configuration parsing.
#pragma once

#include "zypher/core/Base.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zy {

class CommandLine {
public:
    void parse(int argc, const char* const* argv);

    [[nodiscard]] bool has(std::string_view flag) const;
    [[nodiscard]] std::optional<std::string> get(std::string_view key) const;
    [[nodiscard]] std::string getString(std::string_view key, std::string_view fallback) const;
    [[nodiscard]] i64 getInt(std::string_view key, i64 fallback) const;
    [[nodiscard]] f64 getFloat(std::string_view key, f64 fallback) const;
    [[nodiscard]] bool getBool(std::string_view key, bool fallback) const;

    [[nodiscard]] const std::vector<std::string>& positional() const { return m_positional; }
    [[nodiscard]] const std::map<std::string, std::string>& options() const { return m_options; }

private:
    std::map<std::string, std::string> m_options;
    std::vector<std::string> m_positional;
};

class IniFile {
public:
    [[nodiscard]] static IniFile parse(std::string_view text);
    [[nodiscard]] static std::optional<IniFile> load(const std::string& path);

    [[nodiscard]] bool contains(std::string_view key) const;
    [[nodiscard]] std::string getString(std::string_view key, std::string_view fallback) const;
    [[nodiscard]] i64 getInt(std::string_view key, i64 fallback) const;
    [[nodiscard]] f64 getFloat(std::string_view key, f64 fallback) const;
    [[nodiscard]] bool getBool(std::string_view key, bool fallback) const;
    [[nodiscard]] const std::map<std::string, std::string>& entries() const { return m_entries; }

private:
    std::map<std::string, std::string> m_entries;
};

} // namespace zy
