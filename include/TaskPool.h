#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <thread>
#include <vector>

class TaskPool
{
  public:
    struct Task
    {
        void (*function)(void*);
        void* context;
    };

    explicit TaskPool(std::uint32_t threadCount = std::thread::hardware_concurrency());

    void Enqueue(Task task);
    void Enqueue(std::span<const Task> tasks);

    void WaitForTasks();

    ~TaskPool();

  private:
    void Process(std::uint32_t id, std::uint32_t coprime);

    std::atomic_uint32_t threadCount_;
    std::atomic_uint32_t registeredThreadCount_;
    std::atomic_bool     running_;

    struct Queue;
    std::vector<std::thread>            workers_;
    std::vector<std::unique_ptr<Queue>> taskQueues_;
};