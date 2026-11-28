#include "game_character.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"

struct game_character::counter_metric
{
    const char *event_name;
    const char *attribute;
    const char *unit;
};

constexpr game_character::counter_metric game_character::COINS_METRIC = {
    "character.coins_change", "coins", "coins"
};

constexpr game_character::counter_metric game_character::VALOR_METRIC = {
    "character.valor_change", "valor", "valor"
};

constexpr game_character::counter_metric game_character::EXPERIENCE_METRIC = {
    "character.experience_change", "experience", "xp"
};

void game_character::change_counter(int32_t &counter, int64_t delta,
    const counter_metric &metric) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    if (this->_mutex != ft_nullptr)
    {
        const int32_t error = this->lock_internal(&lock_acquired);
        if (error != FT_ERR_SUCCESS)
        {
            this->set_error(error);
            return ;
        }
    }

    int32_t result = FT_ERR_SUCCESS;
    if (delta != 0)
    {
        const int64_t total = static_cast<int64_t>(counter) + delta;
        if (total < INT32_MIN || total > INT32_MAX)
            result = FT_ERR_OUT_OF_RANGE;
        else
        {
            counter = static_cast<int32_t>(total);
            this->emit_game_metric(metric.event_name, metric.attribute, delta, total, metric.unit);
        }
    }
    this->set_error(result);
    if (lock_acquired == FT_TRUE)
        this->unlock_internal(lock_acquired);
    return ;
}

void game_character::add_coins(int32_t coins) noexcept
{
    this->change_counter(this->_coins, coins, COINS_METRIC);
    return ;
}

void game_character::sub_coins(int32_t coins) noexcept
{
    this->change_counter(this->_coins, -static_cast<int64_t>(coins), COINS_METRIC);
    return ;
}

void game_character::add_valor(int32_t valor) noexcept
{
    this->change_counter(this->_valor, valor, VALOR_METRIC);
    return ;
}

void game_character::sub_valor(int32_t valor) noexcept
{
    this->change_counter(this->_valor, -static_cast<int64_t>(valor), VALOR_METRIC);
    return ;
}

void game_character::add_experience(int32_t experience) noexcept
{
    this->change_counter(this->_experience, experience, EXPERIENCE_METRIC);
    return ;
}

void game_character::sub_experience(int32_t experience) noexcept
{
    this->change_counter(this->_experience, -static_cast<int64_t>(experience), EXPERIENCE_METRIC);
    return ;
}

int32_t game_character::add_skill(const game_skill &skill) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_character::add_skill");
    errno_abort_if_uninitialised_or_destroyed(skill._initialised_state,
        "game_character::add_skill source");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));

    game_skill snapshot;
    result = snapshot.initialize(skill);
    if (result == FT_ERR_SUCCESS)
    {
        this->_skills.insert(snapshot._id, snapshot);
        result = this->_skills.get_error();
    }
    this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

void game_character::remove_skill(int32_t id) noexcept
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
    const int32_t result = this->_skills.remove_transactional(id,
        [](game_skill &destination, game_skill &source) noexcept
        {
            destination._mutex = source._mutex;
            source._mutex = ft_nullptr;
            return ;
        },
        [](const game_skill &skill) noexcept
        {
            return (skill._mutex == ft_nullptr);
        });
    this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
}
