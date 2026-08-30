#include "card_game.hpp"

#include "../Basic/class_nullptr.hpp"

card_game_engine::card_game_engine() noexcept
    : _initialised_state(0U), _rules(), _cards(), _effects(), _card_count(0U),
      _effect_count(0U), _board(), _board_count(), _health(), _mana(),
      _turn_number(0U), _active_player(0U), _player_count(0U)
{
    return ;
}

card_game_engine::~card_game_engine() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_engine::initialize(const card_game_rules &rules) noexcept
{
    if (this->_initialised_state == 2U)
        return (FT_ERR_ALREADY_INITIALISED);
    if (rules.max_board_spaces == 0U || rules.max_board_spaces
        > FT_CARD_GAME_MAX_CARDS || rules.max_hand_size == 0U
        || rules.max_turns == 0U || rules.max_mana == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_rules = rules;
    this->_card_count = 0U;
    this->_effect_count = 0U;
    this->_turn_number = 0U;
    this->_active_player = 0U;
    this->_initialised_state = 2U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::destroy() noexcept
{
    if (this->_initialised_state != 2U)
        return (FT_ERR_SUCCESS);
    this->_initialised_state = 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::move(card_game_engine &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != 2U)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_rules = other._rules;
    ft_memcpy(this->_cards, other._cards, sizeof(this->_cards));
    ft_memcpy(this->_effects, other._effects, sizeof(this->_effects));
    ft_memcpy(this->_board, other._board, sizeof(this->_board));
    ft_memcpy(this->_instances, other._instances, sizeof(this->_instances));
    ft_memcpy(this->_board_count, other._board_count, sizeof(this->_board_count));
    ft_memcpy(this->_health, other._health, sizeof(this->_health));
    ft_memcpy(this->_mana, other._mana, sizeof(this->_mana));
    this->_card_count = other._card_count;
    this->_effect_count = other._effect_count;
    this->_turn_number = other._turn_number;
    this->_active_player = other._active_player;
    this->_player_count = other._player_count;
    this->_initialised_state = 2U;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::find_card(uint32_t card_id,
    card_game_card_definition **definition) noexcept
{
    uint32_t index;

    if (definition == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_card_count)
    {
        if (this->_cards[index].card_id == card_id)
        {
            *definition = &this->_cards[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_engine::register_card(
    const card_game_card_definition &definition) noexcept
{
    card_game_card_definition *existing;

    if (this->_initialised_state != 2U || definition.card_id == 0U
        || definition.effect_id >= this->_effect_count
        || this->_card_count >= FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_card(definition.card_id, &existing) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    this->_cards[this->_card_count] = definition;
    this->_card_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::register_effect(card_game_effect_function effect,
    uint32_t *effect_id) noexcept
{
    if (this->_initialised_state != 2U || effect == ft_nullptr
        || effect_id == ft_nullptr || this->_effect_count
            >= FT_CARD_GAME_MAX_EFFECTS)
        return (FT_ERR_INVALID_ARGUMENT);
    *effect_id = this->_effect_count;
    this->_effects[this->_effect_count] = effect;
    this->_effect_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::start_match(uint32_t player_count) noexcept
{
    uint32_t index;

    if (this->_initialised_state != 2U || player_count == 0U
        || player_count > FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < player_count)
    {
        this->_board_count[index] = 0U;
        this->_health[index] = this->_rules.starting_health;
        this->_mana[index] = this->_rules.starting_mana;
        index += 1U;
    }
    this->_turn_number = 1U;
    this->_active_player = 0U;
    this->_player_count = player_count;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::set_player_mana(uint32_t player_id,
    uint32_t mana) noexcept
{
    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || mana > this->_rules.max_mana)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_mana[player_id] = mana;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::modify_player_health(uint32_t player_id,
    int32_t delta) noexcept
{
    int64_t health;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    health = static_cast<int64_t>(this->_health[player_id]) + delta;
    if (health < 0)
        health = 0;
    if (health > static_cast<int64_t>(UINT32_MAX))
        health = static_cast<int64_t>(UINT32_MAX);
    this->_health[player_id] = static_cast<uint32_t>(health);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::play_card(uint32_t player_id, uint32_t card_id,
    uint32_t target_instance, void *context) noexcept
{
    card_game_card_definition *definition;
    uint32_t instance_index;

    if (this->_initialised_state != 2U || player_id != this->_active_player)
        return (FT_ERR_PERMISSION_DENIED);
    if (player_id >= FT_CARD_GAME_MAX_PLAYERS
        || this->_board_count[player_id] >= this->_rules.max_board_spaces)
        return (FT_ERR_FULL);
    if (this->find_card(card_id, &definition) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    if (this->_mana[player_id] < definition->cost)
        return (FT_ERR_OUT_OF_RANGE);
    this->_mana[player_id] -= definition->cost;
    instance_index = this->_board_count[player_id];
    this->_board[player_id][instance_index] = instance_index;
    this->_instances[player_id][instance_index].definition_id = card_id;
    this->_instances[player_id][instance_index].owner_id = player_id;
    this->_instances[player_id][instance_index].attack = definition->attack;
    this->_instances[player_id][instance_index].health = definition->health;
    this->_instances[player_id][instance_index].on_board = FT_TRUE;
    this->_board_count[player_id] += 1U;
    if (definition->effect_id < this->_effect_count
        && this->_effects[definition->effect_id] != ft_nullptr)
        return (this->_effects[definition->effect_id](*this, instance_index,
            target_instance, context));
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::end_turn() noexcept
{
    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    this->_active_player = (this->_active_player + 1U)
        % this->_player_count;
    this->_turn_number += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_player_health(uint32_t player_id,
    uint32_t *health) const noexcept
{
    if (this->_initialised_state != 2U || health == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *health = this->_health[player_id];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_board_count(uint32_t player_id,
    uint32_t *count) const noexcept
{
    if (this->_initialised_state != 2U || count == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_board_count[player_id];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_turn(uint32_t *turn_number,
    uint32_t *active_player) const noexcept
{
    if (this->_initialised_state != 2U || turn_number == ft_nullptr
        || active_player == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *turn_number = this->_turn_number;
    *active_player = this->_active_player;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_instance(uint32_t player_id, uint32_t index,
    card_game_card_instance *instance) const noexcept
{
    if (this->_initialised_state != 2U || instance == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || index >= this->_board_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    *instance = this->_instances[player_id][index];
    return (FT_ERR_SUCCESS);
}
