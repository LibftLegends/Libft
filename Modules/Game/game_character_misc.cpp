#include "game_character.hpp"
#include "../Errno/errno.hpp"
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

void game_character::restore_physical_armor() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->restore_physical_armor_internal();
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_character::restore_magic_armor() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->restore_magic_armor_internal();
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_character::restore_armor() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->restore_physical_armor_internal();
    this->restore_magic_armor_internal();
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_character::take_damage(int64_t damage, uint8_t type) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    if (this->_damage_rule == FT_DAMAGE_RULE_FLAT)
        this->take_damage_flat_internal(damage, type);
    else if (this->_damage_rule == FT_DAMAGE_RULE_SCALED)
        this->take_damage_scaled_internal(damage, type);
    else if (this->_damage_rule == FT_DAMAGE_RULE_BUFFER)
        this->take_damage_buffer_internal(damage, type);
    else if (this->_damage_rule == FT_DAMAGE_RULE_MAGIC_SHIELD)
        this->take_damage_magic_shield_internal(damage, type);
    else
        this->take_damage_flat_internal(damage, type);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_character::take_damage_flat(int64_t damage, uint8_t type) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->take_damage_flat_internal(damage, type);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_character::take_damage_scaled(int64_t damage, uint8_t type) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->take_damage_scaled_internal(damage, type);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_character::take_damage_buffer(int64_t damage, uint8_t type) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->take_damage_buffer_internal(damage, type);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_character::take_damage_magic_shield(int64_t damage, uint8_t type) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->take_damage_magic_shield_internal(damage, type);
    this->unlock_internal(lock_acquired);
    return ;
}

static int64_t subtract_damage_armor(int64_t damage, int32_t armor) noexcept
{
    if (armor < 0 && damage > INT64_MAX + static_cast<int64_t>(armor))
        return (INT64_MAX);
    return (damage - armor);
}

static int64_t scale_damage_for_armor(int64_t damage, double multiplier) noexcept
{
    if (multiplier >= 1.0)
        return (damage);
    const double scaled = static_cast<double>(damage) * multiplier;
    if (scaled >= static_cast<double>(INT64_MAX))
        return (INT64_MAX);
    if (!(scaled > 0.0))
        return (0);
    return (static_cast<int64_t>(scaled));
}

static int64_t absorb_damage_with_armor(int64_t damage, int32_t &armor) noexcept
{
    if (armor <= 0)
        return (damage);
    if (damage <= armor)
    {
        armor -= static_cast<int32_t>(damage);
        return (0);
    }
    damage -= armor;
    armor = 0;
    return (damage);
}

