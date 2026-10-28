#ifndef GAME_CRAFTING_HPP
# define GAME_CRAFTING_HPP

#include "../Template/map.hpp"
#include "../Template/vector.hpp"
#include "game_inventory.hpp"
#include "game_item.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <vector>

struct game_crafting_ingredient
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t             _item_id;
        int32_t             _tag_id;
        int32_t             _count;
        int32_t             _rarity;
        int32_t             _alternative_item_ids[8];
        uint32_t            _alternative_count;
        uint8_t             _initialised_state;
        mutable pt_recursive_mutex *_mutex;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;

        int32_t  lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        game_crafting_ingredient() noexcept;
        game_crafting_ingredient(const game_crafting_ingredient &other) noexcept;
        game_crafting_ingredient(game_crafting_ingredient &&other) noexcept;
        virtual ~game_crafting_ingredient() noexcept;
        game_crafting_ingredient &operator=(const game_crafting_ingredient &other) noexcept = delete;
        game_crafting_ingredient &operator=(game_crafting_ingredient &&other) noexcept = delete;

        int32_t initialize(const game_crafting_ingredient &other) noexcept;
        int32_t initialize(game_crafting_ingredient &&other) noexcept;
        int32_t move(game_crafting_ingredient &other) noexcept;
        int32_t destroy() noexcept;
        static const uint32_t MAX_ALTERNATIVE_ITEM_IDS = 8U;
        int32_t initialize(int32_t item_id, int32_t count, int32_t rarity) noexcept;
        int32_t initialize_alternatives(const int32_t *item_ids,
            uint32_t item_id_count, int32_t count, int32_t rarity) noexcept;
        int32_t initialize_tag(int32_t tag_id, int32_t count,
            int32_t rarity = -1) noexcept;
        int32_t get_tag_id() const noexcept;
        uint32_t get_alternative_count() const noexcept;
        int32_t get_alternative_item_id(uint32_t index) const noexcept;
        ft_bool matches_item_id(int32_t item_id) const noexcept;

        int32_t  enable_thread_safety() noexcept;
        int32_t  disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t get_item_id() const noexcept;
        void set_item_id(int32_t item_id) noexcept;

        int32_t get_count() const noexcept;
        void set_count(int32_t count) noexcept;

        int32_t get_rarity() const noexcept;
        void set_rarity(int32_t rarity) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

struct game_crafting_byproduct
{
    int32_t item_id;
    int32_t quantity;
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
    uint64_t instance_id;
};

/* Recipes may declare up to this many value-copied returned outputs. */
static constexpr ft_size_t GAME_CRAFTING_MAX_BYPRODUCTS = 16U;

struct game_crafting_output
{
    ft_sharedptr<game_item> item_definition;
    int32_t                 quantity;
    ft_size_t               byproduct_count;
    game_crafting_byproduct byproducts[GAME_CRAFTING_MAX_BYPRODUCTS];
};

struct game_crafting_product_snapshot
{
    int32_t item_id;
    int32_t quantity;
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
    uint64_t instance_id;
};

struct game_crafting_output_snapshot
{
    game_crafting_product_snapshot primary;
    uint32_t byproduct_count;
    game_crafting_product_snapshot byproducts[GAME_CRAFTING_MAX_BYPRODUCTS];
};

/* One unsatisfied ingredient group. A rarity of -1 means any rarity. */
struct game_crafting_missing_ingredient
{
    int32_t item_id;
    int32_t rarity;
    ft_size_t alternative_count;
    int32_t alternative_item_ids[8];
    int32_t required_units;
    int32_t available_units;
    int32_t missing_units;
};

/* Read-only recipe status. Missing groups are returned separately in recipe order. */
struct game_crafting_query_result
{
    ft_bool found;
    ft_bool known;
    ft_bool craftable;
    ft_bool station_available;
    ft_size_t missing_group_count;
    int32_t missing_units;
};

