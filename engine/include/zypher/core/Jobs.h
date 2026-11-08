// Job system: a small work stealing free thread pool with a deterministic mode.
//
// The pool is intentionally simple: a mutex protected task queue drained by N
// workers, plus parallelFor implemented on top of the same queue. Deterministic
// mode forces serial execution so the simulation produces identical results on
// every machine.
#pragma once

#include "zypher/core/Base.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace zy {

class JobSystem final : NonCopyable {
public:
    // threadCount == 0 -> hardware_concurrency() - 1 workers (at least one).
    explicit JobSystem(u32 threadCount = 0);
    ~JobSystem();

    void submit(std::function<void()> job);
    void parallelFor(u32 count, const std::function<void(u32)>& body, u32 minBatch = 1);
    void waitIdle();

    [[nodiscard]] u32 threadCount() const { return static_cast<u32>(m_workers.size()); }
    void setDeterministic(bool deterministic) { m_deterministic = deterministic; }
    [[nodiscard]] bool deterministic() const { return m_deterministic; }

    struct Stats {
        u64 submitted = 0;
        u64 executed = 0;
        u32 workers = 0;
    };
    [[nodiscard]] Stats stats() const;

    [[nodiscard]] static u32 hardwareThreads();

private:
    void workerLoop();
    void runInline(const std::function<void()>& job);

    std::vector<std::thread> m_workers;
    std::deque<std::function<void()>> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_workAvailable;
    std::condition_variable m_idle;
    std::atomic<u64> m_submitted{0};
    std::atomic<u64> m_executed{0};
    u32 m_active = 0;
    bool m_stopping = false;
    bool m_deterministic = false;
};

} // namespace zy
