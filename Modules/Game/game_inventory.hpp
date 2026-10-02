#ifndef GAME_INVENTORY_HPP
# define GAME_INVENTORY_HPP

#ifndef USE_INVENTORY_WEIGHT
# define USE_INVENTORY_WEIGHT 1
#endif

#ifndef USE_INVENTORY_SLOTS
# define USE_INVENTORY_SLOTS 1
#endif

#include "game_item.hpp"
#include "../Template/map.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_inventory
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        friend class game_crafting;
        ft_map<int32_t, ft_sharedptr<game_item> > _items;
        ft_size_t                              _capacity;
        ft_size_t                              _used_slots;
        int32_t                                 _weight_limit;
        int32_t                                 _current_weight;
        int32_t                                 _next_slot;
        uint64_t                                _revision;
        pt_recursive_mutex                           *_mutex;
        uint8_t                             _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;


        ft_bool check_item_valid(const ft_sharedptr<game_item> &item) const noexcept;
        int32_t add_item_transactional(
            const ft_sharedptr<game_item> &item) noexcept;
        int32_t add_item_in_place(const ft_sharedptr<game_item> &item) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t bump_revision() noexcept;

    public:
        game_inventory() noexcept;
        game_inventory(const game_inventory &other) noexcept = delete;
        game_inventory(game_inventory &&other) noexcept = delete;
        virtual ~game_inventory() noexcept;
        game_inventory &operator=(const game_inventory &other) noexcept = delete;
        game_inventory &operator=(game_inventory &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(ft_size_t capacity, int32_t weight_limit) noexcept;
        int32_t initialize(const game_inventory &other) noexcept;
        int32_t initialize(game_inventory &&other) noexcept;
        int32_t move(game_inventory &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        ft_map<int32_t, ft_sharedptr<game_item> >       &get_items() noexcept;
        const ft_map<int32_t, ft_sharedptr<game_item> > &get_items() const noexcept;

        ft_size_t get_capacity() const noexcept;
        void   resize(ft_size_t capacity) noexcept;
        ft_size_t get_used() const noexcept;
        void   set_used_slots(ft_size_t used) noexcept;
        ft_bool   is_full() const noexcept;
        int32_t    get_weight_limit() const noexcept;
        void   set_weight_limit(int32_t limit) noexcept;
        int32_t    get_current_weight() const noexcept;
        void   set_current_weight(int32_t weight) noexcept;

        int32_t  add_item(const ft_sharedptr<game_item> &item) noexcept;
        void remove_item(int32_t slot) noexcept;

        /* Transfer is in-process only (no durable request ID/persistence).
         * When enabled, inventory mutexes are acquired in object-address order.
         * quantity must be positive; a missing/empty source slot returns
         * FT_ERR_NOT_FOUND, and self-transfer returns FT_ERR_INVALID_ARGUMENT.
         * Direct mutation through get_items() or externally shared game_item
         * objects bypasses revisions; callers must serialize those accesses
         * and must not use them concurrently with revision-checked transfers. */
        int32_t get_revision(uint64_t *revision) const noexcept;
        int32_t transfer_to(game_inventory &destination, int32_t source_slot,
            int32_t quantity, uint64_t expected_source_revision,
            uint64_t expected_destination_revision) noexcept;

        int32_t  count_item(int32_t item_id) const noexcept;
        ft_bool has_item(int32_t item_id) const noexcept;

        int32_t  count_rarity(int32_t rarity) const noexcept;
        ft_bool has_rarity(int32_t rarity) const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

#endif
