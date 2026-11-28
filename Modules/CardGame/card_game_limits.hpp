#ifndef CARD_GAME_LIMITS_HPP
# define CARD_GAME_LIMITS_HPP

# include <cstdint>

static const uint32_t FT_CARD_GAME_MAX_CARDS = 128U;
static const uint32_t FT_CARD_GAME_MAX_PLAYERS = 8U;
static const uint32_t FT_CARD_GAME_MAX_ZONES = 32U;
static const uint32_t FT_CARD_GAME_MAX_CARD_DEFINITIONS = 65536U;
static const uint32_t FT_CARD_GAME_CARD_DEFINITION_INITIAL_CAPACITY = 256U;
/* Shared defensive ceiling; buffers grow on demand from a 256-record start. */
static const uint32_t FT_CARD_GAME_MAX_EVENT_RECORDS = 16777216U;

#endif
