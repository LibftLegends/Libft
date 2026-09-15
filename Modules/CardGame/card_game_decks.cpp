#include "card_game_internal.hpp"

int32_t card_game_engine::deck_push_top_instance(uint32_t player_id,
    uint32_t instance_id, uint32_t card_id) noexcept
{
    card_game_zone_entry entry;
    int32_t push_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->is_card_registered(card_id) == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    if (this->deck_instance_exists(instance_id) != FT_FALSE)
        return (FT_ERR_ALREADY_EXISTS);
    entry.instance_id = instance_id;
    entry.card_id = card_id;
    push_error = this->_decks[player_id].push_top_entry(entry);
    if (push_error != FT_ERR_SUCCESS)
        return (push_error);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_push_bottom_instance(uint32_t player_id,
    uint32_t instance_id, uint32_t card_id) noexcept
{
    card_game_zone_entry entry;
    int32_t push_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->is_card_registered(card_id) == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    if (this->deck_instance_exists(instance_id) != FT_FALSE)
        return (FT_ERR_ALREADY_EXISTS);
    entry.instance_id = instance_id;
    entry.card_id = card_id;
    push_error = this->_decks[player_id].push_bottom_entry(entry);
    if (push_error != FT_ERR_SUCCESS)
        return (push_error);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_insert_instance_at(uint32_t player_id,
    uint32_t index, uint32_t instance_id, uint32_t card_id) noexcept
{
    card_game_zone_entry entry;
    int32_t insert_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->is_card_registered(card_id) == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    if (this->deck_instance_exists(instance_id) != FT_FALSE)
        return (FT_ERR_ALREADY_EXISTS);
    entry.instance_id = instance_id;
    entry.card_id = card_id;
    insert_error = this->_decks[player_id].insert_entry_at(index, entry);
    if (insert_error != FT_ERR_SUCCESS)
        return (insert_error);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_push_top(uint32_t player_id,
    uint32_t card_id) noexcept
{
    uint32_t instance_id;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->allocate_deck_instance_id(&instance_id) != FT_ERR_SUCCESS)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->deck_push_top_instance(player_id, instance_id, card_id));
}

int32_t card_game_engine::deck_push_bottom(uint32_t player_id,
    uint32_t card_id) noexcept
{
    uint32_t instance_id;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->allocate_deck_instance_id(&instance_id) != FT_ERR_SUCCESS)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->deck_push_bottom_instance(player_id, instance_id, card_id));
}

int32_t card_game_engine::deck_insert_at(uint32_t player_id, uint32_t index,
    uint32_t card_id) noexcept
{
    uint32_t instance_id;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->allocate_deck_instance_id(&instance_id) != FT_ERR_SUCCESS)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->deck_insert_instance_at(player_id, index, instance_id,
        card_id));
}

