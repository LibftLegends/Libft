#include "../test_internal.hpp"
#include "../../Modules/Game/game_progress_tracker.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_progress_tracker_replacement_keeps_previous_quests_on_failure)
{
    game_progress_tracker source;
    game_progress_tracker target;
    game_quest quest;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.initialize());
    quest.set_phases(2);
    quest.set_id(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.register_quest(quest));
    quest.set_id(19);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_quest(quest));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    target.set_quests(source.get_quests());
    const int32_t result = target.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.advance_quest_phase(7));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, target.advance_quest_phase(19));
    target.set_quests(source.get_quests());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_error());
    target.set_quests(target.get_quests());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.get_error());
    FT_ASSERT_EQ(1U, target.get_quests().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.advance_quest_phase(19));
    return (1);
}

FT_TEST(test_game_progress_tracker_copy_replaces_an_initialized_tracker)
{
    game_progress_tracker source;
    game_progress_tracker target;
    game_achievement achievement;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, achievement.initialize());
    achievement.set_id(5);
    achievement.set_goal(3, 9);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_achievement(achievement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize(source));
    FT_ASSERT_EQ(1U, target.get_achievements().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.update_goal_progress(5, 3, 9));
    FT_ASSERT_EQ(FT_TRUE, target.is_achievement_complete(5));
    FT_ASSERT_EQ(FT_FALSE, source.is_achievement_complete(5));
    return (1);
}

FT_TEST(test_game_progress_tracker_move_transfers_both_maps_without_allocating)
{
    game_progress_tracker source;
    game_progress_tracker target;
    game_achievement achievement;
    game_quest quest;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_achievements().enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_quests().enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, achievement.initialize());
    achievement.set_id(11);
    achievement.set_goal(4, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_achievement(achievement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.initialize());
    quest.set_id(8);
    quest.set_phases(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_quest(quest));
    const auto *source_achievement = source.get_achievements().find(11);
    const auto *source_quest = source.get_quests().find(8);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t moved = target.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved);
    FT_ASSERT_EQ(FT_TRUE, target.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT(target.get_achievements().find(11) == source_achievement);
    FT_ASSERT(target.get_quests().find(8) == source_quest);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.update_goal_progress(11, 4, 1));
    FT_ASSERT_EQ(FT_TRUE, target.is_achievement_complete(11));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.advance_quest_phase(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_quests().size());
    return (1);
}
