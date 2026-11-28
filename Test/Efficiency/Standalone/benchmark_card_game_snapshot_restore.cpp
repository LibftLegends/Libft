#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    const ft_bool board = argument_count == 2 && ft_strcmp(arguments[1], "--board") == 0;
    if (argument_count != 1 && !board)
        return (1);
    constexpr uint32_t ITERATIONS = 160U;
    card_game_engine engine;
    card_game_snapshot snapshot;
    card_game_rules rules{4U, 4U, 20U, 5U, 10U, 10U};
    if (board)
        rules.max_board_spaces = FT_CARD_GAME_MAX_CARDS;
    const card_game_card_definition card{1U, CARD_GAME_CREATURE, 0U,
        2, 3, CARD_GAME_NO_EFFECT};
    if (engine.initialize(rules) != FT_ERR_SUCCESS
        || engine.register_card(card) != FT_ERR_SUCCESS
        || engine.start_match(FT_CARD_GAME_MAX_PLAYERS) != FT_ERR_SUCCESS
        || engine.get_snapshot(&snapshot) != FT_ERR_SUCCESS)
        return (1);
    uint32_t player_id = 0U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        card_game_player_snapshot &player = snapshot.players[player_id];
        if (board)
            player.board_count = FT_CARD_GAME_MAX_CARDS;
        else
            player.deck_count = FT_CARD_GAME_MAX_CARDS;
        uint32_t index = 0U;
        while (index < FT_CARD_GAME_MAX_CARDS)
        {
            const uint32_t id =
                (player_id * FT_CARD_GAME_MAX_CARDS + index + 1U) * 0x9E3779B9U;
            if (board)
            {
                player.board[index] = index;
                player.instances[index] = {id, card.card_id, player_id,
                    card.attack, card.health, 0, FT_TRUE};
            }
            else
            {
                player.deck[index] = card.card_id;
                player.deck_instance_ids[index] = id;
            }
            ++index;
        }
        ++player_id;
    }
    if (board)
    {
        snapshot.modifier_count = FT_CARD_GAME_MAX_MODIFIERS;
        uint32_t index = 0U;
        while (index < snapshot.modifier_count)
        {
            const uint32_t player = index % FT_CARD_GAME_MAX_PLAYERS;
            const uint32_t target = FT_CARD_GAME_MAX_CARDS - 1U - index / FT_CARD_GAME_MAX_PLAYERS;
            snapshot.modifiers[index] = {index + 1U, 0U, player,
                snapshot.players[player].instances[target].instance_id,
                target, 1, 0, CARD_GAME_MODIFIER_PERMANENT, 1U, 0U};
            ++index;
        }
    }
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (engine.apply_snapshot(snapshot) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    uint64_t checksum;
    if (engine.get_state_hash(&checksum) != FT_ERR_SUCCESS)
        return (1);
    std::printf("snapshot restore checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / ITERATIONS);
    return (0);
}
