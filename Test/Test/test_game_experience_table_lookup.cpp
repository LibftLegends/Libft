#include "../test_internal.hpp"
#include "../../Modules/Game/game_experience_table.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_experience_table_lookup_matches_threshold_boundaries)
{
    game_experience_table table;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    constexpr int32_t LEVEL_COUNT = 32;
    int32_t levels[LEVEL_COUNT];
    int32_t index = 0;
    while (index < LEVEL_COUNT)
    {
        levels[index] = index * 3 - 12;
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.set_levels(levels, LEVEL_COUNT));
    index = 0;
    while (index < LEVEL_COUNT)
    {
        FT_ASSERT_EQ(index, table.get_level(levels[index] - 1));
        FT_ASSERT_EQ(index + 1, table.get_level(levels[index]));
        ++index;
    }
    FT_ASSERT_EQ(LEVEL_COUNT, table.get_level(levels[LEVEL_COUNT - 1] + 1));
    return (1);
}

FT_TEST(test_game_experience_table_lookup_preserves_invalid_prefix_and_repairs)
{
    game_experience_table table;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    const int32_t levels[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.set_levels(levels, 10));
    table.set_value(3, 1000);
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, table.get_error());
    FT_ASSERT_EQ(50, table.check_for_error());
    FT_ASSERT_EQ(3, table.get_level(500));
    table.set_value(7, 2000);
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, table.get_error());
    table.set_value(3, 40);
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, table.get_error());
    FT_ASSERT_EQ(7, table.get_level(500));

    game_experience_table moved;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.move(table));
    FT_ASSERT_EQ(7, moved.get_level(500));
    moved.set_value(7, 80);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_error());
    FT_ASSERT_EQ(10, moved.get_level(500));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.resize(12));
    FT_ASSERT_EQ(0, moved.check_for_error());
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, moved.get_error());
    FT_ASSERT_EQ(12, moved.get_level(500));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.resize(10));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.check_for_error());
    FT_ASSERT_EQ(9, moved.get_level(98));
    return (1);
}
