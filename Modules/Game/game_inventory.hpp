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
#include <vector>
#include <map>

struct game_inventory_item_snapshot
{
	int32_t slot;
	int32_t item_id;
	uint64_t instance_id;
	int32_t stack_size;
	int32_t max_stack;
	int32_t rarity;
	int32_t width;
	int32_t height;
	int32_t modifier1_id;
	int32_t modifier1_value;
	int32_t modifier2_id;
	int32_t modifier2_value;
	int32_t modifier3_id;
	int32_t modifier3_value;
	int32_t modifier4_id;
	int32_t modifier4_value;
};

struct game_inventory_transfer_result
{
    uint64_t transaction_scope_id;
    uint64_t transaction_id;
    uint64_t source_revision;
    uint64_t destination_revision;
    int32_t transferred_quantity;
    int32_t result_code;
};

struct game_inventory_quantity_result
{
	uint64_t transaction_scope_id;
	uint64_t transaction_id;
	uint64_t inventory_revision;
	int32_t item_id;
	int32_t requested_quantity;
	int32_t removed_quantity;
	int32_t result_code;
};

struct game_inventory_snapshot
{
	ft_size_t capacity;
	ft_size_t used_slots;
	int32_t weight_limit;
	int32_t current_weight;
	int32_t next_slot;
	uint64_t revision;
	std::vector<game_inventory_item_snapshot> items;
};

class game_inventory
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        struct TransferCacheEntry
        {
            game_inventory *destination;
            int32_t source_slot;
            int32_t quantity;
            uint64_t expected_source_revision;
            uint64_t expected_destination_revision;
            uint64_t transaction_scope_id;
            game_inventory_transfer_result result;
            ft_bool completed;
        };
        struct QuantityRemoveCacheEntry
        {
            int32_t item_id;
            int32_t quantity;
            uint64_t expected_revision;
            game_inventory_quantity_result result;
        };
        static const ft_size_t TRANSFER_CACHE_LIMIT = 4096U;
        friend class game_crafting;
        ft_map<int32_t, ft_sharedptr<game_item> > _items;
        std::map<std::pair<uint64_t, uint64_t>, TransferCacheEntry> _transfer_cache;
        std::map<std::pair<uint64_t, uint64_t>, QuantityRemoveCacheEntry>
            _quantity_remove_cache;
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
            const ft_sharedptr<game_item> &item,
            uint64_t expected_revision, ft_bool check_revision,
            int32_t *inserted_slot_out) noexcept;
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

        #ifdef LIBFT_TEST_BUILD
        ft_map<int32_t, ft_sharedptr<game_item> >       &get_items() noexcept;
        const ft_map<int32_t, ft_sharedptr<game_item> > &get_items() const noexcept;
        #endif
		/* Produces value-only records under the inventory and map locks.
		 * Entries remain valid after the inventory changes. */
		int32_t get_snapshot(
			std::vector<game_inventory_item_snapshot> &items_out,
			uint64_t *revision_out) const noexcept;
		int32_t get_snapshot(game_inventory_snapshot &snapshot_out) const noexcept;
		/* Returns a shared handle to the exact item object at slot only when
		 * the inventory revision matches. The caller owns the returned handle;
		 * expected_revision is checked while the inventory and map are locked. */
		int32_t copy_item_at_revision(int32_t slot,
			uint64_t expected_revision,
			ft_sharedptr<game_item> **item_out) const noexcept;
		/* Replaces contents from a value snapshot without replacing this
		 * inventory object or its process-local request caches. Caller must
		 * exclude concurrent access while restoring. */
		int32_t restore_snapshot(
			const game_inventory_snapshot &snapshot) noexcept;

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
        /* Adds an item only at the expected revision and returns its slot.
         * Identified single instances are never merged into another stack. */
        int32_t add_item_revisioned(const ft_sharedptr<game_item> &item,
            uint64_t expected_revision, int32_t *inserted_slot_out) noexcept;
        int32_t clear_items() noexcept;
        void remove_item(int32_t slot) noexcept;
        /* Removes one identified physical instance only if both the slot's
         * instance ID and inventory revision still match. */
        int32_t remove_item_instance(int32_t slot,
            uint64_t expected_revision, uint64_t instance_id) noexcept;

        /* Transfers are in-process only (no durable journal). The transaction
         * overload retains a bounded source-inventory request cache. When
         * enabled, inventory mutexes are acquired in object-address order.
         * quantity must be positive; a missing/empty source slot returns
         * FT_ERR_NOT_FOUND, and self-transfer returns FT_ERR_INVALID_ARGUMENT.
         * The legacy overload is not retry-idempotent. The transaction overload
         * requires a nonzero ID and output pointer, returns the stored result
         * for exact retries and rejects ID reuse with changed parameters. The
         * scoped overload keys IDs by (scope, ID), for example actor and
         * request ID. Its
         * process-local cache holds 4,096 entries; when full, new IDs fail with
         * FT_ERR_FULL until the source is reinitialized or destroyed. This is
         * not durable across process restart. */
        int32_t get_revision(uint64_t *revision) const noexcept;
        /* Restores a persisted revision after the inventory contents load.
         * The caller must exclude concurrent inventory access. */
        int32_t restore_revision(uint64_t revision) noexcept;
        int32_t transfer_to(game_inventory &destination, int32_t source_slot,
            int32_t quantity, uint64_t expected_source_revision,
            uint64_t expected_destination_revision) noexcept;
        int32_t transfer_to(game_inventory &destination, int32_t source_slot,
            int32_t quantity, uint64_t expected_source_revision,
            uint64_t expected_destination_revision, uint64_t transaction_id,
            game_inventory_transfer_result *result_out) noexcept;
        int32_t transfer_to(game_inventory &destination, int32_t source_slot,
            int32_t quantity, uint64_t expected_source_revision,
            uint64_t expected_destination_revision,
            uint64_t transaction_scope_id, uint64_t transaction_id,
            game_inventory_transfer_result *result_out) noexcept;

        /* Atomically consumes stackable quantity across matching stacks.
         * Identified physical items are excluded. Results (including failures)
         * are replayed by scoped request ID; this bounded cache is volatile. */
        int32_t remove_item_quantity(int32_t item_id, int32_t quantity,
            uint64_t expected_revision, uint64_t transaction_scope_id,
            uint64_t transaction_id,
            game_inventory_quantity_result *result_out) noexcept;

        int32_t  count_item(int32_t item_id) const noexcept;
        ft_bool has_item(int32_t item_id) const noexcept;

        int32_t  count_rarity(int32_t rarity) const noexcept;
        ft_bool has_rarity(int32_t rarity) const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

#endif
