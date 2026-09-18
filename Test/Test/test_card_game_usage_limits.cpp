#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_usage_limits.hpp"
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
