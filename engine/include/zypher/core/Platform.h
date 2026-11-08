// Platform abstraction: OS reporting, message boxes and process services.
#pragma once

#include "zypher/core/Base.h"

#include <string>
#include <string_view>

namespace zy::platform {

[[nodiscard]] std::string_view name();          // "macOS", "Windows" or the CI host
[[nodiscard]] std::string_view architecture();  // "arm64", "x86_64", ...
[[nodiscard]] std::string osVersion();
[[nodiscard]] std::string_view configuration(); // "Debug", "Release", ...

[[nodiscard]] bool isDebuggerAttached();
void debugBreak();
void sleepMilliseconds(u32 milliseconds);

// Shows a native modal error dialog. Used before the log/UI is available.
void showErrorMessage(std::string_view title, std::string_view message);

// A stable identifier used in crash reports and telemetry-free diagnostics.
[[nodiscard]] std::string machineIdentifier();

} // namespace zy::platform
