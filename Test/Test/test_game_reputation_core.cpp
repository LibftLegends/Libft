#include "../test_internal.hpp"
#include "../../Modules/Game/game_reputation.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Template/map.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_game_reputation_milestone_updates)
{
    game_reputation reputation;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.set_milestone(1, 10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(10, reputation.get_milestone(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.set_milestone(1, 25);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(25, reputation.get_milestone(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(0, reputation.get_milestone(99));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, reputation.get_error());
    return (1);
}

FT_TEST(test_game_reputation_rep_tracking)
{
    game_reputation reputation;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.set_rep(2, 5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(5, reputation.get_rep(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.set_rep(2, 12);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(12, reputation.get_rep(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(0, reputation.get_rep(-1));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, reputation.get_error());
    return (1);
}

FT_TEST(test_game_reputation_point_updates_report_growth_failure)
{
    game_reputation reputation;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_reps().enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_milestones().enable_thread_safety());
    const int32_t rep_capacity = static_cast<int32_t>(reputation.get_reps().capacity());
    const int32_t milestone_capacity = static_cast<int32_t>(reputation.get_milestones().capacity());
    int32_t id = 0;
    while (id < rep_capacity)
        reputation.set_rep(id++, 11);
    id = 0;
    while (id < milestone_capacity)
        reputation.set_milestone(id++, 21);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    reputation.set_rep(rep_capacity, 12);
    const int32_t rep_error = reputation.get_error();
    reputation.set_milestone(milestone_capacity, 22);
    const int32_t milestone_error = reputation.get_error();
    reputation.set_rep(0, 13);
    const int32_t update_error = reputation.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));

    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, rep_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, milestone_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, update_error);
    FT_ASSERT_EQ(13, reputation.get_rep(0));
    FT_ASSERT_EQ(21, reputation.get_milestone(0));
    FT_ASSERT_EQ(0, reputation.get_rep(rep_capacity));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, reputation.get_error());
    FT_ASSERT_EQ(0, reputation.get_milestone(milestone_capacity));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, reputation.get_error());
    return (1);
}

FT_TEST(test_game_reputation_totals_reflect_changes)
{
    game_reputation reputation;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.set_total_rep(10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.set_current_rep(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.add_current_rep(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(10, reputation.get_total_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(7, reputation.get_current_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.add_current_rep(-5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(10, reputation.get_total_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(2, reputation.get_current_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.sub_total_rep(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(9, reputation.get_total_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.sub_current_rep(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(1, reputation.get_current_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    return (1);
}

FT_TEST(test_game_reputation_counters_preserve_values_on_overflow)
{
    game_reputation reputation;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.enable_thread_safety());
    reputation.set_total_rep(INT32_MAX);
    reputation.add_total_rep(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, reputation.get_error());
    FT_ASSERT_EQ(INT32_MAX, reputation.get_total_rep());
    reputation.sub_total_rep(INT32_MAX);
    reputation.add_total_rep(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.sub_total_rep(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, reputation.get_error());
    FT_ASSERT_EQ(INT32_MIN, reputation.get_total_rep());
    reputation.sub_total_rep(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(0, reputation.get_total_rep());

    reputation.sub_current_rep(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, reputation.get_error());
    FT_ASSERT_EQ(0, reputation.get_current_rep());
    reputation.sub_current_rep(1);
    reputation.sub_current_rep(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(INT32_MAX, reputation.get_current_rep());
    reputation.add_current_rep(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, reputation.get_error());
    FT_ASSERT_EQ(INT32_MAX, reputation.get_current_rep());
    return (1);
}

FT_TEST(test_game_reputation_counter_lock_failure_preserves_value)
{
    game_reputation reputation;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.enable_thread_safety());
    reputation.set_current_rep(12);

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    reputation.add_current_rep(3);
    const int32_t update_error = reputation.get_error();
    const int32_t failed_read = reputation.get_current_rep();
    const int32_t read_error = reputation.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);

    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, update_error);
    FT_ASSERT_EQ(0, failed_read);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, read_error);
    FT_ASSERT_EQ(12, reputation.get_current_rep());
    return (1);
}

FT_TEST(test_game_reputation_map_replacements)
{
    game_reputation reputation;
    ft_map<int, int> first_set;
    ft_map<int, int> second_set;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_set.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_set.initialize());
    first_set.insert(1, 4);
    second_set.insert(5, 8);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    reputation.set_milestones(first_set);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(4, reputation.get_milestone(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(0, reputation.get_milestone(5));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, reputation.get_error());

    reputation.set_milestones(second_set);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(0, reputation.get_milestone(1));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, reputation.get_error());
    FT_ASSERT_EQ(8, reputation.get_milestone(5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());

    reputation.set_reps(first_set);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(4, reputation.get_rep(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());

    reputation.set_reps(second_set);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    FT_ASSERT_EQ(0, reputation.get_rep(1));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, reputation.get_error());
    FT_ASSERT_EQ(8, reputation.get_rep(5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.get_error());
    return (1);
}
