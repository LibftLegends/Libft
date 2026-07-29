#include "../../Modules/Game/game_world_replay.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void replay_initialize_twice(game_world_replay_session &value)
{
    (void)value.initialize();
    (void)value.initialize();
    return ;
}

FT_TEST(test_game_world_replay_session_initialize_twice_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_world_replay_session>(
                        replay_initialize_twice));
    return (1);
}

FT_TEST(test_game_world_replay_session_initialize_succeeds)
{
    game_world_replay_session value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_world_replay_session_destroy_twice_is_safe)
{
    game_world_replay_session value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_world_replay_session_reinitialize_after_destroy)
{
    game_world_replay_session value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    return (1);
}

FT_TEST(test_game_world_replay_session_self_move_is_safe)
{
    game_world_replay_session value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    return (1);
}

FT_TEST(test_game_world_replay_session_clear_snapshot_is_idempotent)
{
    game_world_replay_session value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.clear_snapshot();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    value.clear_snapshot();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}
