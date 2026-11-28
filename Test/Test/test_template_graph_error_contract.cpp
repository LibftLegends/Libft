#include "../test_internal.hpp"
#include "../../Modules/Template/graph.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_template_graph_error_contract_missing_vertex_sets_error)
{
    ft_graph<int32_t> graph;
    ft_vector<ft_size_t> neighbors;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, neighbors.initialize());
    graph.neighbors(0, neighbors);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, graph.get_error());
    return (1);
}

FT_TEST(test_template_graph_error_contract_add_edge_success)
{
    ft_graph<int32_t> graph;
    ft_vector<ft_size_t> neighbors;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, neighbors.initialize());
    FT_ASSERT_EQ(0, graph.add_vertex(1));
    FT_ASSERT_EQ(1, graph.add_vertex(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.get_error());
    graph.add_edge(0, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.get_error());
    graph.neighbors(0, neighbors);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.get_error());
    FT_ASSERT_EQ(1, neighbors.size());
    FT_ASSERT_EQ(1, neighbors[0]);
    return (1);
}

FT_TEST(test_template_graph_neighbors_append_atomically_on_growth_failure)
{
    ft_graph<int32_t> graph;
    ft_vector<ft_size_t> neighbors;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, neighbors.initialize());
    FT_ASSERT_EQ(0U, graph.add_vertex(0));
    FT_ASSERT_EQ(1U, graph.add_vertex(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, neighbors.push_back(99U));
    ft_size_t index = 0U;
    while (index < 10U)
    {
        graph.add_edge(0U, index & 1U);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.get_error());
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    graph.neighbors(0U, neighbors);
    const int32_t result = graph.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(1U, neighbors.size());
    FT_ASSERT_EQ(99U, neighbors[0]);

    graph.neighbors(0U, neighbors);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, graph.get_error());
    FT_ASSERT_EQ(11U, neighbors.size());
    index = 0U;
    while (index < 10U)
    {
        FT_ASSERT_EQ(index & 1U, neighbors[index + 1U]);
        ++index;
    }
    return (1);
}
