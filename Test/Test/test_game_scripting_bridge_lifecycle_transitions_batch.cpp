#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static int32_t
scripting_bridge_noop_callback(game_script_context &context,
                               const ft_vector<ft_string> &arguments) noexcept
{
    (void)context;
    (void)arguments;
    return (FT_ERR_SUCCESS);
}

static void scripting_bridge_move_from_uninitialised(game_script_bridge &value)
{
    game_script_bridge source;

    (void)value.move(source);
    return ;
}

FT_TEST(test_game_script_bridge_move_copies_initialized_language_and_limit)
{
    game_script_bridge source;
    game_script_bridge destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_language("custom");
    source.set_max_operations(9);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_STR_EQ("custom", destination.get_language().c_str());
    FT_ASSERT_EQ(9, destination.get_max_operations());
    return (1);
}

FT_TEST(test_game_script_bridge_move_replaces_initialized_destination)
{
    game_script_bridge source;
    game_script_bridge destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    source.set_max_operations(3);
    destination.set_max_operations(20);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(3, destination.get_max_operations());
    return (1);
}

FT_TEST(
    test_game_script_bridge_move_from_destroyed_source_leaves_destination_destroyed)
{
    game_script_bridge source;
    game_script_bridge destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_script_bridge_move_from_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        scripting_bridge_move_from_uninitialised));
    return (1);
}

FT_TEST(test_game_script_bridge_move_self_preserves_configuration)
{
    game_script_bridge value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_max_operations(14);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    FT_ASSERT_EQ(14, value.get_max_operations());
    return (1);
}

FT_TEST(test_game_script_bridge_move_source_can_be_reinitialized)
{
    game_script_bridge source;
    game_script_bridge destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_max_operations(6);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(32, source.get_max_operations());
    return (1);
}

FT_TEST(
    test_game_script_bridge_move_destination_can_be_reinitialized_after_destroyed_source)
{
    game_script_bridge source;
    game_script_bridge destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(32, destination.get_max_operations());
    return (1);
}

FT_TEST(test_game_script_bridge_move_clears_source_callback_collection)
{
    game_script_bridge source;
    game_script_bridge destination;
    ft_string callback_name;
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)>
        callback(scripting_bridge_noop_callback);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, callback_name.initialize("callback"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 source.register_function(callback_name, callback));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0, source.get_callback_count());
    return (1);
}

FT_TEST(test_game_script_bridge_move_disables_destination_thread_safety)
{
    game_script_bridge source;
    game_script_bridge destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    return (1);
}

FT_TEST(test_game_script_bridge_move_rebinds_active_callbacks_and_keeps_vm_limit)
{
    game_script_bridge source;
    game_script_bridge destination;
    ft_string removed_name;
    ft_string retained_name;
    ft_string script;
    uint32_t callback_calls = 0U;
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)> callback(
        [&callback_calls](game_script_context &, const ft_vector<ft_string> &arguments) noexcept
        {
            if (arguments.size() != 1U || arguments[0] != "12")
                return (FT_ERR_INVALID_ARGUMENT);
            ++callback_calls;
            return (FT_ERR_SUCCESS);
        });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, removed_name.initialize("removed"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, retained_name.initialize("retained"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_function(removed_name, callback));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_function(retained_name, callback));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.remove_function(removed_name));
    source.set_max_operations(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(1U, destination.get_callback_count());
    uint32_t native_id;
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, destination._custom_engine.find_native("removed", 7U, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination._custom_engine.find_native("retained", 8U, &native_id));
    FT_ASSERT_EQ(0U, native_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("return 1 + 2;"));
    FT_ASSERT_EQ(FT_ERR_FULL,
        destination.execute_custom_with_user_data(script, ft_nullptr, ft_nullptr));
    destination.set_max_operations(16);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.assign("retained(12)", sizeof("retained(12)") - 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.execute_custom_with_user_data(script, ft_nullptr, ft_nullptr));
    FT_ASSERT_EQ(1U, callback_calls);
    return (1);
}

FT_TEST(test_game_script_bridge_move_cleanup_is_idempotent)
{
    game_script_bridge source;
    game_script_bridge destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}
