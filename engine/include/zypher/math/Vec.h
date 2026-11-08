// Vector types. Layout is tightly packed so they can be uploaded to the GPU and
// written to the pack file without conversion.
#pragma once

#include "zypher/math/Math.h"

#include <cmath>

namespace zy {

struct Vec2 {
    f32 x = 0.0f;
    f32 y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(f32 xValue, f32 yValue) : x(xValue), y(yValue) {}
    explicit constexpr Vec2(f32 scalar) : x(scalar), y(scalar) {}

    [[nodiscard]] static constexpr Vec2 zero() { return Vec2{0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec2 one() { return Vec2{1.0f, 1.0f}; }

    constexpr Vec2 operator+(const Vec2& other) const { return Vec2{x + other.x, y + other.y}; }
    constexpr Vec2 operator-(const Vec2& other) const { return Vec2{x - other.x, y - other.y}; }
    constexpr Vec2 operator*(f32 scalar) const { return Vec2{x * scalar, y * scalar}; }
    constexpr Vec2 operator/(f32 scalar) const { return Vec2{x / scalar, y / scalar}; }
    constexpr Vec2 operator-() const { return Vec2{-x, -y}; }

    constexpr Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    constexpr Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }
    constexpr Vec2& operator*=(f32 scalar) { x *= scalar; y *= scalar; return *this; }

    [[nodiscard]] constexpr f32 lengthSquared() const { return x * x + y * y; }
    [[nodiscard]] f32 length() const { return std::sqrt(lengthSquared()); }
    [[nodiscard]] Vec2 normalized() const {
        const f32 len = length();
        return len > math::Epsilon ? Vec2{x / len, y / len} : Vec2{};
    }

    [[nodiscard]] constexpr f32& operator[](usize index) { return index == 0 ? x : y; }
    [[nodiscard]] constexpr const f32& operator[](usize index) const { return index == 0 ? x : y; }

    friend constexpr bool operator==(const Vec2&, const Vec2&) = default;
};

[[nodiscard]] constexpr f32 dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
[[nodiscard]] constexpr f32 cross(const Vec2& a, const Vec2& b) { return a.x * b.y - a.y * b.x; }
[[nodiscard]] inline Vec2 normalize(const Vec2& value) { return value.normalized(); }
[[nodiscard]] constexpr Vec2 lerp(const Vec2& a, const Vec2& b, f32 t) { return a + (b - a) * t; }

struct Vec3 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(f32 xValue, f32 yValue, f32 zValue) : x(xValue), y(yValue), z(zValue) {}
    explicit constexpr Vec3(f32 scalar) : x(scalar), y(scalar), z(scalar) {}

