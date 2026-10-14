#include "../PThread/pthread_internal.hpp"
#include "game_character.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
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

int32_t game_character::get_hit_points() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t hit_points;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    hit_points = this->_hit_points;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (hit_points);
}

void game_character::set_hit_points(int32_t hp) noexcept
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
    this->_hit_points = hp;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

ft_bool game_character::is_alive() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool alive;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (FT_FALSE);
    }
    alive = this->_hit_points > 0;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (alive);
}

int32_t game_character::get_physical_armor() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t armor_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    armor_value = this->_physical_armor;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (armor_value);
}

void game_character::set_physical_armor(int32_t armor) noexcept
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
    this->set_physical_armor_internal(armor);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_magic_armor() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t armor_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    armor_value = this->_magic_armor;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (armor_value);
}

void game_character::set_magic_armor(int32_t armor) noexcept
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
    this->set_magic_armor_internal(armor);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_current_physical_armor() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t armor_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    armor_value = this->_current_physical_armor;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (armor_value);
}

void game_character::set_current_physical_armor(int32_t armor) noexcept
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
    this->_current_physical_armor = armor;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_current_magic_armor() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t armor_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    armor_value = this->_current_magic_armor;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (armor_value);
}

void game_character::set_current_magic_armor(int32_t armor) noexcept
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
    this->_current_magic_armor = armor;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_character::set_damage_rule(uint8_t rule) noexcept
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
    this->_damage_rule = rule;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

uint8_t game_character::get_damage_rule() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    uint8_t rule_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (FT_DAMAGE_RULE_FLAT);
    }
    rule_value = this->_damage_rule;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (rule_value);
}

int32_t game_character::get_might() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t attribute;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    attribute = this->_might;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (attribute);
}

void game_character::set_might(int32_t might) noexcept
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
    this->_might = might;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_agility() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t attribute;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    attribute = this->_agility;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (attribute);
}

void game_character::set_agility(int32_t agility) noexcept
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
    this->_agility = agility;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_endurance() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t attribute;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    attribute = this->_endurance;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (attribute);
}

void game_character::set_endurance(int32_t endurance) noexcept
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
    this->_endurance = endurance;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_reason() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t attribute;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    attribute = this->_reason;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (attribute);
}

void game_character::set_reason(int32_t reason) noexcept
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
    this->_reason = reason;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_insigh() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t attribute;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    attribute = this->_insigh;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (attribute);
}

void game_character::set_insigh(int32_t insigh) noexcept
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
    this->_insigh = insigh;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_presence() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t attribute;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    attribute = this->_presence;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (attribute);
}

void game_character::set_presence(int32_t presence) noexcept
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
    this->_presence = presence;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_coins() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t coins_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    coins_value = this->_coins;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (coins_value);
}

void game_character::set_coins(int32_t coins) noexcept
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
    this->_coins = coins;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_valor() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t valor_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    valor_value = this->_valor;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (valor_value);
}

void game_character::set_valor(int32_t valor) noexcept
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
    this->_valor = valor;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_experience() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t experience_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    experience_value = this->_experience;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (experience_value);
}

void game_character::set_experience(int32_t experience) noexcept
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
    this->_experience = experience;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_x() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t coordinate;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    coordinate = this->_x;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (coordinate);
}

void game_character::set_x(int32_t x) noexcept
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
    this->_x = x;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_y() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t coordinate;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    coordinate = this->_y;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (coordinate);
}

void game_character::set_y(int32_t y) noexcept
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
    this->_y = y;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_character::get_z() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t coordinate;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    coordinate = this->_z;
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (coordinate);
}

void game_character::set_z(int32_t z) noexcept
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
    this->_z = z;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

const game_resistance &game_character::get_fire_res() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_fire_res);
    }
    if (this->_fire_res.get_error() != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(this->_fire_res.get_error());
        this->unlock_internal(lock_acquired);
        return (this->_fire_res);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_fire_res);
}

