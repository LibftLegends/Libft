#include "../test_internal.hpp"
#include "../../Modules/Game/game_pathfinding.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_pathfinding_route_cache_checks_both_endpoints)
{
    game_map3d grid;
    game_pathfinding finder;
    ft_vector<game_path_step> path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grid.initialize(5, 1, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.recalculate_path(grid, 0, 0, 0, 4, 0, 0, path));
    FT_ASSERT_EQ(5U, path.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.recalculate_path(grid, 0, 0, 0, 2, 0, 0, path));
    FT_ASSERT_EQ(3U, path.size());
    FT_ASSERT_EQ(2U, path[2].get_x());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.recalculate_path(grid, 1, 0, 0, 2, 0, 0, path));
    FT_ASSERT_EQ(2U, path.size());
    FT_ASSERT_EQ(1U, path[0].get_x());

    grid.toggle_obstacle(2, 0, 0, &finder);
    FT_ASSERT_EQ(FT_ERR_GAME_INVALID_MOVE,
        finder.recalculate_path(grid, 1, 0, 0, 2, 0, 0, path));
    return (1);
}

FT_TEST(test_game_pathfinding_route_cache_moves_and_retries_output_growth)
{
    game_map3d grid;
    game_pathfinding finder;
    ft_vector<game_path_step> path;
    ft_vector<game_path_step> fresh_output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grid.initialize(12, 1, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fresh_output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.recalculate_path(grid, 0, 0, 0, 11, 0, 0, path));
    game_pathfinding moved;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.move(finder));
    FT_ASSERT_EQ(FT_TRUE, moved.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, finder.is_thread_safe());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, fresh_output.push_back(path[3]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t failed = moved.recalculate_path(grid, 0, 0, 0, 11, 0, 0, fresh_output);
    const int32_t reused = moved.recalculate_path(grid, 0, 0, 0, 11, 0, 0, path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reused);
    FT_ASSERT_EQ(1U, fresh_output.size());
    FT_ASSERT_EQ(3U, fresh_output[0].get_x());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.recalculate_path(grid, 0, 0, 0, 11, 0, 0, fresh_output));
    FT_ASSERT_EQ(12U, fresh_output.size());
    FT_ASSERT_EQ(11U, fresh_output[11].get_x());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.initialize());
    return (1);
}

FT_TEST(test_game_pathfinding_route_cache_distinguishes_grid_lifetimes)
{
    game_map3d first;
    game_map3d second;
    game_pathfinding finder;
    ft_vector<game_path_step> path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(5, 1, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(5, 1, 1, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.recalculate_path(first, 0, 0, 0, 4, 0, 0, path));
    FT_ASSERT_EQ(FT_ERR_GAME_INVALID_MOVE,
        finder.recalculate_path(second, 0, 0, 0, 4, 0, 0, path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.recalculate_path(first, 0, 0, 0, 4, 0, 0, path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(5, 1, 1, 1));
    FT_ASSERT_EQ(FT_ERR_GAME_INVALID_MOVE,
        finder.recalculate_path(first, 0, 0, 0, 4, 0, 0, path));
    return (1);
}

FT_TEST(test_game_pathfinding_route_cache_tracks_unnotified_grid_changes)
{
    game_map3d grid;
    game_pathfinding finder;
    ft_vector<game_path_step> path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grid.initialize(12, 2, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grid.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    grid.set(5, 0, 0, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.recalculate_path(grid, 0, 0, 0, 11, 0, 0, path));
    FT_ASSERT_EQ(14U, path.size());

    grid.set(5, 0, 0, 2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t reused = finder.recalculate_path(grid, 0, 0, 0, 11, 0, 0, path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reused);
    FT_ASSERT_EQ(14U, path.size());

    grid.toggle_obstacle(5, 0, 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, finder.recalculate_path(grid, 0, 0, 0, 11, 0, 0, path));
    FT_ASSERT_EQ(12U, path.size());
    grid.resize(12, 2, 1, 1);
    FT_ASSERT_EQ(FT_ERR_GAME_INVALID_MOVE,
        finder.recalculate_path(grid, 0, 0, 0, 11, 0, 0, path));
    return (1);
}
