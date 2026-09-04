#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <concepts>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace SDK::Async
{

class ThreadPool
{
public:
    static ThreadPool& get()
    {
        static ThreadPool instance;
        return instance;
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool()
    {
        shutdown();
    }

    template<typename F, typename... Args>
    std::future<std::invoke_result_t<F, Args...>> enqueue(F&& f, Args&&... args)
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        std::shared_ptr<std::packaged_task<ReturnType()>> task = std::make_shared<std::packaged_task<ReturnType()>>(
            [func = std::forward<F>(f), ...capturedArgs = std::forward<Args>(args)]() mutable
            {
                return std::invoke(std::move(func), std::move(capturedArgs)...);
            }
        );

        std::future<ReturnType> res = task->get_future();

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_workers.empty() || m_stopRequested)
            {
                // Auto-restart thread pool if re-injected
                m_stopRequested = false;
                const unsigned int threadCount = (std::clamp)(std::thread::hardware_concurrency(), 2u, 4u);
                m_workers.reserve(threadCount);
                for (unsigned int i = 0; i < threadCount; ++i)
                {
                    m_workers.emplace_back([this](std::stop_token stopToken)
                    {
                        workerLoop(stopToken);
                    });
                }
            }

            m_tasks.emplace_back([task]()
            {
                (*task)();
            });
        }

        m_cv.notify_one();
        return res;
    }

    void init()
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (!m_workers.empty() && !m_stopRequested)
        {
            return;
        }

        m_stopRequested = false;
        m_tasks.clear();

        const unsigned int threadCount = (std::clamp)(std::thread::hardware_concurrency(), 2u, 4u);
        m_workers.reserve(threadCount);

        for (unsigned int i = 0; i < threadCount; ++i)
        {
            m_workers.emplace_back([this](std::stop_token stopToken)
            {
                workerLoop(stopToken);
            });
        }
    }

    void shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_stopRequested)
            {
                return;
            }
            m_stopRequested = true;
            m_tasks.clear();
        }

        m_cv.notify_all();

        for (std::jthread& worker : m_workers)
        {
            if (worker.joinable())
            {
                worker.request_stop();
            }
        }
        m_cv.notify_all();

        for (std::jthread& worker : m_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        m_workers.clear();
    }

private:
    ThreadPool()
    {
        init();
    }

    void workerLoop(std::stop_token stopToken)
    {
        while (!stopToken.stop_requested() && !m_stopRequested)
        {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_cv.wait_for(lock, std::chrono::milliseconds(25), [this, &stopToken]()
                {
                    return m_stopRequested || stopToken.stop_requested() || !m_tasks.empty();
                });

                if (m_stopRequested || stopToken.stop_requested())
                {
                    return;
                }

                if (!m_tasks.empty())
                {
                    task = std::move(m_tasks.front());
                    m_tasks.pop_front();
                }
            }

            if (task && !m_stopRequested && !stopToken.stop_requested())
            {
                task();
            }
        }
    }

    std::vector<std::jthread> m_workers;
    std::deque<std::function<void()>> m_tasks;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;
    bool m_stopRequested = false;
};

template<typename F, typename... Args>
std::future<std::invoke_result_t<F, Args...>> run(F&& f, Args&&... args)
{
    return ThreadPool::get().enqueue(std::forward<F>(f), std::forward<Args>(args)...);
}

inline void init()
{
    ThreadPool::get().init();
}

inline void shutdown()
{
    ThreadPool::get().shutdown();
}

} // namespace SDK::Async

