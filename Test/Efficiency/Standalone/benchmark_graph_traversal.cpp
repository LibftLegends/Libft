#include "../../../Modules/Template/graph.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr uint32_t SIZE = 64U;
    constexpr uint32_t ITERATIONS = 120000U;
    ft_graph<uint32_t> graph;
    if (graph.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t vertex = 0U;
    while (vertex < SIZE)
    {
        if (graph.add_vertex(vertex) != vertex || graph.get_error() != FT_ERR_SUCCESS)
            return (1);
        if (vertex != 0U)
        {
            graph.add_edge(vertex - 1U, vertex);
            graph.add_edge(vertex, vertex - 1U);
        }
        ++vertex;
    }
    uint64_t sum = 0U;
    uint32_t iteration = 0U;
    const auto visit = [&](uint32_t value)
    {
        sum += value;
        return ;
    };
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        graph.bfs(0U, visit);
        graph.dfs(0U, visit);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (sum != static_cast<uint64_t>(SIZE) * (SIZE - 1U) * ITERATIONS)
        return (1);
    std::printf("graph_traversal_pair ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}
