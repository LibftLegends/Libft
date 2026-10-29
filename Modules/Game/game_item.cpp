#include "../PThread/pthread_internal.hpp"
#include "game_item.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/pthread.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_item_modifier::_last_error = FT_ERR_SUCCESS;
thread_local int32_t game_item::_last_error = FT_ERR_SUCCESS;
static int32_t game_item_reset_modifier(game_item_modifier &modifier)
{
    modifier.set_id(0);
    modifier.set_value(0);
    return (FT_ERR_SUCCESS);
}

game_item_modifier::game_item_modifier() noexcept
    : _id(0), _value(0), _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _mutex(ft_nullptr)
{
    return ;
}

game_item_modifier::~game_item_modifier() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_item_modifier::set_error(int32_t error_code) noexcept
{
    game_item_modifier::_last_error = error_code;
    return (error_code);
}

int32_t game_item_modifier::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "game_item_modifier::initialize",
            "called while object is already initialised");
    this->_id = 0;
    this->_value = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_item_modifier::initialize(int32_t id, int32_t value) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "game_item_modifier::initialize",
            "called while object is already initialised");
    this->_id = id;
    this->_value = value;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_item_modifier::initialize(const game_item_modifier &other) noexcept
{
    int32_t lock_error;

    if (this == &other)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "game_item_modifier::initialize(copy)",
            "source object is uninitialised");
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->_id = other._id;
    this->_value = other._value;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_item_modifier::initialize(game_item_modifier &&other) noexcept
{
    int32_t lock_error;

    if (this == &other)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "game_item_modifier::initialize(move)",
            "source object is uninitialised");
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->_id = other._id;
    this->_value = other._value;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._id = 0;
    other._value = 0;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_item_modifier::move(game_item_modifier &other) noexcept
{
    int32_t destroy_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    return (this->initialize(static_cast<game_item_modifier &&>(other)));
}

int32_t game_item_modifier::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_id = 0;
    this->_value = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_item_modifier::enable_thread_safety() noexcept
{
    int32_t initialize_error;
    pt_recursive_mutex *new_mutex;

    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = new_mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_mutex = new_mutex;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_item_modifier::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *old_mutex;

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

ft_bool game_item_modifier::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_item_modifier::get_id() const noexcept
{
    int32_t modifier_id;
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    modifier_id = this->_id;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->set_error(FT_ERR_SUCCESS);
    return (modifier_id);
}

void game_item_modifier::set_id(int32_t id) noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_id = id;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_item_modifier::get_value() const noexcept
{
    int32_t modifier_value;
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    modifier_value = this->_value;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->set_error(FT_ERR_SUCCESS);
    return (modifier_value);
}

void game_item_modifier::set_value(int32_t value) noexcept
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_value = value;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_item_modifier::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_item_modifier::get_error");
    return (game_item_modifier::_last_error);
}

const char *game_item_modifier::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_item_modifier::get_error_str");
    return (ft_strerror(this->get_error()));
}

static void game_item_sleep_backoff()
{
    pt_thread_sleep(1);
    return ;
}

int32_t game_item::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void game_item::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_item::lock_pair(const game_item &first, const game_item &second,
        ft_bool *first_locked, ft_bool *second_locked)
{
    const game_item *ordered_first;
    const game_item *ordered_second;
    int32_t lock_error;
    ft_bool swapped;

    if (first_locked != ft_nullptr)
        *first_locked = FT_FALSE;
    if (second_locked != ft_nullptr)
        *second_locked = FT_FALSE;
    if (&first == &second)
        return (first.lock_internal(first_locked));
    ordered_first = &first;
    ordered_second = &second;
    swapped = FT_FALSE;
    if (ordered_first > ordered_second)
    {
        const game_item *temporary;

        temporary = ordered_first;
        ordered_first = ordered_second;
        ordered_second = temporary;
        swapped = FT_TRUE;
    }
    while (FT_TRUE)
    {
        ft_bool lower_locked;
        ft_bool upper_locked;

        lower_locked = FT_FALSE;
        upper_locked = FT_FALSE;
        lock_error = ordered_first->lock_internal(&lower_locked);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        lock_error = ordered_second->lock_internal(&upper_locked);
        if (lock_error == FT_ERR_SUCCESS)
        {
            if (!swapped)
            {
                if (first_locked != ft_nullptr)
                    *first_locked = lower_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = upper_locked;
            }
            else
            {
                if (first_locked != ft_nullptr)
                    *first_locked = upper_locked;
                if (second_locked != ft_nullptr)
                    *second_locked = lower_locked;
            }
            return (FT_ERR_SUCCESS);
        }
        if (lock_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            ordered_first->unlock_internal(lower_locked);
            return (lock_error);
        }
        ordered_first->unlock_internal(lower_locked);
        game_item_sleep_backoff();
    }
}

