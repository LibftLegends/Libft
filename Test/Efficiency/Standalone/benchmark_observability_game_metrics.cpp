#include "../../../Modules/Observability/observability_game_metrics.hpp"
#include <chrono>
#include <cstdio>
#include <thread>

static thread_local uint32_t delivered_samples;

static void count_game_metric(const ft_game_observability_sample &sample)
{
    if (sample.error_tag != nullptr && sample.error_code == FT_ERR_SUCCESS)
        ++delivered_samples;
    return ;
}

int main()
{
    constexpr uint32_t WORKERS = 4;
    constexpr uint32_t SAMPLES_PER_WORKER = 100000;
    if (observability_game_metrics_initialize(count_game_metric) != FT_ERR_SUCCESS)
        return (1);
    const ft_game_observability_sample sample{{"coins", "character", "total"}, 1, 1,
        "coins", FT_ERR_SUCCESS, nullptr, FT_TRUE};
    std::thread workers[WORKERS];
    uint32_t totals[WORKERS]{};
    uint32_t worker = 0;
    const auto start = std::chrono::steady_clock::now();
    while (worker < WORKERS)
    {
        workers[worker] = std::thread([&sample, &totals, worker]()
        {
            uint32_t iteration = 0;
            while (iteration < SAMPLES_PER_WORKER)
            {
                observability_game_metrics_record(sample);
                ++iteration;
            }
            totals[worker] = delivered_samples;
            return ;
        });
        ++worker;
    }
    worker = 0;
    while (worker < WORKERS)
        workers[worker++].join();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (observability_game_metrics_shutdown() != FT_ERR_SUCCESS)
        return (1);
    worker = 0;
    while (worker < WORKERS)
    {
        if (totals[worker++] != SAMPLES_PER_WORKER)
            return (1);
    }
    std::printf("%.3f ns/sample\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / (WORKERS * SAMPLES_PER_WORKER));
    return (0);
}
