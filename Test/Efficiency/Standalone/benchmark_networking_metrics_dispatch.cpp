#include "../../../Modules/Observability/observability_networking_metrics.hpp"
#include <chrono>
#include <cstdio>
#include <thread>

static void discard_sample(const ft_networking_observability_sample &)
{
    return ;
}

int main()
{
    constexpr uint32_t THREADS = 4;
    constexpr uint32_t ITERATIONS = 200000;
    if (observability_networking_metrics_enable_thread_safety() != FT_ERR_SUCCESS
        || observability_networking_metrics_initialize(discard_sample) != FT_ERR_SUCCESS)
        return (1);
    const ft_networking_observability_sample sample{
        {"api", "GET", "example.com", "/"}, 1, 1, 1, 200, FT_ERR_SUCCESS, "ok", FT_TRUE
    };
    std::thread workers[THREADS];
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0;
    while (index < THREADS)
    {
        workers[index] = std::thread([&sample]()
        {
            uint32_t iteration = 0;
            while (iteration < ITERATIONS)
            {
                (void)observability_networking_metrics_record(sample);
                ++iteration;
            }
            return ;
        });
        ++index;
    }
    index = 0;
    while (index < THREADS)
    {
        workers[index].join();
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (observability_networking_metrics_disable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("api_networking_metrics_dispatch us/op=%.5f\n",
        microseconds / (THREADS * ITERATIONS));
    return (0);
}
