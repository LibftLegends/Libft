#include "../../../Modules/Game/game_behavior_table.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>

static int32_t measure_behavior_table(ft_bool source_locked)
{
    constexpr int32_t PROFILE_COUNT = 256;
    constexpr int32_t REPETITIONS = 120;
    game_behavior_table source;
    game_behavior_table target;
    game_behavior_profile profile;
    game_behavior_action action;
    if (source.initialize() != FT_ERR_SUCCESS || target.initialize() != FT_ERR_SUCCESS
        || profile.initialize() != FT_ERR_SUCCESS)
        return (1);

    int32_t action_id = 0;
    while (action_id < 4)
    {
        if (action.initialize(20 + action_id, 0.5, 1.25) != FT_ERR_SUCCESS
            || profile.get_actions().push_back(action) != FT_ERR_SUCCESS
            || action.destroy() != FT_ERR_SUCCESS)
            return (1);
        ++action_id;
    }
    int32_t profile_id = 0;
    while (profile_id < PROFILE_COUNT)
    {
        profile.set_profile_id(profile_id);
        if (source.register_profile(profile) != FT_ERR_SUCCESS)
            return (1);
        ++profile_id;
    }
    if (source_locked && (source.enable_thread_safety() != FT_ERR_SUCCESS
        || source.get_profiles().enable_thread_safety() != FT_ERR_SUCCESS))
        return (1);

    uint64_t checksum = 0U;
    int32_t index = 0;
    const auto copy_start = std::chrono::steady_clock::now();
    while (index < REPETITIONS)
    {
        if (target.initialize(source) != FT_ERR_SUCCESS
            || target.get_profiles().size() != static_cast<ft_size_t>(PROFILE_COUNT)
            || target.fetch_profile(index, profile) != FT_ERR_SUCCESS
            || profile.get_actions().size() != 4U)
            return (1);
        checksum += profile.get_profile_id() + profile.get_actions()[index % 4].get_action_id();
        ++index;
    }
    const auto copy_end = std::chrono::steady_clock::now();

    game_behavior_table *move_source = &target;
    game_behavior_table *move_target = &source;
    std::chrono::steady_clock::duration move_duration{};
    index = 0;
    while (index < REPETITIONS)
    {
        if (source_locked && (move_source->enable_thread_safety() != FT_ERR_SUCCESS
            || move_source->get_profiles().enable_thread_safety() != FT_ERR_SUCCESS))
            return (1);
        const auto move_start = std::chrono::steady_clock::now();
        const int32_t result = move_target->move(*move_source);
        move_duration += std::chrono::steady_clock::now() - move_start;
        if (result != FT_ERR_SUCCESS
            || move_target->get_profiles().size() != static_cast<ft_size_t>(PROFILE_COUNT)
            || move_target->fetch_profile(PROFILE_COUNT - 1 - index, profile) != FT_ERR_SUCCESS
            || profile.get_actions().size() != 4U)
            return (1);
        checksum += profile.get_profile_id() + profile.get_actions()[index % 4].get_action_id();
        game_behavior_table *previous_source = move_source;
        move_source = move_target;
        move_target = previous_source;
        ++index;
    }
    std::printf("source_lock=%u copy_us=%.3f move_us=%.3f checksum=%" PRIu64 "\n",
        static_cast<uint32_t>(source_locked),
        std::chrono::duration<double, std::micro>(copy_end - copy_start).count() / REPETITIONS,
        std::chrono::duration<double, std::micro>(move_duration).count() / REPETITIONS,
        checksum);
    return (0);
}

