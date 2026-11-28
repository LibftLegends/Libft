#ifndef GAME_ACHIEVEMENT_HPP
# define GAME_ACHIEVEMENT_HPP

#include <cstdint>
#include "../Template/map.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"

class game_goal
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t              _target;
        int32_t              _progress;
        mutable pt_recursive_mutex *_mutex;
        uint8_t          _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t read_completion(ft_bool &completed) const noexcept;

        friend class game_achievement;


    public:
        game_goal() noexcept;
        game_goal(const game_goal &other) noexcept = delete;
        game_goal(game_goal &&other) noexcept = delete;
        ~game_goal() noexcept;
        game_goal &operator=(const game_goal &other) = delete;
        game_goal &operator=(game_goal &&other) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_goal &other) noexcept;
        int32_t initialize(game_goal &&other) noexcept;
        int32_t move(game_goal &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_target() const noexcept;
        void set_target(int32_t target) noexcept;
        int32_t get_progress() const noexcept;
        void set_progress(int32_t value) noexcept;
        void add_progress(int32_t delta) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

class game_achievement
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t              _id;
        ft_map<int32_t, game_goal> _goals;
        mutable pt_recursive_mutex *_mutex;
        uint8_t          _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        game_achievement() noexcept;
        game_achievement(const game_achievement &other) noexcept = delete;
        game_achievement(game_achievement &&other) noexcept = delete;
        virtual ~game_achievement() noexcept;
        game_achievement &operator=(const game_achievement &other) = delete;
        game_achievement &operator=(game_achievement &&other) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_achievement &other) noexcept;
        int32_t initialize(game_achievement &&other) noexcept;
        int32_t move(game_achievement &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_id() const noexcept;
        void set_id(int32_t id) noexcept;

        ft_map<int32_t, game_goal>       &get_goals() noexcept;
        const ft_map<int32_t, game_goal> &get_goals() const noexcept;
        void set_goals(const ft_map<int32_t, game_goal> &goals) noexcept;
        int32_t get_goal(int32_t id) const noexcept;
        void set_goal(int32_t id, int32_t goal) noexcept;

        int32_t get_progress(int32_t id) const noexcept;
        void set_progress(int32_t id, int32_t progress) noexcept;
        void add_progress(int32_t id, int32_t value) noexcept;

        ft_bool is_goal_complete(int32_t id) const noexcept;
        ft_bool is_complete() const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
