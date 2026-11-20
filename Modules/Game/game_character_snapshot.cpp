#include "game_character_snapshot.hpp"
#include "game_character.hpp"
#include "game_inventory.hpp"
#include "../Basic/limits.hpp"
#include "../CPP_class/class_string.hpp"
#include <charconv>
#include <new>
#include <stdexcept>
#include <string>

namespace
{
    struct numeric_field
    {
        const char *name;
        int32_t value;
    };

    template <typename Integer>
    void append_number(std::string &output, ft_bool &first, const char *prefix,
        const char *key, Integer value, ft_bool quoted = FT_TRUE)
    {
        char decimal[32];
        const auto converted = std::to_chars(decimal, decimal + sizeof(decimal), value);

        if (first == FT_FALSE)
            output.append(",\n");
        output.append("    \"");
        output.append(prefix);
        output.append(key);
        output.append("\": ");
        if (quoted == FT_TRUE)
            output.push_back('"');
        output.append(decimal, converted.ptr);
        if (quoted == FT_TRUE)
            output.push_back('"');
        first = FT_FALSE;
        return ;
    }

    template <ft_size_t Count>
    void append_fields(std::string &output, ft_bool &first, const char *prefix,
        const numeric_field (&fields)[Count])
    {
        ft_size_t index = 0U;
        while (index < Count)
        {
            append_number(output, first, prefix, fields[index].name, fields[index].value);
            index += 1U;
        }
        return ;
    }

    void append_unsigned(std::string &output, ft_bool &first, const char *prefix,
        const char *key, uint64_t value)
    {
        ft_bool quoted = FT_TRUE;
        if (value > static_cast<uint64_t>(FT_LLONG_MAX))
            quoted = FT_FALSE;
        append_number(output, first, prefix, key, value, quoted);
        return ;
    }

    void build_indexed_prefix(char *buffer, const char *prefix, ft_size_t index)
    {
        const ft_size_t length = ft_strlen_size_t(prefix);
        ft_memcpy(buffer, prefix, length);
        const auto converted = std::to_chars(buffer + length, buffer + 31U, index);
        *converted.ptr = '\0';
        return ;
    }

    int32_t append_character(std::string &output, const game_character &character)
    {
        const auto &skills = character.get_skills();
        const ft_size_t skill_count = skills.size();
        const numeric_field attributes[] = {
            {"hit_points", character.get_hit_points()},
            {"physical_armor", character.get_physical_armor()},
            {"magic_armor", character.get_magic_armor()},
            {"current_physical_armor", character.get_current_physical_armor()},
            {"current_magic_armor", character.get_current_magic_armor()},
            {"damage_rule", character.get_damage_rule()},
            {"might", character.get_might()}, {"agility", character.get_agility()},
            {"endurance", character.get_endurance()}, {"reason", character.get_reason()},
            {"insigh", character.get_insigh()}, {"presence", character.get_presence()},
            {"coins", character.get_coins()}, {"valor", character.get_valor()},
            {"experience", character.get_experience()}, {"x", character.get_x()},
            {"y", character.get_y()}, {"z", character.get_z()},
            {"skill_count", static_cast<int32_t>(skill_count)}
        };
        ft_bool first = FT_TRUE;
        output.append("  \"character\": {\n");
        append_fields(output, first, "", attributes);
        ft_size_t skill_index = 0U;
        if (skill_count != 0U)
        {
            const auto *end = skills.end();
            if (end == ft_nullptr)
                return (FT_ERR_INVALID_STATE);
            const auto *entry = end - skill_count;
            while (skill_index < skill_count)
            {
                const game_skill &skill = entry[skill_index].value;
                const numeric_field fields[] = {
                    {"_id", skill.get_id()}, {"_level", skill.get_level()},
                    {"_cooldown", skill.get_cooldown()}, {"_mod1", skill.get_modifier1()},
                    {"_mod2", skill.get_modifier2()}, {"_mod3", skill.get_modifier3()},
                    {"_mod4", skill.get_modifier4()}
                };
                char prefix[32];
                build_indexed_prefix(prefix, "skill_", skill_index);
                append_fields(output, first, prefix, fields);
                skill_index += 1U;
            }
        }
        output.append("\n  },\n");
        return (FT_ERR_SUCCESS);
    }

