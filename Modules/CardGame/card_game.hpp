#ifndef CARD_GAME_HPP
# define CARD_GAME_HPP

# include <cstdint>
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"
# include "card_game_limits.hpp"

static const uint32_t FT_CARD_GAME_MAX_EFFECTS = 256U;
static const uint32_t FT_CARD_GAME_MAX_PHASES = 64U;
static const uint32_t FT_CARD_GAME_MAX_CARD_TYPES = 32U;
static const uint32_t FT_CARD_GAME_MAX_START_OVERRIDES = 256U;
static const uint32_t CARD_GAME_BOARD_ZONE_ID = 1U;
static const uint32_t FT_CARD_GAME_MAX_EVENTS = FT_CARD_GAME_MAX_EVENT_RECORDS;
static const uint32_t FT_CARD_GAME_EVENT_INITIAL_CAPACITY = 256U;
static const uint32_t FT_CARD_GAME_MAX_OPERATIONS = FT_CARD_GAME_MAX_EVENT_RECORDS;
static const uint32_t FT_CARD_GAME_OPERATION_INITIAL_CAPACITY = 256U;
static const uint32_t FT_CARD_GAME_MAX_COMMAND_RECORDS =
    FT_CARD_GAME_MAX_EVENT_RECORDS;
static const uint32_t FT_CARD_GAME_COMMAND_RECORD_INITIAL_CAPACITY = 256U;
static const uint32_t FT_CARD_GAME_MAX_MODIFIERS = 256U;
static const uint32_t FT_CARD_GAME_REPLAY_MAGIC = 0x43475231U;
static const uint32_t FT_CARD_GAME_REPLAY_VERSION = 1U;
static const uint32_t FT_CARD_GAME_REPLAY_HEADER_BYTES = 12U;
static const uint32_t FT_CARD_GAME_REPLAY_RECORD_BYTES = 56U;
/* Convenience buffer size for small replay exports; APIs accept larger buffers. */
static const uint32_t FT_CARD_GAME_MAX_REPLAY_BYTES = 65536U;

# include "card_game_ordered_zone.hpp"
# include "card_game_resources.hpp"
# include "card_game_choices.hpp"
# include "card_game_zone_store.hpp"
# include "card_game_usage_limits.hpp"
# include "card_game_format.hpp"
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

struct card_game_command_record
{
    card_game_command command;
    uint64_t rules_hash;
    uint64_t state_hash_before;
    uint64_t state_hash_after;
};

enum card_game_card_type : uint8_t
{
    CARD_GAME_CREATURE = 0U,
    CARD_GAME_SPELL = 1U,
    CARD_GAME_ARTIFACT = 2U,
    CARD_GAME_ENCHANTMENT = 3U
};

enum card_game_modifier_duration : uint8_t
{
    CARD_GAME_MODIFIER_PERMANENT = 0U,
    CARD_GAME_MODIFIER_UNTIL_END_TURN = 1U
};

