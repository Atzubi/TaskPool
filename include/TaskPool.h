#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class TaskPool
{
  public:
    explicit TaskPool(std::uint32_t threadCount = std::thread::hardware_concurrency());

    template <typename T> void Enqueue(void (*task)(T*), T* context)
    {
        PackagedTask packedTask = {std::move(reinterpret_cast<void (*)(void*)>(task)), std::move(context)};
        EnqueueImpl(std::move(packedTask));
    }

    void WaitForTasks();

    ~TaskPool();

  private:
    struct PackagedTask
    {
        void (*task)(void*);
        void* context;
    };

    void EnqueueImpl(PackagedTask packedTask);

    void Process(std::uint32_t id, std::uint32_t coprime);

    std::atomic_uint32_t threadCount_;
    std::atomic_uint32_t registeredThreadCount_;
    std::atomic_bool     running_;

    std::vector<std::thread> workers_;

    struct Queue
    {
        // Shared
        std::vector<PackagedTask> queue_;
        std::atomic_flag          empty_;

        // Variables used by the writer
        alignas(128) std::mutex headLock_{};
        std::atomic_uint64_t head_;
        std::atomic_uint64_t lazyTail_;

        // Variables used by the reader
        alignas(128) std::mutex tailLock_{};
        std::atomic_uint64_t tail_;
        std::atomic_uint64_t lazyHead_;
    };

    std::vector<std::unique_ptr<Queue>> taskQueues_;
};