void game_character::set_fire_res(int32_t percent, int32_t flat) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t error_code;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    error_code = this->_fire_res.set_values(percent, flat);
    if (this->handle_component_error(error_code) == FT_TRUE)
    {
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

const game_resistance &game_character::get_frost_res() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_frost_res);
    }
    if (this->_frost_res.get_error() != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(this->_frost_res.get_error());
        this->unlock_internal(lock_acquired);
        return (this->_frost_res);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_frost_res);
}

void game_character::set_frost_res(int32_t percent, int32_t flat) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t error_code;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    error_code = this->_frost_res.set_values(percent, flat);
    if (this->handle_component_error(error_code) == FT_TRUE)
    {
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

const game_resistance &game_character::get_lightning_res() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_lightning_res);
    }
    if (this->_lightning_res.get_error() != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(this->_lightning_res.get_error());
        this->unlock_internal(lock_acquired);
        return (this->_lightning_res);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_lightning_res);
}

void game_character::set_lightning_res(int32_t percent, int32_t flat) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t error_code;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    error_code = this->_lightning_res.set_values(percent, flat);
    if (this->handle_component_error(error_code) == FT_TRUE)
    {
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

const game_resistance &game_character::get_air_res() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_air_res);
    }
    if (this->_air_res.get_error() != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(this->_air_res.get_error());
        this->unlock_internal(lock_acquired);
        return (this->_air_res);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_air_res);
}

void game_character::set_air_res(int32_t percent, int32_t flat) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t error_code;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    error_code = this->_air_res.set_values(percent, flat);
    if (this->handle_component_error(error_code) == FT_TRUE)
    {
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

const game_resistance &game_character::get_earth_res() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_earth_res);
    }
    if (this->_earth_res.get_error() != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(this->_earth_res.get_error());
        this->unlock_internal(lock_acquired);
        return (this->_earth_res);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_earth_res);
}

void game_character::set_earth_res(int32_t percent, int32_t flat) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t error_code;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    error_code = this->_earth_res.set_values(percent, flat);
    if (this->handle_component_error(error_code) == FT_TRUE)
    {
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

const game_resistance &game_character::get_chaos_res() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_chaos_res);
    }
    if (this->_chaos_res.get_error() != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(this->_chaos_res.get_error());
        this->unlock_internal(lock_acquired);
        return (this->_chaos_res);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_chaos_res);
}

void game_character::set_chaos_res(int32_t percent, int32_t flat) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t error_code;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    error_code = this->_chaos_res.set_values(percent, flat);
    if (this->handle_component_error(error_code) == FT_TRUE)
    {
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

const game_resistance &game_character::get_physical_res() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_physical_res);
    }
    if (this->_physical_res.get_error() != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(this->_physical_res.get_error());
        this->unlock_internal(lock_acquired);
        return (this->_physical_res);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_physical_res);
}

void game_character::set_physical_res(int32_t percent, int32_t flat) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t error_code;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    error_code = this->_physical_res.set_values(percent, flat);
    if (this->handle_component_error(error_code) == FT_TRUE)
    {
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

ft_map<int32_t, game_skill> &game_character::get_skills() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_skills);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_skills);
}

const ft_map<int32_t, game_skill> &game_character::get_skills() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_skills);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_skills);
}

game_skill *game_character::get_skill(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    Pair<int32_t, game_skill> *found;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (ft_nullptr);
    }
    found = this->_skills.find(id);
    if (found == this->_skills.end())
    {
        this->set_error(FT_ERR_NOT_FOUND);
        this->unlock_internal(lock_acquired);
        return (ft_nullptr);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (&found->value);
}

const game_skill *game_character::get_skill(int32_t id) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const Pair<int32_t, game_skill> *found;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (ft_nullptr);
    }
    found = this->_skills.find(id);
    if (found == this->_skills.end())
    {
        const_cast<game_character *>(this)->set_error(FT_ERR_NOT_FOUND);
        this->unlock_internal(lock_acquired);
        return (ft_nullptr);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (&found->value);
}

ft_map<int32_t, game_buff> &game_character::get_buffs() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_buffs);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_buffs);
}

const ft_map<int32_t, game_buff> &game_character::get_buffs() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_buffs);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_buffs);
}

ft_map<int32_t, game_debuff> &game_character::get_debuffs() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_debuffs);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_debuffs);
}

