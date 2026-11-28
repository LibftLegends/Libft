#include "game_character.hpp"
#include "game_inventory.hpp"
#include "game_item.hpp"
#include "game_quest.hpp"
#include "../Errno/errno.hpp"
#include "../JSon/json.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Template/vector.hpp"
#include "../Template/shared_ptr.hpp"
#include <cstdio>
#include <charconv>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#include "game_achievement.hpp"
#include "game_buff.hpp"
#include "game_debuff.hpp"
#include "game_skill.hpp"
#include "game_upgrade.hpp"

json_group *serialize_character(const game_character &character);
json_group *serialize_inventory(const game_inventory &inventory);
json_group *serialize_equipment(const game_character &character);
json_group *serialize_quest(const game_quest &quest);

static void append_save_field(json_item **&next_field, json_item *field)
{
    *next_field = field;
    next_field = &field->next;
    return ;
}

static int32_t append_item_number(json_item **&next_field, char *key, char *suffix_start,
    const char *suffix, int32_t value)
{
    ft_memcpy(suffix_start, suffix, ft_strlen_size_t(suffix) + 1U);
    json_item *field = json_create_item(key, value);
    if (field == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    append_save_field(next_field, field);
    return (FT_ERR_SUCCESS);
}

static int32_t serialize_item_fields(json_item **&next_field,
    const game_inventory_item_snapshot &item, char *key, char *suffix_start,
    ft_bool rarity_before_dimensions)
{
    ft_memcpy(suffix_start, "_instance_id", sizeof("_instance_id"));
    char instance_id_value[32];
    std::snprintf(instance_id_value, sizeof(instance_id_value), FT_UINT64_DECIMAL_FORMAT,
        item.instance_id);
    json_item *instance_id_item = json_create_item(key, instance_id_value);
    if (instance_id_item == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    append_save_field(next_field, instance_id_item);

    static constexpr const char *suffixes[] = {
        "_max_stack", "_stack_size", "_id", "_width", "_height",
        "_mod1_id", "_mod1_value", "_mod2_id", "_mod2_value",
        "_mod3_id", "_mod3_value", "_mod4_id", "_mod4_value"
    };
    const int32_t values[] = {
        item.max_stack, item.stack_size, item.item_id, item.width, item.height,
        item.modifier1_id, item.modifier1_value, item.modifier2_id, item.modifier2_value,
        item.modifier3_id, item.modifier3_value, item.modifier4_id, item.modifier4_value
    };
    ft_size_t index = 0U;
    while (index < sizeof(values) / sizeof(values[0]))
    {
        // Equipment and quest rewards store rarity before width and height.
        if (rarity_before_dimensions == FT_TRUE && index == 3U
            && append_item_number(next_field, key, suffix_start, "_rarity", item.rarity)
                != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        if (append_item_number(next_field, key, suffix_start, suffixes[index], values[index])
            != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        ++index;
    }
    if (rarity_before_dimensions == FT_FALSE)
        return (append_item_number(next_field, key, suffix_start, "_rarity", item.rarity));
    return (FT_ERR_SUCCESS);
}

static int32_t serialize_indexed_item(json_item **&next_field,
    const game_inventory_item_snapshot &item, const char *prefix, ft_size_t index,
    ft_bool rarity_before_dimensions)
{
    char key[48];
    const ft_size_t prefix_length = ft_strlen_size_t(prefix);
    ft_memcpy(key, prefix, prefix_length);
    const auto number = std::to_chars(key + prefix_length,
        key + sizeof(key) - sizeof("_instance_id"), static_cast<int32_t>(index));
    if (number.ec != std::errc())
        return (FT_ERR_OUT_OF_RANGE);
    return (serialize_item_fields(next_field, item, key, number.ptr, rarity_before_dimensions));
}

static game_inventory_item_snapshot game_save_snapshot_item(const game_item &item)
{
    return (game_inventory_item_snapshot{0, item.get_item_id(), item.get_instance_id(),
        item.get_stack_size(), item.get_max_stack(), item.get_rarity(),
        item.get_width(), item.get_height(),
        item.get_modifier1_id(), item.get_modifier1_value(),
        item.get_modifier2_id(), item.get_modifier2_value(),
        item.get_modifier3_id(), item.get_modifier3_value(),
        item.get_modifier4_id(), item.get_modifier4_value()});
}

json_group *serialize_inventory(const game_inventory &inventory)
{
    game_inventory_snapshot snapshot;
    json_group *group = json_create_json_group("inventory");
    if (!group)
    {
        return (ft_nullptr);
    }
    ft_bool has_error = FT_FALSE;
    // This group is private until all fields have been built.
    json_item **next_field = &group->items;
    do
    {
        if (inventory.get_snapshot(snapshot) != FT_ERR_SUCCESS)
        {
            has_error = FT_TRUE;
            break ;
        }
        json_item *capacity_item = json_create_item("capacity", static_cast<int32_t>(snapshot.capacity));
        if (!capacity_item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, capacity_item);
        json_item *weight_limit_item = json_create_item("weight_limit", snapshot.weight_limit);
        if (!weight_limit_item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, weight_limit_item);
        json_item *current_weight_item = json_create_item("current_weight", snapshot.current_weight);
        if (!current_weight_item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, current_weight_item);
        json_item *used_slots_item = json_create_item("used_slots", static_cast<int32_t>(snapshot.used_slots));
        if (!used_slots_item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, used_slots_item);
        char revision_text[32];
        std::snprintf(revision_text, sizeof(revision_text), "%llu",
            static_cast<unsigned long long>(snapshot.revision));
        json_item *revision_item = json_create_item("revision", revision_text);
        if (!revision_item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, revision_item);
        ft_size_t item_count = snapshot.items.size();
        json_item *count_item = json_create_item("item_count", static_cast<int32_t>(item_count));
        if (!count_item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, count_item);
        ft_size_t item_index = 0;
        while (item_index < item_count)
        {
            const game_inventory_item_snapshot &item = snapshot.items[item_index];
            if (serialize_indexed_item(next_field, item, "item_", item_index, FT_FALSE) != FT_ERR_SUCCESS)
            {
                has_error = FT_TRUE;
                break ;
            }
            item_index++;
        }
        if (has_error)
            break ;
    } while (0);
    if (has_error)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    return (group);
}

static int32_t append_applied_modifiers(json_item **&next_field, char *key, char *suffix_start,
    const game_inventory_item_snapshot &live, const game_item_modifier_value (&applied)[4])
{
    const game_item_modifier_value current[] = {
        {live.modifier1_id, live.modifier1_value}, {live.modifier2_id, live.modifier2_value},
        {live.modifier3_id, live.modifier3_value}, {live.modifier4_id, live.modifier4_value}};
    ft_size_t index = 0;
    while (index < 4U && current[index].id == applied[index].id
        && current[index].value == applied[index].value)
        ++index;
    if (index == 4U)
        return (FT_ERR_SUCCESS);

    char encoded[96];
    char *cursor = encoded;
    char *const end = encoded + sizeof(encoded) - 1U;
    index = 0U;
    while (index < 4U)
    {
        if (index != 0U)
            *cursor++ = ',';
        const auto id = std::to_chars(cursor, end, applied[index].id);
        if (id.ec != std::errc() || id.ptr == end)
            return (FT_ERR_OUT_OF_RANGE);
        *id.ptr = ',';
        const auto value = std::to_chars(id.ptr + 1, end, applied[index].value);
        if (value.ec != std::errc())
            return (FT_ERR_OUT_OF_RANGE);
        cursor = value.ptr;
        ++index;
    }
    *cursor = '\0';
    ft_memcpy(suffix_start, "_applied_modifiers", sizeof("_applied_modifiers"));
    json_item *field = json_create_item(key, encoded);
    if (field == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    append_save_field(next_field, field);
    return (FT_ERR_SUCCESS);
}

static int32_t serialize_equipment_slot(json_item **&next_field, const char *name,
    const ft_sharedptr<game_item> &item, const game_item_modifier_value (&applied)[4])
{
    char key[48];
    const ft_size_t name_length = ft_strlen_size_t(name);
    ft_memcpy(key, name, name_length);
    const game_item *equipped = item.get();
    const int32_t present = static_cast<int32_t>(equipped != ft_nullptr);
    int32_t result = append_item_number(next_field, key, key + name_length, "_present", present);
    if (result == FT_ERR_SUCCESS && equipped != ft_nullptr)
    {
        const auto live = game_save_snapshot_item(*equipped);
        result = serialize_item_fields(next_field, live, key, key + name_length, FT_TRUE);
        if (result == FT_ERR_SUCCESS)
            result = append_applied_modifiers(next_field, key, key + name_length, live, applied);
    }
    return (result);
}

json_group *serialize_equipment(const game_character &character)
{
    json_group *group = json_create_json_group("equipment");
    if (group == ft_nullptr)
        return (ft_nullptr);
    json_item **next_field = &group->items;
    static constexpr const char *SLOT_NAMES[] = {"head", "chest", "weapon", "legs", "feet", "off_hand"};
    int32_t slot = EQUIP_HEAD;
    while (slot <= EQUIP_OFF_HAND)
    {
        ft_sharedptr<game_item> item;
        game_character::equipped_modifier_snapshot applied{};
        if (character.read_equipment_snapshot(slot, item, applied) != FT_ERR_SUCCESS
            || serialize_equipment_slot(next_field, SLOT_NAMES[slot], item, applied.values)
                != FT_ERR_SUCCESS)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        ++slot;
    }
    return (group);
}

json_group *serialize_quest(const game_quest &quest)
{
    json_group *group = json_create_json_group("quest");
    if (!group)
    {
        return (ft_nullptr);
    }
    json_item **next_field = &group->items;
    ft_bool has_error = FT_FALSE;
    do
    {
        json_item *item = json_create_item("id", quest.get_id());
        if (!item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, item);
        item = json_create_item("phases", quest.get_phases());
        if (!item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, item);
        item = json_create_item("current_phase", quest.get_current_phase());
        if (!item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, item);
        item = json_create_item("description", quest.get_description().c_str());
        if (!item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, item);
        item = json_create_item("objective", quest.get_objective().c_str());
        if (!item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, item);
        item = json_create_item("reward_experience", quest.get_reward_experience());
        if (!item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, item);
        ft_size_t item_count = quest.get_reward_items().size();
        item = json_create_item("reward_item_count", static_cast<int32_t>(item_count));
        if (!item)
        {
            has_error = FT_TRUE;
            break ;
        }
        append_save_field(next_field, item);
        const ft_vector<ft_sharedptr<game_item> > &reward_items = quest.get_reward_items();
        const ft_sharedptr<game_item> *item_start = reward_items.begin();
        if (item_count > 0 && !item_start)
        {
            has_error = FT_TRUE;
            break ;
        }
        ft_size_t item_index = 0;
        while (item_index < item_count)
        {
            if (!item_start[item_index])
            {
                has_error = FT_TRUE;
                break ;
            }
            if (serialize_indexed_item(next_field, game_save_snapshot_item(*item_start[item_index]),
                    "reward_item_", item_index, FT_TRUE) != FT_ERR_SUCCESS)
            {
                has_error = FT_TRUE;
                break ;
            }
            item_index++;
        }
        if (has_error)
            break ;
    } while (0);
    if (has_error)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    return (group);
}
