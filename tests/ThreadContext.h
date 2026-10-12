#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <semaphore>
#include <set>
#include <thread>
#include <vector>

inline size_t maxThreads = std::max(1u, std::thread::hardware_concurrency());
inline std::shared_ptr<std::counting_semaphore<>> threadSemaphore;
inline std::set<std::string, std::less<>> selectedTestNames;

struct ThreadPermit {
    ThreadPermit() { threadSemaphore->acquire(); }
    ~ThreadPermit() { threadSemaphore->release(); }
};

class TestWorkers {
    std::mutex mutex_;
    std::condition_variable ready_;
    std::queue<std::move_only_function<void()>> tasks_;
    bool stopping_{};
    std::vector<std::jthread> threads_;

public:
    explicit TestWorkers(const size_t count) {
        if (!count)
            throw std::invalid_argument("Worker count must be positive");
        threads_.reserve(count);
        try {
            for (size_t i = 0; i < count; ++i) {
                threads_.emplace_back([this] {
                    for (;;) {
                        std::move_only_function<void()> task;
                        {
                            std::unique_lock lock(mutex_);
                            ready_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                            if (tasks_.empty())
                                return;
                            task = std::move(tasks_.front());
                            tasks_.pop();
                        }
                        task();
                    }
                });
            }
        } catch (...) {
            {
                const std::scoped_lock lock(mutex_);
                stopping_ = true;
            }
            ready_.notify_all();
            throw;
        }
    }

    ~TestWorkers() {
        {
            const std::scoped_lock lock(mutex_);
            stopping_ = true;
        }
        ready_.notify_all();
    }

    template<class F>
    auto Submit(F &&function) {
        std::packaged_task<std::invoke_result_t<F>()> task(std::forward<F>(function));
        auto result = task.get_future().share();
        {
            const std::scoped_lock lock(mutex_);
            tasks_.emplace([task = std::move(task)]() mutable { task(); });
        }
        ready_.notify_one();
        return result;
    }
};

inline std::unique_ptr<TestWorkers> testWorkers;
