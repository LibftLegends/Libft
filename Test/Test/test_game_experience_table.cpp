#include "../test_internal.hpp"
#include "../../Modules/Game/game_experience_table.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include <limits>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_experience_table_generate_total_resets_zero)
{
    game_experience_table table;
    int initial_levels[3];
    int result;

    initial_levels[0] = 5;
    initial_levels[1] = 10;
    initial_levels[2] = 20;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    result = table.set_levels(initial_levels, 3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    result = table.generate_levels_total(0, 50, 2.0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(0, table.get_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    return (1);
}

FT_TEST(test_experience_table_rejects_unsorted_levels)
{
    game_experience_table table;
    int invalid_levels[3];
    int result;

    invalid_levels[0] = 10;
    invalid_levels[1] = 5;
    invalid_levels[2] = 20;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    result = table.set_levels(invalid_levels, 3);
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, result);
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, table.get_error());
    FT_ASSERT_EQ(3, table.get_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    return (1);
}

FT_TEST(test_experience_table_set_value_updates_single_entry)
{
    game_experience_table table;
    int base_levels[3];

    base_levels[0] = 10;
    base_levels[1] = 20;
    base_levels[2] = 40;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.set_levels(base_levels, 3));
    table.set_value(1, 30);
    FT_ASSERT_EQ(3, table.get_count());
    FT_ASSERT_EQ(30, table.get_value(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    return (1);
}

FT_TEST(test_experience_table_resize_grows_and_keeps_values)
{
    game_experience_table table;
    int levels[2];

    levels[0] = 50;
    levels[1] = 100;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.set_levels(levels, 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.resize(4));
    FT_ASSERT_EQ(4, table.get_count());
    FT_ASSERT_EQ(50, table.get_value(0));
    FT_ASSERT_EQ(100, table.get_value(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    return (1);
}

FT_TEST(test_experience_table_get_level_boundaries)
{
    game_experience_table table;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.generate_levels_scaled(3, 100, 2.0));
    FT_ASSERT_EQ(0, table.get_level(50));
    FT_ASSERT_EQ(1, table.get_level(100));
    FT_ASSERT_EQ(2, table.get_level(350));
    FT_ASSERT_EQ(3, table.get_level(800));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    return (1);
}

FT_TEST(test_experience_table_thread_safety_toggle)
{
    game_experience_table table;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(false, table.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.enable_thread_safety());
    FT_ASSERT_EQ(true, table.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.disable_thread_safety());
    FT_ASSERT_EQ(false, table.is_thread_safe());
    return (1);
}

FT_TEST(test_game_experience_table_generation_checks_integer_conversion)
{
    game_experience_table table;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    constexpr int32_t MAXIMUM = std::numeric_limits<int32_t>::max();
    constexpr int32_t MINIMUM = std::numeric_limits<int32_t>::min();
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        table.generate_levels_total(2, 1, static_cast<double>(MAXIMUM) + 0.5));
    FT_ASSERT_EQ(MAXIMUM, table.get_value(1));
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION,
        table.generate_levels_total(2, 1, static_cast<double>(MAXIMUM) + 1.0));
    FT_ASSERT_EQ(MINIMUM, table.check_for_error());
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION, table.get_error());
    FT_ASSERT_EQ(FT_ERR_CONFIGURATION,
        table.generate_levels_scaled(3, 1, std::numeric_limits<double>::quiet_NaN()));
    FT_ASSERT_EQ(MINIMUM, table.get_value(1));
    FT_ASSERT_EQ(MINIMUM, table.get_value(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.generate_levels_scaled(3, 1, 2.0));
    FT_ASSERT_EQ(7, table.get_value(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.check_for_error());
    return (1);
}

FT_TEST(test_game_experience_table_replaces_existing_capacity_with_allocations_limited)
{
    game_experience_table table;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    const int32_t original[] = {1, 5, 10};
    const int32_t replacement[] = {5, 20, 30, 40};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.set_levels(original, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t replace_result = table.set_levels(replacement, 3);
    const int32_t growth_result = table.set_levels(replacement, 4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replace_result);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, growth_result);
    FT_ASSERT_EQ(3, table.get_count());
    FT_ASSERT_EQ(30, table.get_value(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.check_for_error());
    return (1);
}
