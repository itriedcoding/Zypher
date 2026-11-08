// Lightweight frame profiler. When ZYPHER_WITH_TRACY is enabled the scopes are
// also forwarded to Tracy; the built-in collector is always available so the
// editor can show timings without the external tool.
#pragma once

#include "zypher/core/Base.h"

#include <string>
#include <string_view>
#include <vector>

namespace zy {

struct ProfileScopeSample {
    std::string name;
    f64 milliseconds = 0.0;
    u32 depth = 0;
};

struct FrameProfile {
    u64 frameIndex = 0;
    f64 totalMilliseconds = 0.0;
    std::vector<ProfileScopeSample> scopes;
};

class Profiler final {
public:
    static void beginFrame(u64 frameIndex = 0);
    static void endFrame();
    static void pushScope(std::string_view name);
    static void popScope();

    [[nodiscard]] static const FrameProfile& lastFrame();
    [[nodiscard]] static bool enabled();
    static void setEnabled(bool enabled);

    struct ScopeTimer {
        explicit ScopeTimer(std::string_view name) { Profiler::pushScope(name); }
        ~ScopeTimer() { Profiler::popScope(); }
        ScopeTimer(const ScopeTimer&) = delete;
        ScopeTimer& operator=(const ScopeTimer&) = delete;
        ScopeTimer(ScopeTimer&&) = delete;
        ScopeTimer& operator=(ScopeTimer&&) = delete;
    };
};

} // namespace zy

#if defined(ZYPHER_WITH_TRACY)
#include "Tracy.hpp"
#define ZY_PROFILE_SCOPE(name) ::zy::Profiler::ScopeTimer zy_profile_scope_##__LINE__(name); ZoneScopedN(name)
#define ZY_PROFILE_FRAME() FrameMark
#else
#define ZY_PROFILE_SCOPE(name) ::zy::Profiler::ScopeTimer zy_profile_scope_##__LINE__(name)
#define ZY_PROFILE_FRAME() ((void)0)
#endif
