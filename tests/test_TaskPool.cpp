#include "Catch2/catch_amalgamated.hpp"

#include "TaskPool.h"

namespace
{
    std::atomic_uint64_t counter;

    void Increment(void* argument) { counter += *reinterpret_cast<std::atomic_uint64_t*>(argument); }
} // namespace

TEST_CASE("Sanity Check")
{
    constexpr std::uint32_t count = 1'000'000;
    counter                       = 0;

    std::vector<std::uint64_t> numbers;
    numbers.resize(count);
    for (std::uint64_t i = 0; i < count; ++i)
    {
        numbers[i] = i;
    }

    {
        TaskPool threadPool{};
        for (auto& number : numbers)
        {
            threadPool.Enqueue(TaskPool::Task{Increment, &number});
        }
    }

    std::uint64_t checksum = 0;
    for (const auto& number : numbers)
    {
        checksum += number;
    }
    REQUIRE(checksum == counter);
}