#include "../../../Modules/Logger/logger.hpp"
#include "../../../Modules/PThread/mutex.hpp"
#include <chrono>
#include <cstdio>
#include <thread>

int main()
{
    pt_mutex mutex;
    if (mutex.initialize() != FT_ERR_SUCCESS || mutex.lock() != FT_ERR_SUCCESS)
        return (1);
    std::thread waiter([&mutex]()
    {
        if (mutex.lock() == FT_ERR_SUCCESS)
            (void)mutex.unlock();
        return ;
    });
    ft_log_set_lock_contention_sampling_interval(0U);
    s_log_lock_contention_sample sample;
    ft_size_t count = 0U;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (ft_log_sample_lock_contention(&sample, 1U, &count) != FT_ERR_SUCCESS || count == 1U)
            break ;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    constexpr uint32_t ITERATIONS = 8192U;
    uint32_t sampled = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (count == 1U && sampled < ITERATIONS)
    {
        if (ft_log_sample_lock_contention(&sample, 1U, &count) != FT_ERR_SUCCESS || count != 1U)
            break ;
        sampled += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    (void)mutex.unlock();
    waiter.join();
    ft_log_enable_lock_contention_sampling(FT_FALSE);
    std::printf("sample one lock waiter: %.2f ns/op, checksum %u\n", elapsed / ITERATIONS, sampled);
    if (sampled != ITERATIONS)
        return (1);
    return (0);
}
