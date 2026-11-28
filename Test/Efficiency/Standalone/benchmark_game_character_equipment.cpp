#include "../../../Modules/Game/game_character.hpp"
#include "../../../Modules/JSon/json.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <new>

static int32_t measure_equipment(ft_bool items_locked)
{
    constexpr uint32_t REPLACEMENTS = 40000U;
    game_character character;
    ft_sharedptr<game_item> first(new (std::nothrow) game_item());
    ft_sharedptr<game_item> second(new (std::nothrow) game_item());
    if (!first || !second || character.initialize() != FT_ERR_SUCCESS
        || first->initialize() != FT_ERR_SUCCESS || second->initialize() != FT_ERR_SUCCESS)
        return (1);
    first->set_modifier1_id(1);
    first->set_modifier1_value(2);
    first->set_modifier2_id(1);
    first->set_modifier2_value(3);
    second->set_modifier1_id(1);
    second->set_modifier1_value(4);
    second->set_modifier2_id(1);
    second->set_modifier2_value(5);
    second->set_modifier3_id(1);
    second->set_modifier3_value(6);
    second->set_modifier4_id(1);
    second->set_modifier4_value(7);
    if (items_locked && (first->enable_thread_safety() != FT_ERR_SUCCESS
        || second->enable_thread_safety() != FT_ERR_SUCCESS))
        return (1);
    if (character.equip_item(EQUIP_HEAD, second) != FT_ERR_SUCCESS)
        return (1);

    ft_sharedptr<game_item> *next = &first;
    ft_sharedptr<game_item> *other = &second;
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < REPLACEMENTS)
    {
        if (character.equip_item(EQUIP_HEAD, *next) != FT_ERR_SUCCESS)
            return (1);
        checksum += character.get_physical_armor();
        ft_sharedptr<game_item> *previous = next;
        next = other;
        other = previous;
        ++index;
    }
    const auto end = std::chrono::steady_clock::now();
    character.unequip_item(EQUIP_HEAD);
    if (character.get_error() != FT_ERR_SUCCESS || character.get_physical_armor() != 0
        || checksum != REPLACEMENTS / 2U * 27U)
        return (1);
    std::printf("item_lock=%u replace_ns=%.3f checksum=%" PRIu64 "\n",
        static_cast<uint32_t>(items_locked),
        std::chrono::duration<double, std::nano>(end - start).count() / REPLACEMENTS,
        checksum);
    return (0);
}

static int32_t measure_armor(int32_t base_armor)
{
    constexpr uint32_t REPETITIONS = 60000U;
    game_character character;
    if (character.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < REPETITIONS)
    {
        const int32_t armor = base_armor + static_cast<int32_t>(index % 64U);
        character.set_physical_armor(armor);
        character.set_magic_armor(armor + 1);
        uint64_t multiplier_bits;
        std::memcpy(&multiplier_bits, &character._physical_damage_multiplier,
            sizeof(multiplier_bits));
        checksum += multiplier_bits;
        std::memcpy(&multiplier_bits, &character._magic_damage_multiplier,
            sizeof(multiplier_bits));
        checksum += multiplier_bits;
        ++index;
    }
    const auto end = std::chrono::steady_clock::now();
    std::printf("base_armor=%" PRId32 " update_ns=%.3f checksum=%" PRIu64 "\n",
        base_armor, std::chrono::duration<double, std::nano>(end - start).count() / REPETITIONS,
        checksum);
    return (0);
}

