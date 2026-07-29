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
      _weapon(ft_sharedptr<game_item>()), _mutex(ft_nullptr),
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

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    head_destroy_error = this->_head.destroy();
    chest_destroy_error = this->_chest.destroy();
    weapon_destroy_error = this->_weapon.destroy();
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
    this->set_error(weapon_destroy_error);
    return (weapon_destroy_error);
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
    this->_head = other._head;
    this->_chest = other._chest;
    this->_weapon = other._weapon;
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

ft_bool game_equipment::validate_item(const ft_sharedptr<game_item> &item) const noexcept
{
    if (!item)
        return (FT_FALSE);
    return (FT_TRUE);
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
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::equip");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (this->validate_item(item) == FT_FALSE)
    {
        this->unlock_internal(lock_acquired);
        result = FT_ERR_INVALID_ARGUMENT;
        this->set_error(result);
        return (result);
    }
    if (slot == EQUIP_HEAD)
    {
        this->_head = item;
        result = FT_ERR_SUCCESS;
    }
    else if (slot == EQUIP_CHEST)
    {
        this->_chest = item;
        result = FT_ERR_SUCCESS;
    }
    else if (slot == EQUIP_WEAPON)
    {
        this->_weapon = item;
        result = FT_ERR_SUCCESS;
    }
    else
    {
        result = FT_ERR_INVALID_ARGUMENT;
    }
    this->unlock_internal(lock_acquired);
    this->set_error(result);
    return (result);
}

void game_equipment::unequip(int32_t slot) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::unequip");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (slot == EQUIP_HEAD)
    {
        (void)this->_head.destroy();
        result = this->_head.initialize();
    }
    else if (slot == EQUIP_CHEST)
    {
        (void)this->_chest.destroy();
        result = this->_chest.initialize();
    }
    else if (slot == EQUIP_WEAPON)
    {
        (void)this->_weapon.destroy();
        result = this->_weapon.initialize();
    }
    else
    {
        result = FT_ERR_INVALID_ARGUMENT;
    }
    this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
}

static void game_equipment_delete_item_handle(ft_sharedptr<game_item> *item) noexcept
{
    if (item == ft_nullptr)
        return ;
    (void)item->destroy();
    delete item;
    return ;
}

ft_sharedptr<game_item> *game_equipment::get_item(int32_t slot) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t result_error;
    ft_sharedptr<game_item> *result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::get_item");
    result = new (std::nothrow) ft_sharedptr<game_item>();
    if (result == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    if (result->initialize() != FT_ERR_SUCCESS)
    {
        this->set_error(result->get_error());
        delete result;
        return (ft_nullptr);
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        game_equipment_delete_item_handle(result);
        return (ft_nullptr);
    }
    if (slot == EQUIP_HEAD)
    {
        *result = this->_head;
        result_error = FT_ERR_SUCCESS;
    }
    else if (slot == EQUIP_CHEST)
    {
        *result = this->_chest;
        result_error = FT_ERR_SUCCESS;
    }
    else if (slot == EQUIP_WEAPON)
    {
        *result = this->_weapon;
        result_error = FT_ERR_SUCCESS;
    }
    else
    {
        result_error = FT_ERR_INVALID_ARGUMENT;
    }
    this->unlock_internal(lock_acquired);
    if (result_error == FT_ERR_SUCCESS && result->get_error() != FT_ERR_SUCCESS)
        result_error = result->get_error();
    this->set_error(result_error);
    if (result_error != FT_ERR_SUCCESS)
    {
        game_equipment_delete_item_handle(result);
        return (ft_nullptr);
    }
    return (result);
}

ft_sharedptr<game_item> *game_equipment::get_item(int32_t slot) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_sharedptr<game_item> *result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_equipment::get_item const");
    result = new (std::nothrow) ft_sharedptr<game_item>();
    if (result == ft_nullptr)
    {
        const_cast<game_equipment *>(this)->set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    if (result->initialize() != FT_ERR_SUCCESS)
    {
        const_cast<game_equipment *>(this)->set_error(result->get_error());
        delete result;
        return (ft_nullptr);
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        game_equipment_delete_item_handle(result);
        return (ft_nullptr);
    }
    if (slot == EQUIP_HEAD)
        *result = this->_head;
    else if (slot == EQUIP_CHEST)
        *result = this->_chest;
    else if (slot == EQUIP_WEAPON)
        *result = this->_weapon;
    else
    {
        this->unlock_internal(lock_acquired);
        const_cast<game_equipment *>(this)->set_error(FT_ERR_INVALID_ARGUMENT);
        game_equipment_delete_item_handle(result);
        return (ft_nullptr);
    }
    if (result->get_error() != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        const_cast<game_equipment *>(this)->set_error(result->get_error());
        game_equipment_delete_item_handle(result);
        return (ft_nullptr);
    }
    const_cast<game_equipment *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
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