void game_character::apply_hit_point_damage(int64_t damage) noexcept
{
    const int32_t previous_hit_points = this->_hit_points;
    if (previous_hit_points <= 0 || damage >= previous_hit_points)
        this->_hit_points = 0;
    else if (damage > 0)
        this->_hit_points -= static_cast<int32_t>(damage);
    int64_t damage_applied = static_cast<int64_t>(previous_hit_points) - this->_hit_points;
    if (damage_applied < 0)
        damage_applied = 0;
    this->emit_game_metric("character.damage_taken", "hit_points",
        damage_applied, this->_hit_points, "hp");
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_character::take_damage_flat_internal(int64_t damage, uint8_t type) noexcept
{
    const int32_t skill_error = this->apply_skill_modifiers_internal(damage);
    if (skill_error != FT_ERR_SUCCESS)
    {
        this->set_error(skill_error);
        return ;
    }
    if (type == FT_DAMAGE_PHYSICAL)
        damage = subtract_damage_armor(damage, this->_physical_armor);
    else if (type == FT_DAMAGE_MAGICAL)
        damage = subtract_damage_armor(damage, this->_magic_armor);
    this->apply_hit_point_damage(damage);
    return ;
}

void game_character::take_damage_scaled_internal(int64_t damage, uint8_t type) noexcept
{
    const int32_t skill_error = this->apply_skill_modifiers_internal(damage);
    if (skill_error != FT_ERR_SUCCESS)
    {
        this->set_error(skill_error);
        return ;
    }
    if (type == FT_DAMAGE_PHYSICAL)
        damage = scale_damage_for_armor(damage, this->_physical_damage_multiplier);
    else if (type == FT_DAMAGE_MAGICAL)
        damage = scale_damage_for_armor(damage, this->_magic_damage_multiplier);
    this->apply_hit_point_damage(damage);
    return ;
}

void game_character::take_damage_buffer_internal(int64_t damage, uint8_t type) noexcept
{
    const int32_t skill_error = this->apply_skill_modifiers_internal(damage);
    if (skill_error != FT_ERR_SUCCESS)
    {
        this->set_error(skill_error);
        return ;
    }
    if (type == FT_DAMAGE_PHYSICAL)
        damage = absorb_damage_with_armor(damage, this->_current_physical_armor);
    else if (type == FT_DAMAGE_MAGICAL)
        damage = absorb_damage_with_armor(damage, this->_current_magic_armor);
    this->apply_hit_point_damage(damage);
    return ;
}

void game_character::take_damage_magic_shield_internal(int64_t damage, uint8_t type) noexcept
{
    const int32_t skill_error = this->apply_skill_modifiers_internal(damage);
    if (skill_error != FT_ERR_SUCCESS)
    {
        this->set_error(skill_error);
        return ;
    }
    damage = absorb_damage_with_armor(damage, this->_current_magic_armor);
    if (type == FT_DAMAGE_PHYSICAL)
    {
#if FT_PHYSICAL_DAMAGE_REDUCTION == FT_DAMAGE_RULE_FLAT
        damage = subtract_damage_armor(damage, this->_physical_armor);
#elif FT_PHYSICAL_DAMAGE_REDUCTION == FT_DAMAGE_RULE_SCALED
        damage = scale_damage_for_armor(damage, this->_physical_damage_multiplier);
#endif
    }
    else if (type == FT_DAMAGE_MAGICAL)
    {
#if FT_MAGIC_DAMAGE_REDUCTION == FT_DAMAGE_RULE_FLAT
        damage = subtract_damage_armor(damage, this->_magic_armor);
#elif FT_MAGIC_DAMAGE_REDUCTION == FT_DAMAGE_RULE_SCALED
        damage = scale_damage_for_armor(damage, this->_magic_damage_multiplier);
#endif
    }
    this->apply_hit_point_damage(damage);
    return ;
}

void game_character::move(int32_t dx, int32_t dy, int32_t dz) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_x += dx;
    this->_y += dy;
    this->_z += dz;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

static int64_t add_skill_damage_bonus(int64_t damage, int32_t bonus) noexcept
{
    if (bonus > 0 && damage > INT64_MAX - bonus)
        return (INT64_MAX);
    if (bonus < 0 && damage < INT64_MIN - bonus)
        return (INT64_MIN);
    return (damage + bonus);
}

static int64_t scale_skill_damage(int64_t damage, int32_t percent) noexcept
{
    if (damage >= INT32_MIN && damage <= INT32_MAX)
        return (damage + damage * percent / 100);

    const int64_t factor = static_cast<int64_t>(percent) + 100;
    if (factor == 0)
        return (0);
    const ft_bool negative = (damage < 0) != (factor < 0);
    uint64_t magnitude = static_cast<uint64_t>(damage);
    if (damage < 0)
        magnitude = UINT64_C(0) - magnitude;
    uint64_t factor_magnitude = static_cast<uint64_t>(factor);
    if (factor < 0)
        factor_magnitude = static_cast<uint64_t>(-factor);

    // A reduction below 100 percent rounds the remaining magnitude up because
    // the removed amount was truncated first.
    uint64_t remainder_product = (magnitude % 100U) * factor_magnitude;
    if (percent < 0 && factor > 0)
        remainder_product += 99U;
    const uint64_t tail = remainder_product / 100U;
    const uint64_t quotient = magnitude / 100U;
    uint64_t limit = static_cast<uint64_t>(INT64_MAX);
    if (negative)
        ++limit;
    if (quotient > (limit - tail) / factor_magnitude)
    {
        if (negative)
            return (INT64_MIN);
        return (INT64_MAX);
    }
    const uint64_t result = quotient * factor_magnitude + tail;
    if (!negative)
        return (static_cast<int64_t>(result));
    if (result == limit)
        return (INT64_MIN);
    return (-static_cast<int64_t>(result));
}

int64_t game_character::apply_skill_modifiers(int64_t damage) const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->apply_skill_modifiers_internal(damage);
        this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    if (result != FT_ERR_SUCCESS)
        return (0);
    return (damage);
}

int32_t game_character::apply_skill_modifiers_internal(int64_t &damage) const noexcept
{
    const int32_t result = this->_skills.visit_values(
        [&damage](const game_skill &skill) noexcept
        {
            int32_t bonus = 0;
            int32_t percent = 0;
            const int32_t skill_error = skill.capture_damage_modifiers(bonus, percent);
            if (skill_error == FT_ERR_SUCCESS)
            {
                damage = add_skill_damage_bonus(damage, bonus);
                if (percent != 0)
                    damage = scale_skill_damage(damage, percent);
            }
            return (skill_error);
        });
    if (damage < 0)
        damage = 0;
    return (result);
}

void game_character::apply_modifier(const game_item_modifier &mod, int32_t sign) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->apply_modifier_internal(mod, sign);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_character::apply_modifier_internal(const game_item_modifier &mod, int32_t sign) noexcept
{
    int32_t modifier_error;
    int32_t modifier_identifier;
    int32_t modifier_value;

    modifier_error = mod.get_error();
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        return ;
    }
    modifier_identifier = mod.get_id();
    modifier_error = mod.get_error();
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        return ;
    }
    modifier_value = mod.get_value();
    modifier_error = mod.get_error();
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        return ;
    }
    this->apply_modifier_value(modifier_identifier, modifier_value, sign);
    return ;
}

