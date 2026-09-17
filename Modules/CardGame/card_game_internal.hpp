#ifndef CARD_GAME_INTERNAL_HPP
# define CARD_GAME_INTERNAL_HPP

#include "card_game.hpp"
#include <new>
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"

inline uint64_t card_game_match_random_next(uint64_t *state) noexcept
{
    uint64_t value;

    value = *state;
    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    *state = value;
    return (value * 2685821657736338717ULL);
}
inline card_game_snapshot *card_game_create_snapshot() noexcept
{
    void *memory;

    memory = cma_malloc(sizeof(card_game_snapshot));
    if (memory == ft_nullptr)
        return (ft_nullptr);
    return (::new (memory) card_game_snapshot());
}

inline int32_t card_game_destroy_snapshot(card_game_snapshot *snapshot) noexcept
{
    if (snapshot == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    snapshot->~card_game_snapshot();
    cma_free(snapshot);
    return (FT_ERR_SUCCESS);
}

inline void card_game_hash_u32(uint64_t *hash, uint32_t value) noexcept
{
    *hash ^= static_cast<uint64_t>(value);
    *hash *= 1099511628211ULL;
    return ;
}

inline void card_game_hash_u64(uint64_t *hash, uint64_t value) noexcept
{
    *hash ^= value;
    *hash *= 1099511628211ULL;
    return ;
}

inline void card_game_hash_instance(uint64_t *hash,
    const card_game_card_instance &instance) noexcept
{
    card_game_hash_u32(hash, instance.instance_id);
    card_game_hash_u32(hash, instance.definition_id);
    card_game_hash_u32(hash, instance.owner_id);
    card_game_hash_u32(hash, static_cast<uint32_t>(instance.attack));
    card_game_hash_u32(hash, static_cast<uint32_t>(instance.health));
    card_game_hash_u32(hash, static_cast<uint32_t>(instance.damage_taken));
    card_game_hash_u32(hash, static_cast<uint32_t>(instance.on_board));
    return ;
}

inline void card_game_replay_write_u32(uint8_t *output, uint32_t *offset,
    uint32_t value) noexcept
{
    output[*offset] = static_cast<uint8_t>(value & 255U);
    output[*offset + 1U] = static_cast<uint8_t>((value >> 8U) & 255U);
    output[*offset + 2U] = static_cast<uint8_t>((value >> 16U) & 255U);
    output[*offset + 3U] = static_cast<uint8_t>((value >> 24U) & 255U);
    *offset += 4U;
    return ;
}

inline void card_game_replay_write_u64(uint8_t *output, uint32_t *offset,
    uint64_t value) noexcept
{
    uint32_t index;

    index = 0U;
    while (index < 8U)
    {
        output[*offset + index] = static_cast<uint8_t>(value & 255U);
        value >>= 8U;
        index += 1U;
    }
    *offset += 8U;
    return ;
}

inline int32_t card_game_replay_read_u32(const uint8_t *input,
    uint32_t input_size, uint32_t *offset, uint32_t *value) noexcept
{
    if (input == ft_nullptr || offset == ft_nullptr || value == ft_nullptr
        || *offset > input_size || input_size - *offset < 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    *value = static_cast<uint32_t>(input[*offset])
        | (static_cast<uint32_t>(input[*offset + 1U]) << 8U)
        | (static_cast<uint32_t>(input[*offset + 2U]) << 16U)
        | (static_cast<uint32_t>(input[*offset + 3U]) << 24U);
    *offset += 4U;
    return (FT_ERR_SUCCESS);
}

inline int32_t card_game_replay_read_u64(const uint8_t *input,
    uint32_t input_size, uint32_t *offset, uint64_t *value) noexcept
{
    uint32_t index;
    uint64_t result;

    if (input == ft_nullptr || offset == ft_nullptr || value == ft_nullptr
        || *offset > input_size || input_size - *offset < 8U)
        return (FT_ERR_INVALID_ARGUMENT);
    result = 0U;
    index = 0U;
    while (index < 8U)
    {
        result |= static_cast<uint64_t>(input[*offset + index])
            << (index * 8U);
        index += 1U;
    }
    *offset += 8U;
    *value = result;
    return (FT_ERR_SUCCESS);
}


inline int32_t card_game_validate_player_snapshot(
    const card_game_player_snapshot &player, uint32_t player_id,
    uint32_t max_board_spaces, uint32_t max_hand_size) noexcept
{
    uint32_t index;
    uint32_t previous_index;

    if (player.board_count > max_board_spaces
        || player.board_count > FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (player.deck_count > FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (player.hand_count > max_hand_size
        || player.hand_count > FT_CARD_GAME_MAX_CARDS)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < player.board_count)
    {
        if (player.board[index] >= player.board_count
            || player.instances[index].on_board == FT_FALSE
            || player.instances[index].instance_id == 0U
            || player.instances[index].owner_id != player_id
            || player.instances[index].damage_taken < 0)
            return (FT_ERR_INVALID_ARGUMENT);
        previous_index = 0U;
        while (previous_index < index)
        {
            if (player.instances[previous_index].instance_id
                == player.instances[index].instance_id)
                return (FT_ERR_INVALID_ARGUMENT);
            previous_index += 1U;
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

inline ft_bool card_game_card_instance_equal(
    const card_game_card_instance &first,
    const card_game_card_instance &second) noexcept
{
    if (first.definition_id != second.definition_id
        || first.instance_id != second.instance_id
        || first.owner_id != second.owner_id
        || first.attack != second.attack
        || first.health != second.health
        || first.damage_taken != second.damage_taken
        || first.on_board != second.on_board)
        return (FT_FALSE);
    return (FT_TRUE);
}

inline ft_bool card_game_player_snapshots_equal(
    const card_game_player_snapshot &first,
    const card_game_player_snapshot &second) noexcept
{
    uint32_t index;

    if (first.board_count != second.board_count
        || first.deck_count != second.deck_count
        || first.hand_count != second.hand_count
        || first.health != second.health
        || first.mana != second.mana
        || ft_memcmp(first.board, second.board, sizeof(first.board)) != 0
        || ft_memcmp(first.deck, second.deck, sizeof(first.deck)) != 0
        || ft_memcmp(first.deck_instance_ids, second.deck_instance_ids,
            sizeof(first.deck_instance_ids)) != 0
        || ft_memcmp(first.hand, second.hand, sizeof(first.hand)) != 0
        || ft_memcmp(first.hand_instance_ids, second.hand_instance_ids,
            sizeof(first.hand_instance_ids)) != 0)
        return (FT_FALSE);
    index = 0U;
    while (index < FT_CARD_GAME_MAX_CARDS)
    {
        if (card_game_card_instance_equal(first.instances[index],
                second.instances[index]) == FT_FALSE)
            return (FT_FALSE);
        index += 1U;
    }
    return (FT_TRUE);
}

inline int32_t card_game_copy_player_snapshot(
    card_game_player_snapshot *destination, const uint32_t *board,
    const card_game_card_instance *instances, uint32_t board_count,
    const card_game_ordered_zone &deck, const card_game_deck_card *hand,
    uint32_t hand_count, uint32_t health, uint32_t mana) noexcept
{
    uint32_t deck_index;

    ft_memcpy(destination->board, board,
        sizeof(destination->board));
    ft_memcpy(destination->instances, instances,
        sizeof(destination->instances));
    destination->board_count = board_count;
    destination->deck_count = deck.size();
    destination->hand_count = hand_count;
    deck_index = 0U;
    while (deck_index < hand_count)
    {
        destination->hand[deck_index] = hand[deck_index].card_id;
        destination->hand_instance_ids[deck_index] =
            hand[deck_index].instance_id;
        deck_index += 1U;
    }
    deck_index = 0U;
    while (deck_index < destination->deck_count)
    {
        card_game_zone_entry entry;

        if (deck.get_entry(deck_index, &entry)
            != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        destination->deck[deck_index] = entry.card_id;
        destination->deck_instance_ids[deck_index] = entry.instance_id;
        deck_index += 1U;
    }
    destination->health = health;
    destination->mana = mana;
    return (FT_ERR_SUCCESS);
}
#endif
