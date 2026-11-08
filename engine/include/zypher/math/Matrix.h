// 4x4 matrices (column major, right handed, clip depth 0..1) and quaternions.
#pragma once

#include "zypher/math/Vec.h"

#include <cmath>

namespace zy {

struct Mat4 {
    // Column major storage: m[column * 4 + row], matching GLSL/HLSL/Metal upload
    // conventions and the SPIR-V memory layout.
    f32 m[16] = {1.0f, 0.0f, 0.0f, 0.0f,
                 0.0f, 1.0f, 0.0f, 0.0f,
                 0.0f, 0.0f, 1.0f, 0.0f,
                 0.0f, 0.0f, 0.0f, 1.0f};

    constexpr Mat4() = default;

    [[nodiscard]] static constexpr Mat4 identity() { return Mat4{}; }

    [[nodiscard]] static constexpr Mat4 zero() {
        Mat4 result;
        for (f32& value : result.m) {
            value = 0.0f;
        }
        return result;
    }

    [[nodiscard]] constexpr f32& at(u32 row, u32 column) { return m[column * 4 + row]; }
    [[nodiscard]] constexpr const f32& at(u32 row, u32 column) const { return m[column * 4 + row]; }

    [[nodiscard]] static constexpr Mat4 translation(const Vec3& offset) {
        Mat4 result;
        result.at(0, 3) = offset.x;
        result.at(1, 3) = offset.y;
        result.at(2, 3) = offset.z;
        return result;
    }

    [[nodiscard]] static constexpr Mat4 scaling(const Vec3& factors) {
        Mat4 result;
        result.at(0, 0) = factors.x;
        result.at(1, 1) = factors.y;
        result.at(2, 2) = factors.z;
        return result;
    }

    [[nodiscard]] static Mat4 rotationX(f32 angleRadians) {
        const f32 cosine = std::cos(angleRadians);
        const f32 sine = std::sin(angleRadians);
        Mat4 result;
        result.at(1, 1) = cosine;
        result.at(1, 2) = -sine;
        result.at(2, 1) = sine;
        result.at(2, 2) = cosine;
        return result;
    }

    [[nodiscard]] static Mat4 rotationY(f32 angleRadians) {
        const f32 cosine = std::cos(angleRadians);
        const f32 sine = std::sin(angleRadians);
        Mat4 result;
        result.at(0, 0) = cosine;
        result.at(0, 2) = sine;
        result.at(2, 0) = -sine;
        result.at(2, 2) = cosine;
        return result;
    }

    [[nodiscard]] static Mat4 rotationZ(f32 angleRadians) {
        const f32 cosine = std::cos(angleRadians);
        const f32 sine = std::sin(angleRadians);
        Mat4 result;
        result.at(0, 0) = cosine;
        result.at(0, 1) = -sine;
        result.at(1, 0) = sine;
        result.at(1, 1) = cosine;
        return result;
    }

    // Right handed perspective projection with a 0..1 depth range.
    [[nodiscard]] static Mat4 perspective(f32 fovYRadians, f32 aspectRatio, f32 nearPlane, f32 farPlane) {
        const f32 focal = 1.0f / std::tan(fovYRadians * 0.5f);
        Mat4 result = zero();
        result.at(0, 0) = focal / aspectRatio;
        result.at(1, 1) = focal;
        result.at(2, 2) = farPlane / (nearPlane - farPlane);
        result.at(2, 3) = (nearPlane * farPlane) / (nearPlane - farPlane);
        result.at(3, 2) = -1.0f;
        return result;
    }

    [[nodiscard]] static Mat4 orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearPlane,
                                            f32 farPlane) {
        Mat4 result = zero();
        result.at(0, 0) = 2.0f / (right - left);
        result.at(1, 1) = 2.0f / (top - bottom);
        result.at(2, 2) = -1.0f / (farPlane - nearPlane);
        result.at(0, 3) = -(right + left) / (right - left);
        result.at(1, 3) = -(top + bottom) / (top - bottom);
        result.at(2, 3) = -nearPlane / (farPlane - nearPlane);
        result.at(3, 3) = 1.0f;
        return result;
    }

