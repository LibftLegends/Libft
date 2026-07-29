#include "../../Modules/Game/game_world_replay.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_world_replay_empty_export_is_empty)
{
    game_world_replay_session value;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.export_snapshot(output));
    FT_ASSERT(output.empty() == FT_TRUE);
    return (1);
}

FT_TEST(test_game_world_replay_restore_before_capture_reports_state)
{
    game_world_replay_session value;
    ft_sharedptr<game_world> world_pointer;
    game_character character;
    game_inventory inventory;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
                 value.restore_snapshot(world_pointer, character, inventory));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_world_replay_restore_null_world_reports_argument)
{
    game_world_replay_session value;
    ft_sharedptr<game_world> world_pointer;
    game_character character;
    game_inventory inventory;
    ft_string input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize("snapshot"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.import_snapshot(input));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
                 value.restore_snapshot(world_pointer, character, inventory));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_world_replay_import_empty_snapshot_succeeds)
{
    game_world_replay_session value;
    ft_string input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.import_snapshot(input));
    return (1);
}

FT_TEST(test_game_world_replay_clear_then_export_is_empty)
{
    game_world_replay_session value;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    value.clear_snapshot();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.export_snapshot(output));
    FT_ASSERT(output.empty() == FT_TRUE);
    return (1);
}

FT_TEST(test_game_world_replay_zero_tick_restore_reports_state)
{
    game_world_replay_session value;
    ft_sharedptr<game_world> world_pointer;
    game_character character;
    game_inventory inventory;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
                 value.replay_ticks(world_pointer, character, inventory, 0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_world_replay_negative_tick_restore_reports_state)
{
    game_world_replay_session value;
    ft_sharedptr<game_world> world_pointer;
    game_character character;
    game_inventory inventory;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
                 value.replay_ticks(world_pointer, character, inventory, -1));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_world_replay_invalid_route_coordinates_report_error)
{
    game_world_replay_session value;
    game_world world;
    game_map3d grid;
    ft_vector<game_path_step> path;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grid.initialize(1, 1, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_NEQ(FT_ERR_SUCCESS,
                  value.plan_route(world, grid, 1, 0, 0, 0, 0, 0, path));
    return (1);
}

FT_TEST(test_game_world_replay_import_then_export_preserves_empty_payload)
{
    game_world_replay_session value;
    ft_string input;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.import_snapshot(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.export_snapshot(output));
    FT_ASSERT(output.empty() == FT_TRUE);
    return (1);
}

FT_TEST(test_game_world_replay_repeated_clear_is_safe)
{
    game_world_replay_session value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.clear_snapshot();
    value.clear_snapshot();
    return (1);
}

FT_TEST(test_game_world_replay_export_overwrites_output)
{
    game_world_replay_session value;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("stale"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.export_snapshot(output));
    FT_ASSERT(output.empty() == FT_TRUE);
    return (1);
}
