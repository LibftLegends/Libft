#include "../../../Modules/Game/game_progress_tracker.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure(int32_t mode)
{
    constexpr int32_t QUEST_COUNT = 128;
    constexpr int32_t SAMPLES = 100000;
    game_progress_tracker tracker;
    game_quest quest;
    if (tracker.initialize() != FT_ERR_SUCCESS || quest.initialize() != FT_ERR_SUCCESS)
        return (1);
    quest.set_phases(SAMPLES);
    int32_t id = 0;
    while (id < QUEST_COUNT)
    {
        quest.set_id(id);
        if (tracker.register_quest(quest) != FT_ERR_SUCCESS)
            return (1);
        ++id;
    }
    if (mode > 0 && (tracker.get_quests().enable_thread_safety() != FT_ERR_SUCCESS
        || tracker.get_quests().at(QUEST_COUNT - 1).enable_thread_safety() != FT_ERR_SUCCESS))
        return (1);
    if (mode == 2 && tracker.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    int32_t once = 0;
    const auto start = std::chrono::steady_clock::now();
    while (once < SAMPLES)
    {
        if (tracker.advance_quest_phase(QUEST_COUNT - 1) != FT_ERR_SUCCESS)
            return (1);
        ++once;
    }
    const double ns_per_advance = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count() / SAMPLES;
    const int32_t phase = tracker.get_quests().at(QUEST_COUNT - 1).get_current_phase();
    if (phase != SAMPLES || tracker.advance_quest_phase(QUEST_COUNT - 1)
            != FT_ERR_GAME_GENERAL_ERROR)
        return (1);
    std::printf("mode=%" PRId32 " ns=%.3f phase=%" PRId32 "\n", mode, ns_per_advance, phase);
    return (0);
}

int main()
{
    if (measure(0) != 0 || measure(1) != 0)
        return (1);
    return (measure(2));
}
