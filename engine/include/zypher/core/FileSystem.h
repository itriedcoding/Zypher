// File system helpers. All paths are UTF-8 strings so they round trip through
// JSON, the asset database and the pack file format.
#pragma once

#include "zypher/core/Base.h"
#include "zypher/core/Result.h"

#include <optional>
#include <string>
#include <vector>

namespace zy {

class FileSystem {
public:
    [[nodiscard]] static std::optional<std::vector<u8>> readBinary(const std::string& path);
    [[nodiscard]] static std::optional<std::string> readText(const std::string& path);

    [[nodiscard]] static Status writeBinary(const std::string& path, ByteSpan data);
    [[nodiscard]] static Status writeText(const std::string& path, std::string_view text);

    [[nodiscard]] static bool exists(const std::string& path);
    [[nodiscard]] static bool isDirectory(const std::string& path);
    [[nodiscard]] static bool createDirectories(const std::string& path);
    [[nodiscard]] static bool remove(const std::string& path);

    [[nodiscard]] static std::string join(std::string_view left, std::string_view right);
    [[nodiscard]] static std::string fileName(std::string_view path);
    [[nodiscard]] static std::string stem(std::string_view path);
    [[nodiscard]] static std::string extension(std::string_view path); // includes the dot, lower case
    [[nodiscard]] static std::string parentPath(std::string_view path);
    [[nodiscard]] static std::string normalize(std::string_view path);
    [[nodiscard]] static std::string absolute(std::string_view path);
    [[nodiscard]] static std::vector<std::string> listFiles(const std::string& directory, bool recursive = false);

    [[nodiscard]] static std::string currentWorkingDirectory();
    [[nodiscard]] static std::string executableDirectory();
    [[nodiscard]] static std::string temporaryDirectory();

    [[nodiscard]] static u64 fileSize(const std::string& path);

    // Resolves `path` against a list of search roots, returning the first hit.
    [[nodiscard]] static std::optional<std::string> resolve(const std::string& path,
                                                            const std::vector<std::string>& roots);
};

} // namespace zy
