#include "../../../Modules/Game/game_experience_table.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_table(int32_t count, uint32_t samples)
{
    constexpr int32_t MAX_LEVELS = 2048;
    int32_t levels[MAX_LEVELS];
    int32_t index = 0;
    while (index < count)
    {
        levels[index] = index * 4;
        ++index;
    }
    game_experience_table table;
    if (table.initialize() != FT_ERR_SUCCESS
        || table.set_levels(levels, count) != FT_ERR_SUCCESS)
        return (1);
    uint32_t seed = 1U;
    uint64_t checksum = 0U;
    uint32_t sample = 0U;
    const auto lookup_start = std::chrono::steady_clock::now();
    while (sample < samples)
    {
        seed = seed * 1664525U + 1013904223U;
        const int32_t experience = static_cast<int32_t>(
            (seed >> 16U) % static_cast<uint32_t>(count * 4));
        checksum = checksum * 31U + table.get_level(experience);
        ++sample;
    }
    const auto lookup_end = std::chrono::steady_clock::now();
    sample = 0U;
    while (sample < samples)
    {
        const int32_t threshold = levels[count / 2] + static_cast<int32_t>(sample & 1U);
        table.set_value(count / 2, threshold);
        ++sample;
    }
    const auto edit_end = std::chrono::steady_clock::now();
    if (table.get_error() != FT_ERR_SUCCESS)
        return (1);
    const double lookup_ns = std::chrono::duration<double, std::nano>(
        lookup_end - lookup_start).count() / samples;
    const double edit_ns = std::chrono::duration<double, std::nano>(
        edit_end - lookup_end).count() / samples;
    std::printf("levels=%" PRId32 " lookup=%.3f edit=%.3f checksum=%" PRIu64
        " threshold=%" PRId32 "\n", count, lookup_ns, edit_ns, checksum,
        table.get_value(count / 2));
    return (0);
}

static int32_t measure_generation(int32_t mode)
{
    constexpr int32_t LEVEL_COUNT = 4096;
    constexpr uint32_t SAMPLES = 12000U;
    int32_t levels[LEVEL_COUNT];
    int32_t index = 0;
    while (index < LEVEL_COUNT)
    {
        levels[index] = index * 17;
        ++index;
    }
    game_experience_table table;
    if (table.initialize() != FT_ERR_SUCCESS
        || table.set_levels(levels, LEVEL_COUNT) != FT_ERR_SUCCESS)
        return (1);
    uint32_t sample = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (sample < SAMPLES)
    {
        int32_t result;
        if (mode == 0)
            result = table.set_levels(levels, LEVEL_COUNT);
        else if (mode == 1)
            result = table.generate_levels_total(LEVEL_COUNT, 10000, 1.001);
        else
            result = table.generate_levels_scaled(LEVEL_COUNT, 25, 1.001);
        if (result != FT_ERR_SUCCESS)
            return (1);
        ++sample;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    uint64_t checksum = 0U;
    index = 0;
    while (index < LEVEL_COUNT)
    {
        checksum = checksum * 31U + table.get_value(index);
        ++index;
    }
    std::printf("mode=%" PRId32 " build=%.3f checksum=%" PRIu64 "\n",
        mode, elapsed / SAMPLES, checksum);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--build") == 0)
    {
        if (measure_generation(0) != 0 || measure_generation(1) != 0)
            return (1);
        return (measure_generation(2));
    }
    if (measure_table(8, 500000U) != 0)
        return (1);
    return (measure_table(2048, 200000U));
}
