#ifndef GAME_REPUTATION_HPP
# define GAME_REPUTATION_HPP

#include "../Template/map.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_reputation
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_map<int32_t, int32_t>  _milestones;
        ft_map<int32_t, int32_t>  _reps;
        int32_t               _total_rep;
        int32_t               _current_rep;
        mutable pt_recursive_mutex *_mutex;
        static thread_local int32_t _last_error;
        uint8_t           _initialised_state;
        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal() const noexcept;
        void unlock_internal() const noexcept;
        int32_t read_counter(const int32_t &counter) const noexcept;
        void set_counter(int32_t &counter, int32_t value) noexcept;
        void adjust_counter(int32_t &counter, int64_t delta) noexcept;
        friend class game_character;

    public:
        game_reputation() noexcept;
        game_reputation(const game_reputation &other) noexcept = delete;
        game_reputation(game_reputation &&other) noexcept = delete;
        virtual ~game_reputation() noexcept;
        game_reputation &operator=(const game_reputation &other) = delete;
        game_reputation &operator=(game_reputation &&other) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_map<int32_t, int32_t> &milestones, int32_t total = 0) noexcept;
        int32_t initialize(const game_reputation &other) noexcept;
        int32_t initialize(game_reputation &&other) noexcept;
        int32_t move(game_reputation &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_total_rep() const noexcept;
        void set_total_rep(int32_t rep) noexcept;
        void add_total_rep(int32_t rep) noexcept;
        void sub_total_rep(int32_t rep) noexcept;

        int32_t get_current_rep() const noexcept;
        void set_current_rep(int32_t rep) noexcept;
        void add_current_rep(int32_t rep) noexcept;
        void sub_current_rep(int32_t rep) noexcept;

        ft_map<int32_t, int32_t>       &get_milestones() noexcept;
        const ft_map<int32_t, int32_t> &get_milestones() const noexcept;
        void set_milestones(const ft_map<int32_t, int32_t> &milestones) noexcept;
        int32_t get_milestone(int32_t id) const noexcept;
        void set_milestone(int32_t id, int32_t value) noexcept;

        ft_map<int32_t, int32_t>       &get_reps() noexcept;
        const ft_map<int32_t, int32_t> &get_reps() const noexcept;
        void set_reps(const ft_map<int32_t, int32_t> &reps) noexcept;
        int32_t get_rep(int32_t id) const noexcept;
        void set_rep(int32_t id, int32_t value) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

#endif