int32_t card_game_engine::deck_peek_top(uint32_t player_id,
    uint32_t *card_id) const noexcept
{
    card_game_deck_card card;
    int32_t inspect_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    inspect_error = this->deck_inspect(player_id, 0U, &card);
    if (inspect_error != FT_ERR_SUCCESS)
        return (inspect_error);
    *card_id = card.card_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_peek_bottom(uint32_t player_id,
    uint32_t *card_id) const noexcept
{
    card_game_deck_card card;
    uint32_t count;
    int32_t inspect_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    count = this->_decks[player_id].size();
    if (count == 0U)
        return (FT_ERR_EMPTY);
    inspect_error = this->deck_inspect(player_id, count - 1U, &card);
    if (inspect_error != FT_ERR_SUCCESS)
        return (inspect_error);
    *card_id = card.card_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_draw_top(uint32_t player_id,
    uint32_t *card_id) noexcept
{
    card_game_zone_entry entry;
    int32_t draw_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    draw_error = this->_decks[player_id].pop_top_entry(&entry);
    if (draw_error != FT_ERR_SUCCESS)
        return (draw_error);
    if (card_id != ft_nullptr)
        *card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_draw_bottom(uint32_t player_id,
    uint32_t *card_id) noexcept
{
    card_game_zone_entry entry;
    int32_t draw_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    draw_error = this->_decks[player_id].pop_bottom_entry(&entry);
    if (draw_error != FT_ERR_SUCCESS)
        return (draw_error);
    if (card_id != ft_nullptr)
        *card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_draw_top(uint32_t player_id,
    card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t draw_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    draw_error = this->_decks[player_id].pop_top_entry(&entry);
    if (draw_error != FT_ERR_SUCCESS)
        return (draw_error);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_remove(uint32_t player_id,
    uint32_t card_id) noexcept
{
    uint32_t index;
    card_game_zone_entry entry;
    int32_t get_error;
    int32_t remove_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_decks[player_id].size())
    {
        get_error = this->_decks[player_id].get_entry(index, &entry);
        if (get_error != FT_ERR_SUCCESS)
            return (get_error);
        if (entry.card_id == card_id)
            break ;
        index += 1U;
    }
    if (index >= this->_decks[player_id].size())
        return (FT_ERR_NOT_FOUND);
    remove_error = this->_decks[player_id].remove_entry(entry.instance_id,
        ft_nullptr);
    if (remove_error != FT_ERR_SUCCESS)
        return (remove_error);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_inspect(uint32_t player_id, uint32_t index,
    card_game_deck_card *card) const noexcept
{
    card_game_zone_entry entry;
    int32_t get_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    get_error = this->_decks[player_id].get_entry(index, &entry);
    if (get_error != FT_ERR_SUCCESS)
        return (get_error);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::deck_get_instance(uint32_t player_id,
    uint32_t instance_id, card_game_deck_card *card) const noexcept
{
    uint32_t index;
    int32_t inspect_error;
    card_game_deck_card candidate;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card == ft_nullptr || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_decks[player_id].size())
    {
        inspect_error = this->deck_inspect(player_id, index, &candidate);
        if (inspect_error != FT_ERR_SUCCESS)
            return (inspect_error);
        if (candidate.instance_id == instance_id)
        {
            *card = candidate;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_engine::deck_draw_instance(uint32_t player_id,
    uint32_t instance_id, card_game_deck_card *card) noexcept
{
    card_game_zone_entry entry;
    int32_t remove_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS
        || card == ft_nullptr || instance_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    remove_error = this->_decks[player_id].remove_entry(instance_id, &entry);
    if (remove_error != FT_ERR_SUCCESS)
        return (remove_error);
    card->instance_id = entry.instance_id;
    card->card_id = entry.card_id;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::shuffle_deck(uint32_t player_id,
    uint64_t *random_state) noexcept
{
    int32_t shuffle_error;

    if (this->_initialised_state != 2U || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    shuffle_error = this->_decks[player_id].shuffle(random_state);
    if (shuffle_error != FT_ERR_SUCCESS)
        return (shuffle_error);
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::shuffle_deck(uint32_t player_id) noexcept
{
    uint64_t original_random_state;
    int32_t result;

    if (this->_initialised_state != 2U
        || player_id >= FT_CARD_GAME_MAX_PLAYERS)
        return (FT_ERR_INVALID_ARGUMENT);
    original_random_state = this->_random_state;
    result = this->_decks[player_id].shuffle(&this->_random_state);
    if (result != FT_ERR_SUCCESS)
    {
        this->_random_state = original_random_state;
        return (result);
    }
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::set_random_seed(uint64_t seed) noexcept
{
    if (this->_initialised_state != 2U || seed == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_random_state = seed;
    this->_state_sequence += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_engine::get_random_state(uint64_t *state) const noexcept
{
    if (this->_initialised_state != 2U || state == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *state = this->_random_state;
    return (FT_ERR_SUCCESS);
}

