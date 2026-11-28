#include "card_game_internal.hpp"
#include <algorithm>

static int32_t card_game_clamp_stat(int64_t value) noexcept
{
    return (static_cast<int32_t>(std::clamp(value,
        static_cast<int64_t>(INT32_MIN), static_cast<int64_t>(INT32_MAX))));
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
    uint32_t candidate;
    uint32_t first_candidate;
    uint32_t index;
    ft_bool exhausted;

    if (modifier_id == ft_nullptr || this->_next_modifier_id == 0U)
        return (FT_ERR_OUT_OF_RANGE);
    candidate = this->_next_modifier_id;
    if (candidate == UINT32_MAX)
        candidate = 1U;
    first_candidate = candidate;
    exhausted = FT_FALSE;
    while (exhausted == FT_FALSE)
    {
        index = 0U;
        while (index < this->_modifier_count
            && this->_modifiers[index].modifier_id != candidate)
            index += 1U;
        if (index == this->_modifier_count)
        {
            *modifier_id = candidate;
            this->_next_modifier_id = candidate + 1U;
            if (this->_next_modifier_id == 0U
                || this->_next_modifier_id == UINT32_MAX)
                this->_next_modifier_id = 1U;
            return (FT_ERR_SUCCESS);
        }
        candidate += 1U;
        if (candidate == 0U || candidate == UINT32_MAX)
            candidate = 1U;
        if (candidate == first_candidate)
            exhausted = FT_TRUE;
    }
    return (FT_ERR_FULL);
}