static int32_t measure_action_lists()
{
    constexpr int32_t ACTION_COUNT = 64;
    constexpr int32_t REPETITIONS = 2000;
    game_behavior_table table;
    game_behavior_profile source;
    game_behavior_action action;
    if (table.initialize() != FT_ERR_SUCCESS || source.initialize() != FT_ERR_SUCCESS
        || action.initialize(19, 0.75, 0.25) != FT_ERR_SUCCESS)
        return (1);
    while (source.get_actions().size() < ACTION_COUNT)
    {
        if (source.get_actions().push_back(action) != FT_ERR_SUCCESS)
            return (1);
    }
    if (table.register_profile(source) != FT_ERR_SUCCESS)
        return (1);
    int32_t index = 0;
    uint64_t checksum = 0U;
    const auto copy_start = std::chrono::steady_clock::now();
    while (index < REPETITIONS)
    {
        game_behavior_profile destination;
        if (destination.initialize(source) != FT_ERR_SUCCESS
            || destination.get_actions().size() != ACTION_COUNT)
            return (1);
        checksum += destination.get_actions()[ACTION_COUNT - 1].get_action_id();
        ++index;
    }
    const auto copy_end = std::chrono::steady_clock::now();
    index = 0;
    while (index < REPETITIONS)
    {
        if (table.register_profile(source) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const auto replace_end = std::chrono::steady_clock::now();
    game_behavior_profile fetched;
    if (table.fetch_profile(0, fetched) != FT_ERR_SUCCESS
        || fetched.get_actions().size() != ACTION_COUNT)
        return (1);
    checksum += fetched.get_actions()[ACTION_COUNT - 1].get_action_id();
    std::printf("actions_copy_us=%.3f register_us=%.3f checksum=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(copy_end - copy_start).count() / REPETITIONS,
        std::chrono::duration<double, std::micro>(replace_end - copy_end).count() / REPETITIONS,
        checksum);

    checksum = 0U;
    index = 0;
    const auto fetch_start = std::chrono::steady_clock::now();
    while (index < REPETITIONS)
    {
        if (table.fetch_profile(0, fetched) != FT_ERR_SUCCESS
            || fetched.get_actions().size() != ACTION_COUNT)
            return (1);
        checksum += fetched.get_actions()[ACTION_COUNT - 1].get_action_id();
        ++index;
    }
    const auto fetch_end = std::chrono::steady_clock::now();

    game_behavior_profile *move_source = &source;
    game_behavior_profile *move_target = &fetched;
    index = 0;
    while (index < REPETITIONS)
    {
        if (move_target->move(*move_source) != FT_ERR_SUCCESS
            || move_target->get_actions().size() != ACTION_COUNT)
            return (1);
        checksum += move_target->get_actions()[ACTION_COUNT - 1].get_action_id();
        game_behavior_profile *previous_source = move_source;
        move_source = move_target;
        move_target = previous_source;
        ++index;
    }
    const auto move_end = std::chrono::steady_clock::now();
    std::printf("actions_fetch_us=%.3f actions_move_us=%.3f checksum=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(fetch_end - fetch_start).count() / REPETITIONS,
        std::chrono::duration<double, std::micro>(move_end - fetch_end).count() / REPETITIONS,
        checksum);
    return (0);
}

static int32_t measure_profile_locking(ft_bool locked)
{
    constexpr uint32_t READS = 200000U;
    constexpr uint32_t REGISTRATIONS = 2000U;
    game_behavior_table table;
    game_behavior_profile profile;
    game_behavior_action action;
    if (table.initialize() != FT_ERR_SUCCESS || profile.initialize() != FT_ERR_SUCCESS
        || action.initialize(19, 0.75, 0.25) != FT_ERR_SUCCESS)
        return (1);
    profile.set_profile_id(17);
    profile.set_aggression_weight(0.75);
    profile.set_caution_weight(0.25);
    while (profile.get_actions().size() < 64U)
    {
        if (profile.get_actions().push_back(action) != FT_ERR_SUCCESS)
            return (1);
    }
    if (table.register_profile(profile) != FT_ERR_SUCCESS)
        return (1);
    if (locked && profile.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);

    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto read_start = std::chrono::steady_clock::now();
    while (index < READS)
    {
        checksum += profile.get_profile_id();
        checksum += static_cast<uint32_t>(profile.get_aggression_weight() * 100.0);
        checksum += static_cast<uint32_t>(profile.get_caution_weight() * 100.0);
        ++index;
    }
    const auto read_end = std::chrono::steady_clock::now();
    index = 0U;
    while (index < REGISTRATIONS)
    {
        if (table.register_profile(profile) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const auto register_end = std::chrono::steady_clock::now();
    game_behavior_profile output;
    if (table.fetch_profile(17, output) != FT_ERR_SUCCESS || output.get_actions().size() != 64U)
        return (1);
    checksum += output.get_actions()[63].get_action_id();
    std::printf("profile_lock=%u reads_ns=%.3f register_us=%.3f checksum=%" PRIu64 "\n",
        static_cast<uint32_t>(locked),
        std::chrono::duration<double, std::nano>(read_end - read_start).count() / (READS * 3U),
        std::chrono::duration<double, std::micro>(register_end - read_end).count() / REGISTRATIONS,
        checksum);
    return (0);
}

static int32_t measure_action_records()
{
    constexpr uint32_t REPETITIONS = 400000U;
    game_behavior_action source;
    game_behavior_action target;
    if (source.initialize(19, 0.75, 0.25) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < REPETITIONS)
    {
        if (target.initialize(source) != FT_ERR_SUCCESS)
            return (1);
        checksum += target.get_action_id();
        checksum += static_cast<uint32_t>(target.get_weight() * 100.0);
        checksum += static_cast<uint32_t>(target.get_cooldown_seconds() * 100.0);
        ++index;
    }
    const auto end = std::chrono::steady_clock::now();
    std::printf("action_copy_read_ns=%.3f checksum=%" PRIu64 "\n",
        std::chrono::duration<double, std::nano>(end - start).count() / REPETITIONS,
        checksum);
    return (0);
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && std::strcmp(arguments[1], "--action-records") == 0)
        return (measure_action_records());
    if (measure_behavior_table(FT_FALSE) != 0 || measure_behavior_table(FT_TRUE) != 0)
        return (1);
    if (measure_action_lists() != 0 || measure_profile_locking(FT_FALSE) != 0
        || measure_profile_locking(FT_TRUE) != 0)
        return (1);
    return (measure_action_records());
}
