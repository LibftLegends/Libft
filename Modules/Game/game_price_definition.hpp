#ifndef FT_PRICE_DEFINITION_HPP
# define FT_PRICE_DEFINITION_HPP

#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <stdint.h>

class game_price_definition
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        int32_t      _item_id;
        int32_t      _rarity;
        int32_t      _base_value;
        int32_t      _minimum_value;
        int32_t      _maximum_value;
        pt_recursive_mutex *_mutex;
        uint8_t  _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;


        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        int32_t unlock_internal(ft_bool lock_acquired) const noexcept;
        int32_t copy_values(const game_price_definition &other) noexcept;

        friend class game_economy_table;

    public:
        game_price_definition() noexcept;
        virtual ~game_price_definition() noexcept;
        game_price_definition(const game_price_definition &other) noexcept = delete;
        game_price_definition &operator=(const game_price_definition &other) noexcept = delete;
        game_price_definition(game_price_definition &&other) noexcept = delete;
        game_price_definition &operator=(game_price_definition &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const game_price_definition &other) noexcept;
        int32_t initialize(game_price_definition &&other) noexcept;
        int32_t move(game_price_definition &other) noexcept;
        int32_t initialize(int32_t item_id, int32_t rarity, int32_t base_value,
            int32_t minimum_value, int32_t maximum_value) noexcept;
        int32_t destroy() noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

        int32_t get_item_id() const noexcept;
        void set_item_id(int32_t item_id) noexcept;

        int32_t get_rarity() const noexcept;
        void set_rarity(int32_t rarity) noexcept;

        int32_t get_base_value() const noexcept;
        void set_base_value(int32_t base_value) noexcept;

        int32_t get_minimum_value() const noexcept;
        void set_minimum_value(int32_t minimum_value) noexcept;

        int32_t get_maximum_value() const noexcept;
        void set_maximum_value(int32_t maximum_value) noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
