#include "TaskPool.h"

#include <numeric>

namespace
{
    void GenerateCoprime(std::uint32_t& lastCoprime, const std::uint32_t size)
    {
        do
        {
            ++lastCoprime;
        } while (std::gcd(lastCoprime, size) != 1);
    }
} // namespace

TaskPool::TaskPool(const std::uint32_t threadCount)
{
    threadCount_.store(threadCount, std::memory_order_relaxed);
    workers_.reserve(threadCount);
    taskQueues_.resize(threadCount);
    for (std::uint32_t i = 0; i < threadCount; ++i)
    {
        taskQueues_[i] = std::make_unique<Queue>();
        taskQueues_[i]->queue_.resize(32);
    }
    running_.store(true, std::memory_order_release);
    std::uint32_t lastCoprime = 0;
    for (std::uint32_t i = 0; i < threadCount; ++i)
    {
        GenerateCoprime(lastCoprime, threadCount);
        workers_.emplace_back(&TaskPool::Process, this, i, lastCoprime);
    }
    while (registeredThreadCount_ != threadCount)
    {
        std::this_thread::yield();
    }
}

void TaskPool::WaitForTasks()
{
    for (const auto& queue : taskQueues_)
    {
        std::uint32_t spins = 0;

        // Spin first for lower latency
        while (queue->head_ != queue->tail_)
        {
            if (spins == 100)
            {
                queue->empty_.wait(false);
                break;
            }
            ++spins;
            std::this_thread::yield();
        }
    }
}

TaskPool::~TaskPool()
{
    running_ = false;
    for (auto& q : taskQueues_)
    {
        q->empty_.clear(std::memory_order_release);
        q->empty_.notify_one();
    }
    for (auto& worker : workers_)
    {
        worker.join();
    }
}

void TaskPool::EnqueueImpl(PackagedTask packedTask)
{
    // Randomly put task in one of the queues
    thread_local std::uint64_t roundRobinPointer = 0;
    std::uint64_t              index             = roundRobinPointer % threadCount_.load(std::memory_order_relaxed);
    const auto                 initialIndex      = index;
    ++roundRobinPointer;
    while (!taskQueues_[index]->headLock_.try_lock())
    {
        index = roundRobinPointer % threadCount_.load(std::memory_order_relaxed);
        ++roundRobinPointer;
        if (index == initialIndex)
        {
            taskQueues_[index]->headLock_.lock();
            break;
        }
    }
    auto& q = *taskQueues_[index];

    if (q.head_.load(std::memory_order_relaxed) - q.lazyTail_.load(std::memory_order_relaxed) == q.queue_.size())
    {
        // Queue seems full, update local view of tail and see if reader has done some work since last update
        q.lazyTail_.store(q.tail_.load(std::memory_order_acquire), std::memory_order_relaxed);
        if (q.head_.load(std::memory_order_relaxed) - q.lazyTail_.load(std::memory_order_relaxed) == q.queue_.size())
        {
            // Queue still seems full, increase size
            q.tailLock_.lock(); // Now we are the only thread with access to the queue

            std::vector<PackagedTask> newQueue(q.queue_.size() * 2);
            for (std::uint64_t i = 0; i < q.queue_.size(); ++i)
            {
                newQueue[i] = q.queue_[(q.tail_ + i) % q.queue_.size()];
            }
            q.queue_ = std::move(newQueue);
            q.head_.store(q.head_.load(std::memory_order_relaxed) - q.tail_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
            q.tail_.store(0, std::memory_order_relaxed);
            q.lazyHead_.store(q.head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
            q.lazyTail_.store(q.tail_.load(std::memory_order_relaxed), std::memory_order_relaxed);

            q.tailLock_.unlock();
        }
    }

    q.queue_[q.head_.load(std::memory_order_relaxed) % q.queue_.size()] = std::move(packedTask);
    q.head_.store(q.head_.load(std::memory_order_relaxed) + 1, std::memory_order_release);

    if (q.empty_.test(std::memory_order_acquire))
    {
        q.empty_.clear(std::memory_order_release);
        q.empty_.notify_one();
    }

    q.headLock_.unlock();
}

void TaskPool::Process(const std::uint32_t id, const std::uint32_t coprime)
{
    ++registeredThreadCount_;
    auto& q = *taskQueues_[id];
    // std::uint32_t dequeuePointer = id;
    std::uint32_t spinCounter = 1;
    while (true)
    {
        // Check if thread should terminate
        if (!running_.load(std::memory_order_acquire) &&
            (q.head_.load(std::memory_order_acquire) == q.tail_.load(std::memory_order_acquire)))
        {
            break;
        }

        // Dequeue with an increasing amount of attempts per fail
        bool dequeueSuccess = false;
        for (std::size_t i = 0; i < spinCounter; ++i)
        {
            auto& sq = *taskQueues_[(id + i * coprime) % threadCount_.load(std::memory_order_relaxed)];
            sq.tailLock_.lock();
            if (sq.lazyHead_.load(std::memory_order_relaxed) == sq.tail_.load(std::memory_order_relaxed))
            {
                // Queue seems empty, update local view of head and see if writer has done some work since last
                // update
                sq.lazyHead_.store(sq.head_.load(std::memory_order_acquire), std::memory_order_relaxed);
                if (sq.lazyHead_.load(std::memory_order_relaxed) == sq.tail_.load(std::memory_order_relaxed))
                {
                    // Queue still seems empty, try next queue
                    sq.tailLock_.unlock();
                    continue;
                }
            }
            auto task = std::move(sq.queue_[sq.tail_.load(std::memory_order_relaxed) % sq.queue_.size()]);
            sq.tail_.store(sq.tail_.load(std::memory_order_relaxed) + 1, std::memory_order_release);
            sq.tailLock_.unlock();

            task.task(task.context);
            dequeueSuccess = true;
            spinCounter    = 1;
            break;
        }

        if (dequeueSuccess)
        {
            continue;
        }

        // Keep spinning for a while before we wait
        if ((spinCounter) < threadCount_.load(std::memory_order_relaxed))
        {
            ++spinCounter;
            std::this_thread::yield();
            continue;
        }
        spinCounter = 1;

        // Dequeue attempts failed, wait for new task to be submitted
        q.headLock_.lock();
        q.tailLock_.lock();
        if (q.head_.load(std::memory_order_relaxed) == q.tail_.load(std::memory_order_relaxed))
        {
            q.empty_.test_and_set(std::memory_order_acq_rel);
            q.empty_.notify_one();
        }
        q.headLock_.unlock();
        q.tailLock_.unlock();
        q.empty_.wait(true);
    }
}