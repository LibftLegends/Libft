#ifndef GAME_RECIPE_BOOK_HPP
# define GAME_RECIPE_BOOK_HPP

#include "../Template/map.hpp"
#include "../Template/vector.hpp"
#include "../Buffer/byte_buffer.hpp"
#include <stdint.h>

#define GAME_RECIPE_BOOK_FORMAT_VERSION 1U

struct game_recipe_unlock_record
{
    uint64_t source_event_id;
    uint32_t recipe_id;
};

/*
 * Recipe IDs are stable catalogue IDs supplied at initialization. Import
 * rejects unknown IDs transactionally; callers can retain the original save
 * bytes externally and retry after installing a catalogue that knows them.
 * This class is single-owner/single-threaded; callers must synchronize access.
 */
class game_recipe_book
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_map<uint32_t, uint8_t> _catalogue;
        ft_vector<game_recipe_unlock_record> _unlock_events;
        uint8_t _initialised_state;

    public:
        game_recipe_book() noexcept;
        game_recipe_book(const game_recipe_book &other) noexcept = delete;
        game_recipe_book(game_recipe_book &&other) noexcept = delete;
        ~game_recipe_book() noexcept;

        game_recipe_book &operator=(const game_recipe_book &other) noexcept = delete;
        game_recipe_book &operator=(game_recipe_book &&other) noexcept = delete;

        int32_t initialize(const ft_vector<uint32_t> &recipe_ids) noexcept;
        int32_t initialize(const ft_vector<uint32_t> &recipe_ids,
            const game_recipe_unlock_record *initial_unlocks,
            uint32_t initial_unlock_count) noexcept;
        int32_t destroy() noexcept;
        int32_t unlock_recipe(uint32_t recipe_id, uint64_t source_event_id,
            ft_bool *newly_unlocked) noexcept;
        int32_t is_recipe_known(uint32_t recipe_id, ft_bool *known) const noexcept;
        int32_t export_book(ft_byte_buffer &output) const noexcept;
        int32_t import_book(ft_byte_buffer &input) noexcept;
};

#endif