game_item::game_item() noexcept
    : _max_stack(0), _stack_size(0), _item_id(0), _rarity(0),
      _width(1), _height(1), _instance_id(0U), _modifier1(), _modifier2(),
      _modifier3(), _modifier4(),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_item::~game_item() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_item::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "game_item::initialize",
            "called while object is already initialised");
    this->_max_stack = 0;
    this->_stack_size = 0;
    this->_item_id = 0;
    this->_rarity = 0;
    this->_width = 1;
    this->_height = 1;
    this->_instance_id = 0U;
    (void)this->_modifier1.initialize();
    (void)this->_modifier2.initialize();
    (void)this->_modifier3.initialize();
    (void)this->_modifier4.initialize();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_item::initialize(const game_item &other) noexcept
{
    ft_bool other_locked;
    int32_t lock_error;
    int32_t modifier_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "game_item::initialize(copy)",
            "source object is uninitialised");
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    other_locked = FT_FALSE;
    lock_error = other.lock_internal(&other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->_max_stack = other._max_stack;
    this->_stack_size = other._stack_size;
    this->_item_id = other._item_id;
    this->_rarity = other._rarity;
    this->_width = other._width;
    this->_height = other._height;
    this->_instance_id = other._instance_id;
    modifier_error = this->_modifier1.initialize(other._modifier1);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    modifier_error = this->_modifier2.initialize(other._modifier2);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    modifier_error = this->_modifier3.initialize(other._modifier3);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    modifier_error = this->_modifier4.initialize(other._modifier4);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    this->set_error(other.get_error());
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other.unlock_internal(other_locked);
    return (FT_ERR_SUCCESS);
}

int32_t game_item::initialize(game_item &&other) noexcept
{
    ft_bool this_locked;
    ft_bool other_locked;
    int32_t lock_error;
    int32_t modifier_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "game_item::initialize(move)",
            "source object is uninitialised");
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    this_locked = FT_FALSE;
    other_locked = FT_FALSE;
    lock_error = game_item::lock_pair(*this, other, &this_locked, &other_locked);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->_max_stack = other._max_stack;
    this->_stack_size = other._stack_size;
    this->_item_id = other._item_id;
    this->_rarity = other._rarity;
    this->_width = other._width;
    this->_height = other._height;
    this->_instance_id = other._instance_id;
    modifier_error = this->_modifier1.initialize(other._modifier1);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(this_locked);
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    modifier_error = this->_modifier2.initialize(other._modifier2);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(this_locked);
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    modifier_error = this->_modifier3.initialize(other._modifier3);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(this_locked);
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    modifier_error = this->_modifier4.initialize(other._modifier4);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(this_locked);
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    this->set_error(other.get_error());
    other._max_stack = 0;
    other._stack_size = 0;
    other._item_id = 0;
    other._rarity = 0;
    other._width = 1;
    other._height = 1;
    other._instance_id = 0U;
    modifier_error = game_item_reset_modifier(other._modifier1);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(this_locked);
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    modifier_error = game_item_reset_modifier(other._modifier2);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(this_locked);
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    modifier_error = game_item_reset_modifier(other._modifier3);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(this_locked);
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    modifier_error = game_item_reset_modifier(other._modifier4);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(this_locked);
        other.unlock_internal(other_locked);
        return (modifier_error);
    }
    other.set_error(FT_ERR_SUCCESS);
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->unlock_internal(this_locked);
    other.unlock_internal(other_locked);
    return (FT_ERR_SUCCESS);
}

int32_t game_item::move(game_item &other) noexcept
{
    int32_t destroy_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    return (this->initialize(static_cast<game_item &&>(other)));
}

int32_t game_item::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_max_stack = 0;
    this->_stack_size = 0;
    this->_item_id = 0;
    this->_rarity = 0;
    this->_width = 1;
    this->_height = 1;
    this->_instance_id = 0U;
    (void)this->_modifier1.destroy();
    (void)this->_modifier2.destroy();
    (void)this->_modifier3.destroy();
    (void)this->_modifier4.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_item::enable_thread_safety() noexcept
{
    int32_t initialize_error;
    pt_recursive_mutex *new_mutex;

    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = new_mutex->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        return (initialize_error);
    }
    this->_mutex = new_mutex;
    return (FT_ERR_SUCCESS);
}

int32_t game_item::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *old_mutex;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    old_mutex = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = old_mutex->destroy();
    delete old_mutex;
    return (destroy_error);
}

