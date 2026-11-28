#include "../../../Modules/Game/game_resistance.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_reads(ft_bool thread_safe, ft_bool paired)
{
    constexpr uint32_t ITERATIONS = 200000U;
    game_resistance resistance;
    if (resistance.initialize() != FT_ERR_SUCCESS
        || resistance.set_values(23, 47) != FT_ERR_SUCCESS)
        return (1);
    if (thread_safe && resistance.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        int32_t percent_value;
        int32_t flat_value;
        if (paired)
        {
            if (resistance.get_values(percent_value, flat_value) != FT_ERR_SUCCESS)
                return (1);
        }
        else
        {
            percent_value = resistance.get_percent();
            flat_value = resistance.get_flat();
        }
        checksum += static_cast<uint64_t>(percent_value + flat_value);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != 70U * ITERATIONS)
        return (1);
    std::printf("read_%u_%u %.3f %" PRIu64 "\n", static_cast<uint32_t>(thread_safe),
        static_cast<uint32_t>(paired), elapsed / ITERATIONS, checksum);
    return (0);
}

int main()
{
    if (measure_reads(FT_FALSE, FT_FALSE) != 0 || measure_reads(FT_FALSE, FT_TRUE) != 0
        || measure_reads(FT_TRUE, FT_FALSE) != 0)
        return (1);
    return (measure_reads(FT_TRUE, FT_TRUE));
}
