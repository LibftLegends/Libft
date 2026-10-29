#ifndef GAME_EQUIPMENT_HPP
# define GAME_EQUIPMENT_HPP

#include "game_item.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <cstdint>

enum game_equipment_slot
{
    EQUIP_HEAD,
    EQUIP_CHEST,
    EQUIP_WEAPON,
    EQUIP_LEGS,
    EQUIP_FEET,
    EQUIP_OFF_HAND
};

class game_equipment
{
    #ifdef LIBFT_TEST_BUILD
        public:
    #else
        private:
    #endif
        ft_sharedptr<game_item> _head;
        ft_sharedptr<game_item> _chest;
        ft_sharedptr<game_item> _weapon;
        ft_sharedptr<game_item> _legs;
        ft_sharedptr<game_item> _feet;
        ft_sharedptr<game_item> _off_hand;
        mutable pt_recursive_mutex      *_mutex;
        uint8_t _initialised_state;
        static thread_local int32_t _last_error;

        static int32_t set_error(int32_t error_code) noexcept;

        ft_bool validate_item(const ft_sharedptr<game_item> &item) const noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const noexcept;
        void unlock_internal(ft_bool lock_acquired) const noexcept;

    public:
        game_equipment() noexcept;
        virtual ~game_equipment() noexcept;
        game_equipment(const game_equipment &other) noexcept = delete;
        game_equipment &operator=(const game_equipment &other) noexcept = delete;
        game_equipment(game_equipment &&other) noexcept = delete;
        game_equipment &operator=(game_equipment &&other) noexcept = delete;
        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_equipment &other) noexcept;

        int32_t equip(int32_t slot, const ft_sharedptr<game_item> &item) noexcept;
        void unequip(int32_t slot) noexcept;
        ft_sharedptr<game_item> *get_item(int32_t slot) noexcept;
        ft_sharedptr<game_item> *get_item(int32_t slot) const noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t         get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

#endif
