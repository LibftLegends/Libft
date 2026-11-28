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
#include <charconv>
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

static ft_bool parse_item_instance_id(const json_item *field, uint64_t &instance_id)
{
    instance_id = 0U;
    if (field == ft_nullptr)
        return (FT_TRUE);
    const char *digit = field->value;
    if (digit == ft_nullptr || *digit == '\0')
        return (FT_FALSE);
    while (*digit != '\0')
    {
        if (*digit < '0' || *digit > '9')
            return (FT_FALSE);
        const uint32_t value = static_cast<uint32_t>(*digit - '0');
        if (instance_id > (UINT64_MAX - value) / 10U)
            return (FT_FALSE);
        instance_id = instance_id * 10U + value;
        ++digit;
    }
    return (FT_TRUE);
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
    uint64_t instance_id;
    if (parse_item_instance_id(instance_id_item, instance_id) == FT_FALSE)
        return (FT_ERR_GAME_GENERAL_ERROR);
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

static const json_item *ordered_item_start(json_group *group,
    const char *const *header, ft_size_t count)
{
    if (pt_recursive_mutex_lock_if_not_null(group->_mutex) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    const json_item *field = group->items;
    ft_size_t index = 0U;
    while (field != ft_nullptr && index < count)
    {
        if (field->key == ft_nullptr || ft_strcmp(field->key, header[index]) != 0)
        {
            field = ft_nullptr;
            break ;
        }
        field = field->next;
        ++index;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    return (field);
}

static ft_bool read_ordered_item(game_item &item, json_group *group,
    const json_item *&cursor, char *key, char *suffix_start, ft_bool early_rarity)
{
    struct integer_field
    {
        const char *suffix;
        void (game_item::*set)(int32_t) noexcept;
    };
    static constexpr integer_field FIELDS[] = {
        {"_max_stack", &game_item::set_max_stack},
        {"_stack_size", &game_item::set_stack_size},
        {"_id", &game_item::set_item_id},
        {"_width", &game_item::set_width},
        {"_height", &game_item::set_height},
        {"_mod1_id", &game_item::set_modifier1_id},
        {"_mod1_value", &game_item::set_modifier1_value},
        {"_mod2_id", &game_item::set_modifier2_id},
        {"_mod2_value", &game_item::set_modifier2_value},
        {"_mod3_id", &game_item::set_modifier3_id},
        {"_mod3_value", &game_item::set_modifier3_value},
        {"_mod4_id", &game_item::set_modifier4_id},
        {"_mod4_value", &game_item::set_modifier4_value},
        {"_rarity", &game_item::set_rarity}
    };
    if (cursor == ft_nullptr
        || pt_recursive_mutex_lock_if_not_null(group->_mutex) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    ft_memcpy(suffix_start, "_instance_id", sizeof("_instance_id"));
    uint64_t instance_id;
    if (cursor->key == ft_nullptr || ft_strcmp(cursor->key, key) != 0
        || parse_item_instance_id(cursor, instance_id) == FT_FALSE)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
        return (FT_FALSE);
    }
    constexpr ft_size_t FIELD_COUNT = sizeof(FIELDS) / sizeof(FIELDS[0]);
    constexpr ft_size_t RARITY_INDEX = FIELD_COUNT - 1U;
    const json_item *fields[FIELD_COUNT];
    const json_item *field = cursor->next;
    ft_size_t index = 0U;
    while (index < FIELD_COUNT)
    {
        ft_size_t field_index = index;
        // Equipment and rewards put rarity immediately after the item ID.
        if (early_rarity == FT_TRUE && index >= 3U)
        {
            field_index = RARITY_INDEX;
            if (index != 3U)
                field_index = index - 1U;
        }
        const char *suffix = FIELDS[field_index].suffix;
        ft_memcpy(suffix_start, suffix, ft_strlen_size_t(suffix) + 1U);
        if (field == ft_nullptr || field->key == ft_nullptr || ft_strcmp(field->key, key) != 0)
            break ;
        fields[field_index] = field;
        field = field->next;
        ++index;
    }
    if (index != FIELD_COUNT)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
        return (FT_FALSE);
    }
    item.set_instance_id(instance_id);
    index = 0U;
    while (index < FIELD_COUNT)
    {
        (item.*FIELDS[index].set)(ft_atoi(fields[index]->value));
        ++index;
    }
    cursor = field;
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    return (FT_TRUE);
}

static ft_bool read_ordered_indexed_item(game_item &item, json_group *group,
    const json_item *&cursor, const char *prefix, int32_t item_index, ft_bool early_rarity)
{
    if (cursor == ft_nullptr)
        return (FT_FALSE);
    char key[48];
    const ft_size_t prefix_length = ft_strlen_size_t(prefix);
    ft_memcpy(key, prefix, prefix_length);
    const auto number = std::to_chars(key + prefix_length,
        key + sizeof(key) - sizeof("_instance_id"), item_index);
    if (number.ec != std::errc())
        return (FT_FALSE);
    return (read_ordered_item(item, group, cursor, key, number.ptr, early_rarity));
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
    // Only consume the ordered fields while the list matches the writer from its head.
    static constexpr const char *HEADER[] = {
        "capacity", "weight_limit", "current_weight", "used_slots", "revision", "item_count"
    };
    const json_item *ordered_field = ordered_item_start(group, HEADER,
        sizeof(HEADER) / sizeof(HEADER[0]));
    int32_t item_index = 0;
    int32_t loop_error = FT_ERR_SUCCESS;
    while (item_index < item_count)
    {
        game_item item_temp;
        if (item_temp.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        if (read_ordered_indexed_item(item_temp, group, ordered_field,
                "item_", item_index, FT_FALSE) == FT_FALSE)
        {
            ordered_field = ft_nullptr;
            char item_index_string[32];
            std::snprintf(item_index_string, sizeof(item_index_string), "%d", item_index);
            ft_string item_prefix;
            if (item_prefix.initialize("item_") != FT_ERR_SUCCESS)
                return (FT_ERR_GAME_GENERAL_ERROR);
            item_prefix += item_index_string;
            if (build_item_from_group(item_temp, group, item_prefix) != FT_ERR_SUCCESS)
                return (FT_ERR_GAME_GENERAL_ERROR);
        }
        ft_sharedptr<game_item> item(new game_item());
        if (!item || item->initialize(item_temp) != FT_ERR_SUCCESS)
        {
            return (FT_ERR_NO_MEMORY);
        }
        loop_error = inventory.restore_serialized_item(item);
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

static ft_bool take_ordered_slot_presence(json_group *group, const char *key,
    const json_item *&cursor, int32_t &present)
{
    if (cursor == ft_nullptr
        || pt_recursive_mutex_lock_if_not_null(group->_mutex) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const ft_bool matches = cursor->key != ft_nullptr && ft_strcmp(cursor->key, key) == 0;
    if (matches == FT_TRUE)
    {
        present = ft_atoi(cursor->value);
        cursor = cursor->next;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    return (matches);
}

static int32_t parse_applied_modifiers(const char *encoded,
    game_item_modifier_value (&modifiers)[4])
{
    if (encoded == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const char *current = encoded;
    const char *const end = current + ft_strlen_size_t(current);
    ft_size_t index = 0;
    while (index < 4U)
    {
        const auto id = std::from_chars(current, end, modifiers[index].id);
        if (id.ec != std::errc() || id.ptr == end || *id.ptr != ',')
            return (FT_ERR_INVALID_ARGUMENT);
        const auto value = std::from_chars(id.ptr + 1, end, modifiers[index].value);
        if (value.ec != std::errc())
            return (FT_ERR_INVALID_ARGUMENT);
        ++index;
        if (index == 4U)
        {
            if (value.ptr != end)
                return (FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_SUCCESS);
        }
        if (value.ptr == end || *value.ptr != ',')
            return (FT_ERR_INVALID_ARGUMENT);
        current = value.ptr + 1;
    }
    return (FT_ERR_INVALID_ARGUMENT);
}

static int32_t read_applied_modifiers(json_group *group, const json_item *&cursor,
    char *key, char *suffix_start, game_item_modifier_value (&modifiers)[4], ft_bool &found)
{
    ft_memcpy(suffix_start, "_applied_modifiers", sizeof("_applied_modifiers"));
    int32_t result = pt_recursive_mutex_lock_if_not_null(group->_mutex);
    if (result != FT_ERR_SUCCESS)
        return (result);
    const json_item *field;
    if (cursor != ft_nullptr && cursor->key != ft_nullptr && ft_strcmp(cursor->key, key) == 0)
    {
        field = cursor;
        cursor = cursor->next;
    }
    else
        field = json_find_item(group, key);
    found = field != ft_nullptr;
    if (found)
        result = parse_applied_modifiers(field->value, modifiers);
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    return (result);
}

static int32_t deserialize_equipment_slot(json_group *group, const json_item *&cursor,
    const char *name, ft_bool primary_slot, ft_sharedptr<game_item> &item,
    game_item_modifier_value (&applied)[4], ft_bool &has_applied)
{
    char key[48];
    const ft_size_t name_length = ft_strlen_size_t(name);
    ft_memcpy(key, name, name_length);
    ft_memcpy(key + name_length, "_present", sizeof("_present"));
    int32_t present = 0;
    if (take_ordered_slot_presence(group, key, cursor, present) == FT_FALSE)
    {
        cursor = ft_nullptr;
        const json_item *presence_field = json_find_item(group, key);
        if (presence_field != ft_nullptr)
            present = ft_atoi(presence_field->value);
    }
    if (present != 1)
        return (FT_ERR_SUCCESS);

    game_item item_temp;
    if (item_temp.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_GENERAL_ERROR);
    if (read_ordered_item(item_temp, group, cursor, key, key + name_length, FT_TRUE) == FT_FALSE)
    {
        cursor = ft_nullptr;
        ft_string prefix;
        if (prefix.initialize(name) != FT_ERR_SUCCESS)
            return (FT_ERR_GAME_GENERAL_ERROR);
        const int32_t result = build_item_from_group(item_temp, group, prefix);
        if (result != FT_ERR_SUCCESS)
        {
            if (primary_slot == FT_TRUE)
                return (FT_ERR_GAME_GENERAL_ERROR);
            return (result);
        }
    }

    int32_t result = read_applied_modifiers(group, cursor, key, key + name_length,
        applied, has_applied);
    if (result != FT_ERR_SUCCESS)
        return (result);
    auto *loaded = new (std::nothrow) game_item();
    if (loaded == ft_nullptr || item.initialize(loaded) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    result = loaded->initialize(item_temp);
    if (result != FT_ERR_SUCCESS)
    {
        if (primary_slot == FT_TRUE)
            return (FT_ERR_NO_MEMORY);
        return (result);
    }
    return (FT_ERR_SUCCESS);
}

int32_t deserialize_equipment(game_character &character, json_group *group,
    ft_bool stats_already_applied)
{
    const json_item *cursor = ft_nullptr;
    if (group != ft_nullptr)
        cursor = ordered_item_start(group, ft_nullptr, 0U);
    struct equipment_slot
    {
        int32_t slot;
        const char *name;
        ft_bool primary;
    };
    static constexpr equipment_slot SLOTS[] = {
        {EQUIP_HEAD, "head", FT_TRUE}, {EQUIP_CHEST, "chest", FT_TRUE},
        {EQUIP_WEAPON, "weapon", FT_TRUE}, {EQUIP_LEGS, "legs", FT_FALSE},
        {EQUIP_FEET, "feet", FT_FALSE}, {EQUIP_OFF_HAND, "off_hand", FT_FALSE}
    };
    struct restored_slot
    {
        ft_sharedptr<game_item> item;
        game_character::equipped_modifier_snapshot applied;
        ft_bool has_applied;
    };
    constexpr ft_size_t SLOT_COUNT = sizeof(SLOTS) / sizeof(SLOTS[0]);
    restored_slot restored[SLOT_COUNT]{};
    ft_size_t index = 0U;
    while (index < SLOT_COUNT)
    {
        const equipment_slot &slot = SLOTS[index];
        restored_slot &state = restored[index];
        const int32_t result = deserialize_equipment_slot(group, cursor, slot.name,
            slot.primary, state.item, state.applied.values, state.has_applied);
        if (result != FT_ERR_SUCCESS)
            return (result);
        ++index;
    }
    index = 0U;
    while (index < SLOT_COUNT)
    {
        const restored_slot &state = restored[index];
        const ft_sharedptr<game_item> *equipped = ft_nullptr;
        if (state.item)
            equipped = &state.item;
        const game_character::equipped_modifier_snapshot *snapshot = ft_nullptr;
        if (state.has_applied)
            snapshot = &state.applied;
        const int32_t result = character.replace_equipment_slot(SLOTS[index].slot, equipped, snapshot,
            static_cast<ft_bool>(!stats_already_applied));
        if (result != FT_ERR_SUCCESS)
            return (result);
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

int32_t deserialize_equipment(game_character &character, json_group *group)
{
    return (deserialize_equipment(character, group, FT_FALSE));
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
        static constexpr const char *HEADER[] = {
            "id", "phases", "current_phase", "description", "objective",
            "reward_experience", "reward_item_count"
        };
        const json_item *ordered_field = ordered_item_start(group, HEADER,
            sizeof(HEADER) / sizeof(HEADER[0]));
        int32_t reward_count = ft_atoi(count_item->value);
        int32_t reward_index = 0;
        quest.get_reward_items().clear();
        while (reward_index < reward_count)
        {
            game_item reward_temp;
            if (reward_temp.initialize() != FT_ERR_SUCCESS)
                return (FT_ERR_GAME_GENERAL_ERROR);
            if (read_ordered_indexed_item(reward_temp, group, ordered_field,
                    "reward_item_", reward_index, FT_TRUE) == FT_FALSE)
            {
                ordered_field = ft_nullptr;
                char index_string[32];
                std::snprintf(index_string, sizeof(index_string), "%d", reward_index);
                ft_string prefix;
                if (prefix.initialize("reward_item_") != FT_ERR_SUCCESS)
                    return (FT_ERR_NO_MEMORY);
                prefix += index_string;
                if (build_item_from_group(reward_temp, group, prefix) != FT_ERR_SUCCESS)
                    return (FT_ERR_GAME_GENERAL_ERROR);
            }
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
