#include "../../../Modules/Game/game_pathfinding.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_grid(ft_size_t side, uint32_t samples)
{
    game_map3d grid;
    game_pathfinding finder;
    ft_vector<game_path_step> path;
    if (grid.initialize(side, side, 1, 0) != FT_ERR_SUCCESS
        || finder.initialize() != FT_ERR_SUCCESS
        || path.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t sample = 0;
    const auto start = std::chrono::steady_clock::now();
    while (sample < samples)
    {
        if (finder.astar_grid(grid, 0, 0, 0, side - 1U, side - 1U, 0, path)
            != FT_ERR_SUCCESS)
            return (1);
        ++sample;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    const ft_size_t length = path.size();
    if (length != side * 2U - 1U)
        return (1);
    uint64_t checksum = 0U;
    ft_size_t index = 0U;
    while (index < length)
    {
        checksum = checksum * 31U + path[index].get_x();
        checksum = checksum * 31U + path[index].get_y();
        checksum = checksum * 31U + path[index].get_z();
        ++index;
    }
    std::printf("side=%" PRIu64 " us=%.3f length=%" PRIu64 " checksum=%" PRIu64 "\n",
        static_cast<uint64_t>(side), elapsed / samples, static_cast<uint64_t>(length), checksum);
    return (0);
}

static int32_t measure_cached_route()
{
    constexpr ft_size_t LENGTH = 128U;
    constexpr uint32_t SAMPLES = 10000U;
    game_map3d grid;
    game_pathfinding finder;
    ft_vector<game_path_step> path;
    if (grid.initialize(LENGTH, 1, 1, 0) != FT_ERR_SUCCESS
        || finder.initialize() != FT_ERR_SUCCESS
        || path.initialize() != FT_ERR_SUCCESS
        || finder.recalculate_path(grid, 0, 0, 0, LENGTH - 1U, 0, 0, path) != FT_ERR_SUCCESS)
        return (1);
    uint32_t sample = 0;
    const auto start = std::chrono::steady_clock::now();
    while (sample < SAMPLES)
    {
        if (finder.recalculate_path(grid, 0, 0, 0, LENGTH - 1U, 0, 0, path)
            != FT_ERR_SUCCESS)
            return (1);
        ++sample;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (path.size() != LENGTH)
        return (1);
    uint64_t checksum = 0U;
    ft_size_t index = 0U;
    while (index < LENGTH)
    {
        checksum = checksum * 31U + path[index].get_x();
        checksum = checksum * 31U + path[index].get_y();
        checksum = checksum * 31U + path[index].get_z();
        ++index;
    }
    std::printf("cached us=%.3f checksum=%" PRIu64 "\n", elapsed / SAMPLES, checksum);
    return (0);
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--cache") == 0)
        return (measure_cached_route());
    if (measure_grid(8U, 30U) != 0)
        return (1);
    if (measure_grid(24U, 4U) != 0)
        return (1);
    return (measure_grid(64U, 3U));
}
