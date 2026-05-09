/**
  ******************************************************************************
  * @file           : ThreadPool.h
  * @author         : vivi wu
  * @brief          : 固定大小任务线程池
  * @version        : 0.1.0
  * @date           : 09/05/26
  ******************************************************************************
  */
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    // 提交任务，返回 false 表示池已关闭
    template <typename F>
    bool Submit(F&& task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_.load(std::memory_order_relaxed)) return false;
            tasks_.emplace(std::forward<F>(task));
        }
        cv_.notify_one();
        return true;
    }

    // 停止接收新任务，等待所有已提交任务完成，join 所有工作线程
    void Shutdown();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 当前队列中的待处理任务数（近似值）
    size_t PendingTasks() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }

    // 工作线程数
    size_t WorkerCount() const { return workers_.size(); }

private:
    void WorkerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_{true};
};

#endif // THREAD_POOL_H
