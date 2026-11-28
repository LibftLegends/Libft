#include "../test_internal.hpp"
#include "../../Modules/Game/game_reputation.hpp"
#include "../../Modules/Template/map.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_game_reputation_initialize_copy_retains_state)
{
    ft_map<int, int> milestones;
    ft_map<int, int> reps;
    game_reputation source;
    game_reputation clone;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, milestones.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reps.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    milestones.insert(7, 21);
    reps.insert(9, 18);
    source.set_milestones(milestones);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    source.set_reps(reps);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    source.set_total_rep(12);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    source.set_current_rep(6);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.get_error());
    FT_ASSERT_EQ(21, clone.get_milestone(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.get_error());
    FT_ASSERT_EQ(18, clone.get_rep(9));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.get_error());
    FT_ASSERT_EQ(12, clone.get_total_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.get_error());
    FT_ASSERT_EQ(6, clone.get_current_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, clone.get_error());
    return (1);
}

FT_TEST(test_game_reputation_initialize_move_clears_source)
{
    ft_map<int, int> milestones;
    ft_map<int, int> reps;
    game_reputation source;
    game_reputation destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, milestones.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reps.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    milestones.insert(11, 33);
    reps.insert(22, 44);
    source.set_milestones(milestones);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    source.set_reps(reps);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    source.set_total_rep(19);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    source.set_current_rep(17);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(ft_move(source)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(33, destination.get_milestone(11));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(44, destination.get_rep(22));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(19, destination.get_total_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(17, destination.get_current_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    return (1);
}

FT_TEST(test_game_reputation_move_transfers_both_maps_without_allocation)
{
    game_reputation source;
    game_reputation destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_milestones().enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_reps().enable_thread_safety());
    source.set_milestone(21, 42);
    source.set_rep(37, 84);
    source.set_total_rep(55);
    source.set_current_rep(17);
    destination.set_rep(1, 100);
    const auto *milestone = source.get_milestones().find(21);
    const auto *rep = source.get_reps().find(37);
    FT_ASSERT_EQ(0, source.get_rep(99));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, source.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, destination.get_error());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, destination.get_milestones().is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, destination.get_reps().is_thread_safe());
    FT_ASSERT(milestone == destination.get_milestones().find(21));
    FT_ASSERT(rep == destination.get_reps().find(37));
    FT_ASSERT_EQ(0, destination.get_rep(1));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, destination.get_error());
    FT_ASSERT_EQ(55, destination.get_total_rep());
    FT_ASSERT_EQ(17, destination.get_current_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_reps().size());
    return (1);
}

FT_TEST(test_game_reputation_move_preserves_source_on_map_lock_failure)
{
    game_reputation source;
    game_reputation destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    source.set_milestone(21, 42);
    source.set_rep(37, 84);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_reps().enable_thread_safety());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);

    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, destination.get_error());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(42, source.get_milestone(21));
    FT_ASSERT_EQ(84, source.get_rep(37));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(42, destination.get_milestone(21));
    FT_ASSERT_EQ(84, destination.get_rep(37));
    return (1);
}

FT_TEST(test_game_reputation_copy_reports_partial_allocation_failure)
{
    game_reputation source;
    game_reputation destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    const ft_size_t capacity = destination.get_reps().capacity();
    const int32_t last_id = static_cast<int32_t>(capacity);
    int32_t id = 0;
    while (id <= last_id)
    {
        source.set_rep(id, id + 1);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
        ++id;
    }
    source.set_milestone(1, 12);
    destination.set_rep(99, 5);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(capacity * sizeof(Pair<int32_t, int32_t>)));
    destination.set_reps(source.get_reps());
    const int32_t bulk_result = destination.get_error();
    const int32_t old_value = destination.get_rep(99);
    const int32_t copy_result = destination.initialize(source);
    const int32_t destroyed_error = destination.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, bulk_result);
    FT_ASSERT_EQ(5, old_value);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, copy_result);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, destroyed_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(12, source.get_milestone(1));
    FT_ASSERT_EQ(last_id + 1, source.get_rep(last_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(last_id + 1, destination.get_rep(last_id));
    return (1);
}

FT_TEST(test_game_reputation_accessors_return_values)
{
    ft_map<int, int> milestones;
    ft_map<int, int> reps;
    game_reputation tracker;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, milestones.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reps.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.get_error());
    milestones.insert(5, 20);
    reps.insert(7, 14);
    tracker.set_milestones(milestones);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.get_error());
    tracker.set_reps(reps);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.get_error());
    tracker.set_total_rep(8);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.get_error());
    tracker.set_current_rep(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.get_error());
    FT_ASSERT_EQ(20, tracker.get_milestone(5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.get_error());
    FT_ASSERT_EQ(14, tracker.get_rep(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.get_error());
    FT_ASSERT_EQ(8, tracker.get_total_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.get_error());
    FT_ASSERT_EQ(2, tracker.get_current_rep());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.get_error());
    FT_ASSERT_EQ(0, tracker.get_milestone(999));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, tracker.get_error());
    FT_ASSERT_EQ(0, tracker.get_rep(999));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, tracker.get_error());
    return (1);
}
