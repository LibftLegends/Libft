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

struct game_crafting_ingredient
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t             _item_id;
        int32_t             _count;
        int32_t             _rarity;
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
        int32_t initialize(int32_t item_id, int32_t count, int32_t rarity) noexcept;

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

struct game_crafting_output
{
    ft_sharedptr<game_item> item_definition;
    int32_t                 quantity;
};

/* One unsatisfied ingredient group. A rarity of -1 means any rarity. */
struct game_crafting_missing_ingredient
{
    int32_t item_id;
    int32_t rarity;
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
    ft_size_t _result_capacity;
    ft_size_t _result_used_slots;
    int32_t _result_weight_limit;
    int32_t _result_current_weight;
    int32_t _result_next_slot;
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
        uint8_t                                     _initialised_state;
        mutable pt_recursive_mutex *_mutex;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;

        int32_t  lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;

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

        int32_t register_recipe(int32_t recipe_id, ft_vector<game_crafting_ingredient> &&ingredients) noexcept;
        int32_t register_recipe(int32_t recipe_id, ft_vector<game_crafting_ingredient> &&ingredients,
            ft_bool initially_unlocked) noexcept;
        int32_t register_recipe(int32_t recipe_id,
            ft_vector<game_crafting_ingredient> &&ingredients,
            const ft_sharedptr<game_item> &output_definition,
            int32_t output_quantity, ft_bool initially_unlocked = FT_TRUE) noexcept;
        int32_t unlock_recipe(int32_t recipe_id) noexcept;
        int32_t is_recipe_unlocked(int32_t recipe_id, ft_bool *unlocked) const noexcept;
        /* Station context is intentionally not considered by this query yet. */
        int32_t query_recipe(int32_t recipe_id, const game_inventory &inventory,
            game_crafting_query_result *result,
            ft_vector<game_crafting_missing_ingredient> *missing_groups) const noexcept;
        int32_t prepare_craft(const game_inventory &inventory, int32_t recipe_id,
            int32_t batch_count, game_crafting_prepared *prepared) noexcept;
        int32_t commit_craft(game_inventory &inventory,
            game_crafting_prepared *prepared) noexcept;
        /* Compatibility-only path for recipes registered without a bound output.
         * Recipes with authoritative outputs must use prepare_craft/commit_craft. */
        int32_t craft_item(game_inventory &inventory, int32_t recipe_id, const ft_sharedptr<game_item> &result) noexcept;
        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_crafting &other) noexcept;
};

#endif
