#ifndef GAME_BEHAVIOR_TABLE_HPP
# define GAME_BEHAVIOR_TABLE_HPP

#include "../Template/map.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include "game_behavior_profile.hpp"
#include <stdint.h>

class game_behavior_table
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_map<int32_t, game_behavior_profile> _profiles;
        pt_recursive_mutex                        *_mutex;
        uint8_t                          _initialised_state;
        static thread_local int32_t _last_error;


        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t register_profile_snapshot(const game_behavior_profile &profile) noexcept;
        static int32_t set_error(int32_t error_code) noexcept;

    public:
        game_behavior_table() noexcept;
        ~game_behavior_table() noexcept;
        game_behavior_table(const game_behavior_table &other) noexcept = delete;
        game_behavior_table &operator=(const game_behavior_table &other) noexcept = delete;
        game_behavior_table(game_behavior_table &&other) noexcept = delete;
        game_behavior_table &operator=(game_behavior_table &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_behavior_table &other) noexcept;
        int32_t initialize(game_behavior_table &&other) noexcept;
        int32_t move(game_behavior_table &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        ft_map<int32_t, game_behavior_profile> &get_profiles() noexcept;
        const ft_map<int32_t, game_behavior_profile> &get_profiles() const noexcept;
        void set_profiles(const ft_map<int32_t, game_behavior_profile> &profiles) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

        int32_t register_profile(const game_behavior_profile &profile) noexcept;
        int32_t fetch_profile(int32_t profile_id, game_behavior_profile &profile) const noexcept;

};

#endif
