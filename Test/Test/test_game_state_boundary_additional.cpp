#include "../../Modules/Game/game_state.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_state_missing_variable_reports_not_found)
{
    game_state value;
    ft_string key;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, key.initialize("missing"));
    FT_ASSERT_EQ(ft_nullptr, value.get_variable(key));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, value.get_error());
    return (1);
}

FT_TEST(test_game_state_set_and_get_variable)
{
    game_state value;
    ft_string key;
    ft_string variable_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, key.initialize("score"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, variable_value.initialize("42"));
    value.set_variable(key, variable_value);
    FT_ASSERT_STR_EQ("42", value.get_variable(key)->c_str());
    return (1);
}

FT_TEST(test_game_state_duplicate_variable_replaces_value)
{
    game_state value;
    ft_string key;
    ft_string first_value;
    ft_string second_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, key.initialize("score"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_value.initialize("1"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_value.initialize("2"));
    value.set_variable(key, first_value);
    value.set_variable(key, second_value);
    FT_ASSERT_STR_EQ("2", value.get_variable(key)->c_str());
    return (1);
}

FT_TEST(test_game_state_remove_missing_variable_is_safe)
{
    game_state value;
    ft_string key;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, key.initialize("missing"));
    value.remove_variable(key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_state_variable_updates_and_erase_preserve_other_values)
{
    game_state state;
    ft_string first_key;
    ft_string second_key;
    ft_string third_key;
    ft_string initial_value;
    ft_string shorter_value;
    ft_string larger_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_key.initialize("first"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_key.initialize("second"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, third_key.initialize("third"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initial_value.initialize("previous value"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, shorter_value.initialize("updated"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, larger_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, larger_value.assign(4096U, 'x'));
    state.set_variable(first_key, initial_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    state.set_variable(second_key, initial_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    state.set_variable(third_key, initial_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    state.set_variable(second_key, shorter_value);
    const int32_t update_result = state.get_error();
    state.set_variable(second_key, larger_value);
    const int32_t growth_result = state.get_error();
    state.remove_variable(first_key);
    const int32_t remove_result = state.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, update_result);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, growth_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, remove_result);
    FT_ASSERT_EQ(ft_nullptr, state.get_variable(first_key));
    const ft_string *second = state.get_variable(second_key);
    const ft_string *third = state.get_variable(third_key);
    FT_ASSERT(second != ft_nullptr);
    FT_ASSERT(third != ft_nullptr);
    FT_ASSERT_STR_EQ("updated", second->c_str());
    FT_ASSERT_STR_EQ("previous value", third->c_str());
    return (1);
}

FT_TEST(test_game_state_clear_variables_is_idempotent)
{
    game_state value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.clear_variables();
    value.clear_variables();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_state_remove_character_from_empty_state_reports_error)
{
    game_state value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.remove_character(0);
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, value.get_error());
    return (1);
}

FT_TEST(test_game_state_add_null_character_reports_error)
{
    game_state value;
    ft_sharedptr<game_character> character_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR,
                 value.add_character(character_pointer));
    return (1);
}

FT_TEST(test_game_state_world_and_character_collections_start_empty)
{
    game_state value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(1, value.get_worlds().size());
    FT_ASSERT_EQ(0, value.get_characters().size());
    return (1);
}

FT_TEST(test_game_state_reset_hooks_without_hooks_is_safe)
{
    game_state value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.reset_hooks();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_state_thread_safety_cycle)
{
    game_state value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_state_reinitialize_after_destroy)
{
    game_state value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(1, value.get_worlds().size());
    return (1);
}
