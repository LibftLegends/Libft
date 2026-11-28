#include "../test_internal.hpp"
#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_achievement_completion_checks_locked_goal_values)
{
    game_achievement achievement;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, achievement.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, achievement.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, achievement.is_complete());
    FT_ASSERT_EQ(FT_FALSE, achievement.is_goal_complete(9));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, achievement.get_error());

    achievement.set_goal(7, 2);
    achievement.set_goal(9, 1);
    auto &goals = achievement.get_goals();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, goals.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, goals.at(7).enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, goals.at(9).enable_thread_safety());
    achievement.set_progress(7, 2);
    FT_ASSERT_EQ(FT_TRUE, achievement.is_goal_complete(7));
    FT_ASSERT_EQ(FT_FALSE, achievement.is_goal_complete(9));
    FT_ASSERT_EQ(FT_FALSE, achievement.is_complete());
    achievement.add_progress(9, 1);
    FT_ASSERT_EQ(FT_TRUE, achievement.is_complete());
    achievement.set_goal(7, 3);
    FT_ASSERT_EQ(FT_FALSE, achievement.is_complete());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, achievement.get_error());
    return (1);
}

FT_TEST(test_game_achievement_progress_keeps_existing_goals_on_growth_failure)
{
    game_achievement achievement;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, achievement.initialize());
    const ft_size_t capacity = achievement.get_goals().capacity();
    int32_t goal_id = 0;
    while (static_cast<ft_size_t>(goal_id) < capacity)
    {
        achievement.set_goal(goal_id, 10);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, achievement.get_error());
        ++goal_id;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    achievement.add_progress(0, 3);
    const int32_t updated = achievement.get_error();
    achievement.add_progress(goal_id, 4);
    const int32_t growth_error = achievement.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, updated);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, growth_error);
    FT_ASSERT_EQ(capacity, achievement.get_goals().size());
    FT_ASSERT_EQ(3, achievement.get_progress(0));
    achievement.add_progress(goal_id, 4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, achievement.get_error());
    FT_ASSERT_EQ(4, achievement.get_progress(goal_id));
    FT_ASSERT_EQ(0, achievement.get_goal(goal_id));
    return (1);
}
