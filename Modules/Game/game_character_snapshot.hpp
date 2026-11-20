#ifndef GAME_CHARACTER_SNAPSHOT_HPP
# define GAME_CHARACTER_SNAPSHOT_HPP

# include "../Basic/basic.hpp"

class ft_string;
class game_character;
class game_inventory;

/* Write the character checkpoint accepted by game_world::load_from_buffer.
 * A standalone character checkpoint contains an empty world event list. */
int32_t game_character_save_snapshot(ft_string &output,
    const game_character &character, const game_inventory &inventory) noexcept;

#endif
