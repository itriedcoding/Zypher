// Arena allocator and frame scratch memory.
//
// Hot engine paths allocate from arenas instead of the general heap: reset is
// O(1), layout is cache friendly and allocation cost is a pointer bump.
#pragma once

#include "zypher/core/Base.h"

#include <cstddef>
#include <vector>

namespace zy {

class Arena final : NonCopyable {
public:
    explicit Arena(usize blockSize = 64 * 1024);
    ~Arena();

    Arena(Arena&& other) noexcept;
    Arena& operator=(Arena&& other) noexcept;

    [[nodiscard]] void* allocate(usize size, usize alignment = alignof(std::max_align_t));
    [[nodiscard]] ByteSpan allocateBytes(usize size, usize alignment = 16);

    template <class T, class... Args>
    [[nodiscard]] T* create(Args&&... args) {
        void* memory = allocate(sizeof(T), alignof(T));
        return new (memory) T(std::forward<Args>(args)...);
    }

    template <class T>
    [[nodiscard]] T* createArray(usize count) {
        if (count == 0) {
            return nullptr;
        }
        return static_cast<T*>(allocate(sizeof(T) * count, alignof(T)));
    }

    // Releases every block except the first, keeping the arena reusable.
    void reset();
    void release();

    [[nodiscard]] usize usedBytes() const { return m_usedTotal; }
    [[nodiscard]] usize reservedBytes() const { return m_reservedTotal; }
    [[nodiscard]] usize blockCount() const { return m_blocks.size(); }

private:
    struct Block {
        u8* data = nullptr;
        usize size = 0;
        usize used = 0;
    };

    void addBlock(usize minimumSize);

    std::vector<Block> m_blocks;
    usize m_blockSize = 64 * 1024;
    usize m_usedTotal = 0;
    usize m_reservedTotal = 0;
};

// Process wide allocation counters (useful for editor diagnostics).
struct AllocationStats {
    u64 allocations = 0;
    u64 deallocations = 0;
    u64 bytesInUse = 0;
    u64 peakBytes = 0;
};

[[nodiscard]] AllocationStats allocationStats();
[[nodiscard]] void* alignedAllocate(usize size, usize alignment);
void alignedRelease(void* pointer) noexcept;

} // namespace zy
