#include "../test_internal.hpp"
#include "../../Modules/Game/game_map3d.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_map3d_storage_preserves_cells_when_resize_fails)
{
    game_map3d map;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map.initialize(4U, 3U, 2U, -7));
    map.set(3U, 2U, 1U, 33);
    FT_ASSERT_EQ(-7, map.get(3U, 1U, 1U));
    FT_ASSERT_EQ(33, map.get(3U, 2U, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    map.resize(5U, 3U, 2U, 2);
    const int32_t growth_result = map.get_error();
    const int32_t previous_cell = map.get(3U, 2U, 1U);
    map.resize(2U, 3U, 4U, -11);
    const int32_t reshape_result = map.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, growth_result);
    FT_ASSERT_EQ(33, previous_cell);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reshape_result);
    FT_ASSERT_EQ(2U, map.get_width());
    FT_ASSERT_EQ(4U, map.get_depth());
    FT_ASSERT_EQ(-11, map.get(1U, 2U, 3U));

    map.resize(FT_SYSTEM_SIZE_MAX, 2U, 2U, 0);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, map.get_error());
    FT_ASSERT_EQ(2U, map.get_width());
    FT_ASSERT_EQ(-11, map.get(1U, 2U, 3U));
    return (1);
}

FT_TEST(test_game_map3d_storage_moves_cells_without_allocation)
{
    game_map3d original;
    game_map3d destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize(4U, 3U, 2U, 9));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.enable_thread_safety());
    original.set(2U, 1U, 1U, -1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t moved = destination.move(original);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved);
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, original.is_thread_safe());
    FT_ASSERT_EQ(4U, destination.get_width());
    FT_ASSERT_EQ(-1, destination.get(2U, 1U, 1U));
    FT_ASSERT_EQ(9, destination.get(2U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize());
    return (1);
}