void game_character::apply_modifier_value(int32_t modifier_identifier, int32_t modifier_value,
    int32_t sign) noexcept
{
    if (modifier_identifier > 0 && static_cast<ft_size_t>(modifier_identifier) <= MODIFIER_STAT_COUNT)
    {
        const ft_size_t index = static_cast<ft_size_t>(modifier_identifier - 1);
        const int64_t value = this->*MODIFIER_STAT_FIELDS[index]
            + static_cast<int64_t>(modifier_value) * sign;
        if (value < INT32_MIN || value > INT32_MAX)
        {
            this->set_error(FT_ERR_OUT_OF_RANGE);
            return ;
        }
        this->write_modifier_stat(index, static_cast<int32_t>(value));
    }
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_character::write_modifier_stat(ft_size_t index, int32_t value) noexcept
{
    if (index == 0U)
        this->set_physical_armor_internal(value);
    else if (index == 8U)
        this->set_magic_armor_internal(value);
    else
        this->*MODIFIER_STAT_FIELDS[index] = value;
    return ;
}

void game_character::accumulate_equipment_modifiers(const equipped_modifier_snapshot &modifiers,
    int32_t sign, int64_t (&deltas)[MODIFIER_STAT_COUNT], uint32_t &touched) noexcept
{
    ft_size_t index = 0;
    while (index < 4U)
    {
        const game_item_modifier_value &modifier = modifiers.values[index];
        if (modifier.id > 0 && static_cast<ft_size_t>(modifier.id) <= MODIFIER_STAT_COUNT)
        {
            const ft_size_t field = static_cast<ft_size_t>(modifier.id - 1);
            deltas[field] += static_cast<int64_t>(modifier.value) * sign;
            touched |= UINT32_C(1) << field;
        }
        ++index;
    }
    return ;
}

int32_t game_character::stage_equipment_stats(int64_t (&deltas)[MODIFIER_STAT_COUNT],
    uint32_t touched) const noexcept
{
    ft_size_t index = 0;
    while (touched != 0U)
    {
        if ((touched & 1U) != 0U)
        {
            deltas[index] += this->*MODIFIER_STAT_FIELDS[index];
            if (deltas[index] < INT32_MIN || deltas[index] > INT32_MAX)
                return (FT_ERR_OUT_OF_RANGE);
        }
        touched >>= 1U;
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

void game_character::commit_equipment_stats(const int64_t (&values)[MODIFIER_STAT_COUNT],
    uint32_t touched) noexcept
{
    ft_size_t index = 0;
    while (touched != 0U)
    {
        if ((touched & 1U) != 0U)
            this->write_modifier_stat(index, static_cast<int32_t>(values[index]));
        touched >>= 1U;
        ++index;
    }
    return ;
}

int32_t game_character::read_equipment_snapshot(int32_t slot, ft_sharedptr<game_item> &item,
    equipped_modifier_snapshot &modifiers) const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    result = this->_equipment.read_item(slot, item);
    if (result == FT_ERR_SUCCESS)
        modifiers = this->_equipped_modifiers[slot];
    this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

int32_t game_character::replace_equipment_slot(int32_t slot, const ft_sharedptr<game_item> *item,
    const equipped_modifier_snapshot *saved_modifiers, ft_bool update_stats) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));

    ft_sharedptr<game_item> previous;
    equipped_modifier_snapshot next_modifiers{};
    result = this->_equipment.read_item(slot, previous);
    if (result == FT_ERR_SUCCESS && item != ft_nullptr)
    {
        const game_item *next_item = item->get();
        if (next_item == ft_nullptr)
            result = FT_ERR_INVALID_ARGUMENT;
        else if (saved_modifiers != ft_nullptr)
            next_modifiers = *saved_modifiers;
        else
            result = next_item->capture_modifiers(next_modifiers.values);
    }
    int64_t staged_stats[MODIFIER_STAT_COUNT]{};
    uint32_t touched = 0U;
    if (result == FT_ERR_SUCCESS && update_stats)
    {
        if (previous)
            accumulate_equipment_modifiers(this->_equipped_modifiers[slot], -1, staged_stats, touched);
        if (item != ft_nullptr)
            accumulate_equipment_modifiers(next_modifiers, 1, staged_stats, touched);
        result = this->stage_equipment_stats(staged_stats, touched);
    }
    if (result == FT_ERR_SUCCESS)
    {
        if (item != ft_nullptr)
            result = this->_equipment.equip(slot, *item);
        else
        {
            this->_equipment.unequip(slot);
            result = this->_equipment.get_error();
        }
    }
    if (result == FT_ERR_SUCCESS)
    {
        this->commit_equipment_stats(staged_stats, touched);
        this->_equipped_modifiers[slot] = next_modifiers;
    }
    this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

int32_t game_character::equip_item(int32_t slot, const ft_sharedptr<game_item> &item) noexcept
{
    return (this->replace_equipment_slot(slot, &item, ft_nullptr, FT_TRUE));
}

void game_character::unequip_item(int32_t slot) noexcept
{
    (void)this->replace_equipment_slot(slot, ft_nullptr, ft_nullptr, FT_TRUE);
    return ;
}
