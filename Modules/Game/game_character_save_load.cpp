#include "game_character.hpp"
#include "../Errno/errno.hpp"
#include "../JSon/json.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Printf/printf.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#include "../Template/shared_ptr.hpp"
#include "game_achievement.hpp"
#include "game_buff.hpp"
#include "game_debuff.hpp"
#include "game_quest.hpp"
#include "game_skill.hpp"
#include "game_upgrade.hpp"

static int32_t game_character_build_skill_key(const ft_string &prefix,
    const char *suffix, ft_string &key) noexcept
{
    int32_t string_error;

    if (key.is_initialised() == FT_FALSE)
        string_error = key.initialize();
    else
        string_error = key.clear();
    if (string_error != FT_ERR_SUCCESS)
        return (string_error);
    key = prefix;
    if (key.get_error() != FT_ERR_SUCCESS)
        return (key.get_error());
    key += suffix;
    if (key.get_error() != FT_ERR_SUCCESS)
        return (key.get_error());
    return (FT_ERR_SUCCESS);
}

json_group *serialize_character(const game_character &character)
{
    json_group *group = json_create_json_group("character");
    if (!group)
    {
        return (ft_nullptr);
    }
    json_item *item = json_create_item("hit_points", character.get_hit_points());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("physical_armor", character.get_physical_armor());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("magic_armor", character.get_magic_armor());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("current_physical_armor", character.get_current_physical_armor());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("current_magic_armor", character.get_current_magic_armor());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("damage_rule",
        static_cast<int32_t>(character.get_damage_rule()));
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("might", character.get_might());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("agility", character.get_agility());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("endurance", character.get_endurance());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("reason", character.get_reason());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("insigh", character.get_insigh());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("presence", character.get_presence());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("coins", character.get_coins());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("valor", character.get_valor());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("experience", character.get_experience());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("x", character.get_x());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("y", character.get_y());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    item = json_create_item("z", character.get_z());
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    const ft_map<int32_t, game_skill> &skills = character.get_skills();
    json_item *count = json_create_item("skill_count", static_cast<int32_t>(skills.size()));
    if (!count)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, count);
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
        char skill_index_buffer[32];
        if (pf_snprintf(skill_index_buffer, sizeof(skill_index_buffer), "%d",
                static_cast<int32_t>(skill_index)) < 0)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        ft_string prefix;
        if (prefix.initialize("skill_") != FT_ERR_SUCCESS)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        prefix += skill_index_buffer;
        ft_string key;
        if (game_character_build_skill_key(prefix, "_id", key) != FT_ERR_SUCCESS)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        item = json_create_item(key.c_str(), skill_start[skill_index].value.get_id());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        json_add_item_to_group(group, item);
        if (game_character_build_skill_key(prefix, "_level", key) != FT_ERR_SUCCESS)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        item = json_create_item(key.c_str(), skill_start[skill_index].value.get_level());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        json_add_item_to_group(group, item);
        if (game_character_build_skill_key(prefix, "_cooldown", key) != FT_ERR_SUCCESS)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        item = json_create_item(key.c_str(), skill_start[skill_index].value.get_cooldown());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        json_add_item_to_group(group, item);
        if (game_character_build_skill_key(prefix, "_mod1", key) != FT_ERR_SUCCESS)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        item = json_create_item(key.c_str(), skill_start[skill_index].value.get_modifier1());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        json_add_item_to_group(group, item);
        if (game_character_build_skill_key(prefix, "_mod2", key) != FT_ERR_SUCCESS)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        item = json_create_item(key.c_str(), skill_start[skill_index].value.get_modifier2());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        json_add_item_to_group(group, item);
        if (game_character_build_skill_key(prefix, "_mod3", key) != FT_ERR_SUCCESS)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        item = json_create_item(key.c_str(), skill_start[skill_index].value.get_modifier3());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        json_add_item_to_group(group, item);
        key = prefix;
        key += "_mod4";
        item = json_create_item(key.c_str(), skill_start[skill_index].value.get_modifier4());
        if (!item)
        {
            json_free_groups(group);
            return (ft_nullptr);
        }
        json_add_item_to_group(group, item);
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
    if (count_item)
    {
        int32_t skill_count = ft_atoi(count_item->value);
        int32_t skill_index = 0;
        while (skill_index < skill_count)
        {
            int32_t add_skill_error;
            char skill_index_buffer[32];
            if (pf_snprintf(skill_index_buffer, sizeof(skill_index_buffer), "%d",
                    skill_index) < 0)
                return (FT_ERR_NO_MEMORY);
            ft_string prefix;
            if (prefix.initialize("skill_") != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            prefix += skill_index_buffer;
            ft_string key;
            if (game_character_build_skill_key(prefix, "_id", key) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            json_item *id_item = json_find_item(group, key.c_str());
            if (game_character_build_skill_key(prefix, "_level", key) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            json_item *level_item = json_find_item(group, key.c_str());
            if (game_character_build_skill_key(prefix, "_cooldown", key) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            json_item *cool_item = json_find_item(group, key.c_str());
            if (game_character_build_skill_key(prefix, "_mod1", key) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            json_item *mod1_item = json_find_item(group, key.c_str());
            if (game_character_build_skill_key(prefix, "_mod2", key) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            json_item *mod2_item = json_find_item(group, key.c_str());
            if (game_character_build_skill_key(prefix, "_mod3", key) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            json_item *mod3_item = json_find_item(group, key.c_str());
            if (game_character_build_skill_key(prefix, "_mod4", key) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            json_item *mod4_item = json_find_item(group, key.c_str());
            if (!id_item || !level_item || !cool_item || !mod1_item || !mod2_item || !mod3_item || !mod4_item)
            {
                return (FT_ERR_GAME_GENERAL_ERROR);
            }
            game_skill skill;
            add_skill_error = skill.initialize();
            if (add_skill_error != FT_ERR_SUCCESS)
                return (add_skill_error);
            skill.set_id(ft_atoi(id_item->value));
            skill.set_level(ft_atoi(level_item->value));
            skill.set_cooldown(ft_atoi(cool_item->value));
            skill.set_modifier1(ft_atoi(mod1_item->value));
            skill.set_modifier2(ft_atoi(mod2_item->value));
            skill.set_modifier3(ft_atoi(mod3_item->value));
            skill.set_modifier4(ft_atoi(mod4_item->value));
            add_skill_error = character.add_skill(skill);
            if (add_skill_error != FT_ERR_SUCCESS)
            {
                return (add_skill_error);
            }
            skill_index++;
        }
    }
    return (FT_ERR_SUCCESS);
}
