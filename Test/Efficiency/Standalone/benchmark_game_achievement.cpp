#include "../../../Modules/Game/game_achievement.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_achievement(ft_bool locked_goals, ft_bool locked_map)
{
    constexpr int32_t GOAL_COUNT = 128;
    constexpr uint32_t SAMPLES = 12000U;
    game_achievement achievement;
    if (achievement.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (locked_map && achievement.get_goals().enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    int32_t id = 0;
    while (id < GOAL_COUNT)
    {
        achievement.set_goal(id, id + 10);
        achievement.set_progress(id, id + 10);
        if (locked_goals
            && achievement.get_goals().at(id).enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
        ++id;
    }
    uint32_t completed = 0U;
    uint32_t sample = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (sample < SAMPLES)
    {
        completed += achievement.is_complete();
        ++sample;
    }
    const auto after_complete = std::chrono::steady_clock::now();
    sample = 0U;
    while (sample < SAMPLES)
    {
        achievement.add_progress(GOAL_COUNT - 1, 1);
        ++sample;
    }
    const auto after_update = std::chrono::steady_clock::now();
    const int32_t progress = achievement.get_progress(GOAL_COUNT - 1);
    if (completed != SAMPLES || progress != GOAL_COUNT + 9 + static_cast<int32_t>(SAMPLES))
        return (1);
    std::printf("goal_lock=%u map_lock=%u complete=%.3f update=%.3f count=%" PRIu32
        " progress=%" PRId32 "\n", static_cast<uint32_t>(locked_goals),
        static_cast<uint32_t>(locked_map),
        std::chrono::duration<double, std::micro>(after_complete - start).count() / SAMPLES,
        std::chrono::duration<double, std::nano>(after_update - after_complete).count() / SAMPLES,
        completed, progress);
    return (0);
}

int main()
{
    if (measure_achievement(FT_FALSE, FT_FALSE) != 0
        || measure_achievement(FT_TRUE, FT_FALSE) != 0)
        return (1);
    return (measure_achievement(FT_TRUE, FT_TRUE));
}
