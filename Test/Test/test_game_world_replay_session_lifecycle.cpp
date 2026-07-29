#include "../test_internal.hpp"

#include "../../Modules/Game/game_world_replay.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_world_replay_session_lifecycle)
{
    game_world_replay_session replay_session;
    ft_string snapshot_payload;
    ft_string exported_snapshot;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot_payload.initialize("{\"tick\":1}"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.import_snapshot(snapshot_payload));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.export_snapshot(exported_snapshot));
    FT_ASSERT(exported_snapshot.size() > 0U);
    replay_session.clear_snapshot();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.export_snapshot(exported_snapshot));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), exported_snapshot.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot_payload.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, exported_snapshot.destroy());
    return (1);
}