enum card_game_combat_mode : uint8_t
{
    CARD_GAME_COMBAT_ORDERED = 0U,
    CARD_GAME_COMBAT_SIMULTANEOUS = 1U
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

struct card_game_match_start_config
{
    uint32_t opening_hand_size;
    uint32_t starting_health;
    uint32_t starting_mana;
    uint32_t first_player;
    ft_bool random_first_player;
    ft_bool deal_opening_hand;
};

enum card_game_start_override_field : uint8_t
{
    CARD_GAME_START_HEALTH = 0U,
    CARD_GAME_START_MANA = 1U,
    CARD_GAME_START_OPENING_HAND = 2U,
    CARD_GAME_START_FIRST_PLAYER = 3U
};

enum card_game_start_override_operation : uint8_t
{
    CARD_GAME_START_OVERRIDE_SET = 0U,
    CARD_GAME_START_OVERRIDE_ADD = 1U
};

static const uint32_t CARD_GAME_START_ALL_PLAYERS = UINT32_MAX;

struct card_game_start_override
{
    uint32_t source_id;
    uint32_t player_id;
    card_game_start_override_field field;
    card_game_start_override_operation operation;
    uint32_t value;
    uint32_t priority;
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

struct card_game_deck_card
{
    uint32_t instance_id;
    uint32_t card_id;
};

struct card_game_card_instance
{
    uint32_t definition_id;
    uint32_t owner_id;
    int32_t attack;
    int32_t health;
    int32_t damage_taken;
    ft_bool on_board;
};

struct card_game_card_modifier
{
    uint32_t modifier_id;
    uint32_t source_effect_id;
    uint32_t target_player_id;
    uint32_t target_instance_index;
    int32_t attack_delta;
    int32_t health_delta;
    card_game_modifier_duration duration;
    uint32_t created_turn;
    uint32_t created_phase_id;
};

enum card_game_operation_type : uint8_t
{
    CARD_GAME_OPERATION_HEALTH = 0U,
    CARD_GAME_OPERATION_MANA = 1U,
    CARD_GAME_OPERATION_EMIT_EVENT = 2U,
    CARD_GAME_OPERATION_DAMAGE_INSTANCE = 3U,
    CARD_GAME_OPERATION_HEAL_INSTANCE = 4U,
    CARD_GAME_OPERATION_MODIFY_INSTANCE_STATS = 5U
};

struct card_game_operation
{
    card_game_operation_type type;
    uint32_t player_id;
    int32_t amount;
    uint32_t event_type;
    uint32_t source_instance;
    uint32_t target_instance;
    int32_t attack_delta;
    int32_t health_delta;
    card_game_modifier_duration duration;
    uint32_t source_effect_id;
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
    uint32_t deck_count;
    uint32_t hand_count;
    uint32_t health;
    uint32_t mana;
    uint32_t board[FT_CARD_GAME_MAX_CARDS];
    uint32_t deck[FT_CARD_GAME_MAX_CARDS];
    uint32_t deck_instance_ids[FT_CARD_GAME_MAX_CARDS];
    uint32_t hand[FT_CARD_GAME_MAX_CARDS];
    uint32_t hand_instance_ids[FT_CARD_GAME_MAX_CARDS];
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
    uint64_t random_state;
    uint32_t modifier_count;
    card_game_card_modifier modifiers[FT_CARD_GAME_MAX_MODIFIERS];
    uint32_t event_capacity;
    card_game_event *events;
    card_game_zone_store_snapshot zones;
    card_game_resource_snapshot resources;
    card_game_allowance_snapshot allowances;
    card_game_choice_snapshot choices;
    card_game_usage_limit_snapshot usage_limits;
    card_game_player_snapshot players[FT_CARD_GAME_MAX_PLAYERS];

    card_game_snapshot() noexcept;
    ~card_game_snapshot() noexcept;
    card_game_snapshot(const card_game_snapshot &other) = delete;
    card_game_snapshot(card_game_snapshot &&other) = delete;
    card_game_snapshot &operator=(const card_game_snapshot &other) = delete;
    card_game_snapshot &operator=(card_game_snapshot &&other) = delete;
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
    uint64_t random_state;
    uint32_t modifier_count;
    card_game_card_modifier modifiers[FT_CARD_GAME_MAX_MODIFIERS];
    uint32_t event_capacity;
    card_game_event *events;
    card_game_zone_store_snapshot zones;
    card_game_resource_snapshot resources;
    card_game_allowance_snapshot allowances;
    card_game_choice_snapshot choices;
    card_game_usage_limit_snapshot usage_limits;
    card_game_player_snapshot players[FT_CARD_GAME_MAX_PLAYERS];

    card_game_delta() noexcept;
    ~card_game_delta() noexcept;
    card_game_delta(const card_game_delta &other) = delete;
    card_game_delta(card_game_delta &&other) = delete;
    card_game_delta &operator=(const card_game_delta &other) = delete;
    card_game_delta &operator=(card_game_delta &&other) = delete;
};

class card_game_operation_buffer
{
    private:
        uint8_t _initialised_state;
        card_game_operation *_operations;
        uint32_t _capacity;
        uint32_t _count;

