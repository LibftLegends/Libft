#include "../PThread/pthread_internal.hpp"
#include "game_equipment.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/shared_ptr.hpp"

thread_local int32_t game_equipment::_last_error = FT_ERR_SUCCESS;

int32_t game_equipment::set_error(int32_t error_code) noexcept
{
    game_equipment::_last_error = error_code;
    return (error_code);
}

game_equipment::game_equipment() noexcept
    : _head(ft_sharedptr<game_item>()), _chest(ft_sharedptr<game_item>()),
      _weapon(ft_sharedptr<game_item>()), _legs(ft_sharedptr<game_item>()),
      _feet(ft_sharedptr<game_item>()), _off_hand(ft_sharedptr<game_item>()),
      _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_equipment::~game_equipment() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_equipment::initialize() noexcept
{
    int32_t head_initialize_error;
    int32_t chest_initialize_error;
    int32_t weapon_initialize_error;
    int32_t legs_initialize_error;
    int32_t feet_initialize_error;
    int32_t off_hand_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_equipment::initialize", "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    head_initialize_error = this->_head.initialize();
    if (head_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(head_initialize_error);
        return (head_initialize_error);
    }
    chest_initialize_error = this->_chest.initialize();
    if (chest_initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_head.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(chest_initialize_error);
        return (chest_initialize_error);
    }
    weapon_initialize_error = this->_weapon.initialize();
    if (weapon_initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_chest.destroy();
        (void)this->_head.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(weapon_initialize_error);
        return (weapon_initialize_error);
    }
    legs_initialize_error = this->_legs.initialize();
    if (legs_initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_weapon.destroy();
        (void)this->_chest.destroy();
        (void)this->_head.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(legs_initialize_error);
        return (legs_initialize_error);
    }
    feet_initialize_error = this->_feet.initialize();
    if (feet_initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_legs.destroy();
        (void)this->_weapon.destroy();
        (void)this->_chest.destroy();
        (void)this->_head.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(feet_initialize_error);
        return (feet_initialize_error);
    }
    off_hand_initialize_error = this->_off_hand.initialize();
    if (off_hand_initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_feet.destroy();
        (void)this->_legs.destroy();
        (void)this->_weapon.destroy();
        (void)this->_chest.destroy();
        (void)this->_head.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(off_hand_initialize_error);
        return (off_hand_initialize_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_equipment::destroy() noexcept
{
    int32_t disable_error;
    int32_t head_destroy_error;
    int32_t chest_destroy_error;
    int32_t weapon_destroy_error;
    int32_t legs_destroy_error;
    int32_t feet_destroy_error;
    int32_t off_hand_destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    head_destroy_error = this->_head.destroy();
    chest_destroy_error = this->_chest.destroy();
    weapon_destroy_error = this->_weapon.destroy();
    legs_destroy_error = this->_legs.destroy();
    feet_destroy_error = this->_feet.destroy();
    off_hand_destroy_error = this->_off_hand.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
    {
        this->set_error(disable_error);
        return (disable_error);
    }
    if (head_destroy_error != FT_ERR_SUCCESS)
    {
        this->set_error(head_destroy_error);
        return (head_destroy_error);
    }
    if (chest_destroy_error != FT_ERR_SUCCESS)
    {
        this->set_error(chest_destroy_error);
        return (chest_destroy_error);
    }
    if (weapon_destroy_error != FT_ERR_SUCCESS)
        return (this->set_error(weapon_destroy_error));
    if (legs_destroy_error != FT_ERR_SUCCESS)
        return (this->set_error(legs_destroy_error));
    if (feet_destroy_error != FT_ERR_SUCCESS)
        return (this->set_error(feet_destroy_error));
    this->set_error(off_hand_destroy_error);
    return (off_hand_destroy_error);
}

int32_t game_equipment::move(game_equipment &other) noexcept
{
    int32_t destroy_error;
    int32_t initialize_error;
    int32_t source_error;
    int32_t source_destroy_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_equipment::move",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    this->_head.swap(other._head);
    this->_chest.swap(other._chest);
    this->_weapon.swap(other._weapon);
    this->_legs.swap(other._legs);
    this->_feet.swap(other._feet);
    this->_off_hand.swap(other._off_hand);
    source_error = other.get_error();
    source_destroy_error = other.destroy();
    if (source_destroy_error != FT_ERR_SUCCESS)
    {
        this->set_error(source_destroy_error);
        return (source_destroy_error);
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

game_equipment::slot_member_pointer game_equipment::find_slot(int32_t slot) noexcept
{
    static_assert(EQUIP_HEAD == 0 && EQUIP_CHEST == 1 && EQUIP_WEAPON == 2
        && EQUIP_LEGS == 3 && EQUIP_FEET == 4 && EQUIP_OFF_HAND == 5);
    static constexpr slot_member_pointer SLOTS[] = {
        &game_equipment::_head, &game_equipment::_chest, &game_equipment::_weapon,
        &game_equipment::_legs, &game_equipment::_feet, &game_equipment::_off_hand
    };
    if (slot < EQUIP_HEAD || slot > EQUIP_OFF_HAND)
        return (nullptr);
    return (SLOTS[slot]);
}

int32_t game_equipment::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::lock_internal");
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

void game_equipment::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_equipment::equip(int32_t slot, const ft_sharedptr<game_item> &item) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::equip");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    int32_t result = FT_ERR_INVALID_ARGUMENT;
    if (item)
    {
        const slot_member_pointer member = this->find_slot(slot);
        if (member != nullptr)
        {
            this->*member = item;
            result = FT_ERR_SUCCESS;
        }
    }
    this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

void game_equipment::unequip(int32_t slot) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::unequip");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    const slot_member_pointer member = this->find_slot(slot);
    int32_t result = FT_ERR_INVALID_ARGUMENT;
    if (member != nullptr)
    {
        ft_sharedptr<game_item> &item = this->*member;
        item.reset();
        result = item.get_error();
    }
    this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
}

int32_t game_equipment::read_item(int32_t slot, ft_sharedptr<game_item> &result) const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t status = this->lock_internal(&lock_acquired);
    if (status != FT_ERR_SUCCESS)
        return (status);
    const slot_member_pointer member = this->find_slot(slot);
    if (member == nullptr)
        status = FT_ERR_INVALID_ARGUMENT;
    else
    {
        result = this->*member;
        status = result.get_error();
    }
    this->unlock_internal(lock_acquired);
    return (this->set_error(status));
}

ft_sharedptr<game_item> *game_equipment::get_item(int32_t slot) noexcept
{
    return (static_cast<const game_equipment *>(this)->get_item(slot));
}

ft_sharedptr<game_item> *game_equipment::get_item(int32_t slot) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::get_item const");
    auto *result = new (std::nothrow) ft_sharedptr<game_item>();
    if (result == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    if (this->read_item(slot, *result) != FT_ERR_SUCCESS)
    {
        delete result;
        return (ft_nullptr);
    }
    return (result);
}

int32_t game_equipment::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_equipment::disable_thread_safety() noexcept
{
    pt_recursive_mutex *old_mutex;
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    old_mutex = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = old_mutex->destroy();
    delete old_mutex;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_equipment::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_equipment::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::get_error");
    return (game_equipment::_last_error);
}

const char *game_equipment::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::get_error_str");
    return (ft_strerror(this->get_error()));
}
