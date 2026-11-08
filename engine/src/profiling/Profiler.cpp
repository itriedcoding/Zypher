#include "zypher/profiling/Profiler.h"

#include <chrono>
#include <mutex>

namespace zy {
namespace {

using Clock = std::chrono::steady_clock;

struct ScopeEntry {
    std::string name;
    Clock::time_point start;
    u32 depth = 0;
};

struct ProfileState {
    std::mutex mutex;
    bool enabled = true;
    u64 frameIndex = 0;
    Clock::time_point frameStart = Clock::now();
    std::vector<ScopeEntry> stack;
    FrameProfile lastFrame;
};

ProfileState& state() {
    static ProfileState instance;
    return instance;
}

} // namespace

void Profiler::beginFrame(u64 frameIndex) {
    auto& profile = state();
    std::lock_guard lock(profile.mutex);
    profile.frameIndex = frameIndex;
    profile.frameStart = Clock::now();
    profile.stack.clear();
    profile.lastFrame.scopes.clear();
}

void Profiler::endFrame() {
    auto& profile = state();
    std::lock_guard lock(profile.mutex);
    profile.lastFrame.frameIndex = profile.frameIndex;
    profile.lastFrame.totalMilliseconds =
        std::chrono::duration<f64, std::milli>(Clock::now() - profile.frameStart).count();
    profile.stack.clear();
}

void Profiler::pushScope(std::string_view name) {
    auto& profile = state();
    if (!profile.enabled) {
        return;
    }
    std::lock_guard lock(profile.mutex);
    profile.stack.push_back(ScopeEntry{std::string(name), Clock::now(),
                                      static_cast<u32>(profile.stack.size())});
}

void Profiler::popScope() {
    auto& profile = state();
    std::lock_guard lock(profile.mutex);
    if (profile.stack.empty()) {
        return;
    }
    const ScopeEntry entry = std::move(profile.stack.back());
    profile.stack.pop_back();
    const f64 milliseconds = std::chrono::duration<f64, std::milli>(Clock::now() - entry.start).count();
    profile.lastFrame.scopes.push_back(ProfileScopeSample{entry.name, milliseconds, entry.depth});
}

const FrameProfile& Profiler::lastFrame() {
    return state().lastFrame;
}

bool Profiler::enabled() { return state().enabled; }

void Profiler::setEnabled(bool enabled) {
    auto& profile = state();
    std::lock_guard lock(profile.mutex);
    profile.enabled = enabled;
    if (!enabled) {
        profile.stack.clear();
    }
}

} // namespace zy