        int32_t grow() noexcept;

    public:
        card_game_operation_buffer() noexcept;
        ~card_game_operation_buffer() noexcept;
        card_game_operation_buffer(const card_game_operation_buffer &other) = delete;
        card_game_operation_buffer(card_game_operation_buffer &&other) = delete;
        card_game_operation_buffer &operator=(const card_game_operation_buffer &other) = delete;
        card_game_operation_buffer &operator=(card_game_operation_buffer &&other) = delete;
        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_operation_buffer &other) noexcept;
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
        card_game_card_definition *_cards;
        uint32_t *_card_type_ids;
        uint32_t _card_capacity;
        card_game_card_type_definition _card_types[FT_CARD_GAME_MAX_CARD_TYPES];
        uint32_t _card_type_count;
        card_game_effect_function _effects[FT_CARD_GAME_MAX_EFFECTS];
        card_game_effect_callback _effect_callbacks[FT_CARD_GAME_MAX_EFFECTS];
        void *_effect_user_data[FT_CARD_GAME_MAX_EFFECTS];
        uint32_t _effect_event_types[FT_CARD_GAME_MAX_EFFECTS];
        uint32_t _effect_priorities[FT_CARD_GAME_MAX_EFFECTS];
        uint32_t _effect_usage_limit_ids[FT_CARD_GAME_MAX_EFFECTS];
        card_game_phase_definition _phases[FT_CARD_GAME_MAX_PHASES];
        uint32_t _phase_count;
        card_game_zone_definition _zones[FT_CARD_GAME_MAX_ZONES];
        uint32_t _zone_count;
        uint32_t _current_phase_id;
        card_game_event *_events;
        uint32_t _event_capacity;
        uint32_t _event_count;
        uint64_t _event_sequence;
        uint64_t _random_state;
        uint32_t _card_count;
        uint32_t _effect_count;
        uint32_t _board[FT_CARD_GAME_MAX_PLAYERS][FT_CARD_GAME_MAX_CARDS];
        card_game_card_instance _instances[FT_CARD_GAME_MAX_PLAYERS][FT_CARD_GAME_MAX_CARDS];
        uint32_t _board_count[FT_CARD_GAME_MAX_PLAYERS];
        card_game_deck_card _hand[FT_CARD_GAME_MAX_PLAYERS][FT_CARD_GAME_MAX_CARDS];
        uint32_t _hand_count[FT_CARD_GAME_MAX_PLAYERS];
        card_game_ordered_zone _decks[FT_CARD_GAME_MAX_PLAYERS];
        uint32_t _health[FT_CARD_GAME_MAX_PLAYERS];
        uint32_t _mana[FT_CARD_GAME_MAX_PLAYERS];
        uint32_t _turn_number;
        uint32_t _active_player;
        uint32_t _player_count;
        uint32_t _next_deck_instance_id;
        card_game_card_modifier _modifiers[FT_CARD_GAME_MAX_MODIFIERS];
        uint32_t _modifier_count;
        uint32_t _next_modifier_id;
        uint64_t _state_sequence;
        uint64_t _last_command_sequence;
        card_game_command_record *_command_records;
        uint32_t _command_record_capacity;
        uint32_t _command_record_count;
        card_game_resource_ledger _resources;
        card_game_allowance_ledger _allowances;
        card_game_choice_ledger _choices;
        card_game_zone_store _zone_store;
        card_game_usage_limit_ledger _usage_limits;
        card_game_start_override _start_overrides[
            FT_CARD_GAME_MAX_START_OVERRIDES];
        uint32_t _start_override_count;

        card_game_engine(const card_game_engine &other) = delete;
        card_game_engine(card_game_engine &&other) = delete;
        card_game_engine &operator=(const card_game_engine &other) = delete;
        card_game_engine &operator=(card_game_engine &&other) = delete;