ft_bool game_item::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_item::get_max_stack() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t max_stack_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    max_stack_value = this->_max_stack;
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (max_stack_value);
}

void game_item::set_max_stack(int32_t max) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_max_stack = max;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_stack_size() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t stack_size_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    stack_size_value = this->_stack_size;
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (stack_size_value);
}

void game_item::set_stack_size(int32_t amount) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_stack_size = amount;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_item::add_to_stack(int32_t amount) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_stack_size += amount;
    if (this->_stack_size > this->_max_stack)
        this->_stack_size = this->_max_stack;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

void game_item::sub_from_stack(int32_t amount) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_stack_size -= amount;
    if (this->_stack_size < 0)
        this->_stack_size = 0;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_item_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t item_identifier;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    item_identifier = this->_item_id;
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (item_identifier);
}

void game_item::set_item_id(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_item_id = id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

uint64_t game_item::get_instance_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    uint64_t instance_identifier;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0U);
    }
    instance_identifier = this->_instance_id;
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (instance_identifier);
}

void game_item::set_instance_id(uint64_t instance_id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_instance_id = instance_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
}

int32_t game_item::get_width() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t width_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    width_value = this->_width;
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (width_value);
}

void game_item::set_width(int32_t width) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_width = width;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_height() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t height_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    height_value = this->_height;
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (height_value);
}

void game_item::set_height(int32_t height) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_height = height;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_rarity() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t rarity_value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    rarity_value = this->_rarity;
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (rarity_value);
}

void game_item::set_rarity(int32_t rarity) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_rarity = rarity;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier1(game_item_modifier &modifier) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (lock_error);
    }
    modifier_error = modifier.initialize(this->_modifier1);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (modifier_error);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

void game_item::set_modifier1(const game_item_modifier &mod) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    modifier_error = this->_modifier1.initialize(mod);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier1_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_identifier;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    modifier_identifier = this->_modifier1.get_id();
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (modifier_identifier);
}

void game_item::set_modifier1_id(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier1.set_id(id);
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier1_value() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_value;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    modifier_value = this->_modifier1.get_value();
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (modifier_value);
}

void game_item::set_modifier1_value(int32_t value) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier1.set_value(value);
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier2(game_item_modifier &modifier) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (lock_error);
    }
    modifier_error = modifier.initialize(this->_modifier2);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (modifier_error);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

void game_item::set_modifier2(const game_item_modifier &mod) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    modifier_error = this->_modifier2.initialize(mod);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier2_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_identifier;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    modifier_identifier = this->_modifier2.get_id();
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (modifier_identifier);
}

void game_item::set_modifier2_id(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier2.set_id(id);
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier2_value() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_value;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    modifier_value = this->_modifier2.get_value();
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (modifier_value);
}

void game_item::set_modifier2_value(int32_t value) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier2.set_value(value);
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier3(game_item_modifier &modifier) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (lock_error);
    }
    modifier_error = modifier.initialize(this->_modifier3);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (modifier_error);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

void game_item::set_modifier3(const game_item_modifier &mod) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    modifier_error = this->_modifier3.initialize(mod);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier3_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_identifier;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    modifier_identifier = this->_modifier3.get_id();
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (modifier_identifier);
}

void game_item::set_modifier3_id(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier3.set_id(id);
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier3_value() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_value;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    modifier_value = this->_modifier3.get_value();
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (modifier_value);
}

void game_item::set_modifier3_value(int32_t value) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier3.set_value(value);
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier4(game_item_modifier &modifier) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (lock_error);
    }
    modifier_error = modifier.initialize(this->_modifier4);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (modifier_error);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

void game_item::set_modifier4(const game_item_modifier &mod) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    modifier_error = this->_modifier4.initialize(mod);
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier4_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_identifier;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    modifier_identifier = this->_modifier4.get_id();
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (modifier_identifier);
}

void game_item::set_modifier4_id(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier4.set_id(id);
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::get_modifier4_value() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_value;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(lock_error);
        return (0);
    }
    modifier_value = this->_modifier4.get_value();
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        const_cast<game_item *>(this)->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    const_cast<game_item *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (modifier_value);
}

void game_item::set_modifier4_value(int32_t value) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t modifier_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier4.set_value(value);
    modifier_error = FT_ERR_SUCCESS;
    if (modifier_error != FT_ERR_SUCCESS)
    {
        this->set_error(modifier_error);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_item::set_error(int32_t error_code) noexcept
{
    game_item::_last_error = error_code;
    return (error_code);
}

int32_t game_item::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_item::get_error");
    return (game_item::_last_error);
}

const char *game_item::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_item::get_error_str");
    return (ft_strerror(this->get_error()));
}