    [[nodiscard]] static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        const Vec3 forward = normalize(target - eye);
        Vec3 side = cross(forward, up);
        if (side.lengthSquared() < math::Epsilon) {
            // Camera looking straight up/down: pick a stable fallback axis.
            side = cross(forward, Vec3::right());
        }
        side = normalize(side);
        const Vec3 upCorrected = cross(side, forward);

        Mat4 result;
        result.at(0, 0) = side.x;
        result.at(0, 1) = side.y;
        result.at(0, 2) = side.z;
        result.at(0, 3) = -dot(side, eye);
        result.at(1, 0) = upCorrected.x;
        result.at(1, 1) = upCorrected.y;
        result.at(1, 2) = upCorrected.z;
        result.at(1, 3) = -dot(upCorrected, eye);
        result.at(2, 0) = -forward.x;
        result.at(2, 1) = -forward.y;
        result.at(2, 2) = -forward.z;
        result.at(2, 3) = dot(forward, eye);
        result.at(3, 3) = 1.0f;
        return result;
    }

    [[nodiscard]] Mat4 operator*(const Mat4& other) const {
        Mat4 result = zero();
        for (u32 column = 0; column < 4; ++column) {
            for (u32 row = 0; row < 4; ++row) {
                f32 sum = 0.0f;
                for (u32 k = 0; k < 4; ++k) {
                    sum += at(row, k) * other.at(k, column);
                }
                result.at(row, column) = sum;
            }
        }
        return result;
    }

    [[nodiscard]] constexpr Vec4 transform(const Vec4& value) const {
        return Vec4{
            at(0, 0) * value.x + at(0, 1) * value.y + at(0, 2) * value.z + at(0, 3) * value.w,
            at(1, 0) * value.x + at(1, 1) * value.y + at(1, 2) * value.z + at(1, 3) * value.w,
            at(2, 0) * value.x + at(2, 1) * value.y + at(2, 2) * value.z + at(2, 3) * value.w,
            at(3, 0) * value.x + at(3, 1) * value.y + at(3, 2) * value.z + at(3, 3) * value.w,
        };
    }

    [[nodiscard]] constexpr Vec3 transformPoint(const Vec3& point) const {
        return Vec3{
            at(0, 0) * point.x + at(0, 1) * point.y + at(0, 2) * point.z + at(0, 3),
            at(1, 0) * point.x + at(1, 1) * point.y + at(1, 2) * point.z + at(1, 3),
            at(2, 0) * point.x + at(2, 1) * point.y + at(2, 2) * point.z + at(2, 3),
        };
    }

    [[nodiscard]] constexpr Vec3 transformDirection(const Vec3& direction) const {
        return Vec3{
            at(0, 0) * direction.x + at(0, 1) * direction.y + at(0, 2) * direction.z,
            at(1, 0) * direction.x + at(1, 1) * direction.y + at(1, 2) * direction.z,
            at(2, 0) * direction.x + at(2, 1) * direction.y + at(2, 2) * direction.z,
        };
    }

    [[nodiscard]] Mat4 transposed() const {
        Mat4 result;
        for (u32 row = 0; row < 4; ++row) {
            for (u32 column = 0; column < 4; ++column) {
                result.at(row, column) = at(column, row);
            }
        }
        return result;
    }

    [[nodiscard]] f32 determinant() const {
        const f32 a00 = at(0, 0), a01 = at(0, 1), a02 = at(0, 2), a03 = at(0, 3);
        const f32 a10 = at(1, 0), a11 = at(1, 1), a12 = at(1, 2), a13 = at(1, 3);
        const f32 a20 = at(2, 0), a21 = at(2, 1), a22 = at(2, 2), a23 = at(2, 3);
        const f32 a30 = at(3, 0), a31 = at(3, 1), a32 = at(3, 2), a33 = at(3, 3);
        const f32 b00 = a00 * a11 - a01 * a10;
        const f32 b01 = a00 * a12 - a02 * a10;
        const f32 b02 = a00 * a13 - a03 * a10;
        const f32 b03 = a01 * a12 - a02 * a11;
        const f32 b04 = a01 * a13 - a03 * a11;
        const f32 b05 = a02 * a13 - a03 * a12;
        const f32 b06 = a20 * a31 - a21 * a30;
        const f32 b07 = a20 * a32 - a22 * a30;
        const f32 b08 = a20 * a33 - a23 * a30;
        const f32 b09 = a21 * a32 - a22 * a31;
        const f32 b10 = a21 * a33 - a23 * a31;
        const f32 b11 = a22 * a33 - a23 * a32;
        return b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    }

    // General inverse. Callers are expected to check determinant() first when
    // the matrix may be singular (inverse of a zero matrix returns identity).
    [[nodiscard]] Mat4 inverted() const {
        const f32 a00 = at(0, 0), a01 = at(0, 1), a02 = at(0, 2), a03 = at(0, 3);
        const f32 a10 = at(1, 0), a11 = at(1, 1), a12 = at(1, 2), a13 = at(1, 3);
        const f32 a20 = at(2, 0), a21 = at(2, 1), a22 = at(2, 2), a23 = at(2, 3);
        const f32 a30 = at(3, 0), a31 = at(3, 1), a32 = at(3, 2), a33 = at(3, 3);

        const f32 b00 = a00 * a11 - a01 * a10;
        const f32 b01 = a00 * a12 - a02 * a10;
        const f32 b02 = a00 * a13 - a03 * a10;
        const f32 b03 = a01 * a12 - a02 * a11;
        const f32 b04 = a01 * a13 - a03 * a11;
        const f32 b05 = a02 * a13 - a03 * a12;
        const f32 b06 = a20 * a31 - a21 * a30;
        const f32 b07 = a20 * a32 - a22 * a30;
        const f32 b08 = a20 * a33 - a23 * a30;
        const f32 b09 = a21 * a32 - a22 * a31;
        const f32 b10 = a21 * a33 - a23 * a31;
        const f32 b11 = a22 * a33 - a23 * a32;

        const f32 determinant = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
        if (std::fabs(determinant) < 1.0e-12f) {
            return Mat4::identity();
        }
        const f32 inverseDeterminant = 1.0f / determinant;

        Mat4 result;
        result.at(0, 0) = (a11 * b11 - a12 * b10 + a13 * b09) * inverseDeterminant;
        result.at(0, 1) = (-a01 * b11 + a02 * b10 - a03 * b09) * inverseDeterminant;
        result.at(0, 2) = (a31 * b05 - a32 * b04 + a33 * b03) * inverseDeterminant;
        result.at(0, 3) = (-a21 * b05 + a22 * b04 - a23 * b03) * inverseDeterminant;
        result.at(1, 0) = (-a10 * b11 + a12 * b08 - a13 * b07) * inverseDeterminant;
        result.at(1, 1) = (a00 * b11 - a02 * b08 + a03 * b07) * inverseDeterminant;
        result.at(1, 2) = (-a30 * b05 + a32 * b02 - a33 * b01) * inverseDeterminant;
        result.at(1, 3) = (a20 * b05 - a22 * b02 + a23 * b01) * inverseDeterminant;
        result.at(2, 0) = (a10 * b10 - a11 * b08 + a13 * b06) * inverseDeterminant;
        result.at(2, 1) = (-a00 * b10 + a01 * b08 - a03 * b06) * inverseDeterminant;
        result.at(2, 2) = (a30 * b04 - a31 * b02 + a33 * b00) * inverseDeterminant;
        result.at(2, 3) = (-a20 * b04 + a21 * b02 - a23 * b00) * inverseDeterminant;
        result.at(3, 0) = (-a10 * b09 + a11 * b07 - a12 * b06) * inverseDeterminant;
        result.at(3, 1) = (a00 * b09 - a01 * b07 + a02 * b06) * inverseDeterminant;
        result.at(3, 2) = (-a30 * b03 + a31 * b01 - a32 * b00) * inverseDeterminant;
        result.at(3, 3) = (a20 * b03 - a21 * b01 + a22 * b00) * inverseDeterminant;
        return result;
    }

    // Inverse for affine transforms (rotation + translation + scale), which is
    // what the scene graph and the software rasteriser need.
    [[nodiscard]] Mat4 affineInverted() const {
        const f32 sx = lengthOfColumn(0);
        const f32 sy = lengthOfColumn(1);
        const f32 sz = lengthOfColumn(2);
        if (sx < 1.0e-12f || sy < 1.0e-12f || sz < 1.0e-12f) {
            return Mat4::identity();
        }
        const Vec3 translationPart{at(0, 3), at(1, 3), at(2, 3)};
        // The basis columns are scaled by the transform, so normalise them before
        // transposing; otherwise the result is R^T * S instead of S^-1 * R^T.
        const auto inverseRotation = [this, sx, sy, sz](const Vec3& value) {
            return Vec3{dot(Vec3{at(0, 0) / sx, at(1, 0) / sx, at(2, 0) / sx}, value),
                        dot(Vec3{at(0, 1) / sy, at(1, 1) / sy, at(2, 1) / sy}, value),
                        dot(Vec3{at(0, 2) / sz, at(1, 2) / sz, at(2, 2) / sz}, value)};
        };
        Mat4 result;
        const Vec3 inverseScale{1.0f / sx, 1.0f / sy, 1.0f / sz};
        // -A^-1 * t with A = R * S, so the rotation is undone first and the
        // result is then divided by the scale.
        const Vec3 inverseTranslation = inverseRotation(translationPart) * inverseScale;
        const Vec3 scaledX = inverseRotation(Vec3{1.0f, 0.0f, 0.0f}) * inverseScale;
        const Vec3 scaledY = inverseRotation(Vec3{0.0f, 1.0f, 0.0f}) * inverseScale;
        const Vec3 scaledZ = inverseRotation(Vec3{0.0f, 0.0f, 1.0f}) * inverseScale;
        result.at(0, 0) = scaledX.x;
        result.at(1, 0) = scaledX.y;
        result.at(2, 0) = scaledX.z;
        result.at(0, 1) = scaledY.x;
        result.at(1, 1) = scaledY.y;
        result.at(2, 1) = scaledY.z;
        result.at(0, 2) = scaledZ.x;
        result.at(1, 2) = scaledZ.y;
        result.at(2, 2) = scaledZ.z;
        result.at(0, 3) = -inverseTranslation.x;
        result.at(1, 3) = -inverseTranslation.y;
        result.at(2, 3) = -inverseTranslation.z;
        result.at(3, 3) = 1.0f;
        return result;
    }

    [[nodiscard]] f32 lengthOfColumn(u32 column) const {
        return std::sqrt(at(0, column) * at(0, column) + at(1, column) * at(1, column) +
                         at(2, column) * at(2, column));
    }

    friend bool operator==(const Mat4& left, const Mat4& right) {
        for (usize index = 0; index < 16; ++index) {
            if (!math::approxEqual(left.m[index], right.m[index])) {
                return false;
            }
        }
        return true;
    }
};

