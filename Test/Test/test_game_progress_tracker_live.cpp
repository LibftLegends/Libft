#include "../test_internal.hpp"
#include "../../Modules/Game/game_progress_tracker.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <thread>

FT_TEST(test_game_progress_tracker_live_operations_report_nested_errors)
{
    game_progress_tracker tracker;
    game_achievement achievement;
    game_quest quest;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, achievement.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.initialize());
    achievement.set_id(1);
    quest.set_id(2);
    quest.set_phases(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.register_achievement(achievement));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.register_quest(quest));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, tracker.add_goal_progress(1, -1, 2));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, tracker.get_error());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, tracker.set_quest_phase(2, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.advance_quest_phase(2));
    FT_ASSERT_EQ(1, tracker.get_quests().at(2).get_current_phase());
    FT_ASSERT_EQ(FT_FALSE, tracker.is_quest_complete(3));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, tracker.get_error());
    return (1);
}

FT_TEST(test_game_progress_tracker_live_quest_advances_only_once_at_completion)
{
    game_progress_tracker tracker;
    game_quest quest;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.initialize());
    quest.set_id(2);
    quest.set_phases(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.register_quest(quest));
    auto &quests = tracker.get_quests();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quests.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quests.at(2).enable_thread_safety());
    std::atomic<ft_bool> start{FT_FALSE};
    int32_t results[2] = {};
    const auto advance = [&](ft_size_t index)
    {
        while (!start.load())
            std::this_thread::yield();
        results[index] = tracker.advance_quest_phase(2);
        return ;
    };
    std::thread first(advance, 0U);
    std::thread second(advance, 1U);
    start.store(FT_TRUE);
    first.join();
    second.join();
    FT_ASSERT((results[0] == FT_ERR_SUCCESS && results[1] == FT_ERR_GAME_GENERAL_ERROR)
        || (results[1] == FT_ERR_SUCCESS && results[0] == FT_ERR_GAME_GENERAL_ERROR));
    FT_ASSERT_EQ(1, quests.at(2).get_current_phase());
    FT_ASSERT_EQ(FT_TRUE, tracker.is_quest_complete(2));
    return (1);
}
