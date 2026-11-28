#include "../../../Modules/Game/game_character.hpp"
#include "../../../Modules/Game/game_character_snapshot.hpp"
#include "../../../Modules/CPP_class/class_string.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_snapshot(ft_bool equipped, ft_bool thread_safe, int32_t skill_count)
{
    game_character character;
    ft_string output;
    if (character.initialize() != FT_ERR_SUCCESS || output.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (equipped == FT_TRUE)
    {
        ft_sharedptr<game_item> item(new game_item());
        if (item->initialize() != FT_ERR_SUCCESS)
            return (1);
        item->set_item_id(42);
        item->set_max_stack(1);
        item->set_stack_size(1);
        item->set_width(1);
        item->set_height(1);
        item->set_instance_id(UINT64_MAX - 1U);
        if (character.equip_item(EQUIP_WEAPON, item) != FT_ERR_SUCCESS)
            return (1);
        if (thread_safe == FT_TRUE && item->enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
    }
    game_skill skill;
    if (skill.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t skill_index = 0;
    while (skill_index < skill_count)
    {
        skill.set_id(skill_index + 1);
        skill.set_modifier1(skill_index);
        skill.set_modifier4(-skill_index);
        if (character.add_skill(skill) != FT_ERR_SUCCESS)
            return (1);
        game_skill *stored_skill = character.get_skill(skill_index + 1);
        if (stored_skill == ft_nullptr)
            return (1);
        if (thread_safe == FT_TRUE && stored_skill->enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
        ++skill_index;
    }
    uint32_t iterations = 12000U;
    if (skill_count > 0)
        iterations = 2000U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (game_character_save_snapshot(output, character, character.get_inventory()) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    const char *encoded = output.c_str();
    uint64_t checksum = 0U;
    ft_size_t index = 0U;
    const ft_size_t length = output.size();
    while (index < length)
    {
        checksum = checksum * 31U + static_cast<unsigned char>(encoded[index]);
        ++index;
    }
    std::printf("%u %u %.3f %" PRIu64 " %" PRIu64 "\n",
        static_cast<uint32_t>(skill_count) + static_cast<uint32_t>(equipped),
        static_cast<uint32_t>(thread_safe), elapsed / iterations, static_cast<uint64_t>(length),
        checksum);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--skills") == 0)
    {
        if (measure_snapshot(FT_FALSE, FT_FALSE, 0) != 0
            || measure_snapshot(FT_FALSE, FT_FALSE, 64) != 0)
            return (1);
        return (measure_snapshot(FT_FALSE, FT_TRUE, 64));
    }
    if (measure_snapshot(FT_FALSE, FT_FALSE, 0) != 0
        || measure_snapshot(FT_TRUE, FT_FALSE, 0) != 0)
        return (1);
    return (measure_snapshot(FT_TRUE, FT_TRUE, 0));
}
