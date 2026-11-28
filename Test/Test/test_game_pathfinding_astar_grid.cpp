#include "../test_internal.hpp"
#include "../../Modules/Game/game_pathfinding.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_pathfinding_astar_grid_preserves_tie_order)
{
    game_map3d grid;
    game_pathfinding finder;
    ft_vector<game_path_step> path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grid.initialize(4, 3, 3, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.astar_grid(grid, 0, 0, 0, 3, 2, 2, path));
    FT_ASSERT_EQ(8U, path.size());
    const ft_size_t expected[][3] = {
        {0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0},
        {3, 1, 0}, {3, 2, 0}, {3, 2, 1}, {3, 2, 2}};
    ft_size_t index = 0;
    while (index < path.size())
    {
        FT_ASSERT_EQ(expected[index][0], path[index].get_x());
        FT_ASSERT_EQ(expected[index][1], path[index].get_y());
        FT_ASSERT_EQ(expected[index][2], path[index].get_z());
        ++index;
    }
    return (1);
}

FT_TEST(test_game_pathfinding_astar_grid_reports_allocation_failure_and_retries)
{
    game_map3d grid;
    game_pathfinding finder;
    ft_vector<game_path_step> path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grid.initialize(6, 6, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t trivial = finder.astar_grid(grid, 1, 1, 0, 1, 1, 0, path);
    const int32_t shortage = finder.astar_grid(grid, 0, 0, 0, 5, 5, 0, path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, trivial);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, shortage);
    FT_ASSERT_EQ(0U, path.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.astar_grid(grid, 0, 0, 0, 5, 5, 0, path));
    FT_ASSERT_EQ(11U, path.size());
    return (1);
}

FT_TEST(test_game_pathfinding_astar_grid_uses_improved_frontier_priority)
{
    game_map3d grid;
    game_pathfinding finder;
    ft_vector<game_path_step> path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grid.initialize(7, 7, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    constexpr ft_size_t WALLS[] = {12, 15, 17, 22, 24, 25, 31, 32, 35, 39, 40, 41};
    ft_size_t index = 0U;
    while (index < sizeof(WALLS) / sizeof(WALLS[0]))
    {
        grid.set(WALLS[index] % 7U, WALLS[index] / 7U, 0U, 1);
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.astar_grid(grid, 0, 3, 0, 6, 3, 0, path));
    // The route through cell 9 is shortened after that cell enters the frontier.
    constexpr ft_size_t ROUTE[] = {21, 14, 7, 8, 9, 10, 11, 18, 19, 20, 27};
    FT_ASSERT_EQ(sizeof(ROUTE) / sizeof(ROUTE[0]), path.size());
    index = 0U;
    while (index < path.size())
    {
        FT_ASSERT_EQ(ROUTE[index] % 7U, path[index].get_x());
        FT_ASSERT_EQ(ROUTE[index] / 7U, path[index].get_y());
        FT_ASSERT_EQ(0U, path[index].get_z());
        ++index;
    }
    return (1);
}
