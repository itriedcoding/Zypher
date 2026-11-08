#include "zypher/core/Memory.h"

#include "zypher/core/Assert.h"
#include "zypher/core/Log.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace zy {
namespace {

struct AllocationCounters {
    std::atomic<u64> allocations{0};
    std::atomic<u64> deallocations{0};
    std::atomic<u64> bytesInUse{0};
    std::atomic<u64> peakBytes{0};
};

AllocationCounters g_counters;

void accountAllocation(u64 bytes) {
    g_counters.allocations.fetch_add(1, std::memory_order_relaxed);
    const u64 now = g_counters.bytesInUse.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    u64 peak = g_counters.peakBytes.load(std::memory_order_relaxed);
    while (now > peak && !g_counters.peakBytes.compare_exchange_weak(peak, now, std::memory_order_relaxed)) {
    }
}

void accountDeallocation(u64 bytes) {
    g_counters.deallocations.fetch_add(1, std::memory_order_relaxed);
    g_counters.bytesInUse.fetch_sub(bytes, std::memory_order_relaxed);
}

} // namespace

Arena::Arena(usize blockSize) : m_blockSize(blockSize < 1024 ? 1024 : blockSize) {}

Arena::~Arena() {
    release();
}

Arena::Arena(Arena&& other) noexcept
    : m_blocks(std::move(other.m_blocks)),
      m_blockSize(other.m_blockSize),
      m_usedTotal(other.m_usedTotal),
      m_reservedTotal(other.m_reservedTotal) {
    other.m_blocks.clear();
    other.m_usedTotal = 0;
    other.m_reservedTotal = 0;
}

Arena& Arena::operator=(Arena&& other) noexcept {
    if (this != &other) {
        release();
        m_blocks = std::move(other.m_blocks);
        m_blockSize = other.m_blockSize;
        m_usedTotal = other.m_usedTotal;
        m_reservedTotal = other.m_reservedTotal;
        other.m_blocks.clear();
        other.m_usedTotal = 0;
        other.m_reservedTotal = 0;
    }
    return *this;
}

void Arena::addBlock(usize minimumSize) {
    const usize size = minimumSize > m_blockSize ? minimumSize : m_blockSize;
    auto* memory = static_cast<u8*>(std::malloc(size));
    if (memory == nullptr) {
        ZY_LOG_ERROR("memory", "arena out of memory requesting ", size, " bytes");
        throw std::bad_alloc();
    }
    m_blocks.push_back(Block{memory, size, 0});
    m_reservedTotal += size;
}

void* Arena::allocate(usize size, usize alignment) {
    if (size == 0) {
        return nullptr;
    }
    if (m_blocks.empty()) {
        addBlock(size + alignment);
    }

    Block& block = m_blocks.back();
    const usize base = reinterpret_cast<usize>(block.data);
    const usize aligned = alignUp(base + block.used, alignment);
    const usize offset = aligned - base;
    if (offset + size > block.size) {
        addBlock(size + alignment);
        return allocate(size, alignment);
    }
    block.used = offset + size;
    m_usedTotal += size;
    return block.data + offset;
}

ByteSpan Arena::allocateBytes(usize size, usize alignment) {
    auto* memory = static_cast<u8*>(allocate(size, alignment));
    return ByteSpan{memory, size};
}

void Arena::reset() {
    if (m_blocks.size() > 1) {
        for (usize index = 1; index < m_blocks.size(); ++index) {
            std::free(m_blocks[index].data);
            m_reservedTotal -= m_blocks[index].size;
        }
        m_blocks.resize(1);
    }
    if (!m_blocks.empty()) {
        m_blocks.front().used = 0;
    }
    m_usedTotal = 0;
}

void Arena::release() {
    for (auto& block : m_blocks) {
        std::free(block.data);
    }
    m_blocks.clear();
    m_usedTotal = 0;
    m_reservedTotal = 0;
}

AllocationStats allocationStats() {
    AllocationStats stats;
    stats.allocations = g_counters.allocations.load(std::memory_order_relaxed);
    stats.deallocations = g_counters.deallocations.load(std::memory_order_relaxed);
    stats.bytesInUse = g_counters.bytesInUse.load(std::memory_order_relaxed);
    stats.peakBytes = g_counters.peakBytes.load(std::memory_order_relaxed);
    return stats;
}

void* alignedAllocate(usize size, usize alignment) {
    if (size == 0) {
        return nullptr;
    }
#if defined(_MSC_VER)
    void* pointer = _aligned_malloc(size, alignment);
#else
    void* pointer = std::aligned_alloc(alignment, alignUp(size, alignment));
#endif
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    accountAllocation(size);
    return pointer;
}

void alignedRelease(void* pointer) noexcept {
    if (pointer == nullptr) {
        return;
    }
#if defined(_MSC_VER)
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
    accountDeallocation(0);
}

} // namespace zy
