#include "../test_internal.hpp"
#include "../../Modules/Game/game_behavior_table.hpp"
#include "../../Modules/Game/game_behavior_profile.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Game/game_behavior_action.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int build_profile(game_behavior_profile &profile, int profile_id,
        double aggression, double caution, int action_id)
{
    ft_vector<game_behavior_action> actions;
    game_behavior_action action_entry;
    int32_t error_code;

    error_code = actions.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (actions.get_error());

    if (action_entry.initialize(action_id, 1.25, 2.5) != FT_ERR_SUCCESS)
        return (action_entry.get_error());
    error_code = actions.push_back(action_entry);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (profile.initialize(profile_id, aggression, caution, actions));
}

FT_TEST(test_behavior_table_register_and_fetch_profile)
{
    game_behavior_table table;
    game_behavior_profile fetched;
    game_behavior_profile profile;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(profile, 1, 0.4, 0.6, 11));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_profile(profile));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_profile(1, fetched));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(1, fetched.get_profile_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.get_error());
    return (1);
}

FT_TEST(test_behavior_table_fetch_missing_profile_returns_not_found)
{
    game_behavior_table table;
    game_behavior_profile fetched;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, table.fetch_profile(99, fetched));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, table.get_error());
    return (1);
}

FT_TEST(test_behavior_table_register_updates_entries)
{
    game_behavior_table table;
    game_behavior_profile fetched;
    game_behavior_profile profile_one;
    game_behavior_profile profile_two;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(profile_one, 2, 0.1, 0.1, 5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile_one.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_profile(profile_one));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(profile_two, 2, 0.9, 0.2, 6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile_two.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_profile(profile_two));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_profile(2, fetched));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_DOUBLE_EQ(0.9, fetched.get_aggression_weight());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.get_error());
    return (1);
}

FT_TEST(test_behavior_table_thread_safety_toggle)
{
    game_behavior_table table;
    game_behavior_profile profile;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT(table.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(profile, 7, 0.25, 0.25, 9));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_profile(profile));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.disable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT(!table.is_thread_safe());
    return (1);
}

FT_TEST(test_behavior_table_set_profiles_preserves_catalog_on_failure_and_self_copy)
{
    game_behavior_table source;
    game_behavior_table target;
    game_behavior_profile profile;
    game_behavior_profile fetched;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(profile, 7, 0.25, 0.75, 9));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.register_profile(profile));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(profile, 42, 0.75, 0.25, 19));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_profile(profile));

    const auto &replacement = source.get_profiles();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    target.set_profiles(replacement);
    const int32_t error_code = target.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.fetch_profile(7, fetched));
    FT_ASSERT_EQ(9, fetched.get_actions()[0].get_action_id());

    target.set_profiles(target.get_profiles());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.fetch_profile(7, fetched));
    target.set_profiles(replacement);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_error());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, target.fetch_profile(7, fetched));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.fetch_profile(42, fetched));
    FT_ASSERT_EQ(19, fetched.get_actions()[0].get_action_id());
    return (1);
}

FT_TEST(test_behavior_table_fetch_propagates_profile_map_lock_failure)
{
    game_behavior_table table;
    game_behavior_profile profile;
    game_behavior_profile fetched;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(profile, 7, 0.25, 0.75, 9));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(fetched, 42, 0.75, 0.25, 19));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_profile(profile));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_profiles().enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = table.fetch_profile(7, fetched);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(42, fetched.get_profile_id());
    FT_ASSERT_EQ(19, fetched.get_actions()[0].get_action_id());
    return (1);
}

FT_TEST(test_behavior_table_register_preserves_profile_when_action_copy_fails)
{
    game_behavior_table table;
    game_behavior_profile original;
    game_behavior_profile replacement;
    game_behavior_profile fetched;
    game_behavior_action action;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_profiles().enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(original, 7, 0.25, 0.75, 9));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_profile(original));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(replacement, 7, 0.75, 0.25, 19));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, action.initialize(20, 1.0, 0.5));
    auto &actions = replacement.get_actions();
    while (actions.size() < 64U)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, actions.push_back(action));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t error_code = table.register_profile(replacement);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_profile(7, fetched));
    FT_ASSERT_DOUBLE_EQ(0.25, fetched.get_aggression_weight());
    FT_ASSERT_EQ(1U, fetched.get_actions().size());
    FT_ASSERT_EQ(9, fetched.get_actions()[0].get_action_id());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_profile(replacement));
    const auto *entry = table.get_profiles().find(7);
    FT_ASSERT(entry != table.get_profiles().end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_profile(entry->value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_profile(7, fetched));
    FT_ASSERT_EQ(64U, fetched.get_actions().size());
    FT_ASSERT_EQ(19, fetched.get_actions()[0].get_action_id());
    FT_ASSERT_EQ(20, fetched.get_actions()[63].get_action_id());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.enable_thread_safety());
    const auto *storage = fetched.get_actions().begin();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t fetch_result = table.fetch_profile(7, fetched);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetch_result);
    FT_ASSERT(fetched.get_actions().begin() == storage);
    FT_ASSERT_EQ(FT_TRUE, fetched.is_thread_safe());
    FT_ASSERT_EQ(20, fetched.get_actions()[63].get_action_id());
    return (1);
}

FT_TEST(test_behavior_table_register_propagates_source_profile_lock_failure)
{
    game_behavior_table table;
    game_behavior_profile profile;
    game_behavior_profile fetched;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, build_profile(profile, 21, 0.75, 0.25, 11));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = table.register_profile(profile);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);

    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(0U, table.get_profiles().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_profile(profile));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_profile(21, fetched));
    FT_ASSERT_EQ(21, fetched.get_profile_id());
    FT_ASSERT_EQ(11, fetched.get_actions()[0].get_action_id());
    profile.set_aggression_weight(0.5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_profile(profile));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_profile(21, fetched));
    FT_ASSERT_DOUBLE_EQ(0.5, fetched.get_aggression_weight());
    return (1);
}
