#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_usage_limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include <cstddef>
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_card_game_usage_limits_hard_and_soft_windows)
{
    card_game_usage_limit_ledger ledger;
    card_game_usage_limit limit;
    uint32_t turn_limit;
    uint32_t match_limit;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_limit(100U, 7U,
        CARD_GAME_USAGE_TURN, 3U, 1U, CARD_GAME_USAGE_ON_ACTIVATION,
        0U, &turn_limit));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.can_consume(turn_limit, 1U, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.consume(turn_limit, 1U, 3U));
    FT_ASSERT_EQ(FT_ERR_FULL, ledger.can_consume(turn_limit, 1U, 3U));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, ledger.can_consume(turn_limit, 1U,
        4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.reset_epoch(4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.consume(turn_limit, 1U, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_limit(101U, 7U,
        CARD_GAME_USAGE_MATCH, 0U, 2U, CARD_GAME_USAGE_ON_RESOLUTION,
        500U, &match_limit));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.consume(match_limit, 2U, 99U));
    FT_ASSERT_EQ(FT_ERR_FULL, ledger.can_consume(match_limit, 1U, 99U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(match_limit, &limit));
    FT_ASSERT_EQ(500U, limit.source_instance);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_usage_limits_reject_invalid_and_duplicate_registration)
{
    card_game_usage_limit_ledger ledger;
    uint32_t limit_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ledger.register_limit(0U, 1U,
        CARD_GAME_USAGE_TURN, 1U, 1U, CARD_GAME_USAGE_ON_ATTEMPT,
        0U, &limit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_limit(1U, 1U,
        CARD_GAME_USAGE_TURN, 1U, 1U, CARD_GAME_USAGE_ON_ATTEMPT,
        0U, &limit_id));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, ledger.register_limit(1U, 1U,
        CARD_GAME_USAGE_TURN, 1U, 1U, CARD_GAME_USAGE_ON_ATTEMPT,
        0U, &limit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_usage_limits_snapshot_restores_effect_usage)
{
    card_game_usage_limit_ledger ledger;
    card_game_usage_limit_snapshot saved;
    card_game_usage_limit_snapshot copy;
    card_game_usage_limit_snapshot changed;
    card_game_usage_limit limit;
    uint32_t limit_id;

    ft_bzero(&saved, sizeof(saved));
    ft_bzero(&copy, sizeof(copy));
    ft_bzero(&changed, sizeof(changed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.register_limit(900U, 400U,
        CARD_GAME_USAGE_TURN, 8U, 2U, CARD_GAME_USAGE_ON_RESOLUTION,
        700U, &limit_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.consume(limit_id, 1U, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_snapshot(&saved));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_usage_limit_ledger::clone_snapshot(saved, &copy));
    FT_ASSERT(card_game_usage_limit_ledger::snapshots_equal(saved, copy));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.consume(limit_id, 1U, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(limit_id, &limit));
    FT_ASSERT_EQ(2U, limit.used_uses);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get_snapshot(&changed));
    FT_ASSERT(!card_game_usage_limit_ledger::snapshots_equal(saved, changed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.apply_snapshot(saved));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(limit_id, &limit));
    FT_ASSERT_EQ(1U, limit.used_uses);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_usage_limit_ledger::release_snapshot(&saved));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_usage_limit_ledger::release_snapshot(&copy));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        card_game_usage_limit_ledger::release_snapshot(&changed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.destroy());
    return (1);
}

FT_TEST(test_card_game_usage_limits_restore_keeps_order_and_reuses_storage)
{
    card_game_usage_limit_ledger ledger;
    card_game_usage_limit entries[3] = {
        {10U, 1U, 1U, CARD_GAME_USAGE_TURN, 7U, 5U, 0U, CARD_GAME_USAGE_ON_ATTEMPT, 0U},
        {1U, 2U, 1U, CARD_GAME_USAGE_MATCH, 0U, 5U, 0U, CARD_GAME_USAGE_ON_ATTEMPT, 0U},
        {3U, 3U, 1U, CARD_GAME_USAGE_TURN, 7U, 5U, 0U, CARD_GAME_USAGE_ON_ATTEMPT, 0U}
    };
    const card_game_usage_limit_snapshot snapshot{3U, 11U, 3U, entries};
    card_game_usage_limit limit{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.initialize());
    cma_set_alloc_limit(1);
    const int32_t status = ledger.apply_snapshot(snapshot);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.consume(10U, 2U, 7U));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, ledger.consume(3U, 1U, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.consume(1U, 1U, 8U));
    entries[1].limit_id = 10U;
    entries[2].maximum_uses = 0U;
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, ledger.apply_snapshot(snapshot));
    entries[1].key_id = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ledger.apply_snapshot(snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(10U, &limit));
    FT_ASSERT_EQ(2U, limit.used_uses);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ledger.get(3U, &limit));
    FT_ASSERT_EQ(5U, limit.maximum_uses);
    return (1);
}

FT_TEST(test_card_game_usage_limits_snapshot_equality_ignores_record_padding)
{
    card_game_usage_limit first{};
    first.limit_id = 1U;
    first.key_id = 2U;
    first.subject_id = 3U;
    first.maximum_uses = 2U;
    card_game_usage_limit second{};
    ft_memcpy(&second, &first, sizeof(first));
    constexpr ft_size_t PADDING_BYTE = offsetof(card_game_usage_limit, scope)
        + sizeof(card_game_usage_scope);
    if constexpr (PADDING_BYTE < offsetof(card_game_usage_limit, window_epoch))
        reinterpret_cast<uint8_t *>(&second)[PADDING_BYTE] = 0xa5U;
    const card_game_usage_limit_snapshot first_snapshot{1U, 2U, 1U, &first};
    const card_game_usage_limit_snapshot second_snapshot{1U, 2U, 1U, &second};
    FT_ASSERT_EQ(FT_TRUE,
        card_game_usage_limit_ledger::snapshots_equal(first_snapshot, second_snapshot));
    second.used_uses = 1U;
    FT_ASSERT_EQ(FT_FALSE,
        card_game_usage_limit_ledger::snapshots_equal(first_snapshot, second_snapshot));
    return (1);
}
