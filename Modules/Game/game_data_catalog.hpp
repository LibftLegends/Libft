#ifndef GAME_DATA_CATALOG_HPP
# define GAME_DATA_CATALOG_HPP

#include "../Template/map.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include "game_crafting.hpp"

class game_item_definition
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t             _item_id;
        int32_t             _rarity;
        int32_t             _max_stack;
        int32_t             _width;
        int32_t             _height;
        int32_t             _weight;
        int32_t             _slot_requirement;
        mutable pt_recursive_mutex *_mutex;
        static thread_local int32_t _last_error;
        uint8_t         _initialised_state;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;
        struct item_values
        {
            int32_t item_id;
            int32_t rarity;
            int32_t max_stack;
            int32_t width;
            int32_t height;
            int32_t weight;
            int32_t slot_requirement;
        };

        int32_t read_values(item_values &values) const noexcept;
        void write_values(const item_values &values) noexcept;
        int32_t copy_values(const game_item_definition &other) noexcept;
        friend class game_data_catalog;
        static int32_t set_error(int32_t error_code) noexcept;

    public:
        game_item_definition() noexcept;
        game_item_definition(const game_item_definition &other) noexcept = delete;
        game_item_definition(game_item_definition &&other) noexcept = delete;
        virtual ~game_item_definition() noexcept;
        game_item_definition &operator=(const game_item_definition &other) noexcept = delete;
        game_item_definition &operator=(game_item_definition &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(int32_t item_id, int32_t rarity, int32_t max_stack, int32_t width,
            int32_t height, int32_t weight, int32_t slot_requirement) noexcept;
        int32_t initialize(const game_item_definition &other) noexcept;
        int32_t initialize(game_item_definition &&other) noexcept;
        int32_t move(game_item_definition &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

        int32_t get_item_id() const noexcept;
        void set_item_id(int32_t item_id) noexcept;

        int32_t get_rarity() const noexcept;
        void set_rarity(int32_t rarity) noexcept;

        int32_t get_max_stack() const noexcept;
        void set_max_stack(int32_t max_stack) noexcept;

        int32_t get_width() const noexcept;
        void set_width(int32_t width) noexcept;

        int32_t get_height() const noexcept;
        void set_height(int32_t height) noexcept;

        int32_t get_weight() const noexcept;
        void set_weight(int32_t weight) noexcept;

        int32_t get_slot_requirement() const noexcept;
        void set_slot_requirement(int32_t slot_requirement) noexcept;

};

class game_recipe_blueprint
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t                                  _recipe_id;
        int32_t                                  _result_item_id;
        ft_vector<game_crafting_ingredient>    _ingredients;
        mutable pt_recursive_mutex                    *_mutex;
        static thread_local int32_t _last_error;
        uint8_t                              _initialised_state;
        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        game_recipe_blueprint() noexcept;
        game_recipe_blueprint(const game_recipe_blueprint &other) noexcept = delete;
        game_recipe_blueprint(game_recipe_blueprint &&other) noexcept = delete;
        virtual ~game_recipe_blueprint() noexcept;
        game_recipe_blueprint &operator=(const game_recipe_blueprint &other) noexcept = delete;
        game_recipe_blueprint &operator=(game_recipe_blueprint &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(int32_t recipe_id, int32_t result_item_id,
            const ft_vector<game_crafting_ingredient> &ingredients) noexcept;
        int32_t initialize(const game_recipe_blueprint &other) noexcept;
        int32_t initialize(game_recipe_blueprint &&other) noexcept;
        int32_t move(game_recipe_blueprint &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

        int32_t get_recipe_id() const noexcept;
        void set_recipe_id(int32_t recipe_id) noexcept;

        int32_t get_result_item_id() const noexcept;
        void set_result_item_id(int32_t result_item_id) noexcept;

        ft_vector<game_crafting_ingredient> &get_ingredients() noexcept;
        const ft_vector<game_crafting_ingredient> &get_ingredients() const noexcept;
        void set_ingredients(const ft_vector<game_crafting_ingredient> &ingredients) noexcept;

};

class game_loadout_entry
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t             _slot;
        int32_t             _item_id;
        int32_t             _quantity;
        mutable pt_recursive_mutex *_mutex;
        uint8_t         _initialised_state;
        static thread_local int32_t _last_error;
        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        game_loadout_entry() noexcept;
        game_loadout_entry(const game_loadout_entry &other) noexcept = delete;
        game_loadout_entry(game_loadout_entry &&other) noexcept = delete;
        virtual ~game_loadout_entry() noexcept;
        game_loadout_entry &operator=(const game_loadout_entry &other) noexcept = delete;
        game_loadout_entry &operator=(game_loadout_entry &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(int32_t slot, int32_t item_id, int32_t quantity) noexcept;
        int32_t initialize(const game_loadout_entry &other) noexcept;
        int32_t initialize(game_loadout_entry &&other) noexcept;
        int32_t move(game_loadout_entry &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

        int32_t get_slot() const noexcept;
        void set_slot(int32_t slot) noexcept;

        int32_t get_item_id() const noexcept;
        void set_item_id(int32_t item_id) noexcept;

        int32_t get_quantity() const noexcept;
        void set_quantity(int32_t quantity) noexcept;

};

class game_loadout_blueprint
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t                           _loadout_id;
        ft_vector<game_loadout_entry>   _entries;
        mutable pt_recursive_mutex             *_mutex;
        static thread_local int32_t _last_error;
        uint8_t                       _initialised_state;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        game_loadout_blueprint() noexcept;
        game_loadout_blueprint(const game_loadout_blueprint &other) noexcept = delete;
        game_loadout_blueprint(game_loadout_blueprint &&other) noexcept = delete;
        virtual ~game_loadout_blueprint() noexcept;
        game_loadout_blueprint &operator=(const game_loadout_blueprint &other) noexcept = delete;
        game_loadout_blueprint &operator=(game_loadout_blueprint &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(int32_t loadout_id, const ft_vector<game_loadout_entry> &entries) noexcept;
        int32_t initialize(const game_loadout_blueprint &other) noexcept;
        int32_t initialize(game_loadout_blueprint &&other) noexcept;
        int32_t move(game_loadout_blueprint &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

        int32_t get_loadout_id() const noexcept;
        void set_loadout_id(int32_t loadout_id) noexcept;

        ft_vector<game_loadout_entry> &get_entries() noexcept;
        const ft_vector<game_loadout_entry> &get_entries() const noexcept;
        void set_entries(const ft_vector<game_loadout_entry> &entries) noexcept;

};

class game_data_catalog
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_map<int32_t, game_item_definition>   _item_definitions;
        ft_map<int32_t, game_recipe_blueprint>  _recipes;
        ft_map<int32_t, game_loadout_blueprint> _loadouts;
        mutable pt_recursive_mutex                 *_mutex;
        static thread_local int32_t _last_error;
        uint8_t                           _initialised_state;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        game_data_catalog() noexcept;
        game_data_catalog(const game_data_catalog &other) noexcept = delete;
        game_data_catalog(game_data_catalog &&other) noexcept = delete;
        ~game_data_catalog() noexcept;
        game_data_catalog &operator=(const game_data_catalog &other) noexcept = delete;
        game_data_catalog &operator=(game_data_catalog &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_data_catalog &other) noexcept;
        int32_t initialize(game_data_catalog &&other) noexcept;
        int32_t move(game_data_catalog &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

        ft_map<int32_t, game_item_definition> &get_item_definitions() noexcept;
        const ft_map<int32_t, game_item_definition> &get_item_definitions() const noexcept;
        ft_map<int32_t, game_recipe_blueprint> &get_recipes() noexcept;
        const ft_map<int32_t, game_recipe_blueprint> &get_recipes() const noexcept;
        ft_map<int32_t, game_loadout_blueprint> &get_loadouts() noexcept;
        const ft_map<int32_t, game_loadout_blueprint> &get_loadouts() const noexcept;

        int32_t register_item_definition(const game_item_definition &definition) noexcept;
        int32_t register_recipe(const game_recipe_blueprint &recipe) noexcept;
        int32_t register_loadout(const game_loadout_blueprint &loadout) noexcept;

        int32_t fetch_item_definition(int32_t item_id, game_item_definition &definition) const noexcept;
        int32_t fetch_recipe(int32_t recipe_id, game_recipe_blueprint &recipe) const noexcept;
        int32_t fetch_loadout(int32_t loadout_id, game_loadout_blueprint &loadout) const noexcept;

};

#endif
