#ifndef FT_CURRENCY_RATE_HPP
# define FT_CURRENCY_RATE_HPP

#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_currency_rate
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t      _currency_id;
        double   _rate_to_base;
        int32_t      _display_precision;
        pt_recursive_mutex *_mutex;
        uint8_t  _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;

        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t read_values(int32_t &currency_id, double &rate_to_base,
            int32_t &display_precision) const noexcept;
        int32_t copy_values(const game_currency_rate &other) noexcept;
        friend class game_economy_table;

    public:
        game_currency_rate() noexcept;
        virtual ~game_currency_rate() noexcept;
        game_currency_rate(const game_currency_rate &other) noexcept = delete;
        game_currency_rate &operator=(const game_currency_rate &other) noexcept = delete;
        game_currency_rate(game_currency_rate &&other) noexcept = delete;
        game_currency_rate &operator=(game_currency_rate &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_currency_rate &other) noexcept;
        int32_t initialize(game_currency_rate &&other) noexcept;
        int32_t move(game_currency_rate &other) noexcept;
        int32_t initialize(int32_t currency_id, double rate_to_base,
            int32_t display_precision) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_currency_id() const noexcept;
        void set_currency_id(int32_t currency_id) noexcept;

        double get_rate_to_base() const noexcept;
        void set_rate_to_base(double rate_to_base) noexcept;

        int32_t get_display_precision() const noexcept;
        void set_display_precision(int32_t display_precision) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
