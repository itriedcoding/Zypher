#include "zypher/core/Platform.h"

#include "zypher/core/Log.h"

#include <windows.h>

#include <string>

namespace zy::platform {
namespace {

[[nodiscard]] std::wstring widen(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<usize>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

} // namespace

std::string_view name() { return "Windows"; }

std::string_view architecture() {
#if defined(_M_ARM64)
    return "arm64";
#elif defined(_M_X64)
    return "x86_64";
#elif defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

std::string osVersion() {
    // RtlGetVersion reports the real version even when the manifest requests
    // compatibility mode (unlike the deprecated GetVersion).
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    if (HMODULE ntdll = GetModuleHandleW(L"ntdll.dll"); ntdll != nullptr) {
        const auto function = reinterpret_cast<RtlGetVersionFn>(
            reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")));
        if (function != nullptr) {
            RTL_OSVERSIONINFOW information{};
            information.dwOSVersionInfoSize = sizeof(information);
            if (function(&information) == 0) {
                return "Windows " + std::to_string(information.dwMajorVersion) + "." +
                       std::to_string(information.dwMinorVersion) + " (build " +
                       std::to_string(information.dwBuildNumber) + ")";
            }
        }
    }
    return "Windows";
}

bool isDebuggerAttached() { return IsDebuggerPresent() != 0; }

void debugBreak() { DebugBreak(); }

void showErrorMessage(std::string_view title, std::string_view message) {
    const std::wstring wideTitle = widen(title);
    const std::wstring wideMessage = widen(message);
    MessageBoxW(nullptr, wideMessage.c_str(), wideTitle.c_str(), MB_OK | MB_ICONERROR);
}

} // namespace zy::platform
