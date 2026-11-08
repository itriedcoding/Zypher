#include "zypher/core/Jobs.h"

#include "zypher/core/Log.h"

#include <algorithm>

namespace zy {

u32 JobSystem::hardwareThreads() {
    const u32 detected = std::thread::hardware_concurrency();
    return detected == 0 ? 1u : detected;
}

JobSystem::JobSystem(u32 threadCount) {
    u32 workers = threadCount;
    if (workers == 0) {
        const u32 hardware = hardwareThreads();
        workers = hardware > 1 ? hardware - 1 : 1;
    }
    workers = std::max(1u, std::min(workers, 64u));
    m_workers.reserve(workers);
    for (u32 index = 0; index < workers; ++index) {
        m_workers.emplace_back([this] { workerLoop(); });
    }
    ZY_LOG_DEBUG("jobs", "job system started with ", m_workers.size(), " worker(s)");
}

JobSystem::~JobSystem() {
    {
        std::lock_guard lock(m_mutex);
        m_stopping = true;
    }
    m_workAvailable.notify_all();
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void JobSystem::submit(std::function<void()> job) {
    if (!job) {
        return;
    }
    m_submitted.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard lock(m_mutex);
        m_queue.push_back(std::move(job));
    }
    m_workAvailable.notify_one();
}

void JobSystem::runInline(const std::function<void()>& job) {
    job();
    m_executed.fetch_add(1, std::memory_order_relaxed);
}

void JobSystem::parallelFor(u32 count, const std::function<void(u32)>& body, u32 minBatch) {
    if (count == 0 || !body) {
        return;
    }
    const u32 workers = threadCount();
    if (m_deterministic || workers <= 1 || count <= std::max(1u, minBatch)) {
        for (u32 index = 0; index < count; ++index) {
            body(index);
        }
        return;
    }

    const u32 batch = std::max(1u, std::max(minBatch, count / (workers * 4u)));
    const u32 taskCount = (count + batch - 1) / batch;

    std::mutex completionMutex;
    std::condition_variable completion;
    u32 remaining = taskCount;

    for (u32 task = 0; task < taskCount; ++task) {
        const u32 begin = task * batch;
        const u32 end = std::min(count, begin + batch);
        submit([this, &body, &completionMutex, &completion, &remaining, begin, end] {
            for (u32 index = begin; index < end; ++index) {
                body(index);
            }
            {
                std::lock_guard lock(completionMutex);
                if (remaining > 0) {
                    --remaining;
                }
            }
            completion.notify_one();
        });
    }

    std::unique_lock lock(completionMutex);
    completion.wait(lock, [&remaining] { return remaining == 0; });
}

void JobSystem::waitIdle() {
    std::unique_lock lock(m_mutex);
    m_idle.wait(lock, [this] { return m_queue.empty() && m_active == 0; });
}

JobSystem::Stats JobSystem::stats() const {
    Stats result;
    result.submitted = m_submitted.load(std::memory_order_relaxed);
    result.executed = m_executed.load(std::memory_order_relaxed);
    result.workers = threadCount();
    return result;
}

void JobSystem::workerLoop() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock lock(m_mutex);
            m_workAvailable.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
            if (m_queue.empty()) {
                if (m_stopping) {
                    return;
                }
                continue;
            }
            job = std::move(m_queue.front());
            m_queue.pop_front();
            ++m_active;
        }

        job();
        m_executed.fetch_add(1, std::memory_order_relaxed);

        {
            std::lock_guard lock(m_mutex);
            --m_active;
        }
        m_idle.notify_all();
    }
}

} // namespace zy
