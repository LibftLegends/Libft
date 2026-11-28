#ifndef GAME_RECIPE_BOOK_HPP
# define GAME_RECIPE_BOOK_HPP

#include "../Template/vector.hpp"
#include "../Buffer/byte_buffer.hpp"
#include <stdint.h>

#define GAME_RECIPE_BOOK_FORMAT_VERSION 1U

struct game_recipe_unlock_record
{
    uint64_t source_event_id;
    uint32_t recipe_id;
};

struct game_recipe_unlock_result
{
    ft_bool event_newly_recorded;
    ft_bool recipe_newly_known;
};

/*
 * Recipe IDs are stable catalogue IDs supplied at initialization. Unknown
 * imported IDs remain quarantined and round-trip for future catalogues.
 * This class is single-owner/single-threaded; callers must synchronize access.
 */
class game_recipe_book
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        struct catalogue_entry
        {
            uint32_t recipe_id;
            ft_bool known;
        };

        ft_vector<catalogue_entry> _catalogue;
        ft_vector<game_recipe_unlock_record> _unlock_events;
        ft_vector<game_recipe_unlock_record> _event_index;
        uint64_t _revision;
        uint32_t _quarantined_count;
        uint8_t _initialised_state;
        const catalogue_entry *find_recipe(uint32_t recipe_id) const noexcept;
        catalogue_entry *find_recipe(uint32_t recipe_id) noexcept;
        void rebuild_cached_state() noexcept;

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
        int32_t unlock_recipe_with_result(uint32_t recipe_id,
            uint64_t source_event_id,
            game_recipe_unlock_result *result_out) noexcept;
        int32_t unlock_recipe(uint32_t recipe_id, uint64_t source_event_id,
            ft_bool *newly_unlocked) noexcept;
        int32_t is_recipe_known(uint32_t recipe_id, ft_bool *known) const noexcept;
        /* Stable fingerprint of the current unlock-event state for optimistic
         * query/commit coordination. */
        int32_t get_revision(uint64_t *revision_out) const noexcept;
        int32_t quarantined_unlock_count(uint32_t *count) const noexcept;
        int32_t export_book(ft_byte_buffer &output) const noexcept;
        int32_t import_book(ft_byte_buffer &input) noexcept;
};

#endif
