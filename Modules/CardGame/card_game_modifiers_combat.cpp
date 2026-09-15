#include "card_game_internal.hpp"

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
    int32_t stats_error;

    if (this->_initialised_state != 2U || instance == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || index >= this->_board_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    *instance = this->_instances[player_id][index];
    stats_error = this->get_effective_instance_stats(player_id, index,
        &instance->attack, &instance->health);
    if (stats_error != FT_ERR_SUCCESS)
        return (stats_error);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::allocate_modifier_id(uint32_t *modifier_id) noexcept
{
    if (modifier_id == ft_nullptr || this->_next_modifier_id == 0U)
        return (FT_ERR_OUT_OF_RANGE);
    *modifier_id = this->_next_modifier_id;
    this->_next_modifier_id += 1U;
    if (this->_next_modifier_id == 0U)
        this->_next_modifier_id = 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_effective_instance_stats(uint32_t player_id,
    uint32_t instance_index, int32_t *attack, int32_t *health) const noexcept
{
    const card_game_card_definition *definition;
    int64_t calculated_attack;
    int64_t calculated_health;
    uint32_t index;
    uint32_t definition_index;

    if (this->_initialised_state != 2U || attack == ft_nullptr
        || health == ft_nullptr || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_index >= this->_board_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    definition = ft_nullptr;
    definition_index = 0U;
    while (definition_index < this->_card_count)
    {
        if (this->_cards[definition_index].card_id
            == this->_instances[player_id][instance_index].definition_id)
        {
            definition = &this->_cards[definition_index];
            break ;
        }
        definition_index += 1U;
    }
    if (definition == ft_nullptr)
        return (FT_ERR_NOT_FOUND);
    calculated_attack = definition->attack;
    calculated_health = definition->health
        - this->_instances[player_id][instance_index].damage_taken;
    index = 0U;
    while (index < this->_modifier_count)
    {
        const card_game_card_modifier &modifier = this->_modifiers[index];

        if (modifier.target_player_id == player_id
            && modifier.target_instance_index == instance_index)
        {
            calculated_attack += modifier.attack_delta;
            calculated_health += modifier.health_delta;
        }
        index += 1U;
    }
    if (calculated_attack > static_cast<int64_t>(INT32_MAX))
        calculated_attack = INT32_MAX;
    if (calculated_attack < static_cast<int64_t>(INT32_MIN))
        calculated_attack = INT32_MIN;
    if (calculated_health > static_cast<int64_t>(INT32_MAX))
        calculated_health = INT32_MAX;
    if (calculated_health < static_cast<int64_t>(INT32_MIN))
        calculated_health = INT32_MIN;
    *attack = static_cast<int32_t>(calculated_attack);
    *health = static_cast<int32_t>(calculated_health);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::add_card_modifier(uint32_t player_id,
    uint32_t instance_index, int32_t attack_delta, int32_t health_delta,
    card_game_modifier_duration duration, uint32_t source_effect_id,
    uint32_t *modifier_id) noexcept
{
    card_game_card_modifier *modifier;
    int32_t allocation_error;

    if (this->_initialised_state != 2U || modifier_id == ft_nullptr
        || player_id >= this->_player_count
        || instance_index >= this->_board_count[player_id]
        || (duration != CARD_GAME_MODIFIER_PERMANENT
            && duration != CARD_GAME_MODIFIER_UNTIL_END_TURN))
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_modifier_count >= FT_CARD_GAME_MAX_MODIFIERS)
        return (FT_ERR_FULL);
    modifier = &this->_modifiers[this->_modifier_count];
    allocation_error = this->allocate_modifier_id(modifier_id);
    if (allocation_error != FT_ERR_SUCCESS)
        return (allocation_error);
    modifier->modifier_id = *modifier_id;
    modifier->source_effect_id = source_effect_id;
    modifier->target_player_id = player_id;
    modifier->target_instance_index = instance_index;
    modifier->attack_delta = attack_delta;
    modifier->health_delta = health_delta;
    modifier->duration = duration;
    modifier->created_turn = this->_turn_number;
    modifier->created_phase_id = this->_current_phase_id;
    this->_modifier_count += 1U;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::remove_card_modifier(uint32_t modifier_id) noexcept
{
    uint32_t index;
    uint32_t move_index;

    if (this->_initialised_state != 2U || modifier_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_modifier_count
        && this->_modifiers[index].modifier_id != modifier_id)
        index += 1U;
    if (index >= this->_modifier_count)
        return (FT_ERR_NOT_FOUND);
    move_index = index + 1U;
    while (move_index < this->_modifier_count)
    {
        this->_modifiers[move_index - 1U] = this->_modifiers[move_index];
        move_index += 1U;
    }
    this->_modifier_count -= 1U;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_card_modifier(uint32_t modifier_id,
    card_game_card_modifier *modifier) const noexcept
{
    uint32_t index;

    if (this->_initialised_state != 2U || modifier == ft_nullptr
        || modifier_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_modifier_count)
    {
        if (this->_modifiers[index].modifier_id == modifier_id)
        {
            *modifier = this->_modifiers[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_engine::expire_turn_modifiers() noexcept
{
    uint32_t index;

    index = 0U;
    while (index < this->_modifier_count)
    {
        if (this->_modifiers[index].duration
            == CARD_GAME_MODIFIER_UNTIL_END_TURN)
        {
            if (this->remove_card_modifier(this->_modifiers[index].modifier_id)
                != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
        }
        else
            index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::remove_board_instance(uint32_t player_id,
    uint32_t instance_index) noexcept
{
    uint32_t move_index;

    if (player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_index >= this->_board_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    move_index = instance_index + 1U;
    while (move_index < this->_board_count[player_id])
    {
        this->_board[player_id][move_index - 1U] =
            this->_board[player_id][move_index];
        this->_instances[player_id][move_index - 1U] =
            this->_instances[player_id][move_index];
        move_index += 1U;
    }
    this->_board_count[player_id] -= 1U;
    this->_instances[player_id][this->_board_count[player_id]].on_board =
        FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::resolve_combat(uint32_t attacking_player,
    uint32_t attacker_index, uint32_t defending_player,
    uint32_t defender_index, card_game_combat_mode mode) noexcept
{
    card_game_snapshot before_state;
    int32_t attacker_attack;
    int32_t attacker_health;
    int32_t defender_attack;
    int32_t defender_health;
    int64_t attacker_damage;
    int64_t defender_damage;
    int32_t error_code;
    uint32_t adjusted_attacker_index;

    if (this->_initialised_state != 2U
        || (mode != CARD_GAME_COMBAT_ORDERED
            && mode != CARD_GAME_COMBAT_SIMULTANEOUS)
        || attacking_player >= this->_player_count
        || defending_player >= this->_player_count
        || attacking_player == defending_player
        || attacker_index >= this->_board_count[attacking_player]
        || defender_index >= this->_board_count[defending_player]
        || this->_instances[attacking_player][attacker_index].on_board == FT_FALSE
        || this->_instances[defending_player][defender_index].on_board
            == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->get_snapshot(&before_state) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    error_code = this->get_effective_instance_stats(attacking_player,
        attacker_index, &attacker_attack, &attacker_health);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->get_effective_instance_stats(defending_player,
        defender_index, &defender_attack, &defender_health);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    attacker_damage = attacker_attack;
    defender_damage = defender_attack;
    if (attacker_damage < 0)
        attacker_damage = 0;
    if (defender_damage < 0)
        defender_damage = 0;
    if (attacker_damage > static_cast<int64_t>(INT32_MAX)
        - this->_instances[defending_player][defender_index].damage_taken)
        this->_instances[defending_player][defender_index].damage_taken =
            INT32_MAX;
    else
        this->_instances[defending_player][defender_index].damage_taken +=
            static_cast<int32_t>(attacker_damage);
    if (mode == CARD_GAME_COMBAT_SIMULTANEOUS
        || attacker_damage < static_cast<int64_t>(defender_health))
    {
        if (defender_damage > static_cast<int64_t>(INT32_MAX)
            - this->_instances[attacking_player][attacker_index].damage_taken)
            this->_instances[attacking_player][attacker_index].damage_taken =
                INT32_MAX;
        else
            this->_instances[attacking_player][attacker_index]
                .damage_taken += static_cast<int32_t>(defender_damage);
    }
    if (this->get_effective_instance_stats(defending_player, defender_index,
            &defender_attack, &defender_health) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (this->get_effective_instance_stats(attacking_player, attacker_index,
            &attacker_attack, &attacker_health) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (defender_health <= 0)
    {
        if (this->remove_board_instance(defending_player, defender_index)
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
    }
    if (attacker_health <= 0)
    {
        adjusted_attacker_index = attacker_index;
        if (defender_health <= 0 && attacking_player == defending_player
            && defender_index < attacker_index)
            adjusted_attacker_index -= 1U;
        if (this->remove_board_instance(attacking_player,
                adjusted_attacker_index) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

