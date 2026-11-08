#include "zypher/core/Platform.h"

#include "zypher/core/Log.h"

#include <sys/utsname.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>

namespace zy::platform {

std::string_view name() { return "Linux"; }

std::string_view architecture() {
#if defined(__aarch64__)
    return "arm64";
#elif defined(__x86_64__)
    return "x86_64";
#else
    return "unknown";
#endif
}

std::string osVersion() {
    utsname information{};
    if (uname(&information) != 0) {
        return "unknown";
    }
    return std::string(information.sysname) + " " + information.release;
}

bool isDebuggerAttached() {
    // TracerPid is non-zero when a debugger is attached.
    std::FILE* file = std::fopen("/proc/self/status", "r");
    if (file == nullptr) {
        return false;
    }
    char line[256]{};
    bool attached = false;
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        int pid = 0;
        if (std::sscanf(line, "TracerPid: %d", &pid) == 1) {
            attached = pid != 0;
            break;
        }
    }
    std::fclose(file);
    return attached;
}

void debugBreak() { std::raise(SIGTRAP); }

void showErrorMessage(std::string_view title, std::string_view message) {
    std::fprintf(stderr, "\n[zypher] %.*s\n%*s%.*s\n", static_cast<int>(title.size()), title.data(),
                 static_cast<int>(title.size()), "", static_cast<int>(message.size()), message.data());
    std::fflush(stderr);
}

} // namespace zy::platform
