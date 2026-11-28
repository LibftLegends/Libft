#ifndef GAME_BUFF_HPP
# define GAME_BUFF_HPP

#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_buff
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t              _id;
        int32_t              _duration;
        int32_t              _modifier1;
        int32_t              _modifier2;
        int32_t              _modifier3;
        int32_t              _modifier4;
        mutable pt_recursive_mutex *_mutex;
        uint8_t          _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        void reset_fields() noexcept;
        void copy_fields_from(const game_buff &other) noexcept;
        int32_t copy_with_lifecycle(const game_buff &other) noexcept;
        int32_t read_field(const int32_t &field) const noexcept;
        void write_field(int32_t &field, int32_t value) noexcept;
        void adjust_field(int32_t &field, int64_t delta) noexcept;

    public:
        game_buff() noexcept;
        game_buff(const game_buff &other) noexcept = delete;
        game_buff(game_buff &&other) noexcept = delete;
        virtual ~game_buff() noexcept;
        game_buff &operator=(const game_buff &other) = delete;
        game_buff &operator=(game_buff &&other) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_buff &other) noexcept;
        int32_t initialize(game_buff &&other) noexcept;
        int32_t move(game_buff &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_id() const noexcept;
        void set_id(int32_t id) noexcept;

        int32_t get_duration() const noexcept;
        void set_duration(int32_t duration) noexcept;
        void add_duration(int32_t duration) noexcept;
        void sub_duration(int32_t duration) noexcept;

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
