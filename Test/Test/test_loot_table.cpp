#include "../test_internal.hpp"
#include "../../Modules/RNG/loot_table.hpp"
#include "../../Modules/RNG/rng_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include <climits>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_loot_table_seeded_pops_preserve_weights_after_copy_and_move)
{
    constexpr uint32_t SEED = 979135U;
    constexpr ft_size_t ENTRY_COUNT = 9U;
    int32_t items[ENTRY_COUNT] = {};
    int32_t effective_weights[ENTRY_COUNT] = {1, 4, 3, 1, 7, 2, 1, 4, 2};
    constexpr int32_t WEIGHTS[ENTRY_COUNT] = {1, 4, 7, 4, 21, 9, 2, 8, 2};
    constexpr int32_t RARITIES[ENTRY_COUNT] = {0, 0, 1, INT32_MAX, 2, 3, 6, 1, -4};
    ft_loot_table<int32_t> source;
    ft_loot_table<int32_t> copy;
    ft_loot_table<int32_t> moved;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    int32_t *expected_items[ENTRY_COUNT];
    int32_t total_weight = 0;
    ft_size_t index = 0U;
    while (index < ENTRY_COUNT)
    {
        expected_items[index] = &items[index];
        total_weight += effective_weights[index];
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_element(&items[index], WEIGHTS[index], RARITIES[index]));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), moved.move(copy));
    FT_ASSERT_EQ(0U, copy.size());
    FT_ASSERT_EQ(ENTRY_COUNT, source.size());
    FT_ASSERT_EQ(FT_TRUE, moved.is_thread_safe());
    ft_seed_random_engine(SEED);
    std::mt19937 reference(SEED);
    std::uniform_int_distribution<int32_t> distribution(0, INT32_MAX);

    ft_size_t remaining = ENTRY_COUNT;
    while (remaining != 0U)
    {
        int32_t roll = 1;
        if (total_weight > 1)
            roll += distribution(reference) % total_weight;
        index = 0U;
        while (roll > effective_weights[index])
        {
            roll -= effective_weights[index];
            index += 1U;
        }
        const Pair<int32_t, int32_t *> selected = moved.pop_random_loot();
        FT_ASSERT_EQ(FT_ERR_SUCCESS, selected.key);
        FT_ASSERT(selected.value == expected_items[index]);
        total_weight -= effective_weights[index];
        remaining -= 1U;
        while (index < remaining)
        {
            effective_weights[index] = effective_weights[index + 1U];
            expected_items[index] = expected_items[index + 1U];
            index += 1U;
        }
        FT_ASSERT_EQ(remaining, moved.size());
    }
    FT_ASSERT_EQ(FT_ERR_EMPTY, moved.pop_random_loot().key);
    FT_ASSERT_EQ(distribution(reference), ft_random_int());
    return (1);
}

FT_TEST(test_loot_table_saturated_weight_preserves_overflow_without_a_draw)
{
    constexpr uint32_t SEED = 66107U;
    int32_t item = 4;
    ft_loot_table<int32_t> table;
    ft_loot_table<int32_t> copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.add_element(&item, INT32_MAX, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.add_element(&item, INT32_MAX, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.add_element(&item, INT32_MAX, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(table));
    ft_seed_random_engine(SEED);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, copy.get_random_loot().key);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, table.pop_random_loot().key);
    FT_ASSERT_EQ(3U, table.size());
    std::mt19937 reference(SEED);
    const int32_t expected = std::uniform_int_distribution<int32_t>(0, INT32_MAX)(reference);
    FT_ASSERT_EQ(expected, ft_random_int());
    return (1);
}

FT_TEST(test_loot_table_get_random_success_sets_success)
{
    ft_loot_table<int> table;
    int treasure_value = 42;
    Pair<int, int *> result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    result = table.get_random_loot();
    FT_ASSERT_EQ(FT_ERR_EMPTY, result.key);
    FT_ASSERT_EQ(ft_nullptr, result.value);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.add_element(&treasure_value, 10, 0));
    result = table.get_random_loot();
    if (result.value == ft_nullptr)
        return (0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result.key);
    FT_ASSERT_EQ(&treasure_value, result.value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.destroy());
    return (1);
}

FT_TEST(test_loot_table_get_random_overflow_sets_error)
{
    ft_loot_table<int> table;
    int rare_gem = 100;
    int common_item = 50;
    Pair<int, int *> result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.add_element(&rare_gem, INT_MAX, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.add_element(&common_item, 1, 0));

    result = table.get_random_loot();
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, result.key);
    FT_ASSERT_EQ(ft_nullptr, result.value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.destroy());
    return (1);
}

FT_TEST(test_loot_table_pop_random_success_sets_success)
{
    ft_loot_table<int> table;
    int loot_piece = 7;
    Pair<int, int *> result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    result = table.pop_random_loot();
    FT_ASSERT_EQ(FT_ERR_EMPTY, result.key);
    FT_ASSERT_EQ(ft_nullptr, result.value);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.add_element(&loot_piece, 3, 0));
    result = table.pop_random_loot();
    if (result.value != &loot_piece)
        return (0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result.key);
    FT_ASSERT_EQ(0u, table.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.destroy());
    return (1);
}

FT_TEST(test_loot_table_pop_random_overflow_sets_error)
{
    ft_loot_table<int> table;
    int legendary_item = 5;
    int mundane_item = 6;
    Pair<int, int *> result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.add_element(&legendary_item, INT_MAX, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.add_element(&mundane_item, 1, 0));

    result = table.pop_random_loot();
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, result.key);
    FT_ASSERT_EQ(ft_nullptr, result.value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.destroy());
    return (1);
}
