#ifndef GAME_ECONOMY_TABLE_HPP
# define GAME_ECONOMY_TABLE_HPP

#include "../Template/map.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

#include "game_price_definition.hpp"
#include "game_rarity_band.hpp"
#include "game_vendor_profile.hpp"
#include "game_currency_rate.hpp"

class game_economy_table
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_map<int32_t, game_price_definition> _price_definitions;
        ft_map<int32_t, game_rarity_band>      _rarity_bands;
        ft_map<int32_t, game_vendor_profile>   _vendor_profiles;
        ft_map<int32_t, game_currency_rate>    _currency_rates;
        pt_recursive_mutex                        *_mutex;
        uint8_t                          _initialised_state;
        static thread_local int32_t _last_error;


        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        template <typename Operation>
        int32_t with_table_lock(Operation operation) const noexcept;
        static int32_t set_error(int32_t error_code) noexcept;

    public:
        game_economy_table() noexcept;
        ~game_economy_table() noexcept;
        game_economy_table(const game_economy_table &other) noexcept = delete;
        game_economy_table &operator=(const game_economy_table &other) noexcept = delete;
        game_economy_table(game_economy_table &&other) noexcept = delete;
        game_economy_table &operator=(game_economy_table &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_economy_table &other) noexcept;
        int32_t initialize(game_economy_table &&other) noexcept;
        int32_t move(game_economy_table &other) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        ft_map<int32_t, game_price_definition> &get_price_definitions() noexcept;
        const ft_map<int32_t, game_price_definition> &get_price_definitions() const noexcept;
        ft_map<int32_t, game_rarity_band> &get_rarity_bands() noexcept;
        const ft_map<int32_t, game_rarity_band> &get_rarity_bands() const noexcept;
        ft_map<int32_t, game_vendor_profile> &get_vendor_profiles() noexcept;
        const ft_map<int32_t, game_vendor_profile> &get_vendor_profiles() const noexcept;
        ft_map<int32_t, game_currency_rate> &get_currency_rates() noexcept;
        const ft_map<int32_t, game_currency_rate> &get_currency_rates() const noexcept;

        void set_price_definitions(const ft_map<int32_t, game_price_definition> &price_definitions) noexcept;
        void set_rarity_bands(const ft_map<int32_t, game_rarity_band> &rarity_bands) noexcept;
        void set_vendor_profiles(const ft_map<int32_t, game_vendor_profile> &vendor_profiles) noexcept;
        void set_currency_rates(const ft_map<int32_t, game_currency_rate> &currency_rates) noexcept;

        int32_t register_price_definition(const game_price_definition &definition) noexcept;
        int32_t register_rarity_band(const game_rarity_band &band) noexcept;
        int32_t register_vendor_profile(const game_vendor_profile &profile) noexcept;
        int32_t register_currency_rate(const game_currency_rate &rate) noexcept;

        int32_t fetch_price_definition(int32_t item_id, game_price_definition &definition) const noexcept;
        int32_t fetch_rarity_band(int32_t rarity, game_rarity_band &band) const noexcept;
        int32_t fetch_vendor_profile(int32_t vendor_id, game_vendor_profile &profile) const noexcept;
        int32_t fetch_currency_rate(int32_t currency_id, game_currency_rate &rate) const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;

};

#endif
