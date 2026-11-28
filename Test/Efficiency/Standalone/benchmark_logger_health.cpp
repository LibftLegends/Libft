#include "../../../Modules/Logger/logger_internal.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

static std::atomic<uint32_t> g_probe_count{0U};

static ssize_t count_probe(int32_t, const void *, ft_size_t size, int32_t)
{
    g_probe_count.fetch_add(1U, std::memory_order_release);
    return (static_cast<ssize_t>(size));
}

int32_t main()
{
    constexpr uint32_t ITERATIONS = 5U;
    s_network_sink sink;
    sink.socket_fd = 1001;
    sink.send_function = count_probe;
    if (sink.host.initialize("benchmark") != FT_ERR_SUCCESS
        || ft_log_add_sink(ft_network_sink, &sink) != FT_ERR_SUCCESS)
        return (1);
    ft_log_set_remote_health_interval(3600U);
    double elapsed = 0.0;
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        ft_log_enable_remote_health(FT_TRUE);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (g_probe_count.load(std::memory_order_acquire) == iteration
            && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        const auto start = std::chrono::steady_clock::now();
        ft_log_enable_remote_health(FT_FALSE);
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        if (g_probe_count.load(std::memory_order_acquire) != iteration + 1U)
            break ;
        iteration += 1U;
    }
    ft_log_remove_sink(ft_network_sink, &sink);
    if (iteration != ITERATIONS)
        return (1);
    std::printf("idle health worker stop: %.2f us/op, checksum %u\n",
        elapsed / ITERATIONS, iteration);
    return (0);
}