    [[nodiscard]] static constexpr Vec3 zero() { return Vec3{0.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 one() { return Vec3{1.0f, 1.0f, 1.0f}; }
    [[nodiscard]] static constexpr Vec3 up() { return Vec3{0.0f, 1.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 right() { return Vec3{1.0f, 0.0f, 0.0f}; }
    // Zypher uses a right handed coordinate system with -Z as "forward".
    [[nodiscard]] static constexpr Vec3 forward() { return Vec3{0.0f, 0.0f, -1.0f}; }

    constexpr Vec3 operator+(const Vec3& other) const { return Vec3{x + other.x, y + other.y, z + other.z}; }
    constexpr Vec3 operator-(const Vec3& other) const { return Vec3{x - other.x, y - other.y, z - other.z}; }
    constexpr Vec3 operator*(const Vec3& other) const { return Vec3{x * other.x, y * other.y, z * other.z}; }
    constexpr Vec3 operator*(f32 scalar) const { return Vec3{x * scalar, y * scalar, z * scalar}; }
    constexpr Vec3 operator/(f32 scalar) const { return Vec3{x / scalar, y / scalar, z / scalar}; }
    constexpr Vec3 operator-() const { return Vec3{-x, -y, -z}; }

    constexpr Vec3& operator+=(const Vec3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    constexpr Vec3& operator-=(const Vec3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    constexpr Vec3& operator*=(f32 scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    constexpr Vec3& operator/=(f32 scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }

    [[nodiscard]] constexpr f32 lengthSquared() const { return x * x + y * y + z * z; }
    [[nodiscard]] f32 length() const { return std::sqrt(lengthSquared()); }
    [[nodiscard]] Vec3 normalized() const {
        const f32 len = length();
        return len > math::Epsilon ? Vec3{x / len, y / len, z / len} : Vec3{};
    }

    [[nodiscard]] constexpr f32& operator[](usize index) {
        return index == 0 ? x : (index == 1 ? y : z);
    }
    [[nodiscard]] constexpr const f32& operator[](usize index) const {
        return index == 0 ? x : (index == 1 ? y : z);
    }

    friend constexpr bool operator==(const Vec3&, const Vec3&) = default;
};

[[nodiscard]] constexpr f32 dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

[[nodiscard]] inline Vec3 normalize(const Vec3& value) { return value.normalized(); }
[[nodiscard]] constexpr Vec3 lerp(const Vec3& a, const Vec3& b, f32 t) { return a + (b - a) * t; }
[[nodiscard]] inline f32 distance(const Vec3& a, const Vec3& b) { return (b - a).length(); }
[[nodiscard]] constexpr Vec3 min(const Vec3& a, const Vec3& b) {
    return Vec3{a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z};
}
[[nodiscard]] constexpr Vec3 max(const Vec3& a, const Vec3& b) {
    return Vec3{a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z};
}
[[nodiscard]] constexpr Vec3 abs(const Vec3& value) {
    return Vec3{value.x < 0.0f ? -value.x : value.x, value.y < 0.0f ? -value.y : value.y,
                value.z < 0.0f ? -value.z : value.z};
}

// Component-wise approximate comparison, used by the tests.
[[nodiscard]] inline bool approxEqual(const Vec3& a, const Vec3& b, f32 tolerance = 1.0e-5f) {
    return math::approxEqual(a.x, b.x, tolerance) && math::approxEqual(a.y, b.y, tolerance) &&
           math::approxEqual(a.z, b.z, tolerance);
}

struct Vec4 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;

    constexpr Vec4() = default;
    constexpr Vec4(f32 xValue, f32 yValue, f32 zValue, f32 wValue)
        : x(xValue), y(yValue), z(zValue), w(wValue) {}
    constexpr Vec4(const Vec3& value, f32 wValue) : x(value.x), y(value.y), z(value.z), w(wValue) {}
    explicit constexpr Vec4(f32 scalar) : x(scalar), y(scalar), z(scalar), w(scalar) {}

    [[nodiscard]] static constexpr Vec4 zero() { return Vec4{0.0f, 0.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec4 one() { return Vec4{1.0f, 1.0f, 1.0f, 1.0f}; }

    constexpr Vec4 operator+(const Vec4& other) const {
        return Vec4{x + other.x, y + other.y, z + other.z, w + other.w};
    }
    constexpr Vec4 operator-(const Vec4& other) const {
        return Vec4{x - other.x, y - other.y, z - other.z, w - other.w};
    }
    constexpr Vec4 operator*(f32 scalar) const { return Vec4{x * scalar, y * scalar, z * scalar, w * scalar}; }
    constexpr Vec4 operator/(f32 scalar) const { return Vec4{x / scalar, y / scalar, z / scalar, w / scalar}; }

    [[nodiscard]] constexpr Vec3 xyz() const { return Vec3{x, y, z}; }
    [[nodiscard]] constexpr f32 lengthSquared() const { return x * x + y * y + z * z + w * w; }
    [[nodiscard]] f32 length() const { return std::sqrt(lengthSquared()); }

    [[nodiscard]] constexpr f32& operator[](usize index) {
        return index == 0 ? x : (index == 1 ? y : (index == 2 ? z : w));
    }
    [[nodiscard]] constexpr const f32& operator[](usize index) const {
        return index == 0 ? x : (index == 1 ? y : (index == 2 ? z : w));
    }

    friend constexpr bool operator==(const Vec4&, const Vec4&) = default;
};

[[nodiscard]] constexpr f32 dot(const Vec4& a, const Vec4& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

[[nodiscard]] inline bool approxEqual(const Vec4& a, const Vec4& b, f32 tolerance = 1.0e-5f) {
    return math::approxEqual(a.x, b.x, tolerance) && math::approxEqual(a.y, b.y, tolerance) &&
           math::approxEqual(a.z, b.z, tolerance) && math::approxEqual(a.w, b.w, tolerance);
}

static_assert(sizeof(Vec2) == 8, "Vec2 must be tightly packed");
static_assert(sizeof(Vec3) == 12, "Vec3 must be tightly packed");
static_assert(sizeof(Vec4) == 16, "Vec4 must be tightly packed");

} // namespace zy
