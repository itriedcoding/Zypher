// zypher_runtime - engine runtime entry point.
#include "zypher/app/Application.h"

#include "zypher/core/Config.h"
#include "zypher/core/FileSystem.h"
#include "zypher/core/Log.h"
#include "zypher/core/Platform.h"
#include "zypher/crash/CrashReporter.h"
#include "zypher/render/RenderDevice.h"

#include <cstdio>
#include <string>

namespace {

void printUsage() {
    std::printf(
        "Zypher runtime %s\n"
        "\n"
        "Usage: zypher_runtime [options]\n"
        "\n"
        "Options:\n"
        "  --scene <path>        Scene file to load (default samples/demo/scene.zscene)\n"
        "  --pack <path>        Cooked asset pack (.zpkg)\n"
        "  --script <path>      Gameplay module (.dylib/.dll/.so implementing ScriptApi.h)\n"
        "  --output <dir>       Output directory for report/frames (default artifacts)\n"
        "  --backend <name>     software | vulkan | d3d12 | metal | opengl (default software)\n"
        "  --width <pixels>     Render width (default 1280)\n"
        "  --height <pixels>    Render height (default 720)\n"
        "  --frames <count>     Number of frames to render (default 1)\n"
        "  --steps <count>      Fixed simulation steps to run (default 120)\n"
        "  --fixed-delta <s>    Fixed timestep in seconds (default 0.0166667)\n"
        "  --threads <count>    Worker threads, 0 = automatic\n"
        "  --no-render          Skip rendering (simulation only)\n"
        "  --dump-frame         Write <output>/frame.png\n"
        "  --report             Accepted for symmetry; reports are written by default\n"
        "  --no-report          Do not write <output>/report.json\n"
        "  --list-assets        Print the contents of the pack and exit\n"
        "  --quiet              Only log warnings and errors\n"
        "  --version            Print the version and exit\n"
        "  --help               Print this message and exit\n",
        ZYPHER_VERSION_STRING);
}

} // namespace

int main(int argc, const char* const* argv) {
    zy::CommandLine commandLine;
    commandLine.parse(argc, argv);

    if (commandLine.has("help") || commandLine.has("h")) {
        printUsage();
        return 0;
    }
    if (commandLine.has("version")) {
        std::printf("Zypher %s (%s)\n", ZYPHER_VERSION_STRING, ZYPHER_VERSION_STRING);
        return 0;
    }

    zy::AppOptions options;
    options.scenePath = commandLine.getString("scene", options.scenePath);
    options.packPath = commandLine.getString("pack", options.packPath);
    options.scriptPath = commandLine.getString("script", options.scriptPath);
    options.outputDirectory = commandLine.getString("output", options.outputDirectory);
    options.width = static_cast<zy::u32>(commandLine.getInt("width", options.width));
    options.height = static_cast<zy::u32>(commandLine.getInt("height", options.height));
    options.frames = static_cast<zy::u32>(commandLine.getInt("frames", options.frames));
    options.simulationSteps = static_cast<zy::u32>(commandLine.getInt("steps", options.simulationSteps));
    options.fixedDelta = commandLine.getFloat("fixed-delta", options.fixedDelta);
    options.threads = static_cast<zy::u32>(commandLine.getInt("threads", options.threads));
    options.render = !commandLine.has("no-render");
    options.dumpFrame = commandLine.has("dump-frame");
    options.writeReport = !commandLine.has("no-report");

    if (const auto backendName = commandLine.get("backend"); backendName.has_value()) {
        const auto backend = zy::renderBackendFromString(*backendName);
        if (!backend.has_value()) {
            std::fprintf(stderr, "error: unknown render backend '%s'\n", backendName->c_str());
            return 2;
        }
        options.backend = *backend;
    }

    zy::Log::initialize("Zypher Runtime", zy::FileSystem::join(options.outputDirectory, "zypher.log"));
    if (commandLine.has("quiet")) {
        zy::Log::setLevel(zy::LogLevel::Warn);
    }

    zy::Log::write(zy::LogLevel::Info, "runtime",
                   zy::detail::concat("Zypher ", ZYPHER_VERSION_STRING, " | ", zy::platform::name(), " ",
                                      zy::platform::osVersion(), " | ", zy::platform::architecture(), " | ",
                                      zy::platform::configuration()));

    (void)zy::FileSystem::createDirectories(options.outputDirectory);
    if (const auto status =
            zy::CrashReporter::initialize("Zypher Runtime", zy::FileSystem::join(options.outputDirectory, "crashes"));
        status.hasError()) {
        zy::Log::write(zy::LogLevel::Warn, "crash", status.error().message);
    }

    if (commandLine.has("list-assets")) {
        if (options.packPath.empty()) {
            std::fprintf(stderr, "error: --list-assets requires --pack\n");
            zy::Log::shutdown();
            return 2;
        }
        auto pack = zy::PackFile::load(options.packPath, true);
        if (pack.hasError()) {
            std::fprintf(stderr, "error: %s\n", pack.error().toString().c_str());
            zy::Log::shutdown();
            return 1;
        }
        for (const auto& asset : pack.value().assets()) {
            std::printf("%016llx  %-8s %8u  %s\n", static_cast<unsigned long long>(asset.id),
                        std::string(zy::assetTypeName(asset.type)).c_str(), asset.size, asset.path.c_str());
        }
        std::printf("%zu assets\n", pack.value().assetCount());
        zy::CrashReporter::shutdown();
        zy::Log::shutdown();
        return 0;
    }

    zy::Application application;
    if (const auto status = application.initialize(options); status.hasError()) {
        const std::string message = status.error().toString();
        zy::Log::write(zy::LogLevel::Error, "runtime", message);
        zy::CrashReporter::recordFatal(message);
        std::fprintf(stderr, "error: %s\n", message.c_str());
        zy::CrashReporter::shutdown();
        zy::Log::shutdown();
        return 1;
    }

    const auto result = application.run();
    if (result.hasError()) {
        const std::string message = result.error().toString();
        zy::Log::write(zy::LogLevel::Error, "runtime", message);
        zy::CrashReporter::recordFatal(message);
        std::fprintf(stderr, "error: %s\n", message.c_str());
        application.shutdown();
        zy::CrashReporter::shutdown();
        zy::Log::shutdown();
        return 1;
    }

    const zy::AppReport& report = result.value();
    std::printf("Zypher %s | %s\n", ZYPHER_VERSION_STRING, report.backend.c_str());
    std::printf("  scene     : %s (%u entities)\n", report.sceneName.c_str(), report.entities);
    std::printf("  pack      : %s (%u assets)\n",
                report.packPath.empty() ? "(none)" : report.packPath.c_str(), report.packAssets);
    std::printf("  simulated : %u steps in %.2f ms\n", report.simulationSteps, report.simulationMilliseconds);
    std::printf("  rendered  : %u frame(s), %u draw calls, %llu triangles\n", report.frames,
                report.drawCalls, static_cast<unsigned long long>(report.triangles));
    std::printf("  timing    : %.3f ms/frame, %.2f ms total\n", report.averageFrameMilliseconds,
                report.totalMilliseconds);
    std::printf("  checksums : simulation %016llx, render %016llx\n",
                static_cast<unsigned long long>(report.simulationChecksum),
                static_cast<unsigned long long>(report.renderChecksum));
    for (const auto& warning : report.warnings) {
        std::printf("  warning   : %s\n", warning.c_str());
    }

    application.shutdown();
    zy::CrashReporter::shutdown();
    zy::Log::shutdown();
    return 0;
}
