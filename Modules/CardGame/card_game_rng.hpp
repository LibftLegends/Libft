#ifndef CARD_GAME_RNG_HPP
# define CARD_GAME_RNG_HPP

# include <cstdint>

static inline uint64_t card_game_random_next(uint64_t *state) noexcept
{
	uint64_t value;

	value = *state;
	value ^= value >> 12U;
	value ^= value << 25U;
	value ^= value >> 27U;
	*state = value;
	return (value * 2685821657736338717ULL);
}

#endif
