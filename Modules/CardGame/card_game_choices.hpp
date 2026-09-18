#ifndef CARD_GAME_CHOICES_HPP
# define CARD_GAME_CHOICES_HPP

# include <cstdint>
# include "card_game_limits.hpp"
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_CARD_GAME_MAX_CHOICES = 256U;
static const uint32_t FT_CARD_GAME_MAX_CHOICE_OPTIONS = 32U;
static const uint32_t FT_CARD_GAME_CHOICES_SERIAL_MAGIC = 0x43484331U;
static const uint32_t FT_CARD_GAME_CHOICES_SERIAL_VERSION = 1U;
static const uint32_t FT_CARD_GAME_CHOICE_RECORD_BYTES = 420U;
static const uint32_t FT_CARD_GAME_CHOICES_SERIALIZED_BYTES = 12U
    + (FT_CARD_GAME_MAX_CHOICES * FT_CARD_GAME_CHOICE_RECORD_BYTES);

enum card_game_choice_kind : uint8_t
{
    CARD_GAME_CHOICE_TARGET = 1U,
    CARD_GAME_CHOICE_MODE = 2U,
    CARD_GAME_CHOICE_OBJECT = 3U,
    CARD_GAME_CHOICE_ORDER = 4U,
    CARD_GAME_CHOICE_NUMBER = 5U,
    CARD_GAME_CHOICE_BOOLEAN = 6U,
    CARD_GAME_CHOICE_PAYMENT = 7U,
    CARD_GAME_CHOICE_REPLACEMENT = 8U,
    CARD_GAME_CHOICE_TRIGGER_ORDER = 9U,
    CARD_GAME_CHOICE_MULLIGAN = 10U
};

struct card_game_choice_option
{
    uint32_t option_id;
    uint32_t value_a;
    uint32_t value_b;
};

struct card_game_choice
{
    uint32_t choice_id;
    uint32_t player_id;
    card_game_choice_kind kind;
    uint32_t option_count;
    card_game_choice_option options[FT_CARD_GAME_MAX_CHOICE_OPTIONS];
    uint64_t deadline_epoch;
    uint32_t default_option_id;
    uint32_t selected_option_id;
    ft_bool resolved;
};

struct card_game_choice_snapshot
{
    uint32_t count;
    uint32_t next_id;
    card_game_choice *choices;
};

class card_game_choice_ledger
{
    private:
        uint8_t _initialised_state;
        uint32_t _count;
        uint32_t _next_id;
        card_game_choice _choices[FT_CARD_GAME_MAX_CHOICES];

        card_game_choice_ledger(const card_game_choice_ledger &other) = delete;
        card_game_choice_ledger(card_game_choice_ledger &&other) = delete;
        card_game_choice_ledger &operator=(
            const card_game_choice_ledger &other) = delete;
        card_game_choice_ledger &operator=(
            card_game_choice_ledger &&other) = delete;

        int32_t find(uint32_t choice_id, uint32_t *index) const noexcept;
        int32_t find_option(const card_game_choice &choice,
            uint32_t option_id) const noexcept;

    public:
        card_game_choice_ledger() noexcept;
        ~card_game_choice_ledger() noexcept;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_choice_ledger &other) noexcept;
        static int32_t release_snapshot(
            card_game_choice_snapshot *snapshot) noexcept;
        int32_t get_snapshot(
            card_game_choice_snapshot *snapshot) const noexcept;
        static int32_t clone_snapshot(
            const card_game_choice_snapshot &source,
            card_game_choice_snapshot *destination) noexcept;
        int32_t apply_snapshot(
            const card_game_choice_snapshot &snapshot) noexcept;
        static ft_bool snapshots_equal(
            const card_game_choice_snapshot &first,
            const card_game_choice_snapshot &second) noexcept;
        int32_t open(uint32_t player_id, card_game_choice_kind kind,
            uint64_t deadline_epoch, uint32_t default_option_id,
            uint32_t *choice_id) noexcept;
        int32_t add_option(uint32_t choice_id,
            const card_game_choice_option &option) noexcept;
        int32_t choose(uint32_t choice_id, uint32_t player_id,
            uint32_t option_id, uint64_t epoch) noexcept;
        int32_t resolve_expired(uint64_t epoch) noexcept;
        int32_t get(uint32_t choice_id, card_game_choice *choice) const noexcept;
        int32_t serialize(uint8_t *output, uint32_t output_capacity,
            uint32_t *output_size) const noexcept;
        int32_t deserialize(const uint8_t *input,
            uint32_t input_size) noexcept;
        uint32_t size() const noexcept;
};

#endif
