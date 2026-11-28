#ifndef FT_BEHAVIOR_ACTION_HPP
# define FT_BEHAVIOR_ACTION_HPP

#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"

class game_behavior_action
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t              _action_id;
        double           _weight;
        double           _cooldown_seconds;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;
        mutable pt_recursive_mutex *_mutex;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;
        void copy_values_from(const game_behavior_action &other) noexcept;
        int32_t copy_locked(const game_behavior_action &other) noexcept;

    public:
        game_behavior_action() noexcept;
        game_behavior_action(const game_behavior_action &other) noexcept = delete;
        game_behavior_action(game_behavior_action &&other) noexcept = delete;
        virtual ~game_behavior_action() noexcept;
        game_behavior_action &operator=(const game_behavior_action &other) noexcept = delete;
        game_behavior_action &operator=(game_behavior_action &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(int32_t action_id, double weight, double cooldown_seconds) noexcept;
        int32_t initialize(const game_behavior_action &other) noexcept;
        int32_t initialize(game_behavior_action &&other) noexcept;
        int32_t move(game_behavior_action &other) noexcept;
        int32_t destroy() noexcept;

        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t get_action_id() const noexcept;
        void set_action_id(int32_t action_id) noexcept;

        double get_weight() const noexcept;
        void set_weight(double weight) noexcept;

        double get_cooldown_seconds() const noexcept;
        void set_cooldown_seconds(double cooldown_seconds) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
