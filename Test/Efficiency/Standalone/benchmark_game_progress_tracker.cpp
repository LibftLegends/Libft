#include "../../../Modules/Game/game_progress_tracker.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr int32_t QUEST_COUNT = 256;
    constexpr uint32_t SAMPLES = 64U;
    game_progress_tracker source;
    game_quest quest;
    if (source.initialize() != FT_ERR_SUCCESS || quest.initialize() != FT_ERR_SUCCESS)
        return (1);
    quest.set_phases(3);
    quest.set_current_phase(1);
    int32_t quest_id = 1;
    while (quest_id <= QUEST_COUNT)
    {
        quest.set_id(quest_id);
        quest.set_reward_experience(quest_id * 10);
        if (source.register_quest(quest) != FT_ERR_SUCCESS)
            return (1);
        ++quest_id;
    }
    game_progress_tracker target;
    if (target.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t sample = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (sample < SAMPLES)
    {
        target.set_quests(source.get_quests());
        if (target.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++sample;
    }
    const auto after_replace = std::chrono::steady_clock::now();
    sample = 0U;
    while (sample < SAMPLES)
    {
        game_progress_tracker copy;
        if (copy.initialize(source) != FT_ERR_SUCCESS)
            return (1);
        ++sample;
    }
    const auto end = std::chrono::steady_clock::now();
    double move_us = 0.0;
    sample = 0U;
    while (sample < SAMPLES)
    {
        game_progress_tracker donor;
        game_progress_tracker receiver;
        if (donor.initialize(source) != FT_ERR_SUCCESS)
            return (1);
        const auto move_start = std::chrono::steady_clock::now();
        if (receiver.move(donor) != FT_ERR_SUCCESS)
            return (1);
        move_us += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - move_start).count();
        if (receiver.get_quests().size() != static_cast<ft_size_t>(QUEST_COUNT))
            return (1);
        ++sample;
    }
    const auto &quests = target.get_quests();
    if (quests.size() != static_cast<ft_size_t>(QUEST_COUNT))
        return (1);
    uint64_t checksum = 0U;
    const auto *last = quests.end();
    const auto *entry = last - QUEST_COUNT;
    while (entry != last)
    {
        checksum = checksum * 31U + entry->value.get_reward_experience();
        ++entry;
    }
    std::printf("replace=%.3f copy=%.3f move=%.3f checksum=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(after_replace - start).count() / SAMPLES,
        std::chrono::duration<double, std::micro>(end - after_replace).count() / SAMPLES,
        move_us / SAMPLES, checksum);
    return (0);
}
