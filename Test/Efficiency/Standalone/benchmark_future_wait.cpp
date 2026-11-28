#include "../../../Modules/Template/future.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

int main()
{
    using benchmark_clock = std::chrono::steady_clock;
    constexpr uint32_t ITERATIONS = 64U;
    benchmark_clock::duration elapsed{};
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        ft_promise<uint32_t> promise;
        ft_future<uint32_t> future(promise);
        if (future.initialize() != FT_ERR_SUCCESS)
            return (1);
        std::atomic<ft_bool> entered(FT_FALSE);
        uint32_t received = 0U;
        benchmark_clock::time_point complete;
        std::thread waiter([&]()
        {
            entered.store(FT_TRUE, std::memory_order_release);
            received = future.get();
            complete = benchmark_clock::now();
            return ;
        });
        while (entered.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        const auto publish_after = benchmark_clock::now() + std::chrono::microseconds(100);
        while (benchmark_clock::now() < publish_after)
            std::this_thread::yield();
        const auto start = benchmark_clock::now();
        promise.set_value(iteration + 1U);
        waiter.join();
        if (received != iteration + 1U)
            return (1);
        elapsed += complete - start;
        ++iteration;
    }
    std::printf("future_publication_to_get ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}
