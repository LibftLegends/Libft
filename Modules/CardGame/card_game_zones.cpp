#include "card_game_internal.hpp"

int32_t card_game_engine::zone_push_top(uint32_t player_id,
    uint32_t zone_id, uint32_t card_id, uint32_t *instance_id) noexcept
{
    card_game_zone_entry entry;
    uint32_t type_id;
    uint32_t previous_instance_id;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_card_type_id(card_id, &type_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    previous_instance_id = this->_next_deck_instance_id;
    result = this->allocate_deck_instance_id(instance_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    entry.instance_id = *instance_id;
    entry.card_id = card_id;
    result = this->_zone_store.insert_top(player_id, zone_id, entry, type_id);
    if (result != FT_ERR_SUCCESS)
    {
        this->_next_deck_instance_id = previous_instance_id;
        return (result);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_push_bottom(uint32_t player_id,
    uint32_t zone_id, uint32_t card_id, uint32_t *instance_id) noexcept
{
    card_game_zone_entry entry;
    uint32_t type_id;
    uint32_t previous_instance_id;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_card_type_id(card_id, &type_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    previous_instance_id = this->_next_deck_instance_id;
    result = this->allocate_deck_instance_id(instance_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    entry.instance_id = *instance_id;
    entry.card_id = card_id;
    result = this->_zone_store.insert_bottom(player_id, zone_id, entry,
        type_id);
    if (result != FT_ERR_SUCCESS)
    {
        this->_next_deck_instance_id = previous_instance_id;
        return (result);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_insert_at(uint32_t player_id,
    uint32_t zone_id, uint32_t index, uint32_t card_id,
    uint32_t *instance_id) noexcept
{
    card_game_zone_entry entry;
    uint32_t type_id;
    uint32_t previous_instance_id;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_card_type_id(card_id, &type_id) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_FOUND);
    previous_instance_id = this->_next_deck_instance_id;
    result = this->allocate_deck_instance_id(instance_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    entry.instance_id = *instance_id;
    entry.card_id = card_id;
    result = this->_zone_store.insert_at(player_id, zone_id, index, entry,
        type_id);
    if (result != FT_ERR_SUCCESS)
    {
        this->_next_deck_instance_id = previous_instance_id;
        return (result);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_inspect(uint32_t player_id, uint32_t zone_id,
    uint32_t index, card_game_deck_card *card) const noexcept
{
    card_game_zone_entry entry;
    int32_t result;

    if (this->_initialised_state != 2U || card == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.inspect(player_id, zone_id, index, &entry);
    if (result != FT_ERR_SUCCESS)
        return (result);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_pop_top(uint32_t player_id, uint32_t zone_id,
    card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t result;

    if (this->_initialised_state != 2U || card == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.pop_top(player_id, zone_id, &entry);
    if (result != FT_ERR_SUCCESS)
        return (result);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_pop_bottom(uint32_t player_id,
    uint32_t zone_id, card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t result;

    if (this->_initialised_state != 2U || card == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.pop_bottom(player_id, zone_id, &entry);
    if (result != FT_ERR_SUCCESS)
        return (result);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_remove_instance(uint32_t player_id,
    uint32_t zone_id, uint32_t instance_id, card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.remove(player_id, zone_id, instance_id,
        &entry);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (card != ft_nullptr)
    {
        card->instance_id = entry.instance_id;
        card->card_id = entry.card_id;
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_move_instance(uint32_t player_id,
    uint32_t source_zone_id, uint32_t destination_zone_id,
    uint32_t instance_id) noexcept
{
    card_game_deck_card card;
    uint32_t source_index;
    uint32_t source_count;
    uint32_t type_id;
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    source_count = this->_zone_store.size(player_id, source_zone_id);
    source_index = 0U;
    while (source_index < source_count)
    {
        result = this->zone_inspect(player_id, source_zone_id, source_index,
            &card);
        if (result != FT_ERR_SUCCESS)
            return (result);
        if (card.instance_id == instance_id)
            break ;
        source_index += 1U;
    }
    if (source_index >= source_count)
        return (FT_ERR_NOT_FOUND);
    if (this->find_card_type_id(card.card_id, &type_id) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    result = this->_zone_store.move_instance(player_id, source_zone_id,
        destination_zone_id, instance_id, type_id);
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::zone_shuffle(uint32_t player_id, uint32_t zone_id,
    uint64_t *random_state) noexcept
{
    int32_t result;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_zone_store.shuffle(player_id, zone_id, random_state);
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_zone_count(uint32_t player_id,
    uint32_t zone_id, uint32_t *count) const noexcept
{
    if (this->_initialised_state != 2U || count == ft_nullptr
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_zone_store.size(player_id, zone_id);
    return (FT_ERR_SUCCESS);
}

