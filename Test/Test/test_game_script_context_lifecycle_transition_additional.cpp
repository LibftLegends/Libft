#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void script_context_copy_uninitialised(game_script_context &value)
{
    game_script_context source;

    (void)value.initialize(source);
    return ;
}

static void
script_context_move_initialize_uninitialised(game_script_context &value)
{
    game_script_context source;

    (void)value.initialize(static_cast<game_script_context &&>(source));
    return ;
}

static void script_context_move_uninitialised(game_script_context &value)
{
    game_script_context source;

    (void)value.move(source);
    return ;
}

FT_TEST(test_game_script_context_copy_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_context>(
                        script_context_copy_uninitialised));
    return (1);
}

FT_TEST(test_game_script_context_move_initialize_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_context>(
                        script_context_move_initialize_uninitialised));
    return (1);
}

FT_TEST(test_game_script_context_move_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_context>(
                        script_context_move_uninitialised));
    return (1);
}

FT_TEST(test_game_script_context_copy_destroyed_source_propagates)
{
    game_script_context source;
    game_script_context destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_script_context_move_destroyed_source_propagates)
{
    game_script_context source;
    game_script_context destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(
        FT_ERR_SUCCESS,
        destination.initialize(static_cast<game_script_context &&>(source)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_script_context_copy_into_initialized_destination_commits)
{
    game_script_context source;
    game_script_context destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(ft_nullptr, destination.get_state());
    return (1);
}

FT_TEST(test_game_script_context_move_into_initialized_destination_commits)
{
    game_script_context source;
    game_script_context destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(
        FT_ERR_SUCCESS,
        destination.initialize(static_cast<game_script_context &&>(source)));
    FT_ASSERT_EQ(ft_nullptr, destination.get_state());
    return (1);
}

FT_TEST(test_game_script_context_self_copy_is_safe)
{
    game_script_context value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(value));
    return (1);
}

FT_TEST(test_game_script_context_explicit_move_keeps_owned_variables_and_result)
{
    game_script_context source;
    game_script_context destination;
    ft_string key;
    ft_string value;
    uint32_t user_data = 7U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, key.initialize("record"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize("kept"));
    source.set_variable(key, value);
    const ft_string *original_value = source.get_variable(key);
    FT_ASSERT(original_value != ft_nullptr);
    source.set_result_integer(19);
    source.set_user_data(&user_data);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    const ft_string *moved_value = destination.get_variable(key);
    FT_ASSERT(moved_value == original_value);
    FT_ASSERT_STR_EQ("kept", moved_value->c_str());
    FT_ASSERT_EQ(FT_TRUE, destination.has_result_integer());
    FT_ASSERT_EQ(19, destination.get_result_integer());
    FT_ASSERT(destination.get_user_data() == &user_data);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT(source.get_variable(key) == ft_nullptr);
    return (1);
}

FT_TEST(test_game_script_context_self_move_is_safe_again)
{
    game_script_context value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    return (1);
}

FT_TEST(test_game_script_context_destructor_after_destroy_is_safe)
{
    game_script_context *value;

    value = new game_script_context();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->destroy());
    delete value;
    FT_ASSERT_EQ(1, 1);
    return (1);
}
