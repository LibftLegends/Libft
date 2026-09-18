#ifndef GAME_SCRIPTING_BRIDGE_HPP
# define GAME_SCRIPTING_BRIDGE_HPP

#include "game_state.hpp"
#include "game_world.hpp"
#include "../Scripting/scripting.hpp"
#include "../Template/function.hpp"
#include "../Template/map.hpp"
#include "../Template/vector.hpp"
#include "../Template/shared_ptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <cstddef>

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
        int64_t                              _result_integer;
        ft_bool                              _result_integer_set;
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
        void clear_result() noexcept;
        void set_result_integer(int64_t value) noexcept;
        ft_bool has_result_integer() const noexcept;
        int64_t get_result_integer() const noexcept;

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
        scripting_engine _custom_engine;
        game_script_context *_custom_context;
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
        static int32_t custom_callback_dispatch(
            const scripting_call_context *call_context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, void *user_data) noexcept;
    public:
        game_script_bridge() noexcept;
        game_script_bridge(const game_script_bridge &other) noexcept = delete;
        game_script_bridge(game_script_bridge &&other) noexcept = delete;
        ~game_script_bridge() noexcept;
        game_script_bridge &operator=(const game_script_bridge &other) noexcept = delete;
        game_script_bridge &operator=(game_script_bridge &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_sharedptr<game_world> &world,
            const char *language = "custom") noexcept;
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
        int32_t execute_custom_with_user_data(const ft_string &script,
            game_state *state, void *user_data) noexcept;
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
