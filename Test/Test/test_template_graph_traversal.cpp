#include "../test_internal.hpp"
#include "../../Modules/Template/graph.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <array>

FT_TEST(test_ft_graph_traversals_keep_order_with_cycles_and_cross_edges)
{
    ft_graph<uint32_t> graph;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.enable_thread_safety());
    uint32_t vertex = 0U;
    while (vertex < 5U)
    {
        FT_ASSERT_EQ(vertex, graph.add_vertex(vertex));
        ++vertex;
    }
    graph.add_edge(0U, 1U);
    graph.add_edge(0U, 2U);
    graph.add_edge(0U, 3U);
    graph.add_edge(1U, 2U);
    graph.add_edge(1U, 4U);
    graph.add_edge(2U, 0U);
    graph.add_edge(4U, 0U);
    std::array<uint32_t, 5U> order{};
    ft_size_t count = 0U;
    const auto visit = [&](uint32_t value)
    {
        if (count < order.size())
            order[count] = value;
        ++count;
        return ;
    };
    graph.bfs(0U, visit);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.get_error());
    FT_ASSERT_EQ(order.size(), count);
    const std::array<uint32_t, 5U> breadth_first{0U, 1U, 2U, 3U, 4U};
    FT_ASSERT(order == breadth_first);
    count = 0U;
    graph.dfs(0U, visit);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.get_error());
    FT_ASSERT_EQ(order.size(), count);
    const std::array<uint32_t, 5U> depth_first{0U, 1U, 2U, 4U, 3U};
    FT_ASSERT(order == depth_first);
    return (1);
}

FT_TEST(test_ft_graph_dense_dfs_visits_each_vertex_once)
{
    constexpr uint32_t SIZE = 24U;
    ft_graph<uint32_t> graph(1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.initialize());
    uint32_t from = 0U;
    while (from < SIZE)
    {
        FT_ASSERT_EQ(from, graph.add_vertex(from));
        ++from;
    }
    from = 0U;
    while (from < SIZE)
    {
        uint32_t to = 0U;
        while (to < SIZE)
        {
            graph.add_edge(from, to);
            graph.add_edge(from, to);
            FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.get_error());
            ++to;
        }
        ++from;
    }
    uint32_t count = 0U;
    ft_bool in_order = FT_TRUE;
    graph.dfs(0U, [&](uint32_t value)
    {
        if (value != count)
            in_order = FT_FALSE;
        ++count;
        return ;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.get_error());
    FT_ASSERT_EQ(SIZE, count);
    FT_ASSERT_EQ(FT_TRUE, in_order);

    ft_graph<uint32_t> oversized(FT_SYSTEM_SIZE_MAX);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, oversized.initialize());
    return (1);
}
