#ifndef CARD_GAME_FORMAT_HPP
# define CARD_GAME_FORMAT_HPP

# include <cstdint>
# include "card_game_deck_code.hpp"
# include "card_game_limits.hpp"
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_CARD_GAME_FORMAT_MAX_LEGAL_CARDS = 65536U;
static const uint32_t FT_CARD_GAME_FORMAT_MAX_BAN_ENTRIES = 4096U;
static const uint32_t FT_CARD_GAME_FORMAT_MAX_LIMIT_GROUPS = 1024U;
static const uint32_t FT_CARD_GAME_FORMAT_MAX_EXCEPTIONS = 4096U;
static const uint32_t FT_CARD_GAME_FORMAT_SCHEMA_VERSION = 1U;

enum card_game_format_card_policy : uint8_t
{
    CARD_GAME_FORMAT_CARD_LEGAL = 0U,
    CARD_GAME_FORMAT_CARD_BANNED = 1U,
    CARD_GAME_FORMAT_CARD_RESTRICTED = 2U
};

struct card_game_format_card_rule
{
    uint32_t card_id;
    uint32_t copy_limit;
    uint32_t limit_group_id;
    card_game_format_card_policy policy;
};

struct card_game_format_ban_entry
{
    uint32_t card_id;
    uint32_t copy_limit;
    uint32_t zone_mask;
    uint32_t source_revision;
};

struct card_game_format_exception
{
    uint32_t exception_id;
    uint32_t card_id;
    uint32_t rule_id;
    uint32_t replacement_program_id;
    uint32_t priority;
};

struct card_game_format_config
{
    uint32_t format_id;
    uint32_t revision;
    uint32_t profile_id;
    uint32_t corpus_version;
    uint32_t global_copy_limit;
    uint32_t minimum_main_cards;
    uint32_t maximum_main_cards;
    uint32_t minimum_extra_cards;
    uint32_t maximum_extra_cards;
    uint32_t minimum_side_cards;
    uint32_t maximum_side_cards;
    uint32_t legal_card_count;
    card_game_format_card_rule legal_cards[FT_CARD_GAME_FORMAT_MAX_LEGAL_CARDS];
    uint32_t ban_count;
    card_game_format_ban_entry bans[FT_CARD_GAME_FORMAT_MAX_BAN_ENTRIES];
    uint32_t exception_count;
    card_game_format_exception exceptions[FT_CARD_GAME_FORMAT_MAX_EXCEPTIONS];
};

struct card_game_format_diagnostic
{
    int32_t error_code;
    uint32_t card_id;
    uint32_t zone_id;
    uint32_t observed_count;
    uint32_t permitted_count;
    uint32_t deciding_rule_id;
};

struct card_game_format_hash
{
    uint8_t bytes[32];
};

class card_game_format
{
    private:
        uint8_t _initialised_state;
        card_game_format_config *_config;
        card_game_format_hash _hash;

        card_game_format(const card_game_format &other) = delete;
        card_game_format(card_game_format &&other) = delete;
        card_game_format &operator=(const card_game_format &other) = delete;
        card_game_format &operator=(card_game_format &&other) = delete;

        int32_t validate_config(const card_game_format_config &config) const noexcept;
        int32_t calculate_hash(const card_game_format_config &config,
            card_game_format_hash *hash) const noexcept;
        int32_t find_rule(uint32_t card_id,
            card_game_format_card_rule *rule) const noexcept;

    public:
        card_game_format() noexcept;
        ~card_game_format() noexcept;

        int32_t initialize(const card_game_format_config &config) noexcept;
        int32_t destroy() noexcept;
        int32_t move(card_game_format &other) noexcept;
        int32_t get_config(card_game_format_config *config) const noexcept;
        int32_t get_hash(card_game_format_hash *hash) const noexcept;
        int32_t is_card_legal(uint32_t card_id,
            uint32_t *copy_limit) const noexcept;
        int32_t get_exception(uint32_t card_id, uint32_t rule_id,
            card_game_format_exception *exception) const noexcept;
        int32_t validate_deck(const card_game_deck &deck,
            card_game_format_diagnostic *diagnostic) const noexcept;
};

#endif