        int32_t find_card(uint32_t card_id,
            card_game_card_definition **definition) noexcept;
        ft_bool is_card_registered(uint32_t card_id) const noexcept;
        int32_t find_card_type_id(uint32_t card_id,
            uint32_t *type_id) const noexcept;
        uint32_t get_board_capacity() const noexcept;
        int32_t register_card_internal(
            const card_game_card_definition &definition,
            uint32_t type_id) noexcept;
        ft_bool is_command_allowed(uint32_t command_mask) const noexcept;
        int32_t apply_operation(const card_game_operation &operation) noexcept;
        int32_t allocate_deck_instance_id(uint32_t *instance_id) noexcept;
        ft_bool deck_instance_exists(uint32_t instance_id) const noexcept;
        ft_bool zone_instance_exists(uint32_t player_id,
            uint32_t instance_id) const noexcept;
        int32_t allocate_modifier_id(uint32_t *modifier_id) noexcept;
        int32_t expire_turn_modifiers() noexcept;
        int32_t grow_command_records() noexcept;
        int32_t grow_events() noexcept;
        int32_t grow_card_definitions() noexcept;
        int32_t resolve_start_override(
            card_game_start_override_field field, uint32_t player_id,
            uint32_t baseline, uint32_t *value) const noexcept;
        int32_t remove_board_instance(uint32_t player_id,
            uint32_t instance_index) noexcept;

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
        int32_t register_effect_callback_with_priority(
            card_game_effect_callback callback, void *user_data,
            uint32_t event_type, uint32_t priority,
            uint32_t *effect_id) noexcept;
        int32_t register_effect_callback_with_usage_limit(
            card_game_effect_callback callback, void *user_data,
            uint32_t event_type, uint32_t priority,
            uint32_t usage_limit_id, uint32_t *effect_id) noexcept;
        int32_t register_phase(const card_game_phase_definition &phase) noexcept;
        int32_t register_zone(const card_game_zone_definition &zone) noexcept;
        int32_t get_zone(uint32_t zone_id,
            card_game_zone_definition *zone) const noexcept;
        int32_t start_match(uint32_t player_count) noexcept;
        int32_t start_match(uint32_t player_count,
            const card_game_match_start_config &config) noexcept;
        int32_t register_start_override(
            const card_game_start_override &override_rule) noexcept;
        int32_t register_usage_limit(uint32_t key_id, uint32_t subject_id,
            card_game_usage_scope scope, uint64_t window_epoch,
            uint32_t maximum_uses,
            card_game_usage_attempt_policy attempt_policy,
            uint32_t source_instance, uint32_t *limit_id) noexcept;
        int32_t get_usage_limit(uint32_t limit_id,
            card_game_usage_limit *limit) const noexcept;
        int32_t consume_usage_limit(uint32_t limit_id, uint32_t amount,
            uint64_t current_epoch) noexcept;
        int32_t reset_usage_limits(uint64_t current_epoch) noexcept;
        int32_t register_resource_pool(uint32_t owner_id,
            uint32_t resource_type_id, uint32_t maximum_amount,
            uint32_t *pool_id) noexcept;
        int32_t add_resource_units(uint32_t owner_id,
            uint32_t resource_type_id, uint32_t amount, uint32_t tags,
            uint64_t expiry_epoch, ft_bool temporary,
            uint32_t *unit_id) noexcept;
        int32_t get_resource_pool(uint32_t owner_id,
            uint32_t resource_type_id, card_game_resource_pool *pool) const noexcept;
        int32_t lock_resource_units(uint32_t owner_id,
            uint32_t resource_type_id, uint32_t amount,
            uint64_t unlock_epoch) noexcept;
        int32_t create_resource_payment_plan(uint32_t owner_id,
            const card_game_resource_requirement &requirement,
            card_game_payment_plan *plan) const noexcept;
        int32_t spend_resource_payment(
            const card_game_payment_plan &plan) noexcept;
        int32_t create_resource_cost_plan(uint32_t owner_id,
            const card_game_cost &cost, uint32_t variable_amount,
            card_game_cost_plan *plan) const noexcept;
        int32_t spend_resource_cost(
            const card_game_cost_plan &plan) noexcept;
        int32_t register_allowance_predicate(uint32_t predicate_id,
            card_game_allowance_predicate predicate, void *user_data) noexcept;
        int32_t grant_action_allowance(uint32_t owner_id, uint32_t action_id,
            uint32_t action_tags, uint32_t uses, uint64_t expiry_epoch,
            uint32_t source_instance, uint32_t source_effect_id,
            uint32_t predicate_id, uint32_t predicate_context_id,
            uint32_t *allowance_id) noexcept;
        int32_t consume_action_allowance(uint32_t owner_id,
            uint32_t action_id, uint32_t action_tags, uint64_t epoch,
            uint32_t *allowance_id) noexcept;
        int32_t open_choice(uint32_t player_id, card_game_choice_kind kind,
            uint64_t deadline_epoch, uint32_t default_option_id,
            uint32_t *choice_id) noexcept;
        int32_t add_choice_option(uint32_t choice_id,
            const card_game_choice_option &option) noexcept;
        int32_t choose_option(uint32_t choice_id, uint32_t player_id,
            uint32_t option_id, uint64_t epoch) noexcept;
        int32_t get_choice(uint32_t choice_id,
            card_game_choice *choice) const noexcept;
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
        int32_t get_player_mana(uint32_t player_id, uint32_t *mana) const noexcept;
        int32_t get_board_count(uint32_t player_id, uint32_t *count) const noexcept;
        int32_t get_deck_count(uint32_t player_id, uint32_t *count) const noexcept;
        int32_t get_hand_count(uint32_t player_id, uint32_t *count) const noexcept;
        int32_t hand_inspect(uint32_t player_id, uint32_t index,
            card_game_deck_card *card) const noexcept;
        int32_t draw_to_hand(uint32_t player_id,
            card_game_deck_card *card) noexcept;
        int32_t hand_remove_instance(uint32_t player_id, uint32_t instance_id,
            card_game_deck_card *card) noexcept;
        int32_t mulligan_hand(uint32_t player_id, const uint32_t *instance_ids,
            uint32_t count, uint64_t *random_state) noexcept;
        int32_t play_card_from_hand(uint32_t player_id, uint32_t instance_id,
            uint32_t target_instance, void *context) noexcept;
        int32_t zone_push_top(uint32_t player_id, uint32_t zone_id,
            uint32_t card_id, uint32_t *instance_id) noexcept;
        int32_t zone_push_bottom(uint32_t player_id, uint32_t zone_id,
            uint32_t card_id, uint32_t *instance_id) noexcept;
        int32_t zone_insert_at(uint32_t player_id, uint32_t zone_id,
            uint32_t index, uint32_t card_id, uint32_t *instance_id) noexcept;
        int32_t zone_inspect(uint32_t player_id, uint32_t zone_id,
            uint32_t index, card_game_deck_card *card) const noexcept;
        int32_t zone_pop_top(uint32_t player_id, uint32_t zone_id,
            card_game_deck_card *card) noexcept;
        int32_t zone_pop_bottom(uint32_t player_id, uint32_t zone_id,
            card_game_deck_card *card) noexcept;
        int32_t zone_remove_instance(uint32_t player_id, uint32_t zone_id,
            uint32_t instance_id, card_game_deck_card *card) noexcept;
        int32_t zone_move_instance(uint32_t player_id, uint32_t source_zone_id,
            uint32_t destination_zone_id, uint32_t instance_id) noexcept;
        int32_t zone_shuffle(uint32_t player_id, uint32_t zone_id,
            uint64_t *random_state) noexcept;
        int32_t get_zone_count(uint32_t player_id, uint32_t zone_id,
            uint32_t *count) const noexcept;
        int32_t deck_push_top(uint32_t player_id, uint32_t card_id) noexcept;
        int32_t deck_push_bottom(uint32_t player_id, uint32_t card_id) noexcept;
        int32_t deck_insert_at(uint32_t player_id, uint32_t index,
            uint32_t card_id) noexcept;
        int32_t deck_push_top_instance(uint32_t player_id,
            uint32_t instance_id, uint32_t card_id) noexcept;
        int32_t deck_push_bottom_instance(uint32_t player_id,
            uint32_t instance_id, uint32_t card_id) noexcept;
        int32_t deck_insert_instance_at(uint32_t player_id, uint32_t index,
            uint32_t instance_id, uint32_t card_id) noexcept;
        int32_t deck_inspect(uint32_t player_id, uint32_t index,
            card_game_deck_card *card) const noexcept;
        int32_t deck_get_instance(uint32_t player_id, uint32_t instance_id,
            card_game_deck_card *card) const noexcept;
        int32_t deck_draw_instance(uint32_t player_id, uint32_t instance_id,
            card_game_deck_card *card) noexcept;
        int32_t deck_peek_top(uint32_t player_id, uint32_t *card_id) const noexcept;
        int32_t deck_peek_bottom(uint32_t player_id,
            uint32_t *card_id) const noexcept;
        int32_t deck_draw_top(uint32_t player_id, uint32_t *card_id) noexcept;
        int32_t deck_draw_top(uint32_t player_id,
            card_game_deck_card *card) noexcept;
        int32_t deck_draw_bottom(uint32_t player_id, uint32_t *card_id) noexcept;
        int32_t deck_remove(uint32_t player_id, uint32_t card_id) noexcept;
        int32_t shuffle_deck(uint32_t player_id, uint64_t *random_state) noexcept;
        int32_t shuffle_deck(uint32_t player_id) noexcept;
        int32_t set_random_seed(uint64_t seed) noexcept;
        int32_t get_random_state(uint64_t *state) const noexcept;
        int32_t get_turn(uint32_t *turn_number, uint32_t *active_player) const noexcept;
        int32_t get_current_phase(uint32_t *phase_id) const noexcept;
        int32_t get_phase(uint32_t phase_id,
            card_game_phase_definition *phase) const noexcept;
        int32_t get_instance(uint32_t player_id, uint32_t index,
            card_game_card_instance *instance) const noexcept;
        int32_t add_card_modifier(uint32_t player_id, uint32_t instance_index,
            int32_t attack_delta, int32_t health_delta,
            card_game_modifier_duration duration, uint32_t source_effect_id,
            uint32_t *modifier_id) noexcept;
        int32_t remove_card_modifier(uint32_t modifier_id) noexcept;
        int32_t get_card_modifier(uint32_t modifier_id,
            card_game_card_modifier *modifier) const noexcept;
        int32_t get_effective_instance_stats(uint32_t player_id,
            uint32_t instance_index, int32_t *attack,
            int32_t *health) const noexcept;
        int32_t resolve_combat(uint32_t attacking_player,
            uint32_t attacker_index, uint32_t defending_player,
            uint32_t defender_index, card_game_combat_mode mode) noexcept;
        int32_t get_snapshot(card_game_snapshot *snapshot) const noexcept;
        int32_t get_rules_hash(uint64_t *hash) const noexcept;
        int32_t get_state_hash(uint64_t *hash) const noexcept;
        int32_t apply_snapshot(const card_game_snapshot &snapshot) noexcept;
        int32_t create_delta(const card_game_snapshot &baseline,
            card_game_delta *delta) const noexcept;
        int32_t apply_delta(const card_game_delta &delta) noexcept;
        int32_t submit_command(const card_game_command &command,
            void *context) noexcept;
        int32_t get_command_record_count(uint32_t *count) const noexcept;
        int32_t get_command_record(uint32_t index,
            card_game_command_record *record) const noexcept;
        int32_t serialize_command_records(uint8_t *output,
            uint32_t output_capacity, uint32_t *output_size) const noexcept;
        int32_t deserialize_command_records(const uint8_t *input,
            uint32_t input_size) noexcept;
        int32_t replay_command_records(
            const card_game_command_record *records, uint32_t record_count,
            void *context) noexcept;
};

#endif
