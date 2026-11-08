// Entity handle: a 32 bit slot index plus a 32 bit generation counter.
#pragma once

#include "zypher/core/Base.h"

#include <functional>

namespace zy {

struct Entity {
    static constexpr u32 kInvalidIndex = 0xFFFFFFFFu;

    u32 index = kInvalidIndex;
    u32 generation = 0;

    [[nodiscard]] constexpr bool valid() const noexcept { return index != kInvalidIndex; }
    explicit constexpr operator bool() const noexcept { return valid(); }

    friend constexpr bool operator==(const Entity&, const Entity&) = default;
};

struct EntityHash {
    [[nodiscard]] size_t operator()(const Entity& entity) const noexcept {
        return (static_cast<size_t>(entity.index) << 32) ^ static_cast<size_t>(entity.generation);
    }
};

} // namespace zy
