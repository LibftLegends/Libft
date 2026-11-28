#include "../../../Modules/Game/game_character.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static constexpr uint32_t ITERATIONS = 400000U;

static int32_t measure_lookup(game_character &character, ft_bool thread_safe)
{
    if (thread_safe == FT_TRUE && character.get_skills().enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    const game_character &reader = character;
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const game_skill *skill = reader.get_skill(7);
        if (skill == ft_nullptr)
            return (1);
        checksum += static_cast<uint64_t>(skill->get_level());
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (thread_safe == FT_TRUE && character.get_skills().disable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    std::printf("lookup_%u %.3f %" PRIu64 "\n", static_cast<uint32_t>(thread_safe),
        elapsed / ITERATIONS, checksum);
    return (0);
}

static int32_t measure_replace(game_character &character, game_skill &skill, ft_bool thread_safe)
{
    if (thread_safe == FT_TRUE && skill.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (character.add_skill(skill) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    const game_skill *stored = character.get_skill(7);
    if (stored == ft_nullptr || stored->get_level() != 19 || character.get_skills().size() != 1U)
        return (1);
    std::printf("replace_%u %.3f %" PRId32 "\n", static_cast<uint32_t>(thread_safe),
        elapsed / ITERATIONS, stored->get_level());
    return (0);
}

static int32_t measure_remove(int32_t skill_count, ft_bool tail)
{
    game_character character;
    game_skill skill;
    if (character.initialize() != FT_ERR_SUCCESS || skill.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t id = 1;
    while (id <= skill_count)
    {
        skill.set_id(id);
        if (character.add_skill(skill) != FT_ERR_SUCCESS)
            return (1);
        ++id;
    }
    constexpr uint32_t REMOVAL_ITERATIONS = 30000U;
    id = 1;
    if (tail == FT_TRUE)
        id = skill_count;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < REMOVAL_ITERATIONS)
    {
        character.remove_skill(id);
        if (character.get_error() != FT_ERR_SUCCESS)
            return (1);
        skill.set_id(id);
        if (character.add_skill(skill) != FT_ERR_SUCCESS)
            return (1);
        if (tail == FT_FALSE)
        {
            ++id;
            if (id > skill_count)
                id = 1;
        }
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    const auto &skills = character.get_skills();
    if (skills.size() != static_cast<ft_size_t>(skill_count))
        return (1);
    uint64_t checksum = 0U;
    const int32_t result = skills.visit_values([&checksum](const game_skill &entry) noexcept
    {
        checksum = checksum * 31U + static_cast<uint64_t>(entry.get_id());
        return (FT_ERR_SUCCESS);
    });
    if (result != FT_ERR_SUCCESS)
        return (1);
    std::printf("remove_%" PRId32 "_%u %.3f %" PRIu64 "\n", skill_count,
        static_cast<uint32_t>(tail), elapsed / REMOVAL_ITERATIONS, checksum);
    return (0);
}

int32_t main(int32_t argc, char **argv)
{
    if (argc == 2 && ft_strcmp(argv[1], "--remove") == 0)
    {
        if (measure_remove(8, FT_TRUE) != 0 || measure_remove(8, FT_FALSE) != 0)
            return (1);
        return (measure_remove(64, FT_FALSE));
    }
    game_character character;
    game_skill skill;
    if (character.initialize() != FT_ERR_SUCCESS || skill.initialize() != FT_ERR_SUCCESS)
        return (1);
    skill.set_id(7);
    skill.set_level(19);
    if (character.add_skill(skill) != FT_ERR_SUCCESS
        || measure_lookup(character, FT_FALSE) != 0
        || measure_lookup(character, FT_TRUE) != 0
        || measure_replace(character, skill, FT_FALSE) != 0)
        return (1);
    return (measure_replace(character, skill, FT_TRUE));
}
