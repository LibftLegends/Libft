#include "game_character.hpp"
#include "../Errno/errno.hpp"
#include "../JSon/json.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#include "../Template/shared_ptr.hpp"
#include "game_achievement.hpp"
#include "game_buff.hpp"
#include "game_debuff.hpp"
#include "game_quest.hpp"
#include "game_skill.hpp"
#include "game_upgrade.hpp"

#include <charconv>
#include <limits>

static constexpr char SKILL_KEY_PREFIX[] = "skill_";
static constexpr ft_size_t SKILL_KEY_PREFIX_SIZE = sizeof(SKILL_KEY_PREFIX) - 1U;
static constexpr ft_size_t SKILL_KEY_INDEX_SIZE = std::numeric_limits<int32_t>::digits10 + 2U;
static constexpr ft_size_t SKILL_KEY_SIZE = SKILL_KEY_PREFIX_SIZE
    + SKILL_KEY_INDEX_SIZE + sizeof("_cooldown");

enum skill_save_field
{
    SKILL_ID,
    SKILL_LEVEL,
    SKILL_COOLDOWN,
    SKILL_MODIFIER_1,
    SKILL_MODIFIER_2,
    SKILL_MODIFIER_3,
    SKILL_MODIFIER_4,
    SKILL_FIELD_COUNT
};

static constexpr char SKILL_SUFFIXES[SKILL_FIELD_COUNT][sizeof("_cooldown")] = {
    "_id", "_level", "_cooldown", "_mod1", "_mod2", "_mod3", "_mod4"
};

static char *game_character_write_skill_prefix(char (&key)[SKILL_KEY_SIZE],
    int32_t skill_index) noexcept
{
    ft_memcpy_fixed<SKILL_KEY_PREFIX_SIZE>(key, SKILL_KEY_PREFIX);
    char *index = key + SKILL_KEY_PREFIX_SIZE;
    return (std::to_chars(index, index + SKILL_KEY_INDEX_SIZE, skill_index).ptr);
}

static const char *game_character_set_skill_suffix(const char *key, char *suffix,
    ft_size_t field_index) noexcept
{
    ft_memcpy_fixed<sizeof(SKILL_SUFFIXES[0])>(suffix, SKILL_SUFFIXES[field_index]);
    return (key);
}

