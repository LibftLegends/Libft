#include "../test_internal.hpp"
#include "../../Modules/Game/game_pathfinding.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_pathfinding_dijkstra_graph_keeps_adjacency_order)
{
    ft_graph<int32_t> graph;
    game_pathfinding finder;
    ft_vector<ft_size_t> path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    ft_size_t vertex = 0U;
    while (vertex < 5U)
    {
        FT_ASSERT_EQ(vertex, graph.add_vertex(0));
        ++vertex;
    }
    graph.add_edge(0U, 2U);
    graph.add_edge(0U, 1U);
    graph.add_edge(2U, 0U);
    graph.add_edge(2U, 3U);
    graph.add_edge(1U, 3U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.dijkstra_graph(graph, 0U, 3U, path));
    FT_ASSERT_EQ(3U, path.size());
    FT_ASSERT_EQ(0U, path[0]);
    FT_ASSERT_EQ(2U, path[1]);
    FT_ASSERT_EQ(3U, path[2]);
    FT_ASSERT_EQ(FT_ERR_GAME_INVALID_MOVE, finder.dijkstra_graph(graph, 0U, 4U, path));
    FT_ASSERT_EQ(0U, path.size());
    return (1);
}

FT_TEST(test_game_pathfinding_dijkstra_graph_allocation_failure_can_be_retried)
{
    ft_graph<int32_t> graph;
    game_pathfinding finder;
    ft_vector<ft_size_t> path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    ft_size_t vertex = 0U;
    while (vertex < 20U)
    {
        FT_ASSERT_EQ(vertex, graph.add_vertex(0));
        if (vertex > 0U)
            graph.add_edge(vertex - 1U, vertex);
        ++vertex;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t same_vertex = finder.dijkstra_graph(graph, 5U, 5U, path);
    const int32_t failed = finder.dijkstra_graph(graph, 0U, 19U, path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, same_vertex);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed);
    FT_ASSERT_EQ(0U, path.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.dijkstra_graph(graph, 0U, 19U, path));
    FT_ASSERT_EQ(20U, path.size());
    FT_ASSERT_EQ(19U, path[19]);
    return (1);
}
