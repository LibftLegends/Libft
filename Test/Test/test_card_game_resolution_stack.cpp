#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_resolution_stack.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"

FT_TEST(test_card_game_resolution_stack_applies_lifo_and_fifo_order)
{
    card_game_resolution_stack lifo;
    card_game_resolution_stack fifo;
    card_game_resolution_entry entry;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, lifo.initialize(4U,
        CARD_GAME_RESOLUTION_LIFO, CARD_GAME_RESOLUTION_CLOSED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fifo.initialize(4U,
        CARD_GAME_RESOLUTION_FIFO, CARD_GAME_RESOLUTION_CLOSED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lifo.push(1U, 10U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lifo.push(2U, 11U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lifo.push(3U, 12U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fifo.push(1U, 10U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fifo.push(2U, 11U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lifo.begin_resolution());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fifo.begin_resolution());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lifo.pop_next(&entry));
    FT_ASSERT_EQ(3U, entry.entry_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lifo.pop_next(&entry));
    FT_ASSERT_EQ(2U, entry.entry_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fifo.pop_next(&entry));
    FT_ASSERT_EQ(1U, entry.entry_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lifo.end_resolution());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fifo.end_resolution());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lifo.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fifo.destroy());
    return (1);
}

FT_TEST(test_card_game_resolution_stack_enforces_admission_and_capacity)
{
    card_game_resolution_stack closed;
    card_game_resolution_stack current;
    card_game_resolution_stack deferred;
    card_game_resolution_entry entry;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, closed.initialize(2U,
        CARD_GAME_RESOLUTION_LIFO, CARD_GAME_RESOLUTION_CLOSED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, closed.push(1U, 1U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, closed.push(1U, 2U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, closed.begin_resolution());
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, closed.push(2U, 2U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, closed.end_resolution());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, current.initialize(2U,
        CARD_GAME_RESOLUTION_LIFO,
        CARD_GAME_RESOLUTION_OPEN_CURRENT_BATCH));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, current.push(1U, 1U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, current.begin_resolution());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, current.push(2U, 2U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_FULL, current.push(3U, 3U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deferred.initialize(2U,
        CARD_GAME_RESOLUTION_LIFO, CARD_GAME_RESOLUTION_OPEN_DEFERRED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deferred.push(1U, 1U, 0U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deferred.begin_resolution());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deferred.push(2U, 2U, 0U, 0U));
    FT_ASSERT_EQ(1U, deferred.size());
    FT_ASSERT_EQ(1U, deferred.deferred_size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deferred.pop_next(&entry));
    FT_ASSERT_EQ(1U, entry.entry_id);
    FT_ASSERT_EQ(FT_ERR_EMPTY, deferred.pop_next(&entry));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deferred.end_resolution());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deferred.begin_resolution());
    FT_ASSERT_EQ(1U, deferred.size());
    FT_ASSERT_EQ(0U, deferred.deferred_size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, closed.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, current.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deferred.destroy());
    return (1);
}

FT_TEST(test_card_game_resolution_stack_supports_large_configured_capacity)
{
    card_game_resolution_stack stack;
    card_game_resolution_entry entry;
    uint32_t index;
    int32_t push_error;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.initialize(
        1024U, CARD_GAME_RESOLUTION_LIFO,
        CARD_GAME_RESOLUTION_OPEN_CURRENT_BATCH));
    index = 0U;
    while (index < 1024U)
    {
        push_error = stack.push(
            static_cast<uint64_t>(index + 1U), index + 1U, 0U, 0U);
        if (push_error != FT_ERR_SUCCESS)
            FT_ASSERT_EQ(1024U, index);
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_FULL, stack.push(2048U, 1U, 0U, 0U));
    FT_ASSERT_EQ(1024U, stack.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.pop_next(&entry));
    FT_ASSERT_EQ(1024U, entry.entry_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack.destroy());
    return (1);
}