// --- Quaternion ------------------------------------------------------------

struct Quat {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 1.0f;

    constexpr Quat() = default;
    constexpr Quat(f32 xValue, f32 yValue, f32 zValue, f32 wValue)
        : x(xValue), y(yValue), z(zValue), w(wValue) {}

    [[nodiscard]] static constexpr Quat identity() { return Quat{}; }

    [[nodiscard]] static Quat fromAxisAngle(const Vec3& axis, f32 angleRadians) {
        const f32 magnitude = axis.length();
        if (magnitude < math::Epsilon) {
            return Quat::identity();
        }
        const Vec3 normalised = axis / magnitude;
        const f32 half = angleRadians * 0.5f;
        const f32 sine = std::sin(half);
        return Quat{normalised.x * sine, normalised.y * sine, normalised.z * sine, std::cos(half)};
    }

    // Intrinsic rotation order: X, then Y, then Z (yaw around +Y).
    [[nodiscard]] static Quat fromEuler(const Vec3& eulerRadians) {
        const Quat qx = fromAxisAngle(Vec3{1.0f, 0.0f, 0.0f}, eulerRadians.x);
        const Quat qy = fromAxisAngle(Vec3{0.0f, 1.0f, 0.0f}, eulerRadians.y);
        const Quat qz = fromAxisAngle(Vec3{0.0f, 0.0f, 1.0f}, eulerRadians.z);
        return qz * qy * qx;
    }