int32_t card_game_engine::get_effective_instance_stats(uint32_t player_id,
    uint32_t instance_index, int32_t *attack, int32_t *health) const noexcept
{
    if (this->_initialised_state != 2U || attack == ft_nullptr
        || health == ft_nullptr || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_index >= this->_board_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    int64_t calculated_attack;
    int64_t calculated_health;
    const int32_t result = this->get_unclamped_instance_stats(player_id,
        instance_index, &calculated_attack, &calculated_health);
    if (result != FT_ERR_SUCCESS)
        return (result);
    *attack = card_game_clamp_stat(calculated_attack);
    *health = card_game_clamp_stat(calculated_health);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_unclamped_instance_stats(uint32_t player_id,
    uint32_t instance_index, int64_t *attack, int64_t *health) const noexcept
{
    const card_game_card_instance &instance = this->_instances[player_id][instance_index];
    const uint32_t definition_index = this->find_card_index(instance.definition_id);
    if (definition_index == UINT32_MAX)
        return (FT_ERR_NOT_FOUND);
    const card_game_card_definition &definition = this->_cards[definition_index];
    int64_t calculated_attack = definition.attack;
    int64_t calculated_health = static_cast<int64_t>(definition.health) - instance.damage_taken;
    uint32_t index = 0U;
    while (index < this->_modifier_count)
    {
        const card_game_card_modifier &modifier = this->_modifiers[index];

        if (modifier.target_player_id == player_id
            && ((modifier.target_instance_id != 0U
                && modifier.target_instance_id == instance.instance_id)
                || (modifier.target_instance_id == 0U
                    && modifier.target_instance_index == instance_index)))
        {
            calculated_attack += modifier.attack_delta;
            calculated_health += modifier.health_delta;
        }
        index += 1U;
    }
    *attack = calculated_attack;
    *health = calculated_health;
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
    modifier->target_instance_id =
        this->_instances[player_id][instance_index].instance_id;
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

    if (this->_initialised_state != 2U || modifier_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_modifier_count
        && this->_modifiers[index].modifier_id != modifier_id)
        index += 1U;
    if (index >= this->_modifier_count)
        return (FT_ERR_NOT_FOUND);
    this->_modifier_count -= 1U;
    ft_memmove(this->_modifiers + index, this->_modifiers + index + 1U,
        (this->_modifier_count - index) * sizeof(*this->_modifiers));
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

void card_game_engine::expire_turn_modifiers() noexcept
{
    uint32_t index = 0U;
    uint32_t surviving_count = 0U;
    while (index < this->_modifier_count)
    {
        if (this->_modifiers[index].duration != CARD_GAME_MODIFIER_UNTIL_END_TURN)
        {
            if (surviving_count != index)
                this->_modifiers[surviving_count] = this->_modifiers[index];
            surviving_count += 1U;
        }
        index += 1U;
    }
    this->_state_sequence += this->_modifier_count - surviving_count;
    this->_modifier_count = surviving_count;
    return ;
}

int32_t card_game_engine::remove_board_instance(uint32_t player_id,
    uint32_t instance_index) noexcept
{
    uint32_t modifier_index;
    uint32_t removed_instance_id;

    if (player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_index >= this->_board_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    removed_instance_id = this->_instances[player_id][instance_index].instance_id;
    uint32_t *board = this->_board[player_id];
    uint32_t board_slot = instance_index;
    if (board[board_slot] != instance_index)
    {
        const uint32_t *end = board + this->_board_count[player_id];
        const uint32_t *match = std::find(static_cast<const uint32_t *>(board),
            end, instance_index);
        if (match != end)
            board_slot = static_cast<uint32_t>(match - board);
    }
    const uint32_t remaining_count = this->_board_count[player_id] - 1U;
    this->_board_count[player_id] = remaining_count;
    const uint32_t moved_count = remaining_count - instance_index;
    ft_memmove(board + board_slot, board + board_slot + 1U,
        (remaining_count - board_slot) * sizeof(*board));
    /* Board slots refer to positions in the compacted instance array. */
    uint32_t board_index = 0U;
    while (board_index < remaining_count)
    {
        if (board[board_index] > instance_index
            || board[board_index] == remaining_count)
            board[board_index] -= 1U;
        board_index += 1U;
    }
    ft_memmove(this->_instances[player_id] + instance_index,
        this->_instances[player_id] + instance_index + 1U,
        moved_count * sizeof(this->_instances[player_id][0U]));
    this->_instances[player_id][remaining_count].on_board = FT_FALSE;
    modifier_index = 0U;
    uint32_t surviving_count = 0U;
    while (modifier_index < this->_modifier_count)
    {
        card_game_card_modifier &modifier = this->_modifiers[modifier_index];
        if (modifier.target_player_id == player_id)
        {
            if ((modifier.target_instance_id != 0U
                    && modifier.target_instance_id == removed_instance_id)
                || (modifier.target_instance_id == 0U
                    && modifier.target_instance_index == instance_index))
            {
                modifier_index += 1U;
                continue ;
            }
            if (modifier.target_instance_id == 0U
                && modifier.target_instance_index > instance_index)
                modifier.target_instance_index -= 1U;
        }
        if (surviving_count != modifier_index)
            this->_modifiers[surviving_count] = modifier;
        surviving_count += 1U;
        modifier_index += 1U;
    }
    this->_state_sequence += this->_modifier_count - surviving_count;
    this->_modifier_count = surviving_count;
    return (FT_ERR_SUCCESS);
}

static int64_t card_game_apply_combat_damage(card_game_card_instance &instance,
    int64_t damage) noexcept
{
    const int64_t previous_damage = instance.damage_taken;
    instance.damage_taken = card_game_clamp_stat(previous_damage + damage);
    return (static_cast<int64_t>(instance.damage_taken) - previous_damage);
}

int32_t card_game_engine::resolve_combat(uint32_t attacking_player,
    uint32_t attacker_index, uint32_t defending_player,
    uint32_t defender_index, card_game_combat_mode mode) noexcept
{
    int64_t attacker_attack;
    int64_t attacker_health;
    int64_t defender_attack;
    int64_t defender_health;
    int32_t error_code;

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
    error_code = this->get_unclamped_instance_stats(attacking_player,
        attacker_index, &attacker_attack, &attacker_health);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->get_unclamped_instance_stats(defending_player,
        defender_index, &defender_attack, &defender_health);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const int64_t attacker_damage = std::clamp(attacker_attack,
        INT64_C(0), static_cast<int64_t>(INT32_MAX));
    const int32_t initial_defender_health = card_game_clamp_stat(defender_health);
    defender_health -= card_game_apply_combat_damage(
        this->_instances[defending_player][defender_index], attacker_damage);
    if (mode == CARD_GAME_COMBAT_SIMULTANEOUS
        || attacker_damage < initial_defender_health)
        attacker_health -= card_game_apply_combat_damage(
            this->_instances[attacking_player][attacker_index],
            std::clamp(defender_attack, INT64_C(0), static_cast<int64_t>(INT32_MAX)));
    if (defender_health <= 0)
    {
        if (this->remove_board_instance(defending_player, defender_index)
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
    }
    if (attacker_health <= 0)
    {
        if (this->remove_board_instance(attacking_player,
                attacker_index) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}
