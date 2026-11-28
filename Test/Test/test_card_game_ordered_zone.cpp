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

FT_TEST(test_card_game_ordered_zone_wraps_without_changing_entry_order)
{
    card_game_ordered_zone source;
    card_game_ordered_zone moved;
    card_game_ordered_zone reference;
    card_game_zone_entry entry{};
    constexpr uint32_t ROTATIONS = FT_CARD_GAME_MAX_CARDS - 2U;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(8U, FT_TRUE));
    uint32_t index = 1U;
    while (index <= 8U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.push_bottom_entry({index, index + 1000U}));
        ++index;
    }
    index = 1U;
    while (index <= ROTATIONS)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.pop_top_entry(&entry));
        FT_ASSERT_EQ(index, entry.instance_id);
        FT_ASSERT_EQ(index + 1000U, entry.card_id);
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            source.push_bottom_entry({index + 8U, index + 1008U}));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.pop_bottom_entry(&entry));
    FT_ASSERT_EQ(ROTATIONS + 8U, entry.instance_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.insert_entry_at(1U, {999U, 9U}));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.remove_entry(999U, &entry));
    FT_ASSERT_EQ(9U, entry.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.insert_entry_at(3U, {ROTATIONS + 1U, 9U}));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.remove_entry(ROTATIONS + 1U, &entry));
    FT_ASSERT_EQ(ROTATIONS + 1001U, entry.card_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.move(source));
    FT_ASSERT_EQ(0U, source.size());
    FT_ASSERT_EQ(8U, moved.capacity());
    FT_ASSERT_EQ(7U, moved.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reference.initialize(8U, FT_TRUE));
    const uint32_t expected_ids[] = {
        ROTATIONS + 2U, ROTATIONS + 3U, ROTATIONS + 1U, ROTATIONS + 4U,
        ROTATIONS + 5U, ROTATIONS + 6U, ROTATIONS + 7U
    };
    index = 0U;
    while (index < moved.size())
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_entry(index, &entry));
        FT_ASSERT_EQ(expected_ids[index], entry.instance_id);
        if (index == 2U)
            FT_ASSERT_EQ(9U, entry.card_id);
        else
            FT_ASSERT_EQ(entry.instance_id + 1000U, entry.card_id);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, reference.push_bottom_entry(entry));
        ++index;
    }
    uint64_t moved_seed = 1234567U;
    uint64_t reference_seed = moved_seed;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.shuffle(&moved_seed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reference.shuffle(&reference_seed));
    FT_ASSERT_EQ(reference_seed, moved_seed);
    while (moved.size() != 0U)
    {
        card_game_zone_entry expected{};
        FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.pop_top_entry(&entry));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, reference.pop_top_entry(&expected));
        FT_ASSERT_EQ(expected.instance_id, entry.instance_id);
        FT_ASSERT_EQ(expected.card_id, entry.card_id);
    }
    return (1);
}