class game_crafting_prepared
{
    friend class game_crafting;
    ft_map<int32_t, ft_sharedptr<game_item>> _expected_items;
    ft_map<int32_t, ft_sharedptr<game_item>> _result_items;
    ft_size_t _expected_capacity;
    ft_size_t _expected_used_slots;
    int32_t _expected_weight_limit;
    int32_t _expected_current_weight;
    int32_t _expected_next_slot;
    uint64_t _expected_revision;
    ft_size_t _result_capacity;
    ft_size_t _result_used_slots;
    int32_t _result_weight_limit;
    int32_t _result_current_weight;
    int32_t _result_next_slot;
    int32_t _primary_output_slot;
    int32_t _primary_output_item_id;
    int32_t _primary_output_quantity;
    uint8_t _initialised_state;
    ft_bool _ready;

    int32_t clear() noexcept;

public:
    game_crafting_prepared() noexcept;
    game_crafting_prepared(const game_crafting_prepared &) = delete;
    game_crafting_prepared &operator=(const game_crafting_prepared &) = delete;
    ~game_crafting_prepared() noexcept;
    int32_t initialize() noexcept;
    int32_t destroy() noexcept;
    ft_bool is_ready() const noexcept;
};

class game_crafting
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_map<int32_t, ft_vector<game_crafting_ingredient>> _recipes;
        ft_map<int32_t, ft_bool> _unlocked_recipes;
        ft_map<int32_t, game_crafting_output> _recipe_outputs;
        ft_map<int32_t, int32_t> _recipe_stations;
        static const uint32_t MAXIMUM_ITEM_TAGS = 64U;
        static const uint32_t MAXIMUM_TAG_MEMBERS = 8U;
        int32_t _item_tag_ids[MAXIMUM_ITEM_TAGS];
        uint32_t _item_tag_member_counts[MAXIMUM_ITEM_TAGS];
        int32_t _item_tag_members[MAXIMUM_ITEM_TAGS][MAXIMUM_TAG_MEMBERS];
        uint32_t _item_tag_count;
        uint8_t                                     _initialised_state;
        mutable pt_recursive_mutex *_mutex;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;

        int32_t  lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t prepare_craft_internal(const game_inventory &inventory,
            int32_t recipe_id, int32_t station_id, int32_t batch_count,
            game_crafting_prepared *prepared, ft_bool include_outputs,
            game_crafting_output_snapshot *output_out) noexcept;

    public:
        game_crafting() noexcept;
        game_crafting(const game_crafting &other) noexcept = delete;
        game_crafting(game_crafting &&other) noexcept = delete;
        virtual ~game_crafting() noexcept;
        game_crafting &operator=(const game_crafting &other) noexcept = delete;
        game_crafting &operator=(game_crafting &&other) noexcept = delete;

        int32_t  enable_thread_safety() noexcept;
        int32_t  disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        ft_map<int32_t, ft_vector<game_crafting_ingredient>>       &get_recipes() noexcept;
        const ft_map<int32_t, ft_vector<game_crafting_ingredient>> &get_recipes() const noexcept;

        int32_t  get_error() const noexcept;
        const char *get_error_str() const noexcept;

        /* Register bounded stable tags before the first recipe. Tagged
         * ingredients are expanded to immutable alternatives at registration. */
        int32_t register_item_tag(int32_t tag_id, const int32_t *item_ids,
            uint32_t item_id_count) noexcept;

        int32_t register_recipe(int32_t recipe_id, ft_vector<game_crafting_ingredient> &&ingredients) noexcept;
        int32_t register_recipe(int32_t recipe_id, ft_vector<game_crafting_ingredient> &&ingredients,
            ft_bool initially_unlocked) noexcept;
        /* Bound outputs are templates; physical instance IDs are minted by the host per craft. */
        int32_t register_recipe(int32_t recipe_id,
            ft_vector<game_crafting_ingredient> &&ingredients,
            const ft_sharedptr<game_item> &output_definition,
            int32_t output_quantity, ft_bool initially_unlocked = FT_TRUE) noexcept;
        /* Byproducts are immutable item-value templates copied at registration. */
        int32_t register_recipe(int32_t recipe_id,
            ft_vector<game_crafting_ingredient> &&ingredients,
            const ft_sharedptr<game_item> &output_definition,
            int32_t output_quantity,
            const std::vector<game_crafting_byproduct> &byproducts,
            ft_bool initially_unlocked = FT_TRUE) noexcept;
        int32_t unlock_recipe(int32_t recipe_id) noexcept;
        int32_t lock_recipe(int32_t recipe_id) noexcept;
        int32_t is_recipe_unlocked(int32_t recipe_id, ft_bool *unlocked) const noexcept;
        int32_t query_recipe(int32_t recipe_id, const game_inventory &inventory,
            game_crafting_query_result *result,
            ft_vector<game_crafting_missing_ingredient> *missing_groups) const noexcept;
        /* Assign exactly once before unlocking; station 0 means no station. */
        int32_t set_recipe_station(int32_t recipe_id, int32_t station_id) noexcept;
        int32_t query_recipe_at_station(int32_t recipe_id,
            const game_inventory &inventory, int32_t station_id,
            game_crafting_query_result *result,
            ft_vector<game_crafting_missing_ingredient> *missing_groups) const noexcept;
        /* Evaluates against one caller-captured immutable inventory revision.
         * The snapshot and its items must remain unchanged for the duration
         * of this call. */
        int32_t query_recipe_at_station_snapshot(int32_t recipe_id,
            const game_inventory_snapshot &inventory_snapshot,
            int32_t station_id, game_crafting_query_result *result,
            ft_vector<game_crafting_missing_ingredient> *missing_groups) const noexcept;
        int32_t query_recipe_at_station_snapshot(int32_t recipe_id,
            const game_inventory_snapshot &inventory_snapshot,
            int32_t station_id, int32_t batch_count,
            game_crafting_query_result *result,
            ft_vector<game_crafting_missing_ingredient> *missing_groups) const noexcept;
        /* Returns immutable product metadata without checking ingredients or
         * changing inventory. Quantities are scaled for the requested batch. */
        int32_t query_recipe_output_snapshot(int32_t recipe_id,
            int32_t batch_count,
            game_crafting_output_snapshot *output_out) const noexcept;
        int32_t prepare_craft(const game_inventory &inventory, int32_t recipe_id,
            int32_t batch_count, game_crafting_prepared *prepared) noexcept;
        int32_t prepare_craft_at_station(const game_inventory &inventory,
            int32_t recipe_id, int32_t station_id, int32_t batch_count,
            game_crafting_prepared *prepared) noexcept;
        /* Consumes only recipe inputs at commit and returns a value-only
         * product plan for a timed job to hold until completion. */
        int32_t prepare_processing_at_station(
            const game_inventory &inventory, int32_t recipe_id,
            int32_t station_id, int32_t batch_count,
            game_crafting_prepared *prepared,
            game_crafting_output_snapshot *output_out) noexcept;
        int32_t commit_craft(game_inventory &inventory,
            game_crafting_prepared *prepared) noexcept;
        /* The host may provide a freshly minted ID for a unique one-item
         * primary output. Zero keeps the output definition's ID unchanged. */
        int32_t commit_craft(game_inventory &inventory,
            game_crafting_prepared *prepared,
            uint64_t output_instance_id) noexcept;
        int32_t commit_processing_inputs(game_inventory &inventory,
            game_crafting_prepared *prepared) noexcept;
        /* Compatibility-only path for recipes registered without a bound output.
         * Recipes with authoritative outputs must use prepare_craft/commit_craft. */
        int32_t craft_item(game_inventory &inventory, int32_t recipe_id, const ft_sharedptr<game_item> &result) noexcept;
        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_crafting &other) noexcept;
};

#endif
