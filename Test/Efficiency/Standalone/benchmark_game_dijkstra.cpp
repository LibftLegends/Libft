#include "../../../Modules/Game/game_pathfinding.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_graph(ft_size_t width, uint32_t samples)
{
    ft_graph<int32_t> graph;
    game_pathfinding finder;
    ft_vector<ft_size_t> path;
    if (graph.initialize() != FT_ERR_SUCCESS || finder.initialize() != FT_ERR_SUCCESS
        || path.initialize() != FT_ERR_SUCCESS)
        return (1);
    const ft_size_t goal = width + 1U;
    ft_size_t index = 0U;
    while (index <= goal)
    {
        if (graph.add_vertex(0) != index || graph.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    index = 1U;
    while (index <= width)
    {
        graph.add_edge(0U, index);
        if (graph.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    graph.add_edge(width, goal);
    if (graph.get_error() != FT_ERR_SUCCESS)
        return (1);

    uint32_t sample = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (sample < samples)
    {
        if (finder.dijkstra_graph(graph, 0U, goal, path) != FT_ERR_SUCCESS)
            return (1);
        ++sample;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (path.size() != 3U || path[0] != 0U || path[1] != width || path[2] != goal)
        return (1);
    std::printf("width=%" PRIu64 " us=%.3f via=%" PRIu64 " goal=%" PRIu64 "\n",
        static_cast<uint64_t>(width), elapsed / samples,
        static_cast<uint64_t>(path[1]), static_cast<uint64_t>(path[2]));
    return (0);
}

int32_t main()
{
    if (measure_graph(32U, 100U) != 0 || measure_graph(256U, 20U) != 0)
        return (1);
    return (measure_graph(1024U, 3U));
}
