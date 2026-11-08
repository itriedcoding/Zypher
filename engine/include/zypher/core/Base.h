// Zypher engine - fundamental types and small utilities shared by every module.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace zy {

// Fixed width aliases used throughout the engine. The engine never uses plain
// `int` in public data structures so serialised layouts stay stable.
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;
using usize = std::size_t;
using isize = std::ptrdiff_t;
using byte = std::byte;

template <class T>
using Scope = std::unique_ptr<T>;
template <class T>
using Ref = std::shared_ptr<T>;
template <class T>
using RefView = std::weak_ptr<T>;

template <class T>
using Span = std::span<T>;
using ByteSpan = std::span<const u8>;

template <class T, class... Args>
[[nodiscard]] constexpr Scope<T> makeScope(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <class T, class... Args>
[[nodiscard]] constexpr Ref<T> makeRef(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// Non-copyable helper for subsystems that own resources.
class NonCopyable {
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = delete;
    NonCopyable& operator=(NonCopyable&&) = delete;
};

[[nodiscard]] constexpr u32 nextPowerOfTwo(u32 value) {
    if (value <= 1) {
        return 1;
    }
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value + 1;
}

constexpr usize alignUp(usize value, usize alignment) {
    return (alignment == 0) ? value : (value + alignment - 1) & ~(alignment - 1);
}

template <class T>
[[nodiscard]] constexpr T clamp(T value, T low, T high) {
    return (value < low) ? low : ((value > high) ? high : value);
}

template <class T>
[[nodiscard]] constexpr T minOf(T a, T b) {
    return (b < a) ? b : a;
}

template <class T>
[[nodiscard]] constexpr T maxOf(T a, T b) {
    return (a < b) ? b : a;
}

// FNV-1a 64 bit. Used for asset ids, content hashes and scene checksums.
// The offset basis is the standard 64 bit FNV constant (0xcbf29ce484222325);
// the Rust cooker and the TypeScript editor use the same value.
[[nodiscard]] constexpr u64 fnv1a64(std::string_view text, u64 seed = 0xcbf29ce484222325ULL) {
    u64 hash = seed;
    for (const char character : text) {
        hash ^= static_cast<u64>(static_cast<unsigned char>(character));
        hash *= 1099511628211ULL;
    }
    return hash;
}

[[nodiscard]] constexpr u64 fnv1a64(const void* data, usize size, u64 seed = 0xcbf29ce484222325ULL) {
    const auto* bytes = static_cast<const u8*>(data);
    u64 hash = seed;
    for (usize index = 0; index < size; ++index) {
        hash ^= static_cast<u64>(bytes[index]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace zy
