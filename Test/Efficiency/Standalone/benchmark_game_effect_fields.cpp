#include "../../../Modules/Game/game_buff.hpp"
#include "../../../Modules/Game/game_debuff.hpp"
#include "../../../Modules/Game/game_skill.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>

template <typename Effect>
static int32_t measure_effect(const char *name, ft_bool locked)
{
    constexpr uint32_t REPETITIONS = 160000U;
    Effect effect;
    if (effect.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (locked && effect.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    effect.set_duration(25);
    effect.set_modifier1(50);
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < REPETITIONS)
    {
        effect.add_duration(2);
        effect.sub_duration(2);
        effect.add_modifier1(3);
        effect.sub_modifier1(3);
        checksum += effect.get_duration() + effect.get_modifier1();
        ++index;
    }
    const auto end = std::chrono::steady_clock::now();
    if (effect.get_error() != FT_ERR_SUCCESS || checksum != REPETITIONS * 75U)
        return (1);
    std::printf("%s lock=%u field_cycle_ns=%.3f checksum=%" PRIu64 "\n",
        name, static_cast<uint32_t>(locked),
        std::chrono::duration<double, std::nano>(end - start).count() / REPETITIONS,
        checksum);
    return (0);
}

template <typename Effect, auto set_period = &Effect::set_duration,
    auto get_period = &Effect::get_duration>
static int32_t measure_lifecycle(const char *name)
{
    constexpr uint32_t BATCH_SIZE = 128U;
    constexpr uint32_t BATCHES = 400U;
    constexpr uint32_t MOVES = 200000U;
    Effect source;
    Effect copies[BATCH_SIZE];
    if (source.initialize() != FT_ERR_SUCCESS)
        return (1);
    (source.*set_period)(25);
    source.set_modifier1(-4);
    source.set_modifier4(50);

    std::chrono::steady_clock::duration copy_duration{};
    uint64_t checksum = 0U;
    uint32_t batch = 0U;
    while (batch < BATCHES)
    {
        uint32_t index = 0U;
        const auto start = std::chrono::steady_clock::now();
        while (index < BATCH_SIZE)
        {
            if (copies[index].initialize(source) != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        copy_duration += std::chrono::steady_clock::now() - start;
        index = 0U;
        while (index < BATCH_SIZE)
        {
            checksum += (copies[index].*get_period)() + copies[index].get_modifier4();
            if (copies[index].destroy() != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        ++batch;
    }

    uint32_t count = 0U;
    Effect destination;
    Effect *from = &source;
    Effect *to = &destination;
    const auto move_start = std::chrono::steady_clock::now();
    while (count < MOVES)
    {
        if (to->move(*from) != FT_ERR_SUCCESS)
            return (1);
        Effect *previous_source = from;
        from = to;
        to = previous_source;
        ++count;
    }
    const auto move_end = std::chrono::steady_clock::now();
    if (from->get_modifier1() != -4 || (from->*get_period)() != 25
        || from->get_modifier4() != 50 || checksum != BATCH_SIZE * BATCHES * 75U)
        return (1);
    std::printf("%s copy_ns=%.3f move_ns=%.3f checksum=%" PRIu64 "\n", name,
        std::chrono::duration<double, std::nano>(copy_duration).count() / (BATCH_SIZE * BATCHES),
        std::chrono::duration<double, std::nano>(move_end - move_start).count() / MOVES,
        checksum);
    return (0);
}

static int32_t measure_skill_fields(ft_bool locked)
{
    constexpr uint32_t REPETITIONS = 160000U;
    game_skill skill;
    if (skill.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (locked && skill.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    skill.set_cooldown(25);
    skill.set_modifier3(50);
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < REPETITIONS)
    {
        skill.add_cooldown(2);
        skill.sub_cooldown(2);
        skill.add_modifier3(3);
        skill.sub_modifier3(3);
        checksum += skill.get_cooldown() + skill.get_modifier3();
        ++index;
    }
    const auto end = std::chrono::steady_clock::now();
    if (skill.get_error() != FT_ERR_SUCCESS || checksum != REPETITIONS * 75U)
        return (1);
    std::printf("skill lock=%u field_cycle_ns=%.3f checksum=%" PRIu64 "\n",
        static_cast<uint32_t>(locked),
        std::chrono::duration<double, std::nano>(end - start).count() / REPETITIONS,
        checksum);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && std::strcmp(arguments[1], "--skill") == 0)
    {
        if (measure_skill_fields(FT_FALSE) != 0 || measure_skill_fields(FT_TRUE) != 0)
            return (1);
        return (measure_lifecycle<game_skill, &game_skill::set_cooldown,
            &game_skill::get_cooldown>("skill"));
    }
    if (argument_count == 2 && std::strcmp(arguments[1], "--lifecycle") == 0)
    {
        if (measure_lifecycle<game_buff>("buff") != 0)
            return (1);
        return (measure_lifecycle<game_debuff>("debuff"));
    }
    if (measure_effect<game_buff>("buff", FT_FALSE) != 0
        || measure_effect<game_debuff>("debuff", FT_FALSE) != 0
        || measure_effect<game_buff>("buff", FT_TRUE) != 0)
        return (1);
    return (measure_effect<game_debuff>("debuff", FT_TRUE));
}
