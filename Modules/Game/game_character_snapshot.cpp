#include "game_character_snapshot.hpp"
#include "game_character.hpp"
#include "game_inventory.hpp"
#include "../Basic/limits.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Errno/errno_internal.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct equipment_slot_field
    {
        int32_t slot;
        std::string_view name;
    };

    constexpr equipment_slot_field EQUIPMENT_SLOTS[] = {
        {EQUIP_HEAD, "head"}, {EQUIP_CHEST, "chest"}, {EQUIP_WEAPON, "weapon"},
        {EQUIP_LEGS, "legs"}, {EQUIP_FEET, "feet"}, {EQUIP_OFF_HAND, "off_hand"}
    };

    struct numeric_field
    {
        std::string_view name;
        int32_t value;
    };

    constexpr ft_size_t MAX_FIELD_LABEL_LENGTH = 64U;
    constexpr std::string_view FIELD_INDENT = "    \"";

    template <typename Integer>
    constexpr ft_size_t field_buffer_size() noexcept
    {
        constexpr ft_size_t MAX_DIGITS = std::numeric_limits<Integer>::digits10
            + 1U + std::numeric_limits<Integer>::is_signed;
        return (2U + FIELD_INDENT.size() + MAX_FIELD_LABEL_LENGTH + 3U + 2U + MAX_DIGITS);
    }

    template <typename Integer>
    char *write_number(char *destination, ft_bool &first, std::string_view prefix,
        std::string_view key, Integer value, ft_bool quoted)
    {
        if (prefix.size() > MAX_FIELD_LABEL_LENGTH
            || key.size() > MAX_FIELD_LABEL_LENGTH - prefix.size())
            throw std::length_error("checkpoint field label");
        char *end = destination;
        if (first == FT_FALSE)
        {
            *end++ = ',';
            *end++ = '\n';
        }
        end = std::copy(FIELD_INDENT.begin(), FIELD_INDENT.end(), end);
        end = std::copy(prefix.begin(), prefix.end(), end);
        end = std::copy(key.begin(), key.end(), end);
        *end++ = '"';
        *end++ = ':';
        *end++ = ' ';
        if (quoted == FT_TRUE)
            *end++ = '"';
        end = std::to_chars(end, destination + field_buffer_size<Integer>() - 1U, value).ptr;
        if (quoted == FT_TRUE)
            *end++ = '"';
        first = FT_FALSE;
        return (end);
    }

    template <typename Integer>
    void append_number(std::string &output, ft_bool &first, std::string_view prefix,
        std::string_view key, Integer value, ft_bool quoted = FT_TRUE)
    {
        char buffer[field_buffer_size<Integer>()];
        const char *end = write_number(buffer, first, prefix, key, value, quoted);
        output.append(buffer, static_cast<std::size_t>(end - buffer));
        return ;
    }

    template <ft_size_t Count>
    void append_fields(std::string &output, ft_bool &first, std::string_view prefix,
        const numeric_field (&fields)[Count])
    {
        char buffer[Count * field_buffer_size<int32_t>()];
        char *end = buffer;
        ft_size_t index = 0U;
        while (index < Count)
        {
            end = write_number(end, first, prefix, fields[index].name, fields[index].value, FT_TRUE);
            ++index;
        }
        output.append(buffer, static_cast<std::size_t>(end - buffer));
        return ;
    }

    void append_unsigned(std::string &output, ft_bool &first, std::string_view prefix,
        std::string_view key, uint64_t value)
    {
        ft_bool quoted = FT_TRUE;
        if (value > static_cast<uint64_t>(FT_LLONG_MAX))
            quoted = FT_FALSE;
        append_number(output, first, prefix, key, value, quoted);
        return ;
    }

    template <std::size_t Count>
    std::string_view build_indexed_prefix(char (&buffer)[32], const char (&prefix)[Count],
        ft_size_t index)
    {
        static_assert(Count + std::numeric_limits<ft_size_t>::digits10 <= sizeof(buffer));
        char *end = std::copy(prefix, prefix + Count - 1U, buffer);
        end = std::to_chars(end, buffer + sizeof(buffer), index).ptr;
        return (std::string_view(buffer, static_cast<std::size_t>(end - buffer)));
    }

    void append_item(std::string &output, ft_bool &first, std::string_view prefix,
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
            append_item(output, first, build_indexed_prefix(prefix, "item_", item_index),
                inventory.items[item_index], FT_FALSE);
            item_index += 1U;
        }
        output.append("\n  },\n");
        return ;
    }

}