    [[nodiscard]] constexpr f32 lengthSquared() const { return x * x + y * y + z * z + w * w; }
    [[nodiscard]] f32 length() const { return std::sqrt(lengthSquared()); }

    [[nodiscard]] Quat normalized() const {
        const f32 magnitude = length();
        if (magnitude < math::Epsilon) {
            return Quat::identity();
        }
        return Quat{x / magnitude, y / magnitude, z / magnitude, w / magnitude};
    }

    [[nodiscard]] constexpr Quat conjugate() const { return Quat{-x, -y, -z, w}; }

    // Hamilton product: applies `other` first, then `*this`.
    [[nodiscard]] constexpr Quat operator*(const Quat& other) const {
        return Quat{
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w,
            w * other.w - x * other.x - y * other.y - z * other.z,
        };
    }

    [[nodiscard]] Vec3 rotate(const Vec3& value) const {
        const Vec3 axis{x, y, z};
        const Vec3 twiceCross = cross(axis, value) * 2.0f;
        return value + twiceCross * w + cross(axis, twiceCross);
    }

    [[nodiscard]] Vec3 rotateInverse(const Vec3& value) const { return conjugate().rotate(value); }

    [[nodiscard]] Mat4 toMat4() const {
        const f32 xx = x * x;
        const f32 yy = y * y;
        const f32 zz = z * z;
        const f32 xy = x * y;
        const f32 xz = x * z;
        const f32 yz = y * z;
        const f32 wx = w * x;
        const f32 wy = w * y;
        const f32 wz = w * z;

        Mat4 result;
        result.at(0, 0) = 1.0f - 2.0f * (yy + zz);
        result.at(0, 1) = 2.0f * (xy - wz);
        result.at(0, 2) = 2.0f * (xz + wy);
        result.at(1, 0) = 2.0f * (xy + wz);
        result.at(1, 1) = 1.0f - 2.0f * (xx + zz);
        result.at(1, 2) = 2.0f * (yz - wx);
        result.at(2, 0) = 2.0f * (xz - wy);
        result.at(2, 1) = 2.0f * (yz + wx);
        result.at(2, 2) = 1.0f - 2.0f * (xx + yy);
        return result;
    }

