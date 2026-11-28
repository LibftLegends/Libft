#ifndef GAME_UPGRADE_HPP
# define GAME_UPGRADE_HPP

#include <cstdint>
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"

class game_upgrade
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t              _id;
        uint16_t         _current_level;
        uint16_t         _max_level;
        int32_t              _modifier1;
        int32_t              _modifier2;
        int32_t              _modifier3;
        int32_t              _modifier4;
        mutable pt_recursive_mutex *_mutex;
        static thread_local int32_t _last_error;
        uint8_t          _initialised_state;
        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        void change_modifier(int32_t &modifier, int64_t delta, const char *operation) noexcept;

    public:
        game_upgrade() noexcept;
        game_upgrade(const game_upgrade &other) noexcept = delete;
        game_upgrade(game_upgrade &&other) noexcept = delete;
        virtual ~game_upgrade() noexcept;
        game_upgrade &operator=(const game_upgrade &other) = delete;
        game_upgrade &operator=(game_upgrade &&other) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_upgrade &other) noexcept;
        int32_t initialize(game_upgrade &&other) noexcept;
        // Moving requires exclusive access to both records.
        int32_t move(game_upgrade &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_id() const noexcept;
        void set_id(int32_t id) noexcept;

        uint16_t get_current_level() const noexcept;
        void set_current_level(uint16_t level) noexcept;
        void add_level(uint16_t level) noexcept;
        void sub_level(uint16_t level) noexcept;

        uint16_t get_max_level() const noexcept;
        void set_max_level(uint16_t level) noexcept;

        int32_t get_modifier1() const noexcept;
        void set_modifier1(int32_t mod) noexcept;
        void add_modifier1(int32_t mod) noexcept;
        void sub_modifier1(int32_t mod) noexcept;

        int32_t get_modifier2() const noexcept;
        void set_modifier2(int32_t mod) noexcept;
        void add_modifier2(int32_t mod) noexcept;
        void sub_modifier2(int32_t mod) noexcept;

        int32_t get_modifier3() const noexcept;
        void set_modifier3(int32_t mod) noexcept;
        void add_modifier3(int32_t mod) noexcept;
        void sub_modifier3(int32_t mod) noexcept;

        int32_t get_modifier4() const noexcept;
        void set_modifier4(int32_t mod) noexcept;
        void add_modifier4(int32_t mod) noexcept;
        void sub_modifier4(int32_t mod) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

#endif