int32_t game_character::append_checkpoint_character(std::string &output) const
{
    std::vector<game_skill::checkpoint_fields> skills;
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_skills.visit_values([&skills](const game_skill &skill) noexcept -> int32_t
    {
        game_skill::checkpoint_fields fields;
        const int32_t capture_error = skill.capture_checkpoint_fields(fields);
        if (capture_error != FT_ERR_SUCCESS)
            return (capture_error);
        try
        {
            skills.push_back(fields);
        }
        catch (const std::bad_alloc &)
        {
            return (FT_ERR_NO_MEMORY);
        }
        catch (const std::length_error &)
        {
            return (FT_ERR_OUT_OF_RANGE);
        }
        return (FT_ERR_SUCCESS);
    });
    this->unlock_internal(lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (skills.size() > INT32_MAX)
        return (FT_ERR_OUT_OF_RANGE);
    constexpr std::size_t SKILL_RESERVE_BYTES = 200U;
    if (!skills.empty() && skills.size() <= (output.max_size() - output.capacity()) / SKILL_RESERVE_BYTES)
        output.reserve(output.capacity() + skills.size() * SKILL_RESERVE_BYTES);

    const numeric_field attributes[] = {
        {"hit_points", this->get_hit_points()},
        {"physical_armor", this->get_physical_armor()},
        {"magic_armor", this->get_magic_armor()},
        {"current_physical_armor", this->get_current_physical_armor()},
        {"current_magic_armor", this->get_current_magic_armor()},
        {"damage_rule", this->get_damage_rule()},
        {"might", this->get_might()}, {"agility", this->get_agility()},
        {"endurance", this->get_endurance()}, {"reason", this->get_reason()},
        {"insigh", this->get_insigh()}, {"presence", this->get_presence()},
        {"coins", this->get_coins()}, {"valor", this->get_valor()},
        {"experience", this->get_experience()}, {"x", this->get_x()},
        {"y", this->get_y()}, {"z", this->get_z()},
        {"skill_count", static_cast<int32_t>(skills.size())}
    };
    ft_bool first = FT_TRUE;
    output.append("  \"character\": {\n");
    append_fields(output, first, "", attributes);
    ft_size_t skill_index = 0U;
    while (skill_index < skills.size())
    {
        const auto &skill = skills[skill_index];
        const numeric_field fields[] = {
            {"_id", skill.id}, {"_level", skill.level}, {"_cooldown", skill.cooldown},
            {"_mod1", skill.modifier1}, {"_mod2", skill.modifier2},
            {"_mod3", skill.modifier3}, {"_mod4", skill.modifier4}
        };
        char prefix[32];
        append_fields(output, first, build_indexed_prefix(prefix, "skill_", skill_index), fields);
        ++skill_index;
    }
    output.append("\n  },\n");
    return (FT_ERR_SUCCESS);
}

int32_t game_character::capture_checkpoint_item(int32_t slot,
    game_inventory_item_snapshot &snapshot, ft_bool &present) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_character::capture_checkpoint_item");
    ft_sharedptr<game_item> handle;
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    result = this->_equipment.read_item(slot, handle);
    this->unlock_internal(lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));

    const game_item *item = handle.get();
    result = handle.get_error();
    if (result == FT_ERR_SUCCESS && item != ft_nullptr)
        result = item->capture_inventory_fields(slot, snapshot);
    if (result == FT_ERR_SUCCESS)
        present = (item != ft_nullptr);
    return (this->set_error(result));
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
        result = character.append_checkpoint_character(encoded);
        if (result != FT_ERR_SUCCESS)
            return (result);
        append_inventory(encoded, inventory_snapshot);
        encoded.append("  \"equipment\": {\n");
        ft_bool first = FT_TRUE;
        ft_size_t slot_index = 0U;
        while (slot_index < sizeof(EQUIPMENT_SLOTS) / sizeof(EQUIPMENT_SLOTS[0]))
        {
            const equipment_slot_field &field = EQUIPMENT_SLOTS[slot_index];
            game_inventory_item_snapshot item = {};
            ft_bool present = FT_FALSE;
            result = character.capture_checkpoint_item(field.slot, item, present);
            if (result != FT_ERR_SUCCESS)
                return (result);
            append_number(encoded, first, field.name, "_present", static_cast<int32_t>(present));
            if (present == FT_TRUE)
                append_item(encoded, first, field.name, item, FT_TRUE);
            ++slot_index;
        }
        encoded.append("\n  }\n}\n");
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
