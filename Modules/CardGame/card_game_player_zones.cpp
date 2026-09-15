#include "card_game_internal.hpp"

int32_t card_game_engine::get_player_health(uint32_t player_id,
    uint32_t *health) const noexcept
{
    if (this->_initialised_state != 2U || health == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *health = this->_health[player_id];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_player_mana(uint32_t player_id,
    uint32_t *mana) const noexcept
{
    if (this->_initialised_state != 2U || mana == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *mana = this->_mana[player_id];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_current_phase(uint32_t *phase_id) const noexcept
{
    if (this->_initialised_state != 2U || phase_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_phase_count == 0U)
        return (FT_ERR_NOT_FOUND);
    *phase_id = this->_current_phase_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_phase(uint32_t phase_id,
    card_game_phase_definition *phase) const noexcept
{
    uint32_t index;

    if (this->_initialised_state != 2U || phase == ft_nullptr
        || phase_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_phase_count)
    {
        if (this->_phases[index].phase_id == phase_id)
        {
            *phase = this->_phases[index];
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
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

int32_t card_game_engine::get_deck_count(uint32_t player_id,
    uint32_t *count) const noexcept
{
    if (this->_initialised_state != 2U || count == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_decks[player_id].size();
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_hand_count(uint32_t player_id,
    uint32_t *count) const noexcept
{
    if (this->_initialised_state != 2U || count == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_hand_count[player_id];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::hand_inspect(uint32_t player_id, uint32_t index,
    card_game_deck_card *card) const noexcept
{
    if (this->_initialised_state != 2U || card == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || index >= this->_hand_count[player_id])
        return (FT_ERR_INVALID_ARGUMENT);
    *card = this->_hand[player_id][index];
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::draw_to_hand(uint32_t player_id,
    card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t draw_error;

    if (this->_initialised_state != 2U || card == ft_nullptr
        || player_id >= this->_player_count)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_hand_count[player_id] >= this->_rules.max_hand_size)
        return (FT_ERR_FULL);
    draw_error = this->_decks[player_id].pop_top_entry(&entry);
    if (draw_error != FT_ERR_SUCCESS)
        return (draw_error);
    this->_hand[player_id][this->_hand_count[player_id]].instance_id =
        entry.instance_id;
    this->_hand[player_id][this->_hand_count[player_id]].card_id = entry.card_id;
    *card = this->_hand[player_id][this->_hand_count[player_id]];
    this->_hand_count[player_id] += 1U;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::hand_remove_instance(uint32_t player_id,
    uint32_t instance_id, card_game_deck_card *card) noexcept
{
    uint32_t index;
    uint32_t move_index;

    if (this->_initialised_state != 2U || player_id >= this->_player_count
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_hand_count[player_id]
        && this->_hand[player_id][index].instance_id != instance_id)
        index += 1U;
    if (index >= this->_hand_count[player_id])
        return (FT_ERR_NOT_FOUND);
    if (card != ft_nullptr)
        *card = this->_hand[player_id][index];
    move_index = index + 1U;
    while (move_index < this->_hand_count[player_id])
    {
        this->_hand[player_id][move_index - 1U] =
            this->_hand[player_id][move_index];
        move_index += 1U;
    }
    this->_hand_count[player_id] -= 1U;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::mulligan_hand(uint32_t player_id,
    const uint32_t *instance_ids, uint32_t count,
    uint64_t *random_state) noexcept
{
    card_game_deck_card original_hand[FT_CARD_GAME_MAX_CARDS];
    card_game_zone_entry original_deck[FT_CARD_GAME_MAX_CARDS];
    card_game_deck_card selected[FT_CARD_GAME_MAX_CARDS];
    card_game_deck_card replacement;
    uint32_t original_hand_count;
    uint32_t original_deck_count;
    uint32_t selected_count;
    uint32_t request_index;
    uint32_t hand_index;
    uint32_t deck_index;
    uint64_t original_random_state;
    uint64_t original_state_sequence;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= this->_player_count
        || random_state == ft_nullptr || *random_state == 0U
        || count > this->_hand_count[player_id]
        || (count != 0U && instance_ids == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (count == 0U)
        return (FT_ERR_SUCCESS);
    if (this->_decks[player_id].size() + count > FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_FULL);
    original_hand_count = this->_hand_count[player_id];
    original_deck_count = this->_decks[player_id].size();
    original_random_state = *random_state;
    original_state_sequence = this->_state_sequence;
    ft_memcpy(original_hand, this->_hand[player_id],
        sizeof(original_hand));
    deck_index = 0U;
    while (deck_index < original_deck_count)
    {
        result = this->_decks[player_id].get_entry(deck_index,
            &original_deck[deck_index]);
        if (result != FT_ERR_SUCCESS)
            return (result);
        deck_index += 1U;
    }
    selected_count = 0U;
    request_index = 0U;
    while (request_index < count)
    {
        hand_index = 0U;
        while (hand_index < original_hand_count
            && original_hand[hand_index].instance_id
                != instance_ids[request_index])
            hand_index += 1U;
        if (hand_index >= original_hand_count
            || instance_ids[request_index] == 0U)
            return (FT_ERR_NOT_FOUND);
        deck_index = 0U;
        while (deck_index < selected_count
            && selected[deck_index].instance_id != instance_ids[request_index]
            )
            deck_index += 1U;
        if (deck_index < selected_count)
            return (FT_ERR_ALREADY_EXISTS);
        selected[selected_count] = original_hand[hand_index];
        selected_count += 1U;
        request_index += 1U;
    }
    request_index = 0U;
    while (request_index < selected_count)
    {
        result = this->hand_remove_instance(player_id,
            selected[request_index].instance_id, ft_nullptr);
        if (result != FT_ERR_SUCCESS)
            break ;
        request_index += 1U;
    }
    if (result == FT_ERR_SUCCESS)
    {
        request_index = 0U;
        while (request_index < selected_count)
        {
            card_game_zone_entry entry;

            entry.instance_id = selected[request_index].instance_id;
            entry.card_id = selected[request_index].card_id;
            result = this->_decks[player_id].push_bottom_entry(entry);
            if (result != FT_ERR_SUCCESS)
                break ;
            request_index += 1U;
        }
    }
    if (result == FT_ERR_SUCCESS)
        result = this->shuffle_deck(player_id, random_state);
    if (result == FT_ERR_SUCCESS)
    {
        request_index = 0U;
        while (request_index < selected_count)
        {
            result = this->draw_to_hand(player_id, &replacement);
            if (result != FT_ERR_SUCCESS)
                break ;
            request_index += 1U;
        }
    }
    if (result == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    this->_hand_count[player_id] = original_hand_count;
    ft_memcpy(this->_hand[player_id], original_hand, sizeof(original_hand));
    (void)this->_decks[player_id].clear();
    deck_index = 0U;
    while (deck_index < original_deck_count)
    {
        (void)this->_decks[player_id].push_bottom_entry(original_deck[deck_index]);
        deck_index += 1U;
    }
    *random_state = original_random_state;
    this->_state_sequence = original_state_sequence;
    return (result);
}

int32_t card_game_engine::play_card_from_hand(uint32_t player_id,
    uint32_t instance_id, uint32_t target_instance, void *context) noexcept
{
    card_game_deck_card card;
    card_game_snapshot before_state;
    uint32_t hand_index;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= this->_player_count
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = FT_ERR_NOT_FOUND;
    hand_index = 0U;
    while (hand_index < this->_hand_count[player_id])
    {
        if (this->_hand[player_id][hand_index].instance_id == instance_id)
        {
            card = this->_hand[player_id][hand_index];
            result = FT_ERR_SUCCESS;
            break ;
        }
        hand_index += 1U;
    }
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->get_snapshot(&before_state);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->play_card(player_id, card.card_id, target_instance, context);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->hand_remove_instance(player_id, instance_id, ft_nullptr);
    if (result != FT_ERR_SUCCESS)
    {
        int32_t restore_error = this->apply_snapshot(before_state);

        if (restore_error != FT_ERR_SUCCESS)
            return (restore_error);
        return (result);
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::allocate_deck_instance_id(
    uint32_t *instance_id) noexcept
{
    if (instance_id == ft_nullptr || this->_next_deck_instance_id == 0U)
        return (FT_ERR_OUT_OF_RANGE);
    *instance_id = this->_next_deck_instance_id;
    this->_next_deck_instance_id += 1U;
    if (this->_next_deck_instance_id == 0U)
        this->_next_deck_instance_id = 1U;
    return (FT_ERR_SUCCESS);
}

ft_bool card_game_engine::deck_instance_exists(uint32_t instance_id) const noexcept
{
    uint32_t player_id;

    player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        if (this->_decks[player_id].contains_instance(instance_id)
            != FT_FALSE)
            return (FT_TRUE);
        uint32_t hand_index;

        hand_index = 0U;
        while (hand_index < this->_hand_count[player_id])
        {
            if (this->_hand[player_id][hand_index].instance_id == instance_id)
                return (FT_TRUE);
            hand_index += 1U;
        }
        if (this->zone_instance_exists(player_id, instance_id) != FT_FALSE)
            return (FT_TRUE);
        player_id += 1U;
    }
    return (FT_FALSE);
}

ft_bool card_game_engine::zone_instance_exists(uint32_t player_id,
    uint32_t instance_id) const noexcept
{
    uint32_t zone_index;

    if (player_id >= FT_CARD_GAME_MAX_PLAYERS || instance_id == 0U)
        return (FT_FALSE);
    zone_index = 0U;
    while (zone_index < this->_zone_count)
    {
        if (this->_zone_store.contains(player_id,
                this->_zones[zone_index].zone_id, instance_id) != FT_FALSE)
            return (FT_TRUE);
        zone_index += 1U;
    }
    return (FT_FALSE);
}