static int32_t measure_equipment_save()
{
    constexpr uint32_t SAVES = 6000U;
    game_character character;
    ft_sharedptr<game_item> item(new (std::nothrow) game_item());
    if (!item || character.initialize() != FT_ERR_SUCCESS || item->initialize() != FT_ERR_SUCCESS)
        return (1);
    item->set_modifier1_id(1);
    item->set_modifier1_value(4);
    if (character.equip_item(EQUIP_HEAD, item) != FT_ERR_SUCCESS
        || character.equip_item(EQUIP_CHEST, item) != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < SAVES)
    {
        json_group *group = serialize_equipment(character);
        if (group == ft_nullptr)
            return (1);
        const json_item *field = group->items;
        while (field != ft_nullptr)
        {
            checksum += static_cast<uint8_t>(field->key[0])
                + static_cast<uint8_t>(field->value[0]);
            field = field->next;
        }
        json_free_groups(group);
        ++index;
    }
    const auto end = std::chrono::steady_clock::now();
    std::printf("equipment_save_us=%.3f checksum=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(end - start).count() / SAVES, checksum);
    return (0);
}

static int32_t measure_damage(uint8_t rule, ft_bool skills_enabled = FT_FALSE)
{
    constexpr uint32_t HITS = 400000U;
    game_character character;
    if (character.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (skills_enabled)
    {
        game_skill skill;
        if (skill.initialize() != FT_ERR_SUCCESS)
            return (1);
        int32_t skill_id = 1;
        while (skill_id <= 8)
        {
            skill.set_id(skill_id);
            skill.set_cooldown(skill_id % 3);
            skill.set_modifier1(2);
            skill.set_modifier2(-7);
            if (character.add_skill(skill) != FT_ERR_SUCCESS)
                return (1);
            ++skill_id;
        }
    }
    character.set_damage_rule(rule);
    character.set_physical_armor(12);
    character.set_magic_armor(9);
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < HITS)
    {
        character.set_hit_points(100);
        character.take_damage(40 + index % 10U, static_cast<uint8_t>(index % 2U));
        checksum += character.get_hit_points();
        ++index;
    }
    const auto end = std::chrono::steady_clock::now();
    std::printf("damage_rule=%u hit_ns=%.3f checksum=%" PRIu64 "\n",
        static_cast<uint32_t>(rule),
        std::chrono::duration<double, std::nano>(end - start).count() / HITS, checksum);
    return (0);
}

template <ft_bool UseCallerHandle>
static int32_t measure_equipment_read()
{
    constexpr uint32_t READS = 400000U;
    game_character character;
    ft_sharedptr<game_item> helmet(new game_item());
    if (character.initialize() != FT_ERR_SUCCESS || !helmet
        || helmet->initialize() != FT_ERR_SUCCESS)
        return (1);
    helmet->set_item_id(42);
    if (character.equip_item(EQUIP_HEAD, helmet) != FT_ERR_SUCCESS)
        return (1);
    const game_character &reader = character;
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < READS)
    {
        int32_t slot = EQUIP_HEAD;
        if ((index & 1U) != 0U)
            slot = EQUIP_FEET;
        if constexpr (UseCallerHandle == FT_TRUE)
        {
            ft_sharedptr<game_item> item;
            if (reader.read_equipped_item(slot, item) != FT_ERR_SUCCESS)
                return (1);
            if (item)
                checksum += static_cast<uint64_t>(item->get_item_id());
        }
        else
        {
            ft_sharedptr<game_item> *item = reader.get_equipped_item(slot);
            if (item == ft_nullptr)
                return (1);
            if (*item)
                checksum += static_cast<uint64_t>((*item)->get_item_id());
            delete item;
        }
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != (READS / 2U) * 42U)
        return (1);
    std::printf("equipment_read %.3f %" PRIu64 "\n", elapsed / READS, checksum);
    return (0);
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && std::strcmp(arguments[1], "--read-heap") == 0)
        return (measure_equipment_read<FT_FALSE>());
    if (argument_count == 2 && std::strcmp(arguments[1], "--read-stack") == 0)
        return (measure_equipment_read<FT_TRUE>());
    if (argument_count == 2 && std::strcmp(arguments[1], "--armor") == 0)
    {
        if (measure_armor(64) != 0)
            return (1);
        return (measure_armor(768));
    }
    if (argument_count == 2 && std::strcmp(arguments[1], "--equipment-save") == 0)
        return (measure_equipment_save());
    if (argument_count == 2 && std::strcmp(arguments[1], "--damage") == 0)
    {
        if (measure_damage(FT_DAMAGE_RULE_FLAT) != 0)
            return (1);
        return (measure_damage(FT_DAMAGE_RULE_SCALED));
    }
    if (argument_count == 2 && std::strcmp(arguments[1], "--skill-damage") == 0)
    {
        if (measure_damage(FT_DAMAGE_RULE_FLAT, FT_TRUE) != 0)
            return (1);
        return (measure_damage(FT_DAMAGE_RULE_SCALED, FT_TRUE));
    }
    if (measure_equipment(FT_FALSE) != 0)
        return (1);
    return (measure_equipment(FT_TRUE));
}
