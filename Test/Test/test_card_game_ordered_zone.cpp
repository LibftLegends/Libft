#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_ordered_zone.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_card_game_ordered_zone_preserves_top_bottom_and_index_order)
{
    card_game_ordered_zone zone;
    uint32_t value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.initialize(5U, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.push_bottom(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.push_bottom(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.push_top(3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.insert_at(1U, 4U));
    FT_ASSERT_EQ(4U, zone.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.peek_top(&value));
    FT_ASSERT_EQ(3U, value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.peek_bottom(&value));
    FT_ASSERT_EQ(2U, value);
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, zone.push_bottom(4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.pop_top(&value));
    FT_ASSERT_EQ(3U, value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.get(0U, &value));
    FT_ASSERT_EQ(4U, value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.remove_instance(4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.pop_bottom(&value));
    FT_ASSERT_EQ(2U, value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.destroy());
    return (1);
}

FT_TEST(test_card_game_ordered_zone_enforces_capacity_and_empty_behavior)
{
    card_game_ordered_zone zone;
    uint32_t value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.initialize(2U, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.push_bottom(7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.push_bottom(7U));
    FT_ASSERT_EQ(FT_ERR_FULL, zone.push_bottom(8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.clear());
    FT_ASSERT_EQ(FT_ERR_EMPTY, zone.peek_top(&value));
    FT_ASSERT_EQ(FT_ERR_EMPTY, zone.pop_bottom(&value));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, zone.insert_at(1U, 9U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, zone.destroy());
    return (1);
}

FT_TEST(test_card_game_ordered_zone_shuffle_is_deterministic_and_seeded)
{
    card_game_ordered_zone first;
    card_game_ordered_zone second;
    uint64_t first_seed;
    uint64_t second_seed;
    uint32_t index;
    uint32_t first_value;
    uint32_t second_value;

    first_seed = 0x123456789abcdef1ULL;
    second_seed = first_seed;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(8U, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(8U, FT_FALSE));
    index = 0U;
    while (index < 8U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, first.push_bottom(index + 1U));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, second.push_bottom(index + 1U));
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.shuffle(&first_seed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.shuffle(&second_seed));
    index = 0U;
    while (index < 8U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, first.get(index, &first_value));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, second.get(index, &second_value));
        FT_ASSERT_EQ(first_value, second_value);
        index += 1U;
    }
    first_seed = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, first.shuffle(&first_seed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}