    void append_item(std::string &output, ft_bool &first, const char *prefix,
        const game_inventory_item_snapshot &item, ft_bool equipped)
    {
        const numeric_field stack[] = {{"_max_stack", item.max_stack},
            {"_stack_size", item.stack_size}, {"_id", item.item_id}};
        const numeric_field metadata[] = {{"_width", item.width}, {"_height", item.height},
            {"_mod1_id", item.modifier1_id}, {"_mod1_value", item.modifier1_value},
            {"_mod2_id", item.modifier2_id}, {"_mod2_value", item.modifier2_value},
            {"_mod3_id", item.modifier3_id}, {"_mod3_value", item.modifier3_value},
            {"_mod4_id", item.modifier4_id}, {"_mod4_value", item.modifier4_value}};
        append_unsigned(output, first, prefix, "_instance_id", item.instance_id);
        append_fields(output, first, prefix, stack);
        if (equipped == FT_TRUE)
            append_number(output, first, prefix, "_rarity", item.rarity);
        append_fields(output, first, prefix, metadata);
        if (equipped == FT_FALSE)
            append_number(output, first, prefix, "_rarity", item.rarity);
        return ;
    }

    void append_inventory(std::string &output, const game_inventory_snapshot &inventory)
    {
        ft_bool first = FT_TRUE;
        const numeric_field attributes[] = {{"capacity", static_cast<int32_t>(inventory.capacity)},
            {"weight_limit", inventory.weight_limit}, {"current_weight", inventory.current_weight},
            {"used_slots", static_cast<int32_t>(inventory.used_slots)}};
        output.append("  \"inventory\": {\n");
        append_fields(output, first, "", attributes);
        append_unsigned(output, first, "", "revision", inventory.revision);
        append_number(output, first, "", "item_count", static_cast<int32_t>(inventory.items.size()));
        ft_size_t item_index = 0U;
        while (item_index < inventory.items.size())
        {
            char prefix[32];
            build_indexed_prefix(prefix, "item_", item_index);
            append_item(output, first, prefix, inventory.items[item_index], FT_FALSE);
            item_index += 1U;
        }
        output.append("\n  },\n");
        return ;
    }

    int32_t append_equipment(std::string &output, const game_character &character)
    {
        const int32_t slots[] = {EQUIP_HEAD, EQUIP_CHEST, EQUIP_WEAPON,
            EQUIP_LEGS, EQUIP_FEET, EQUIP_OFF_HAND};
        const char *const names[] = {"head", "chest", "weapon", "legs", "feet", "off_hand"};
        ft_bool first = FT_TRUE;
        ft_size_t slot_index = 0U;
        output.append("  \"equipment\": {\n");
        while (slot_index < sizeof(slots) / sizeof(slots[0]))
        {
            ft_sharedptr<game_item> *handle = character.get_equipped_item(slots[slot_index]);
            if (handle == ft_nullptr)
            {
                const int32_t error_code = character.get_error();
                if (error_code == FT_ERR_SUCCESS)
                    return (FT_ERR_INVALID_STATE);
                return (error_code);
            }
            game_inventory_item_snapshot snapshot = {};
            const game_item *item = handle->get();
            int32_t present = 0;
            if (item != ft_nullptr)
            {
                present = 1;
                snapshot = {slots[slot_index], item->get_item_id(), item->get_instance_id(),
                    item->get_stack_size(), item->get_max_stack(), item->get_rarity(),
                    item->get_width(), item->get_height(), item->get_modifier1_id(),
                    item->get_modifier1_value(), item->get_modifier2_id(), item->get_modifier2_value(),
                    item->get_modifier3_id(), item->get_modifier3_value(), item->get_modifier4_id(),
                    item->get_modifier4_value()};
            }
            (void)handle->destroy();
            delete handle;
            append_number(output, first, names[slot_index], "_present", present);
            if (present != 0)
                append_item(output, first, names[slot_index], snapshot, FT_TRUE);
            slot_index += 1U;
        }
        output.append("\n  }\n");
        return (FT_ERR_SUCCESS);
    }
}

int32_t game_character_save_snapshot(ft_string &output,
    const game_character &character, const game_inventory &inventory) noexcept
{
    game_inventory_snapshot inventory_snapshot;
    int32_t result = inventory.get_snapshot(inventory_snapshot);
    if (result != FT_ERR_SUCCESS)
        return (result);
    try
    {
        std::string encoded;
        encoded.reserve(1024U);
        encoded.append("{\n  \"world\": {\n    \"event_count\": \"0\"\n  },\n");
        result = append_character(encoded, character);
        if (result != FT_ERR_SUCCESS)
            return (result);
        append_inventory(encoded, inventory_snapshot);
        result = append_equipment(encoded, character);
        if (result != FT_ERR_SUCCESS)
            return (result);
        encoded.append("}\n");
        return (output.assign(encoded.data(), FT_NATIVE_SIZE_TO_FT_SIZE_CAST(encoded.size())));
    }
    catch (const std::bad_alloc &)
    {
        return (FT_ERR_NO_MEMORY);
    }
    catch (const std::length_error &)
    {
        return (FT_ERR_OUT_OF_RANGE);
    }
}
