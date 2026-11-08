// Timing helpers: wall clock stopwatch and a fixed timestep frame accumulator.
#pragma once

#include "zypher/core/Base.h"

#include <chrono>

namespace zy {

class Stopwatch {
public:
    Stopwatch() : m_start(Clock::now()) {}

    void reset() { m_start = Clock::now(); }
    [[nodiscard]] f64 elapsedSeconds() const {
        return std::chrono::duration<f64>(Clock::now() - m_start).count();
    }
    [[nodiscard]] f64 elapsedMilliseconds() const { return elapsedSeconds() * 1000.0; }

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_start;
};

// Drives a fixed simulation rate independent from the render rate and clamps
// catch-up so a stalled frame cannot spiral.
class FrameTimer {
public:
    explicit FrameTimer(f64 fixedDelta = 1.0 / 60.0, u32 maxStepsPerFrame = 8);

    void beginFrame();
    void endFrame();

    [[nodiscard]] f64 deltaSeconds() const { return m_delta; }
    [[nodiscard]] f64 fixedDelta() const { return m_fixedDelta; }
    [[nodiscard]] f64 totalSeconds() const { return m_total; }
    [[nodiscard]] u32 frameCount() const { return m_frames; }
    [[nodiscard]] f64 smoothedDelta() const { return m_smoothed; }
    [[nodiscard]] f64 alpha() const { return m_accumulator / m_fixedDelta; }

    // Returns how many fixed steps should run this frame (already consumed).
    [[nodiscard]] u32 consumeFixedSteps();

    void setFixedDelta(f64 fixedDelta) { m_fixedDelta = fixedDelta > 0.0 ? fixedDelta : 1.0 / 60.0; }

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point m_last = Clock::now();
    f64 m_delta = 0.0;
    f64 m_smoothed = 1.0 / 60.0;
    f64 m_total = 0.0;
    f64 m_fixedDelta = 1.0 / 60.0;
    f64 m_accumulator = 0.0;
    u32 m_maxStepsPerFrame = 8;
    u32 m_frames = 0;
    bool m_started = false;
};

} // namespace zy
