#ifndef CARD_GAME_HPP
# define CARD_GAME_HPP

# include <cstdint>
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_CARD_GAME_MAX_CARDS = 128U;
static const uint32_t FT_CARD_GAME_MAX_EFFECTS = 256U;
static const uint32_t FT_CARD_GAME_MAX_PLAYERS = 8U;
static const uint32_t FT_CARD_GAME_MAX_PHASES = 64U;
static const uint32_t FT_CARD_GAME_MAX_ZONES = 32U;
static const uint32_t FT_CARD_GAME_MAX_CARD_TYPES = 32U;
static const uint32_t CARD_GAME_BOARD_ZONE_ID = 1U;
static const uint32_t FT_CARD_GAME_MAX_EVENTS = 256U;
static const uint32_t FT_CARD_GAME_MAX_OPERATIONS = 256U;
static const uint32_t FT_CARD_GAME_STATE_FORMAT_VERSION = 1U;
static const uint32_t CARD_GAME_COMMAND_PLAY_CARD = 1U << 0U;
static const uint32_t CARD_GAME_COMMAND_END_TURN = 1U << 1U;
static const uint32_t CARD_GAME_COMMAND_ADVANCE_PHASE = 1U << 2U;
static const uint32_t CARD_GAME_NO_EFFECT = UINT32_MAX;

enum card_game_command_type : uint8_t
{
    CARD_GAME_INTENT_PLAY_CARD = 1U,
    CARD_GAME_INTENT_END_TURN = 2U,
    CARD_GAME_INTENT_ADVANCE_PHASE = 3U
};

struct card_game_command
{
    uint64_t command_sequence;
    uint64_t expected_state_sequence;
    uint32_t player_id;
    card_game_command_type type;
    uint32_t card_id;
    uint32_t target_instance;
};

enum card_game_card_type : uint8_t
{
    CARD_GAME_CREATURE = 0U,
    CARD_GAME_SPELL = 1U,
    CARD_GAME_ARTIFACT = 2U,
    CARD_GAME_ENCHANTMENT = 3U
};

struct card_game_card_type_definition
{
    uint32_t type_id;
    uint32_t allowed_zone_mask;
    uint32_t max_copies_per_player;
};

struct card_game_zone_definition
{
    uint32_t zone_id;
    uint32_t capacity;
    uint32_t allowed_card_type_mask;
    ft_bool owner_scoped;
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

enum card_game_operation_type : uint8_t
{
    CARD_GAME_OPERATION_HEALTH = 0U,
    CARD_GAME_OPERATION_MANA = 1U,
    CARD_GAME_OPERATION_EMIT_EVENT = 2U
};

struct card_game_operation
{
    card_game_operation_type type;
    uint32_t player_id;
    int32_t amount;
    uint32_t event_type;
    uint32_t source_instance;
    uint32_t target_instance;
};

struct card_game_event
{
    uint64_t sequence;
    uint32_t event_type;
    uint32_t source_instance;
    uint32_t target_instance;
};

struct card_game_phase_definition
{
    uint32_t phase_id;
    uint32_t next_phase_id;
    uint32_t entry_event_type;
    uint32_t exit_event_type;
    uint32_t allowed_command_mask;
};

struct card_game_effect_context
{
    uint32_t event_type;
    uint32_t source_instance;
    uint32_t target_instance;
    uint32_t active_player;
    uint32_t turn_number;
};

struct card_game_player_snapshot
{
    uint32_t board_count;
    uint32_t health;
    uint32_t mana;
    uint32_t board[FT_CARD_GAME_MAX_CARDS];
    card_game_card_instance instances[FT_CARD_GAME_MAX_CARDS];
};

struct card_game_snapshot
{
    uint32_t format_version;
    uint64_t state_sequence;
    uint32_t player_count;
    uint32_t turn_number;
    uint32_t active_player;
    uint32_t current_phase_id;
    uint32_t event_count;
    uint64_t event_sequence;
    card_game_event events[FT_CARD_GAME_MAX_EVENTS];
    card_game_player_snapshot players[FT_CARD_GAME_MAX_PLAYERS];
};

struct card_game_delta
{
    uint32_t format_version;
    uint64_t base_state_sequence;
    uint64_t target_state_sequence;
    uint64_t changed_player_mask;
    ft_bool global_state_changed;
    uint32_t player_count;
    uint32_t turn_number;
    uint32_t active_player;
    uint32_t current_phase_id;
    uint32_t event_count;
    uint64_t event_sequence;
    card_game_event events[FT_CARD_GAME_MAX_EVENTS];
    card_game_player_snapshot players[FT_CARD_GAME_MAX_PLAYERS];
};

class card_game_operation_buffer
{
    private:
        card_game_operation _operations[FT_CARD_GAME_MAX_OPERATIONS];
        uint32_t _count;

