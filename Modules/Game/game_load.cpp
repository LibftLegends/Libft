#include "game_character.hpp"
#include "game_inventory.hpp"
#include "game_item.hpp"
#include "game_quest.hpp"
#include "../JSon/json.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Template/shared_ptr.hpp"
#include <cstdio>
#include <stdint.h>
#include <limits>
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#include "../Template/vector.hpp"
#include "game_achievement.hpp"
#include "game_buff.hpp"
#include "game_debuff.hpp"
#include "game_skill.hpp"
#include "game_upgrade.hpp"

int32_t deserialize_character(game_character &character, json_group *group);
int32_t deserialize_inventory(game_inventory &inventory, json_group *group);
int32_t deserialize_equipment(game_character &character, json_group *group);
int32_t deserialize_quest(game_quest &quest, json_group *group);

static int32_t parse_item_field(json_group *group, const ft_string &key, int32_t &out_value)
{
    json_item *json_item_ptr = json_find_item(group, key.c_str());
    if (!json_item_ptr)
    {
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    out_value = ft_atoi(json_item_ptr->value);
    return (FT_ERR_SUCCESS);
}

static int32_t build_item_field_key(const ft_string &item_prefix,
    const char *suffix, ft_string &key) noexcept
{
    int32_t error_code;

    error_code = key.initialize(item_prefix);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = key.append(suffix);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

static int32_t build_item_from_group(game_item &item, json_group *group, const ft_string &item_prefix)
{
    int32_t value;
    ft_string key_max;
    if (build_item_field_key(item_prefix, "_max_stack", key_max) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_max, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_max_stack(value);
    ft_string key_current;
    if (build_item_field_key(item_prefix, "_stack_size", key_current) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_current, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_stack_size(value);
    ft_string key_id;
    if (build_item_field_key(item_prefix, "_id", key_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_id, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_item_id(value);
    ft_string key_instance_id;
    if (build_item_field_key(item_prefix, "_instance_id", key_instance_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    json_item *instance_id_item = json_find_item(group, key_instance_id.c_str());
    uint64_t instance_id = 0;
    if (instance_id_item)
    {
        const char *digit = instance_id_item->value;
        if (!digit || !*digit)
            return (FT_ERR_GAME_GENERAL_ERROR);
        while (*digit)
        {
            uint32_t number;
            if (*digit < '0' || *digit > '9')
                return (FT_ERR_GAME_GENERAL_ERROR);
            number = static_cast<uint32_t>(*digit - '0');
            if (instance_id > (UINT64_MAX - number) / 10)
                return (FT_ERR_GAME_GENERAL_ERROR);
            instance_id = instance_id * 10 + number;
            ++digit;
        }
    }
    item.set_instance_id(instance_id);
    ft_string key_rarity;
    if (build_item_field_key(item_prefix, "_rarity", key_rarity) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    json_item *rarity_item = json_find_item(group, key_rarity.c_str());
    if (rarity_item)
    {
        value = ft_atoi(rarity_item->value);
        item.set_rarity(value);
    }
    ft_string key_width;
    if (build_item_field_key(item_prefix, "_width", key_width) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_width, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_width(value);
    ft_string key_height;
    if (build_item_field_key(item_prefix, "_height", key_height) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_height, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_height(value);
    ft_string key_mod1_id;
    if (build_item_field_key(item_prefix, "_mod1_id", key_mod1_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_mod1_id, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_modifier1_id(value);
    ft_string key_mod1_value;
    if (build_item_field_key(item_prefix, "_mod1_value", key_mod1_value) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_mod1_value, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_modifier1_value(value);
    ft_string key_mod2_id;
    if (build_item_field_key(item_prefix, "_mod2_id", key_mod2_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_mod2_id, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_modifier2_id(value);
    ft_string key_mod2_value;
    if (build_item_field_key(item_prefix, "_mod2_value", key_mod2_value) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_mod2_value, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_modifier2_value(value);
    ft_string key_mod3_id;
    if (build_item_field_key(item_prefix, "_mod3_id", key_mod3_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_mod3_id, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_modifier3_id(value);
    ft_string key_mod3_value;
    if (build_item_field_key(item_prefix, "_mod3_value", key_mod3_value) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_mod3_value, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_modifier3_value(value);
    ft_string key_mod4_id;
    if (build_item_field_key(item_prefix, "_mod4_id", key_mod4_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_mod4_id, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_modifier4_id(value);
    ft_string key_mod4_value;
    if (build_item_field_key(item_prefix, "_mod4_value", key_mod4_value) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (parse_item_field(group, key_mod4_value, value) != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    item.set_modifier4_value(value);
    return (FT_ERR_SUCCESS);
}

int32_t deserialize_inventory(game_inventory &inventory, json_group *group)
{
    int32_t initialize_error;
    json_item *revision_item;

    initialize_error = inventory.destroy();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    initialize_error = inventory.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    json_item *capacity_item = json_find_item(group, "capacity");
    if (!capacity_item)
    {
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    inventory.resize(ft_atoi(capacity_item->value));
    json_item *weight_item = json_find_item(group, "weight_limit");
    if (!weight_item)
    {
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    inventory.set_weight_limit(ft_atoi(weight_item->value));
    json_item *cur_weight_item = json_find_item(group, "current_weight");
    if (!cur_weight_item)
    {
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    int32_t serialized_weight = ft_atoi(cur_weight_item->value);
    json_item *used_slots_item = json_find_item(group, "used_slots");
    if (!used_slots_item)
    {
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    int32_t serialized_slots = ft_atoi(used_slots_item->value);
    if (inventory.clear_items() != FT_ERR_SUCCESS)
        return (inventory.get_error());
    json_item *count_item = json_find_item(group, "item_count");
    if (!count_item)
    {
        return (FT_ERR_GAME_GENERAL_ERROR);
    }
    int32_t item_count = ft_atoi(count_item->value);
    int32_t item_index = 0;
    int32_t loop_error = FT_ERR_SUCCESS;
    while (item_index < item_count)
    {
        char item_index_string[32];

        std::snprintf(item_index_string, sizeof(item_index_string), "%d", item_index);
        ft_string item_prefix;
        if (item_prefix.initialize("item_") != FT_ERR_SUCCESS)
        {
            return (FT_ERR_GAME_GENERAL_ERROR);
        }
        item_prefix += item_index_string;
        game_item item_temp;
        if (item_temp.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        if (build_item_from_group(item_temp, group, item_prefix) != FT_ERR_SUCCESS)
        {
            loop_error = FT_ERR_GAME_GENERAL_ERROR;
            break ;
        }
        ft_sharedptr<game_item> item(new game_item());
        if (!item || item->initialize(item_temp) != FT_ERR_SUCCESS)
        {
            return (FT_ERR_NO_MEMORY);
        }
        loop_error = inventory.add_item(item);
        if (loop_error != FT_ERR_SUCCESS)
        {
            break ;
        }
        item_index++;
    }
    if (loop_error != FT_ERR_SUCCESS)
    {
        return (loop_error);
    }
    inventory.set_current_weight(serialized_weight);
    inventory.set_used_slots(serialized_slots);
    revision_item = json_find_item(group, "revision");
    if (revision_item != ft_nullptr)
    {
        uint64_t revision = 0U;
        const char *cursor = revision_item->value;

        if (cursor == ft_nullptr || *cursor == '\0')
            return (FT_ERR_INVALID_ARGUMENT);
        while (*cursor != '\0')
        {
            uint32_t digit;

            if (*cursor < '0' || *cursor > '9')
                return (FT_ERR_INVALID_ARGUMENT);
            digit = static_cast<uint32_t>(*cursor - '0');
            if (revision > (std::numeric_limits<uint64_t>::max() - digit)
                    / 10U)
                return (FT_ERR_OUT_OF_RANGE);
            revision = revision * 10U + digit;
            ++cursor;
        }
        return (inventory.restore_revision(revision));
    }
    return (FT_ERR_SUCCESS);
}

static int32_t deserialize_equipment_extra_slot(game_character &character,
    json_group *group, int32_t slot, const char *name)
{
    char present_name[32];
    if (std::snprintf(present_name, sizeof(present_name), "%s_present", name)
        < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    json_item *present = json_find_item(group, present_name);
    if (present == ft_nullptr || ft_atoi(present->value) != 1)
    {
        character.unequip_item(slot);
        return (FT_ERR_SUCCESS);
    }
    game_item item_temp;
    ft_string prefix;
    if (prefix.initialize(name) != FT_ERR_SUCCESS
        || item_temp.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    int32_t result = build_item_from_group(item_temp, group, prefix);
    if (result != FT_ERR_SUCCESS)
        return (result);
    ft_sharedptr<game_item> item(new (std::nothrow) game_item());
    if (!item)
        return (FT_ERR_NO_MEMORY);
    result = item->initialize(item_temp);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = character.equip_item(slot, item);
    return (result);
}

int32_t deserialize_equipment(game_character &character, json_group *group)
{
    json_item *present = json_find_item(group, "head_present");
    if (present && ft_atoi(present->value) == 1)
    {
        game_item item_temp;
        ft_string head_prefix;
        if (head_prefix.initialize("head") != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        if (item_temp.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        if (build_item_from_group(item_temp, group, head_prefix) != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        ft_sharedptr<game_item> item(new game_item());
        if (!item || item->initialize(item_temp) != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        if (character.equip_item(EQUIP_HEAD, item) != FT_ERR_SUCCESS)
            return (character.get_error());
    }
    else
        character.unequip_item(EQUIP_HEAD);
    present = json_find_item(group, "chest_present");
    if (present && ft_atoi(present->value) == 1)
    {
        game_item item_temp;
        ft_string chest_prefix;
        if (chest_prefix.initialize("chest") != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        if (item_temp.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        if (build_item_from_group(item_temp, group, chest_prefix) != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        ft_sharedptr<game_item> item(new game_item());
        if (!item || item->initialize(item_temp) != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        if (character.equip_item(EQUIP_CHEST, item) != FT_ERR_SUCCESS)
            return (character.get_error());
    }
    else
        character.unequip_item(EQUIP_CHEST);
    present = json_find_item(group, "weapon_present");
    if (present && ft_atoi(present->value) == 1)
    {
        game_item item_temp;
        ft_string weapon_prefix;
        if (weapon_prefix.initialize("weapon") != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        if (item_temp.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        if (build_item_from_group(item_temp, group, weapon_prefix) != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        ft_sharedptr<game_item> item(new game_item());
        if (!item || item->initialize(item_temp) != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        if (character.equip_item(EQUIP_WEAPON, item) != FT_ERR_SUCCESS)
            return (character.get_error());
    }
    else
        character.unequip_item(EQUIP_WEAPON);
    int32_t extra_result = deserialize_equipment_extra_slot(character, group,
        EQUIP_LEGS, "legs");
    if (extra_result == FT_ERR_SUCCESS)
        extra_result = deserialize_equipment_extra_slot(character, group,
            EQUIP_FEET, "feet");
    if (extra_result == FT_ERR_SUCCESS)
        extra_result = deserialize_equipment_extra_slot(character, group,
            EQUIP_OFF_HAND, "off_hand");
    if (extra_result != FT_ERR_SUCCESS)
        return (extra_result);
    return (FT_ERR_SUCCESS);
}

int32_t deserialize_quest(game_quest &quest, json_group *group)
{
    json_item *item = json_find_item(group, "id");
    if (item)
        quest.set_id(ft_atoi(item->value));
    item = json_find_item(group, "phases");
    if (item)
        quest.set_phases(ft_atoi(item->value));
    item = json_find_item(group, "current_phase");
    if (item)
        quest.set_current_phase(ft_atoi(item->value));
    item = json_find_item(group, "description");
    if (item)
    {
        ft_string description;
        if (description.initialize(item->value) != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        quest.set_description(description);
    }
    item = json_find_item(group, "objective");
    if (item)
    {
        ft_string objective;
        if (objective.initialize(item->value) != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        quest.set_objective(objective);
    }
    item = json_find_item(group, "reward_experience");
    if (item)
        quest.set_reward_experience(ft_atoi(item->value));
    json_item *count_item = json_find_item(group, "reward_item_count");
    if (count_item)
    {
        int32_t reward_count = ft_atoi(count_item->value);
        int32_t reward_index = 0;
        quest.get_reward_items().clear();
        while (reward_index < reward_count)
        {
            char index_string[32];

            std::snprintf(index_string, sizeof(index_string), "%d", reward_index);
            ft_string prefix;
            if (prefix.initialize("reward_item_") != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            prefix += index_string;
            game_item reward_temp;
            if (reward_temp.initialize() != FT_ERR_SUCCESS)
                return (FT_ERR_GAME_GENERAL_ERROR);
            if (build_item_from_group(reward_temp, group, prefix) != FT_ERR_SUCCESS)
                return (FT_ERR_GAME_GENERAL_ERROR);
            ft_sharedptr<game_item> reward(new game_item());
            if (!reward || reward->initialize(reward_temp) != FT_ERR_SUCCESS)
            {
                return (FT_ERR_NO_MEMORY);
            }
            quest.get_reward_items().push_back(reward);
            reward_index++;
        }
    }
    return (FT_ERR_SUCCESS);
}
