#ifndef FT_RARITY_BAND_HPP
# define FT_RARITY_BAND_HPP

#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_rarity_band
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t      _rarity;
        double   _value_multiplier;
        pt_recursive_mutex *_mutex;
        uint8_t  _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;


        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t read_values(int32_t &rarity, double &value_multiplier) const noexcept;
        int32_t copy_values(const game_rarity_band &other) noexcept;
        friend class game_economy_table;

    public:
        game_rarity_band() noexcept;
        virtual ~game_rarity_band() noexcept;
        game_rarity_band(const game_rarity_band &other) noexcept = delete;
        game_rarity_band &operator=(const game_rarity_band &other) noexcept = delete;
        game_rarity_band(game_rarity_band &&other) noexcept = delete;
        game_rarity_band &operator=(game_rarity_band &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_rarity_band &other) noexcept;
        int32_t initialize(game_rarity_band &&other) noexcept;
        int32_t move(game_rarity_band &other) noexcept;
        int32_t initialize(int32_t rarity, double value_multiplier) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_rarity() const noexcept;
        void set_rarity(int32_t rarity) noexcept;

        double get_value_multiplier() const noexcept;
        void set_value_multiplier(double value_multiplier) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
