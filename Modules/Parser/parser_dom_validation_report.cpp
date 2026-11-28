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
#include "../Template/vector.hpp"
#include "../Template/move.hpp"

thread_local int32_t ft_dom_validation_report::_last_error = FT_ERR_SUCCESS;

int32_t ft_dom_validation_report::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

ft_dom_schema_rule::ft_dom_schema_rule() noexcept
    : path(), type(FT_DOM_NODE_NULL), required(false)
{
    return ;
}

ft_dom_schema_rule::~ft_dom_schema_rule() noexcept
{
    return ;
}

int32_t ft_dom_schema_rule::initialize(const ft_dom_schema_rule &other) noexcept
{
    int32_t error_code;

    error_code = this->path.initialize(other.path);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->type = other.type;
    this->required = other.required;
    return (FT_ERR_SUCCESS);
}

int32_t ft_dom_schema_rule::destroy() noexcept
{
    this->type = FT_DOM_NODE_NULL;
    this->required = false;
    return (this->path.destroy());
}

int32_t ft_dom_schema_rule::initialize(ft_dom_schema_rule &&other) noexcept
{
    const int32_t status = this->path.initialize(ft_move(other.path));
    if (status != FT_ERR_SUCCESS)
        return (status);
    this->type = other.type;
    this->required = other.required;
    return (FT_ERR_SUCCESS);
}

ft_dom_validation_error::ft_dom_validation_error() noexcept
    : path(), message()
{
    return ;
}

ft_dom_validation_error::~ft_dom_validation_error() noexcept
{
    return ;
}

int32_t ft_dom_validation_error::initialize(
    const ft_dom_validation_error &other) noexcept
{
    int32_t error_code;

    error_code = this->path.initialize(other.path);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->message.initialize(other.message);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t ft_dom_validation_error::destroy() noexcept
{
    int32_t first_error;
    int32_t error_code;

    first_error = this->path.destroy();
    error_code = this->message.destroy();
    if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    return (first_error);
}

ft_dom_validation_report::ft_dom_validation_report() noexcept
    : _valid(true), _errors(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    (void)set_error(FT_ERR_SUCCESS);
    return ;
}


int32_t ft_dom_validation_report::initialize() noexcept
{
    int32_t member_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_dom_validation_report::initialize", "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_valid = true;
    member_initialize_error = this->_errors.initialize();
    if (member_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(member_initialize_error));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_dom_validation_report::destroy() noexcept
{
    int32_t disable_error;
    int32_t errors_destroy_error;
    int32_t first_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    errors_destroy_error = this->_errors.destroy();
    if (first_error == FT_ERR_SUCCESS
        && errors_destroy_error != FT_ERR_SUCCESS)
        first_error = errors_destroy_error;
    this->_valid = true;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(first_error));
}

int32_t ft_dom_validation_report::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised(this->_initialised_state, "ft_dom_validation_report::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (set_error(FT_ERR_NO_MEMORY));
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (set_error(initialize_error));
    }
    this->_mutex = mutex_pointer;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_dom_validation_report::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (set_error(destroy_error));
}

ft_bool ft_dom_validation_report::is_thread_safe() const noexcept
{
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

int32_t ft_dom_validation_report::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_dom_validation_report::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == false)
        return (set_error(FT_ERR_SUCCESS));
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_dom_validation_report::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_validation_report::lock");
    return (this->lock_internal(lock_acquired));
}

void ft_dom_validation_report::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_dom_validation_report::unlock");
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t ft_dom_validation_report::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "ft_dom_validation_report::get_error");
    return (_last_error);
}

const char *ft_dom_validation_report::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "ft_dom_validation_report::get_error_str");
    return (ft_strerror(_last_error));
}

ft_dom_validation_report::~ft_dom_validation_report() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t ft_dom_validation_report::reset() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_validation_report::reset");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    this->_errors.clear();
    const int32_t status = this->_errors.get_error();
    if (status == FT_ERR_SUCCESS)
        this->_valid = FT_TRUE;
    (void)this->unlock_internal(lock_acquired);
    return (set_error(status));
}

void ft_dom_validation_report::mark_valid() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised(this->_initialised_state, "ft_dom_validation_report::mark_valid");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->_valid = true;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

void ft_dom_validation_report::mark_invalid() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_validation_report::mark_invalid");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->_valid = false;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

ft_bool ft_dom_validation_report::valid() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_bool valid_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_validation_report::valid");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return (false);
    }
    valid_value = this->_valid;
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return (valid_value);
}

int32_t ft_dom_validation_report::add_error(const ft_string &path,
    const ft_string &message) noexcept
{
    ft_dom_validation_error error_entry;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised(this->_initialised_state, "ft_dom_validation_report::add_error");
    lock_error = error_entry.path.initialize(path);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    lock_error = error_entry.message.initialize(message);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    const int32_t status = this->_errors.push_back(error_entry);
    (void)this->unlock_internal(lock_acquired);
    return (set_error(status));
}

const ft_vector<ft_dom_validation_error> &ft_dom_validation_report::errors() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_dom_validation_report::errors");
    set_error(FT_ERR_SUCCESS);
    return (this->_errors);
}
