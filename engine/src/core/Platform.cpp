#include "zypher/core/Platform.h"

#include "zypher/core/FileSystem.h"

#include <chrono>
#include <cstdio>
#include <thread>

namespace zy::platform {

std::string_view configuration() {
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
}

void sleepMilliseconds(u32 milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

std::string machineIdentifier() {
    // Derived from the executable path so it is stable per installation without
    // touching any user identifying data.
    const auto path = FileSystem::executableDirectory();
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%016llx",
                  static_cast<unsigned long long>(fnv1a64(path, 0x9e3779b97f4a7c15ULL)));
    return std::string(buffer);
}

} // namespace zy::platform
