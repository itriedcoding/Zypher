// Scalar math helpers shared by the engine.
#pragma once

#include "zypher/core/Base.h"

#include <cmath>
#include <limits>

namespace zy::math {

inline constexpr f32 Pi = 3.14159265358979323846f;
inline constexpr f32 Tau = 6.28318530717958647692f;
inline constexpr f32 HalfPi = 1.57079632679489661923f;
inline constexpr f32 Epsilon = 1.0e-6f;
inline constexpr f32 GoldenRatio = 1.61803398874989484820f;

[[nodiscard]] constexpr f32 radians(f32 degrees) { return degrees * (Pi / 180.0f); }
[[nodiscard]] constexpr f32 degrees(f32 radians) { return radians * (180.0f / Pi); }

[[nodiscard]] constexpr f32 clamp01(f32 value) { return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); }

[[nodiscard]] constexpr f32 lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

[[nodiscard]] constexpr f32 inverseLerp(f32 a, f32 b, f32 value) {
    return (b - a) == 0.0f ? 0.0f : (value - a) / (b - a);
}

[[nodiscard]] constexpr f32 smoothStep(f32 edge0, f32 edge1, f32 value) {
    const f32 t = clamp01(inverseLerp(edge0, edge1, value));
    return t * t * (3.0f - 2.0f * t);
}

[[nodiscard]] inline bool approxEqual(f32 a, f32 b, f32 tolerance = 1.0e-5f) {
    return std::fabs(a - b) <= tolerance;
}

[[nodiscard]] inline bool approxEqual(f64 a, f64 b, f64 tolerance = 1.0e-9) {
    return std::fabs(a - b) <= tolerance;
}

[[nodiscard]] inline bool isFinite(f32 value) { return std::isfinite(value); }

[[nodiscard]] constexpr f32 sign(f32 value) { return value < 0.0f ? -1.0f : 1.0f; }

[[nodiscard]] constexpr f32 square(f32 value) { return value * value; }

[[nodiscard]] constexpr f32 moveTowards(f32 current, f32 target, f32 maxDelta) {
    // std::fabs is not constexpr, so use a constexpr-friendly magnitude.
    const f32 difference = target - current;
    const f32 magnitude = difference < 0.0f ? -difference : difference;
    if (magnitude <= maxDelta) {
        return target;
    }
    return current + maxDelta * (difference < 0.0f ? -1.0f : 1.0f);
}

// Wraps an angle to (-Pi, Pi].
[[nodiscard]] inline f32 wrapAngle(f32 angle) {
    const f32 wrapped = std::fmod(angle + Pi, Tau);
    return (wrapped < 0.0f ? wrapped + Tau : wrapped) - Pi;
}

} // namespace zy::math
