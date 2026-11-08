#include "zypher/core/FileSystem.h"

#include "zypher/core/Log.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

#if defined(ZYPHER_PLATFORM_WINDOWS)
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace zy {
namespace {

// UTF-8 std::string -> filesystem path (correct on every platform, unlike the
// deprecated u8path overloads).
[[nodiscard]] fs::path toPath(std::string_view text) {
    const std::u8string encoded(reinterpret_cast<const char8_t*>(text.data()), text.size());
    return fs::path(encoded);
}

} // namespace

std::optional<std::vector<u8>> FileSystem::readBinary(const std::string& path) {
    std::error_code errorCode;
    const auto size = fs::file_size(toPath(path), errorCode);
    if (errorCode) {
        ZY_LOG_WARN("fs", "cannot stat '", path, "': ", errorCode.message());
        return std::nullopt;
    }
    std::ifstream stream(toPath(path), std::ios::binary);
    if (!stream.is_open()) {
        ZY_LOG_WARN("fs", "cannot open '", path, "' for reading");
        return std::nullopt;
    }
    std::vector<u8> data(static_cast<usize>(size));
    if (size > 0) {
        stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        if (!stream) {
            ZY_LOG_WARN("fs", "short read on '", path, "'");
            return std::nullopt;
        }
    }
    return data;
}

std::optional<std::string> FileSystem::readText(const std::string& path) {
    auto data = readBinary(path);
    if (!data.has_value()) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char*>(data->data()), data->size());
}

Status FileSystem::writeBinary(const std::string& path, ByteSpan data) {
    const auto parent = parentPath(path);
    if (!parent.empty() && !exists(parent) && !createDirectories(parent)) {
        return Error{ErrorCode::Io, "cannot create directory '" + parent + "'"};
    }
    std::ofstream stream(toPath(path), std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return Error{ErrorCode::Io, "cannot open '" + path + "' for writing"};
    }
    if (!data.empty()) {
        stream.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    if (!stream) {
        return Error{ErrorCode::Io, "failed writing '" + path + "'"};
    }
    return ok();
}

Status FileSystem::writeText(const std::string& path, std::string_view text) {
    return writeBinary(path, ByteSpan{reinterpret_cast<const u8*>(text.data()), text.size()});
}

bool FileSystem::exists(const std::string& path) {
    std::error_code errorCode;
    return fs::exists(toPath(path), errorCode);
}

bool FileSystem::isDirectory(const std::string& path) {
    std::error_code errorCode;
    return fs::is_directory(toPath(path), errorCode);
}

bool FileSystem::createDirectories(const std::string& path) {
    std::error_code errorCode;
    fs::create_directories(toPath(path), errorCode);
    if (errorCode) {
        ZY_LOG_WARN("fs", "cannot create directories '", path, "': ", errorCode.message());
        return false;
    }
    return true;
}

bool FileSystem::remove(const std::string& path) {
    std::error_code errorCode;
    fs::remove(toPath(path), errorCode);
    return !errorCode;
}

std::string FileSystem::join(std::string_view left, std::string_view right) {
    if (left.empty()) {
        return std::string(right);
    }
    if (right.empty()) {
        return std::string(left);
    }
    std::string result(left);
    if (result.back() != '/' && result.back() != '\\') {
        result.push_back('/');
    }
    result.append(right);
    return normalize(result);
}

std::string FileSystem::fileName(std::string_view path) {
    const auto position = path.find_last_of("/\\");
    return std::string(position == std::string_view::npos ? path : path.substr(position + 1));
}

std::string FileSystem::stem(std::string_view path) {
    const auto name = fileName(path);
    const auto position = name.find_last_of('.');
    return (position == std::string::npos || position == 0) ? name : name.substr(0, position);
}

std::string FileSystem::extension(std::string_view path) {
    const auto name = fileName(path);
    const auto position = name.find_last_of('.');
    if (position == std::string::npos || position == 0) {
        return {};
    }
    std::string result = name.substr(position);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
}

std::string FileSystem::parentPath(std::string_view path) {
    const auto position = path.find_last_of("/\\");
    if (position == std::string_view::npos) {
        return {};
    }
    return std::string(path.substr(0, position));
}

std::string FileSystem::normalize(std::string_view path) {
    std::string result(path);
    std::replace(result.begin(), result.end(), '\\', '/');
    std::string collapsed;
    collapsed.reserve(result.size());
    for (const char character : result) {
        if (character == '/' && !collapsed.empty() && collapsed.back() == '/') {
            continue;
        }
        collapsed.push_back(character);
    }
    return collapsed;
}

std::string FileSystem::absolute(std::string_view path) {
    std::error_code errorCode;
    const auto absolutePath = fs::absolute(toPath(path), errorCode);
    if (errorCode) {
        return std::string(path);
    }
    return normalize(absolutePath.string());
}

std::vector<std::string> FileSystem::listFiles(const std::string& directory, bool recursive) {
    std::vector<std::string> files;
    std::error_code errorCode;
    if (!fs::is_directory(toPath(directory), errorCode)) {
        return files;
    }
    const auto collect = [&files](auto& iterator) {
        std::error_code entryError;
        for (const auto& entry : iterator) {
            if (entry.is_regular_file(entryError)) {
                files.push_back(normalize(entry.path().generic_string()));
            }
        }
    };
    if (recursive) {
        fs::recursive_directory_iterator iterator(toPath(directory), errorCode);
        if (errorCode) {
            return files;
        }
        collect(iterator);
    } else {
        fs::directory_iterator iterator(toPath(directory), errorCode);
        if (errorCode) {
            return files;
        }
        collect(iterator);
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string FileSystem::currentWorkingDirectory() {
    return normalize(fs::current_path().string());
}

std::string FileSystem::executableDirectory() {
#if defined(ZYPHER_PLATFORM_WINDOWS)
    char buffer[4096]{};
    const auto length = GetModuleFileNameA(nullptr, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (length > 0) {
        return parentPath(std::string(buffer, length));
    }
    return currentWorkingDirectory();
#elif defined(ZYPHER_PLATFORM_MACOS)
    extern std::string macosExecutableDirectory();
    return macosExecutableDirectory();
#else
    std::error_code errorCode;
    const auto target = fs::read_symlink("/proc/self/exe", errorCode);
    if (!errorCode) {
        return parentPath(normalize(target.string()));
    }
    return currentWorkingDirectory();
#endif
}

std::string FileSystem::temporaryDirectory() {
    std::error_code errorCode;
    const auto directory = fs::temp_directory_path(errorCode);
    return errorCode ? std::string(".") : normalize(directory.string());
}

u64 FileSystem::fileSize(const std::string& path) {
    std::error_code errorCode;
    const auto size = fs::file_size(toPath(path), errorCode);
    return errorCode ? 0 : static_cast<u64>(size);
}

std::optional<std::string> FileSystem::resolve(const std::string& path, const std::vector<std::string>& roots) {
    if (exists(path)) {
        return path;
    }
    for (const auto& root : roots) {
        const auto candidate = join(root, path);
        if (exists(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

} // namespace zy
