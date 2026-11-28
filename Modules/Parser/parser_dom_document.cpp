#include "dom.hpp"
#include "../CMA/CMA.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/pthread.hpp"
#include "../Basic/basic.hpp"
#include "../Printf/printf.hpp"
#include "../Errno/errno_internal.hpp"
#include "../System_utils/system_utils.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"

ft_dom_document::ft_dom_document() noexcept
    : _root(ft_nullptr), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_dom_document::~ft_dom_document() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


int32_t ft_dom_document::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_dom_document::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_root = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_dom_document::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    this->clear();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (disable_error);
}

int32_t ft_dom_document::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_document::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t ft_dom_document::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool ft_dom_document::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t ft_dom_document::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (FT_ERR_SUCCESS);
}

int32_t ft_dom_document::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == false)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_dom_document::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_document::lock");
    return (this->lock_internal(lock_acquired));
}

void ft_dom_document::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_document::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

void ft_dom_document::set_root(ft_dom_node *root) noexcept
{
    (void)this->replace_root(root);
    return ;
}

int32_t ft_dom_document::replace_root(ft_dom_node *root) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_document::replace_root");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    ft_dom_node *previous_root = this->_root;
    this->_root = root;
    (void)this->unlock_internal(lock_acquired);
    if (previous_root != root)
        delete previous_root;
    return (FT_ERR_SUCCESS);
}

ft_dom_node *ft_dom_document::get_root() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_dom_node *root_pointer;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_document::get_root");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    root_pointer = this->_root;
    (void)this->unlock_internal(lock_acquired);
    return (root_pointer);
}

void ft_dom_document::clear() noexcept
{
    (void)this->replace_root(ft_nullptr);
    return ;
}