    public:
        card_game_operation_buffer() noexcept;
        ~card_game_operation_buffer() noexcept;
        card_game_operation_buffer(const card_game_operation_buffer &other) = delete;
        card_game_operation_buffer(card_game_operation_buffer &&other) = delete;
        card_game_operation_buffer &operator=(const card_game_operation_buffer &other) = delete;
        card_game_operation_buffer &operator=(card_game_operation_buffer &&other) = delete;
        int32_t clear() noexcept;
        int32_t append(const card_game_operation &operation) noexcept;
        uint32_t size() const noexcept;
        int32_t get(uint32_t index, card_game_operation *operation) const noexcept;
};

class card_game_engine;
typedef int32_t (*card_game_effect_function)(card_game_engine &engine,
    uint32_t source_instance, uint32_t target_instance, void *context) noexcept;
typedef int32_t (*card_game_effect_callback)(const card_game_engine &engine,
    const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept;

class card_game_engine
{
    private:
        uint8_t _initialised_state;
        card_game_rules _rules;
        card_game_card_definition _cards[FT_CARD_GAME_MAX_CARDS];
        uint32_t _card_type_ids[FT_CARD_GAME_MAX_CARDS];
        card_game_card_type_definition _card_types[FT_CARD_GAME_MAX_CARD_TYPES];
        uint32_t _card_type_count;
        card_game_effect_function _effects[FT_CARD_GAME_MAX_EFFECTS];
        card_game_effect_callback _effect_callbacks[FT_CARD_GAME_MAX_EFFECTS];
        void *_effect_user_data[FT_CARD_GAME_MAX_EFFECTS];
        uint32_t _effect_event_types[FT_CARD_GAME_MAX_EFFECTS];
        card_game_phase_definition _phases[FT_CARD_GAME_MAX_PHASES];
        uint32_t _phase_count;
        card_game_zone_definition _zones[FT_CARD_GAME_MAX_ZONES];
        uint32_t _zone_count;
        uint32_t _current_phase_id;
        card_game_event _events[FT_CARD_GAME_MAX_EVENTS];
        uint32_t _event_count;
        uint64_t _event_sequence;
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
        uint64_t _state_sequence;
        uint64_t _last_command_sequence;

        card_game_engine(const card_game_engine &other) = delete;
        card_game_engine(card_game_engine &&other) = delete;
        card_game_engine &operator=(const card_game_engine &other) = delete;
        card_game_engine &operator=(card_game_engine &&other) = delete;

        int32_t find_card(uint32_t card_id,
            card_game_card_definition **definition) noexcept;
        int32_t find_card_type_id(uint32_t card_id,
            uint32_t *type_id) const noexcept;
        uint32_t get_board_capacity() const noexcept;
        int32_t register_card_internal(
            const card_game_card_definition &definition,
            uint32_t type_id) noexcept;
        ft_bool is_command_allowed(uint32_t command_mask) const noexcept;
        int32_t apply_operation(const card_game_operation &operation) noexcept;

    public:
        card_game_engine() noexcept;
        ~card_game_engine() noexcept;
        int32_t initialize(const card_game_rules &rules) noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_engine &other) noexcept;
        int32_t register_card(const card_game_card_definition &definition) noexcept;
        int32_t register_card_with_type(
            const card_game_card_definition &definition,
            uint32_t type_id) noexcept;
        int32_t register_card_type(
            const card_game_card_type_definition &type) noexcept;
        int32_t get_card_type(uint32_t type_id,
            card_game_card_type_definition *type) const noexcept;
        int32_t register_effect(card_game_effect_function effect,
            uint32_t *effect_id) noexcept;
        int32_t register_effect_callback(card_game_effect_callback callback,
            void *user_data, uint32_t event_type, uint32_t *effect_id) noexcept;
        int32_t register_phase(const card_game_phase_definition &phase) noexcept;
        int32_t register_zone(const card_game_zone_definition &zone) noexcept;
        int32_t get_zone(uint32_t zone_id,
            card_game_zone_definition *zone) const noexcept;
        int32_t start_match(uint32_t player_count) noexcept;
        int32_t set_player_mana(uint32_t player_id, uint32_t mana) noexcept;
        int32_t modify_player_health(uint32_t player_id, int32_t delta) noexcept;
        int32_t play_card(uint32_t player_id, uint32_t card_id,
            uint32_t target_instance, void *context) noexcept;
        int32_t end_turn() noexcept;
        int32_t emit_event(uint32_t event_type, uint32_t source_instance,
            uint32_t target_instance) noexcept;
        int32_t resolve_events() noexcept;
        int32_t advance_phase() noexcept;
        int32_t get_player_health(uint32_t player_id, uint32_t *health) const noexcept;
        int32_t get_board_count(uint32_t player_id, uint32_t *count) const noexcept;
        int32_t get_turn(uint32_t *turn_number, uint32_t *active_player) const noexcept;
        int32_t get_instance(uint32_t player_id, uint32_t index,
            card_game_card_instance *instance) const noexcept;
        int32_t get_snapshot(card_game_snapshot *snapshot) const noexcept;
        int32_t get_rules_hash(uint64_t *hash) const noexcept;
        int32_t get_state_hash(uint64_t *hash) const noexcept;
        int32_t apply_snapshot(const card_game_snapshot &snapshot) noexcept;
        int32_t create_delta(const card_game_snapshot &baseline,
            card_game_delta *delta) const noexcept;
        int32_t apply_delta(const card_game_delta &delta) noexcept;
        int32_t submit_command(const card_game_command &command,
            void *context) noexcept;
};

#endif
