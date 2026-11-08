#include "zypher/core/Time.h"

namespace zy {

FrameTimer::FrameTimer(f64 fixedDelta, u32 maxStepsPerFrame)
    : m_fixedDelta(fixedDelta > 0.0 ? fixedDelta : 1.0 / 60.0), m_maxStepsPerFrame(maxStepsPerFrame < 1 ? 1 : maxStepsPerFrame) {}

void FrameTimer::beginFrame() {
    const auto now = Clock::now();
    if (!m_started) {
        m_last = now;
        m_started = true;
    }
    const f64 raw = std::chrono::duration<f64>(now - m_last).count();
    m_last = now;
    // Clamp pathological deltas (debugger pauses, disk stalls).
    m_delta = raw > 0.25 ? 0.25 : (raw < 0.0 ? 0.0 : raw);
    m_smoothed = m_smoothed * 0.9 + m_delta * 0.1;
    m_total += m_delta;
}

void FrameTimer::endFrame() {
    ++m_frames;
}

u32 FrameTimer::consumeFixedSteps() {
    m_accumulator += m_delta;
    u32 steps = 0;
    while (m_accumulator >= m_fixedDelta && steps < m_maxStepsPerFrame) {
        m_accumulator -= m_fixedDelta;
        ++steps;
    }
    if (steps == m_maxStepsPerFrame) {
        // Drop the backlog instead of accumulating unbounded debt.
        m_accumulator = 0.0;
    }
    return steps;
}

} // namespace zy
