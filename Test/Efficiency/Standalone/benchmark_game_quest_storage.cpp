#include "../../../Modules/Game/game_quest.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_text_replacement()
{
    constexpr uint32_t SAMPLES = 30000U;
    game_quest quest;
    ft_string first;
    ft_string second;
    if (quest.initialize() != FT_ERR_SUCCESS
        || first.initialize("Deliver supplies and return to the east gate.") != FT_ERR_SUCCESS
        || second.initialize("Report to camp when the supplies have arrived.") != FT_ERR_SUCCESS)
        return (1);
    quest.set_description(second);
    if (quest.get_error() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t allocations_before = 0U;
    ft_size_t allocations_after = 0U;
    ft_size_t frees = 0U;
    if (cma_get_stats(&allocations_before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint32_t sample = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (sample < SAMPLES)
    {
        if (sample % 2U == 0U)
            quest.set_description(first);
        else
            quest.set_description(second);
        if (quest.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++sample;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&allocations_after, &frees) != FT_ERR_SUCCESS
        || ft_strcmp(quest.get_description().c_str(), second.c_str()) != 0)
        return (1);
    std::printf("text_ns=%.3f allocations=%" PRIu64 " size=%" PRIu64 "\n", elapsed / SAMPLES,
        static_cast<uint64_t>(allocations_after - allocations_before),
        static_cast<uint64_t>(quest.get_description().size()));
    return (0);
}

static int32_t measure_reward_replacement(ft_size_t source_capacity)
{
    constexpr ft_size_t REWARDS = 32U;
    constexpr uint32_t SAMPLES = 1200U;
    game_quest quest;
    ft_vector<ft_sharedptr<game_item> > incoming;
    ft_sharedptr<game_item> item(new game_item());
    if (quest.initialize() != FT_ERR_SUCCESS || incoming.initialize() != FT_ERR_SUCCESS
        || !item || item->initialize() != FT_ERR_SUCCESS)
        return (1);
    incoming.reserve(source_capacity);
    if (incoming.get_error() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t count = 0U;
    while (count < REWARDS)
    {
        if (incoming.push_back(item) != FT_ERR_SUCCESS)
            return (1);
        ++count;
    }
    uint32_t sample = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (sample < SAMPLES)
    {
        quest.set_reward_items(incoming);
        if (quest.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++sample;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    const auto &rewards = quest.get_reward_items();
    if (rewards.size() != REWARDS || rewards[REWARDS - 1U].get() != item.get())
        return (1);
    std::printf("source=%" PRIu64 " reward_us=%.3f destination=%" PRIu64 " items=%" PRIu64 "\n",
        static_cast<uint64_t>(source_capacity), elapsed / SAMPLES,
        static_cast<uint64_t>(rewards.capacity()), static_cast<uint64_t>(rewards.size()));
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--replace-text") == 0)
        return (measure_text_replacement());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--replace-rewards") == 0)
    {
        if (measure_reward_replacement(32U) != 0)
            return (1);
        return (measure_reward_replacement(32768U));
    }
    constexpr ft_size_t REWARDS = 128U;
    constexpr uint32_t SAMPLES = 3000U;
    game_quest source;
    ft_string text;
    if (source.initialize() != FT_ERR_SUCCESS
        || text.initialize("Deliver the supplies to the gate and return to camp.") != FT_ERR_SUCCESS)
        return (1);
    source.set_description(text);
    source.set_objective(text);
    ft_sharedptr<game_item> reward(new game_item());
    if (!reward || reward->initialize() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t index = 0U;
    while (index < REWARDS)
    {
        if (source.get_reward_items().push_back(reward) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    uint32_t sample = 0U;
    double move_us = 0.0;
    double copy_us = 0.0;
    uint64_t checksum = 0U;
    while (sample < SAMPLES)
    {
        game_quest donor;
        game_quest receiver;
        const auto copy_start = std::chrono::steady_clock::now();
        if (donor.initialize(source) != FT_ERR_SUCCESS)
            return (1);
        const auto move_start = std::chrono::steady_clock::now();
        if (receiver.move(donor) != FT_ERR_SUCCESS)
            return (1);
        const auto move_end = std::chrono::steady_clock::now();
        copy_us += std::chrono::duration<double, std::micro>(move_start - copy_start).count();
        move_us += std::chrono::duration<double, std::micro>(move_end - move_start).count();
        if (receiver.get_reward_items().size() != REWARDS)
            return (1);
        checksum += receiver.get_description().size() + receiver.get_objective().size();
        ++sample;
    }
    std::printf("copy=%.3f move=%.3f checksum=%" PRIu64 "\n",
        copy_us / SAMPLES, move_us / SAMPLES, checksum);
    return (0);
}
