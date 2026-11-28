#include "../../../Modules/Voxel/voxel_scripting_bridge.hpp"
#include "../../../Modules/Voxel/voxel_api.hpp"
#include "../../../Modules/Game/game_world.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <new>

static int32_t measure_script(game_script_bridge &bridge, game_voxel_chunk &chunk,
    voxel_generation_config &config, const char *label, const char *separator)
{
    constexpr uint32_t ITERATIONS = 1024U;
    ft_string script;
    if (script.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t command = 0U;
    while (command < 16U)
    {
        if (script.append("voxel_write_generated_block(1, 254, 1, 42)") != FT_ERR_SUCCESS
            || script.append(separator) != FT_ERR_SUCCESS)
            return (1);
        ++command;
    }
    uint32_t iteration = 0U;
    ft_size_t allocations_before;
    ft_size_t allocations_after;
    ft_size_t frees;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (voxel_script_execute(bridge, script, chunk, 0, 0, "benchmark",
                config, ".") != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    uint32_t block_id;
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS
        || chunk.read_block(1, 254, 1, &block_id) != FT_ERR_SUCCESS || block_id != 42U)
        return (1);
    std::printf("%s %.2f us/script %" PRIu64 " allocations\n", label, elapsed / ITERATIONS,
        ft_platform_cast<uint64_t>(allocations_after - allocations_before));
    return (0);
}

static int32_t measure_registration(const ft_sharedptr<game_world> &world)
{
    constexpr uint32_t ITERATIONS = 1024U;
    double elapsed = 0.0;
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        game_script_bridge bridge;
        if (bridge.initialize(world, "custom") != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        const int32_t status = voxel_script_register_api(bridge);
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        if (status != FT_ERR_SUCCESS || bridge.get_callback_count() != 11U)
            return (1);
        ++iteration;
    }
    std::printf("registration %.2f us/bridge\n", elapsed / ITERATIONS);
    return (0);
}

static int32_t measure_multiline_expression(game_script_bridge &bridge,
    game_voxel_chunk &chunk, voxel_generation_config &config)
{
    constexpr uint32_t ITERATIONS = 1024U;
    ft_string script;
    if (script.initialize("return (\r\n") != FT_ERR_SUCCESS)
        return (1);
    uint32_t line_index = 0U;
    while (line_index < 96U)
    {
        if (script.append("    1 +\r\n") != FT_ERR_SUCCESS)
            return (1);
        ++line_index;
    }
    if (script.append("    1)") != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (voxel_script_execute(bridge, script, chunk, 0, 0, "benchmark",
                config, ".") != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("multiline expression %.2f us/script\n", elapsed / ITERATIONS);
    return (0);
}

int main()
{
    ft_sharedptr<game_world> world;
    game_script_bridge bridge;
    game_voxel_chunk chunk;
    voxel_generation_config config;
    if (world.initialize(new (std::nothrow) game_world()) != FT_ERR_SUCCESS
        || world.get() == ft_nullptr || world->initialize() != FT_ERR_SUCCESS
        || bridge.initialize(world, "custom") != FT_ERR_SUCCESS
        || voxel_script_register_api(bridge) != FT_ERR_SUCCESS
        || chunk.initialize() != FT_ERR_SUCCESS
        || voxel_default_generation_config(config) != FT_ERR_SUCCESS
        || measure_script(bridge, chunk, config, "single line", ";") != 0
        || measure_script(bridge, chunk, config, "CRLF", "\r\n") != 0
        || measure_multiline_expression(bridge, chunk, config) != 0
        || measure_registration(world) != 0)
        return (1);
    return (0);
}