static const json_item *game_character_first_skill_item(json_group *group) noexcept
{
    if (pt_recursive_mutex_lock_if_not_null(group->_mutex) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    const json_item *item = group->items;
    while (item != ft_nullptr)
    {
        if (item->key != ft_nullptr
            && ft_strncmp(item->key, SKILL_KEY_PREFIX, SKILL_KEY_PREFIX_SIZE) == 0
            && ft_strcmp(item->key, "skill_count") != 0)
            break ;
        item = item->next;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
    return (item);
}

static void game_character_read_skill_items(json_group *group, const json_item *&next_ordered,
    int32_t skill_index, const json_item *(&items)[SKILL_FIELD_COUNT]) noexcept
{
    char key[SKILL_KEY_SIZE];
    char *suffix = game_character_write_skill_prefix(key, skill_index);
    if (next_ordered != ft_nullptr
        && pt_recursive_mutex_lock_if_not_null(group->_mutex) == FT_ERR_SUCCESS)
    {
        ft_size_t field_index = 0;
        while (field_index < SKILL_FIELD_COUNT && next_ordered != ft_nullptr
            && next_ordered->key != ft_nullptr
            && ft_strcmp(next_ordered->key,
                game_character_set_skill_suffix(key, suffix, field_index)) == 0)
        {
            items[field_index] = next_ordered;
            next_ordered = next_ordered->next;
            ++field_index;
        }
        (void)pt_recursive_mutex_unlock_if_not_null(group->_mutex);
        if (field_index == SKILL_FIELD_COUNT)
            return ;
    }
    // A full lookup preserves the first match once the saved field order differs.
    next_ordered = ft_nullptr;
    ft_size_t field_index = 0;
    while (field_index < SKILL_FIELD_COUNT)
    {
        items[field_index] = json_find_item(group,
            game_character_set_skill_suffix(key, suffix, field_index));
        ++field_index;
    }
    return ;
}

static void game_character_append_save_item(json_item **&tail, json_item *item) noexcept
{
    *tail = item;
    tail = &item->next;
    return ;
}

json_group *serialize_character(const game_character &character)
{
    json_group *group = json_create_json_group("character");
    if (!group)
    {
        return (ft_nullptr);
    }
    // The group is private until it is returned.
    json_item **item_tail = &group->items;
    json_item *item = json_create_item("hit_points", character.get_hit_points());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("physical_armor", character.get_physical_armor());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("magic_armor", character.get_magic_armor());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("current_physical_armor", character.get_current_physical_armor());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("current_magic_armor", character.get_current_magic_armor());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("damage_rule",
        static_cast<int32_t>(character.get_damage_rule()));
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("might", character.get_might());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("agility", character.get_agility());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("endurance", character.get_endurance());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("reason", character.get_reason());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("insigh", character.get_insigh());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("presence", character.get_presence());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("coins", character.get_coins());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("valor", character.get_valor());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("experience", character.get_experience());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("x", character.get_x());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("y", character.get_y());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    item = json_create_item("z", character.get_z());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, item);
    const ft_map<int32_t, game_skill> &skills = character.get_skills();
    json_item *count = json_create_item("skill_count", static_cast<int32_t>(skills.size()));
    if (!count)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    game_character_append_save_item(item_tail, count);
    ft_size_t skill_index = 0;
    ft_size_t skill_count = skills.size();
    const Pair<int32_t, game_skill> *skills_end = skills.end();
    if (skill_count > 0 && !skills_end)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    const Pair<int32_t, game_skill> *skill_start = skills_end;
    if (skill_count > 0)
        skill_start = skills_end - skill_count;
    while (skill_index < skill_count)
    {
        char key[SKILL_KEY_SIZE];
        char *suffix = game_character_write_skill_prefix(key, static_cast<int32_t>(skill_index));
        item = json_create_item(game_character_set_skill_suffix(key, suffix, SKILL_ID),
            skill_start[skill_index].value.get_id());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        game_character_append_save_item(item_tail, item);
        item = json_create_item(game_character_set_skill_suffix(key, suffix, SKILL_LEVEL),
            skill_start[skill_index].value.get_level());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        game_character_append_save_item(item_tail, item);
        item = json_create_item(game_character_set_skill_suffix(key, suffix, SKILL_COOLDOWN),
            skill_start[skill_index].value.get_cooldown());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        game_character_append_save_item(item_tail, item);
        item = json_create_item(game_character_set_skill_suffix(key, suffix, SKILL_MODIFIER_1),
            skill_start[skill_index].value.get_modifier1());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        game_character_append_save_item(item_tail, item);
        item = json_create_item(game_character_set_skill_suffix(key, suffix, SKILL_MODIFIER_2),
            skill_start[skill_index].value.get_modifier2());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        game_character_append_save_item(item_tail, item);
        item = json_create_item(game_character_set_skill_suffix(key, suffix, SKILL_MODIFIER_3),
            skill_start[skill_index].value.get_modifier3());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        game_character_append_save_item(item_tail, item);
        item = json_create_item(game_character_set_skill_suffix(key, suffix, SKILL_MODIFIER_4),
            skill_start[skill_index].value.get_modifier4());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        game_character_append_save_item(item_tail, item);
        skill_index++;
    }
    return (group);
}

int32_t deserialize_character(game_character &character, json_group *group)
{
    json_item *item = json_find_item(group, "hit_points");
    if (item)
        character.set_hit_points(ft_atoi(item->value));
    item = json_find_item(group, "physical_armor");
    if (item)
        character.set_physical_armor(ft_atoi(item->value));
    item = json_find_item(group, "magic_armor");
    if (item)
        character.set_magic_armor(ft_atoi(item->value));
    item = json_find_item(group, "current_physical_armor");
    if (item)
        character.set_current_physical_armor(ft_atoi(item->value));
    item = json_find_item(group, "current_magic_armor");
    if (item)
        character.set_current_magic_armor(ft_atoi(item->value));
    item = json_find_item(group, "damage_rule");
    if (item)
        character.set_damage_rule(static_cast<uint8_t>(ft_atoi(item->value)));
    item = json_find_item(group, "might");
    if (item)
        character.set_might(ft_atoi(item->value));
    item = json_find_item(group, "agility");
    if (item)
        character.set_agility(ft_atoi(item->value));
    item = json_find_item(group, "endurance");
    if (item)
        character.set_endurance(ft_atoi(item->value));
    item = json_find_item(group, "reason");
    if (item)
        character.set_reason(ft_atoi(item->value));
    item = json_find_item(group, "insigh");
    if (item)
        character.set_insigh(ft_atoi(item->value));
    item = json_find_item(group, "presence");
    if (item)
        character.set_presence(ft_atoi(item->value));
    item = json_find_item(group, "coins");
    if (item)
        character.set_coins(ft_atoi(item->value));
    item = json_find_item(group, "valor");
    if (item)
        character.set_valor(ft_atoi(item->value));
    item = json_find_item(group, "experience");
    if (item)
        character.set_experience(ft_atoi(item->value));
    item = json_find_item(group, "x");
    if (item)
        character.set_x(ft_atoi(item->value));
    item = json_find_item(group, "y");
    if (item)
        character.set_y(ft_atoi(item->value));
    item = json_find_item(group, "z");
    if (item)
        character.set_z(ft_atoi(item->value));
    json_item *count_item = json_find_item(group, "skill_count");
    if (count_item != ft_nullptr)
    {
        const int32_t skill_count = ft_atoi(count_item->value);
        const json_item *next_ordered = ft_nullptr;
        if (skill_count > 0)
            next_ordered = game_character_first_skill_item(group);
        int32_t skill_index = 0;
        while (skill_index < skill_count)
        {
            const json_item *skill_items[SKILL_FIELD_COUNT];
            game_character_read_skill_items(group, next_ordered, skill_index, skill_items);
            ft_size_t field_index = 0;
            while (field_index < SKILL_FIELD_COUNT)
            {
                if (skill_items[field_index] == ft_nullptr)
                    return (FT_ERR_GAME_GENERAL_ERROR);
                ++field_index;
            }
            game_skill skill;
            int32_t add_skill_error = skill.initialize();
            if (add_skill_error != FT_ERR_SUCCESS)
                return (add_skill_error);
            skill.set_id(ft_atoi(skill_items[SKILL_ID]->value));
            skill.set_level(ft_atoi(skill_items[SKILL_LEVEL]->value));
            skill.set_cooldown(ft_atoi(skill_items[SKILL_COOLDOWN]->value));
            skill.set_modifier1(ft_atoi(skill_items[SKILL_MODIFIER_1]->value));
            skill.set_modifier2(ft_atoi(skill_items[SKILL_MODIFIER_2]->value));
            skill.set_modifier3(ft_atoi(skill_items[SKILL_MODIFIER_3]->value));
            skill.set_modifier4(ft_atoi(skill_items[SKILL_MODIFIER_4]->value));
            add_skill_error = character.add_skill(skill);
            if (add_skill_error != FT_ERR_SUCCESS)
                return (add_skill_error);
            ++skill_index;
        }
    }
    return (FT_ERR_SUCCESS);
}
