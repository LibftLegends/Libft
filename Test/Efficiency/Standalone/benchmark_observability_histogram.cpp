#include "../../../Modules/Observability/observability_histogram.hpp"
#include <chrono>
#include <cstdio>
#include <inttypes.h>
#include <string>

struct histogram_export_statistics
{
    ft_size_t chunks;
    ft_size_t bytes;
};

static void count_histogram_chunk(const char *chunk, void *context)
{
    auto *statistics = static_cast<histogram_export_statistics *>(context);
    ++statistics->chunks;
    while (*chunk != '\0')
    {
        ++statistics->bytes;
        ++chunk;
    }
    return ;
}

int main()
{
    constexpr uint32_t BUCKET_COUNT = 256;
    constexpr uint32_t SAMPLES = 500000;
    constexpr uint32_t EXPORTS = 2000;
    const std::string name(4096, 'n');
    ft_observability_histogram_bucket buckets[BUCKET_COUNT];
    uint32_t index = 0;
    while (index < BUCKET_COUNT)
    {
        buckets[index].upper_bound = index;
        ++index;
    }
    ft_observability_histogram histogram{};
    if (observability_histogram_initialize(&histogram, name.c_str(), "ms", buckets, BUCKET_COUNT)
        != FT_ERR_SUCCESS)
        return (1);
    index = 0;
    const auto record_start = std::chrono::steady_clock::now();
    while (index < SAMPLES)
    {
        const double sample = static_cast<double>(240U + index % 16U);
        if (observability_histogram_record(&histogram, sample) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const auto record_elapsed = std::chrono::steady_clock::now() - record_start;
    if (histogram.count != SAMPLES || buckets[239].count != 0 || buckets[240].count != SAMPLES / 16
        || buckets[254].count != SAMPLES / 16 * 15 || buckets[255].count != SAMPLES)
        return (1);
    histogram_export_statistics statistics{};
    index = 0;
    const auto export_start = std::chrono::steady_clock::now();
    while (index < EXPORTS)
    {
        if (observability_histogram_export_json(&histogram, count_histogram_chunk, &statistics)
            != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const auto export_elapsed = std::chrono::steady_clock::now() - export_start;
    std::printf("record=%.3f ns export=%.3f us chunks=%" PRIu64 " bytes=%" PRIu64 "\n",
        std::chrono::duration<double, std::nano>(record_elapsed).count() / SAMPLES,
        std::chrono::duration<double, std::micro>(export_elapsed).count() / EXPORTS,
        static_cast<uint64_t>(statistics.chunks / EXPORTS),
        static_cast<uint64_t>(statistics.bytes / EXPORTS));
    return (0);
}
