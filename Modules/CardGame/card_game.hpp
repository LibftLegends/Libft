#ifndef CARD_GAME_HPP
# define CARD_GAME_HPP

# include <cstdint>
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_CARD_GAME_MAX_CARDS = 128U;
static const uint32_t FT_CARD_GAME_MAX_EFFECTS = 256U;
static const uint32_t FT_CARD_GAME_MAX_PLAYERS = 8U;

enum card_game_card_type : uint8_t
{
    CARD_GAME_CREATURE = 0U,
    CARD_GAME_SPELL = 1U,
    CARD_GAME_ARTIFACT = 2U,
    CARD_GAME_ENCHANTMENT = 3U
};

struct card_game_rules
{
    uint32_t max_board_spaces;
    uint32_t max_hand_size;
    uint32_t starting_health;
    uint32_t starting_mana;
    uint32_t max_mana;
    uint32_t max_turns;
};

struct card_game_card_definition
{
    uint32_t card_id;
    card_game_card_type type;
    uint32_t cost;
    int32_t attack;
    int32_t health;
    uint32_t effect_id;
};

struct card_game_card_instance
{
    uint32_t definition_id;
    uint32_t owner_id;
    int32_t attack;
    int32_t health;
    ft_bool on_board;
};

class card_game_engine;
typedef int32_t (*card_game_effect_function)(card_game_engine &engine,
    uint32_t source_instance, uint32_t target_instance, void *context) noexcept;

class card_game_engine
{
    private:
        uint8_t _initialised_state;
        card_game_rules _rules;
        card_game_card_definition _cards[FT_CARD_GAME_MAX_CARDS];
        card_game_effect_function _effects[FT_CARD_GAME_MAX_EFFECTS];
        uint32_t _card_count;
        uint32_t _effect_count;
        uint32_t _board[FT_CARD_GAME_MAX_PLAYERS][FT_CARD_GAME_MAX_CARDS];
        card_game_card_instance _instances[FT_CARD_GAME_MAX_PLAYERS][FT_CARD_GAME_MAX_CARDS];
        uint32_t _board_count[FT_CARD_GAME_MAX_PLAYERS];
        uint32_t _health[FT_CARD_GAME_MAX_PLAYERS];
        uint32_t _mana[FT_CARD_GAME_MAX_PLAYERS];
        uint32_t _turn_number;
        uint32_t _active_player;
        uint32_t _player_count;

        card_game_engine(const card_game_engine &other) = delete;
        card_game_engine(card_game_engine &&other) = delete;
        card_game_engine &operator=(const card_game_engine &other) = delete;
        card_game_engine &operator=(card_game_engine &&other) = delete;

        int32_t find_card(uint32_t card_id,
            card_game_card_definition **definition) noexcept;

    public:
        card_game_engine() noexcept;
        ~card_game_engine() noexcept;
        int32_t initialize(const card_game_rules &rules) noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_engine &other) noexcept;
        int32_t register_card(const card_game_card_definition &definition) noexcept;
        int32_t register_effect(card_game_effect_function effect,
            uint32_t *effect_id) noexcept;
        int32_t start_match(uint32_t player_count) noexcept;
        int32_t set_player_mana(uint32_t player_id, uint32_t mana) noexcept;
        int32_t modify_player_health(uint32_t player_id, int32_t delta) noexcept;
        int32_t play_card(uint32_t player_id, uint32_t card_id,
            uint32_t target_instance, void *context) noexcept;
        int32_t end_turn() noexcept;
        int32_t get_player_health(uint32_t player_id, uint32_t *health) const noexcept;
        int32_t get_board_count(uint32_t player_id, uint32_t *count) const noexcept;
        int32_t get_turn(uint32_t *turn_number, uint32_t *active_player) const noexcept;
        int32_t get_instance(uint32_t player_id, uint32_t index,
            card_game_card_instance *instance) const noexcept;
};

#endif
