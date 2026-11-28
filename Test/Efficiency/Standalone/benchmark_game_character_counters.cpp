#include "../../../Modules/Game/game_character.hpp"
#include "../../../Modules/Observability/observability_game_metrics.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static uint64_t g_export_count;
static uint64_t g_export_checksum;

static void export_sample(const ft_game_observability_sample &sample)
{
    ++g_export_count;
    g_export_checksum = g_export_checksum * 31U + static_cast<uint64_t>(sample.delta_value)
        + static_cast<uint64_t>(sample.total_value);
    return ;
}

static int32_t measure_counter(const char *name, int32_t amount, ft_bool exporter_enabled)
{
    game_character character;
    if (character.initialize() != FT_ERR_SUCCESS)
        return (1);
    g_export_count = 0U;
    g_export_checksum = 0U;
    if (exporter_enabled == FT_TRUE
        && observability_game_metrics_initialize(export_sample) != FT_ERR_SUCCESS)
        return (1);

    constexpr uint32_t ITERATIONS = 400000U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        character.add_coins(amount);
        character.sub_coins(amount);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (exporter_enabled == FT_TRUE
        && observability_game_metrics_shutdown() != FT_ERR_SUCCESS)
        return (1);
    uint64_t expected_count = 0U;
    if (exporter_enabled == FT_TRUE && amount != 0)
        expected_count = 2U * ITERATIONS;
    if (character.get_coins() != 0 || g_export_count != expected_count)
        return (1);
    std::printf("%s %.3f %" PRIu64 " %" PRIu64 "\n",
        name, elapsed / (2U * ITERATIONS), g_export_count, g_export_checksum);
    return (0);
}

int main()
{
    if (measure_counter("zero", 0, FT_FALSE) != 0
        || measure_counter("no_exporter", 1, FT_FALSE) != 0)
        return (1);
    return (measure_counter("exporter", 1, FT_TRUE));
}