const ft_map<int32_t, game_debuff> &game_character::get_debuffs() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_debuffs);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_debuffs);
}

ft_map<int32_t, game_upgrade> &game_character::get_upgrades() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_upgrades);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_upgrades);
}

const ft_map<int32_t, game_upgrade> &game_character::get_upgrades() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_upgrades);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_upgrades);
}

ft_map<int32_t, game_quest> &game_character::get_quests() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_quests);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_quests);
}

const ft_map<int32_t, game_quest> &game_character::get_quests() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_quests);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_quests);
}

ft_map<int32_t, game_achievement> &game_character::get_achievements() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_achievements);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_achievements);
}

const ft_map<int32_t, game_achievement> &game_character::get_achievements() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_achievements);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_achievements);
}

game_reputation &game_character::get_reputation() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_reputation);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_reputation);
}

const game_reputation &game_character::get_reputation() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_reputation);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_reputation);
}

game_inventory &game_character::get_inventory() noexcept
{
    return (this->_inventory);
}

const game_inventory &game_character::get_inventory() const noexcept
{
    return (this->_inventory);
}

game_experience_table &game_character::get_experience_table() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_experience_table);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_experience_table);
}

const game_experience_table &game_character::get_experience_table() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (this->_experience_table);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_experience_table);
}

int32_t game_character::get_level() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t level_value;
    int32_t experience_error;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (0);
    }
    level_value = this->_experience_table.get_level(this->_experience);
    experience_error = this->_experience_table.get_error();
    if (experience_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(experience_error);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (level_value);
}

ft_sharedptr<game_item> *game_character::get_equipped_item(int32_t slot) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t equipment_error;
    ft_sharedptr<game_item> *item;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (ft_nullptr);
    }
    item = this->_equipment.get_item(slot);
    equipment_error = this->_equipment.get_error();
    if (equipment_error != FT_ERR_SUCCESS)
    {
        this->handle_component_error(equipment_error);
        this->unlock_internal(lock_acquired);
        return (ft_nullptr);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (item);
}

ft_sharedptr<game_item> *game_character::get_equipped_item(int32_t slot) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t equipment_error;
    ft_sharedptr<game_item> *item;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->set_error(lock_error);
        return (ft_nullptr);
    }
    item = this->_equipment.get_item(slot);
    equipment_error = this->_equipment.get_error();
    if (equipment_error != FT_ERR_SUCCESS)
    {
        const_cast<game_character *>(this)->handle_component_error(equipment_error);
        this->unlock_internal(lock_acquired);
        return (ft_nullptr);
    }
    const_cast<game_character *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (item);
}

int32_t game_character::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_character::get_error");
    return (game_character::_last_error);
}

const char *game_character::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_character::get_error_str");
    return (ft_strerror(game_character::_last_error));
}

void game_character::set_physical_armor_internal(int32_t armor) noexcept
{
    int32_t index;

    this->_physical_armor = armor;
    this->_current_physical_armor = armor;
    this->_physical_damage_multiplier = 1.0;
    index = 0;
    while (index < armor)
    {
        this->_physical_damage_multiplier = this->_physical_damage_multiplier * FT_ARMOR_POINT_REDUCTION;
        index++;
    }
    return ;
}

void game_character::set_magic_armor_internal(int32_t armor) noexcept
{
    int32_t index;

    this->_magic_armor = armor;
    this->_current_magic_armor = armor;
    this->_magic_damage_multiplier = 1.0;
    index = 0;
    while (index < armor)
    {
        this->_magic_damage_multiplier = this->_magic_damage_multiplier * FT_ARMOR_POINT_REDUCTION;
        index++;
    }
    return ;
}

void game_character::restore_physical_armor_internal() noexcept
{
    this->_current_physical_armor = this->_physical_armor;
    return ;
}

void game_character::restore_magic_armor_internal() noexcept
{
    this->_current_magic_armor = this->_magic_armor;
    return ;
}

int32_t game_character::set_error(int32_t error_code) noexcept
{
    game_character::_last_error = error_code;
    return (error_code);
}

int32_t game_character::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void game_character::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}