    [[nodiscard]] static Quat slerp(const Quat& a, const Quat& b, f32 t) {
        Quat target = b;
        f32 cosine = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        if (cosine < 0.0f) {
            target = Quat{-b.x, -b.y, -b.z, -b.w};
            cosine = -cosine;
        }
        if (cosine > 0.9995f) {
            const Quat lerped{a.x + (target.x - a.x) * t, a.y + (target.y - a.y) * t,
                              a.z + (target.z - a.z) * t, a.w + (target.w - a.w) * t};
            return lerped.normalized();
        }
        const f32 theta = std::acos(cosine);
        const f32 sine = std::sin(theta);
        const f32 weightA = std::sin((1.0f - t) * theta) / sine;
        const f32 weightB = std::sin(t * theta) / sine;
        return Quat{a.x * weightA + target.x * weightB, a.y * weightA + target.y * weightB,
                    a.z * weightA + target.z * weightB, a.w * weightA + target.w * weightB};
    }

    friend bool operator==(const Quat& left, const Quat& right) {
        return math::approxEqual(left.x, right.x) && math::approxEqual(left.y, right.y) &&
               math::approxEqual(left.z, right.z) && math::approxEqual(left.w, right.w);
    }
};

// Composes translation * rotation * scale.
[[nodiscard]] inline Mat4 fromTRS(const Vec3& translation, const Quat& rotation, const Vec3& scale) {
    const Mat4 rotationMatrix = rotation.toMat4();
    Mat4 result = rotationMatrix;
    result.at(0, 0) *= scale.x;
    result.at(1, 0) *= scale.x;
    result.at(2, 0) *= scale.x;
    result.at(0, 1) *= scale.y;
    result.at(1, 1) *= scale.y;
    result.at(2, 1) *= scale.y;
    result.at(0, 2) *= scale.z;
    result.at(1, 2) *= scale.z;
    result.at(2, 2) *= scale.z;
    result.at(0, 3) = translation.x;
    result.at(1, 3) = translation.y;
    result.at(2, 3) = translation.z;
    result.at(3, 3) = 1.0f;
    return result;
}

static_assert(sizeof(Mat4) == 64, "Mat4 must be 64 bytes");
static_assert(sizeof(Quat) == 16, "Quat must be 16 bytes");

} // namespace zy
