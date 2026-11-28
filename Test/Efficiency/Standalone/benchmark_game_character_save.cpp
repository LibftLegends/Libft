#include "../../../Modules/Game/game_character.hpp"
#include "../../../Modules/Game/game_skill.hpp"
#include "../../../Modules/JSon/json.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int32_t main()
{
    constexpr int32_t SKILLS = 128;
    constexpr int32_t ITERATIONS = 150;
    game_character character;
    game_skill skill;
    if (character.initialize() != FT_ERR_SUCCESS || skill.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t index = 0;
    while (index < SKILLS)
    {
        skill.set_id(index + 1);
        skill.set_level(index + 2);
        skill.set_cooldown(index + 3);
        skill.set_modifier4(-index);
        if (character.add_skill(skill) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    json_group *sample = serialize_character(character);
    if (sample == ft_nullptr)
        return (1);
    char *wire = json_write_to_string(sample);
    if (wire == ft_nullptr)
    {
        json_free_groups(sample);
        return (1);
    }
    uint64_t checksum = 14695981039346656037ULL;
    const char *byte_cursor = wire;
    while (*byte_cursor != '\0')
    {
        checksum ^= static_cast<uint8_t>(*byte_cursor);
        checksum *= 1099511628211ULL;
        ++byte_cursor;
    }
    cma_free(wire);

    ft_size_t alloc_before;
    ft_size_t alloc_after_save;
    ft_size_t alloc_after_load;
    ft_size_t frees;
    double save_ns = 0.0;
    double load_ns = 0.0;
    (void)cma_get_stats(&alloc_before, &frees);
    index = 0;
    while (index < ITERATIONS)
    {
        const auto start = std::chrono::steady_clock::now();
        json_group *group = serialize_character(character);
        save_ns += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        if (group == ft_nullptr)
        {
            json_free_groups(sample);
            return (1);
        }
        json_free_groups(group);
        ++index;
    }
    (void)cma_get_stats(&alloc_after_save, &frees);
    index = 0;
    while (index < ITERATIONS)
    {
        game_character restored;
        if (restored.initialize() != FT_ERR_SUCCESS)
        {
            json_free_groups(sample);
            return (1);
        }
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = deserialize_character(restored, sample);
        load_ns += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - start).count();
        const game_skill *last = restored.get_skill(SKILLS);
        if (result != FT_ERR_SUCCESS || last == ft_nullptr || last->get_modifier4() != 1 - SKILLS)
        {
            json_free_groups(sample);
            return (1);
        }
        ++index;
    }
    (void)cma_get_stats(&alloc_after_load, &frees);
    json_free_groups(sample);
    std::printf("character_save %.3f %" PRIu64 "\ncharacter_load %.3f %" PRIu64 "\nwire %" PRIu64 "\n",
        save_ns / ITERATIONS, ft_platform_cast<uint64_t>(alloc_after_save - alloc_before),
        load_ns / ITERATIONS, ft_platform_cast<uint64_t>(alloc_after_load - alloc_after_save), checksum);
    return (0);
}
