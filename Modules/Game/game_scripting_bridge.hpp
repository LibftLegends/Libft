#ifndef GAME_SCRIPTING_BRIDGE_HPP
# define GAME_SCRIPTING_BRIDGE_HPP

#include "game_state.hpp"
#include "game_world.hpp"
#include "../Template/function.hpp"
#include "../Template/map.hpp"
#include "../Template/vector.hpp"
#include "../Template/shared_ptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"

struct lua_State;
struct lua_Debug;

class game_script_context
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        game_state                         *_state;
        void                               *_user_data;
        ft_sharedptr<game_world>                _world;
        ft_map<ft_string, ft_string>          _variables;
        uint8_t                               _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;

    public:
        game_script_context() noexcept;
        game_script_context(const game_script_context &other) noexcept = delete;
        game_script_context(game_script_context &&other) noexcept = delete;
        ~game_script_context() noexcept;
        int32_t destroy() noexcept;
        game_script_context &operator=(const game_script_context &other) noexcept = delete;
        game_script_context &operator=(game_script_context &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(game_state *state,
            const ft_sharedptr<game_world> &world) noexcept;
        int32_t initialize(const game_script_context &other) noexcept;
        int32_t move(game_script_context &other) noexcept;

        game_state *get_state() const noexcept;
        void *get_user_data() const noexcept;
        const ft_sharedptr<game_world> &get_world() const noexcept;

        void set_state(game_state *state) noexcept;
        void set_user_data(void *user_data) noexcept;
        void set_world(const ft_sharedptr<game_world> &world) noexcept;

        void set_variable(const ft_string &key, const ft_string &value) noexcept;
        const ft_string *get_variable(const ft_string &key) const noexcept;
        void remove_variable(const ft_string &key) noexcept;
        void clear_variables() noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

class game_script_bridge
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_sharedptr<game_world> _world;
        ft_map<ft_string, ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)> > _callbacks;
        ft_string _language;
        int32_t _max_operations;
        int32_t _lua_instruction_limit;
        int32_t _lua_instruction_count;
        int32_t _lua_callback_error;
        ft_size_t _lua_memory_limit;
        ft_size_t _lua_memory_used;
        lua_State *_lua_state;
        game_script_context *_lua_context;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;
        mutable pt_recursive_mutex *_mutex;

        static int32_t set_error(int32_t error_code) noexcept;
        static ft_bool is_supported_language(const ft_string &language) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t execute_line(game_script_context &context, const ft_string &line) noexcept;
        int32_t handle_call(game_script_context &context, const ft_vector<ft_string> &tokens) noexcept;
        int32_t handle_set(game_script_context &context, const ft_vector<ft_string> &tokens) noexcept;
        int32_t handle_unset(game_script_context &context, const ft_vector<ft_string> &tokens) noexcept;
        void tokenize_line(const ft_string &line, ft_vector<ft_string> &tokens) const noexcept;
        int32_t initialize_lua_runtime() noexcept;
        void destroy_lua_runtime() noexcept;
        int32_t register_lua_callbacks() noexcept;
        static void *lua_allocate(void *user_data, void *pointer,
            ft_size_t old_size, ft_size_t new_size) noexcept;
        static void lua_instruction_hook(lua_State *lua_state,
            lua_Debug *debug_record) noexcept;
        static int32_t lua_callback_dispatch(lua_State *lua_state) noexcept;

    public:
        game_script_bridge() noexcept;
        game_script_bridge(const game_script_bridge &other) noexcept = delete;
        game_script_bridge(game_script_bridge &&other) noexcept = delete;
        ~game_script_bridge() noexcept;
        game_script_bridge &operator=(const game_script_bridge &other) noexcept = delete;
        game_script_bridge &operator=(game_script_bridge &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_sharedptr<game_world> &world,
            const char *language = "lua") noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_script_bridge &other) noexcept;

        void set_language(const char *language) noexcept;
        const ft_string &get_language() const noexcept;

        void set_max_operations(int32_t limit) noexcept;
        int32_t get_max_operations() const noexcept;

        ft_size_t get_callback_count() const noexcept;

        int32_t register_function(const ft_string &name, const ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)> &callback) noexcept;
        int32_t remove_function(const ft_string &name) noexcept;

        int32_t execute(const ft_string &script, game_state &state) noexcept;
        int32_t execute_with_user_data(const ft_string &script,
            game_state *state, void *user_data) noexcept;
        int32_t execute_lua(const ft_string &script, game_state &state) noexcept;
        int32_t execute_lua_with_user_data(const ft_string &script,
            game_state *state, void *user_data) noexcept;

        void set_lua_instruction_limit(int32_t limit) noexcept;
        int32_t get_lua_instruction_limit() const noexcept;
        void set_lua_memory_limit(ft_size_t limit) noexcept;
        ft_size_t get_lua_memory_limit() const noexcept;
        ft_size_t get_lua_memory_used() const noexcept;

        int32_t check_sandbox_capabilities(const ft_string &script, ft_vector<ft_string> &violations) noexcept;
        int32_t validate_dry_run(const ft_string &script, ft_vector<ft_string> &warnings) noexcept;
        int32_t inspect_bytecode_budget(const ft_string &script, int32_t &required_operations) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
