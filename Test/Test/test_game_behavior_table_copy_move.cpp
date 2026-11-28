#include "../test_internal.hpp"
#include "../../Modules/Game/game_behavior_table.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Game/game_behavior_action.hpp"
#include "../../Modules/Game/game_behavior_profile.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int register_profile(game_behavior_table &table, int id, double aggression, double caution)
{
    ft_vector<game_behavior_action> actions;
    game_behavior_profile profile;
    game_behavior_action action_entry;
    int initialize_error;

    initialize_error = actions.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    if (action_entry.initialize(id * 2, 0.5, 1.5) != FT_ERR_SUCCESS)
        return (action_entry.get_error());
    actions.push_back(action_entry);
    if (actions.get_error() != FT_ERR_SUCCESS)
        return (actions.get_error());
    initialize_error = profile.initialize(id, aggression, caution, actions);
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    return (table.register_profile(profile));
}

FT_TEST(test_behavior_table_move_semantics)
{
    game_behavior_table source;
    game_behavior_profile fetched;
    game_behavior_table moved;
    game_behavior_table reassigned;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_profile(source, 20, 0.7, 0.3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.fetch_profile(20, fetched));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(20, fetched.get_profile_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.get_error());

    const auto *entry = source.get_profiles().find(20);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t move_result = moved.initialize(ft_move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, move_result);
    FT_ASSERT(moved.get_profiles().find(20) == entry);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.fetch_profile(20, fetched));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_error());
    FT_ASSERT_EQ(20, fetched.get_profile_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, reassigned.initialize(ft_move(moved)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reassigned.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reassigned.fetch_profile(20, fetched));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reassigned.get_error());
    FT_ASSERT_EQ(20, fetched.get_profile_id());
    return (1);
}

FT_TEST(test_behavior_table_copy_constructor_reset_errno)
{
    game_behavior_table source;
    game_behavior_profile fetched;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_profile(source, 30, 0.8, 0.2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());

    game_behavior_table copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.fetch_profile(30, fetched));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.get_error());
    FT_ASSERT_EQ(30, fetched.get_profile_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.get_error());
    return (1);
}

FT_TEST(test_behavior_table_copy_assignment_reset_errno)
{
    game_behavior_table source;
    game_behavior_table destination;
    game_behavior_profile fetched;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_profile(source, 44, 0.25, 0.75));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_profile(destination, 50, 0.6, 0.4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.fetch_profile(44, fetched));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(44, fetched.get_profile_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched.get_error());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, destination.fetch_profile(50, fetched));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, destination.get_error());
    return (1);
}
