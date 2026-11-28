#ifndef CARD_GAME_DECK_CODE_HPP
# define CARD_GAME_DECK_CODE_HPP

# include <cstdint>
# include "card_game_limits.hpp"
# include "../Basic/basic.hpp"
# include "../CPP_class/class_string.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_CARD_GAME_DECK_CODE_MAX_ZONES = 8U;
static const uint32_t FT_CARD_GAME_DECK_CODE_MAX_ENTRIES = 128U;
static const uint32_t FT_CARD_GAME_DECK_CODE_MAX_TOTAL_CARDS = 500U;
static const uint32_t FT_CARD_GAME_DECK_CODE_MAX_BYTES = 4096U;
static const uint32_t FT_CARD_GAME_DECK_CODE_MAX_TEXT_BYTES = 8192U;
static const uint32_t FT_CARD_GAME_DECK_CODE_VERSION = 1U;
static const uint32_t CARD_GAME_DECK_ZONE_MAIN = 1U;
static const uint32_t CARD_GAME_DECK_ZONE_EXTRA = 2U;
static const uint32_t CARD_GAME_DECK_ZONE_SIDE = 3U;
static const uint32_t CARD_GAME_DECK_ZONE_COMMANDER = 4U;
static const uint32_t CARD_GAME_DECK_ZONE_COMPANION = 5U;
static const uint32_t CARD_GAME_DECK_ZONE_HERO = 6U;
static const uint32_t CARD_GAME_DECK_ZONE_QUEST = 7U;
static const uint32_t CARD_GAME_DECK_FLAG_PRINTINGS = 1U << 0U;

struct card_game_deck_entry
{
    uint32_t definition_id;
    uint32_t printing_id;
    uint32_t quantity;
};

struct card_game_deck_zone
{
    uint32_t zone_id;
    uint32_t entry_count;
    card_game_deck_entry entries[FT_CARD_GAME_DECK_CODE_MAX_ENTRIES];
};

struct card_game_deck
{
    uint32_t profile_id;
    uint32_t format_id;
    uint32_t corpus_version;
    uint32_t flags;
    uint32_t zone_count;
    card_game_deck_zone zones[FT_CARD_GAME_DECK_CODE_MAX_ZONES];
};

struct card_game_deck_hash_value
{
    uint8_t bytes[32];
};

int32_t card_game_deck_encode(const card_game_deck &deck,
    ft_string *output) noexcept;
int32_t card_game_deck_decode(const ft_string &input,
    card_game_deck *deck) noexcept;
int32_t card_game_deck_hash(const card_game_deck &deck,
    card_game_deck_hash_value *hash) noexcept;

#endif
