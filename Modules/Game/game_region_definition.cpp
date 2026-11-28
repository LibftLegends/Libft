#include "../PThread/pthread_internal.hpp"
#include "game_region_definition.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_region_definition::_last_error = FT_ERR_SUCCESS;

game_region_definition::game_region_definition() noexcept
    : _region_id(0), _name(), _description(), _recommended_level(0),
      _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_region_definition::~game_region_definition() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return ;
    (void)this->destroy();
    return ;
}

int32_t game_region_definition::set_error(int32_t error_code) noexcept
{
    game_region_definition::_last_error = error_code;
    return (error_code);
}

int32_t game_region_definition::initialize() noexcept
{
    int32_t name_error;
    int32_t description_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_region_definition::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    name_error = this->_name.initialize();
    if (name_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(name_error);
        return (name_error);
    }
    description_error = this->_description.initialize();
    if (description_error != FT_ERR_SUCCESS)
    {
        (void)this->_name.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(description_error);
        return (description_error);
    }
    this->_region_id = 0;
    this->_recommended_level = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_region_definition::initialize(const game_region_definition &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_region_definition::initialize(copy)",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->_name.assign(other._name);
        if (result == FT_ERR_SUCCESS)
            result = this->_description.assign(other._description);
        if (result == FT_ERR_SUCCESS)
        {
            this->_region_id = other._region_id;
            this->_recommended_level = other._recommended_level;
        }
        (void)other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_region_definition::initialize(game_region_definition &&other) noexcept
{
    return (this->move(other));
}

int32_t game_region_definition::move(game_region_definition &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_region_definition::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    result = this->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        // The private strings are synchronized by the region mutex.
        result = this->_name.move(other._name);
        if (result == FT_ERR_SUCCESS)
        {
            result = this->_description.move(other._description);
            if (result != FT_ERR_SUCCESS)
                (void)other._name.move(this->_name);
        }
        if (result == FT_ERR_SUCCESS)
        {
            this->_region_id = other._region_id;
            this->_recommended_level = other._recommended_level;
        }
        (void)other.unlock_internal(source_locked);
    }
    if (result == FT_ERR_SUCCESS)
        result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_region_definition::initialize(int32_t region_id, const ft_string &name,
    const ft_string &description, int32_t recommended_level) noexcept
{
    ft_string staged_name;
    ft_string staged_description;
    int32_t result = FT_ERR_INVALID_ARGUMENT;

    // Either argument may refer to a string in this region.
    if (name.is_initialised() && description.is_initialised())
    {
        result = staged_name.initialize();
        if (result == FT_ERR_SUCCESS)
            result = staged_description.initialize();
        if (result == FT_ERR_SUCCESS)
            result = staged_name.assign(name);
        if (result == FT_ERR_SUCCESS)
            result = staged_description.assign(description);
    }
    const int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    if (result != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    result = this->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_name.move(staged_name);
    if (result == FT_ERR_SUCCESS)
        result = this->_description.move(staged_description);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->_region_id = region_id;
    this->_recommended_level = recommended_level;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_region_definition::destroy() noexcept
{
    int32_t disable_error;
    int32_t description_error;
    int32_t name_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    description_error = this->_description.destroy();
    name_error = this->_name.destroy();
    this->_region_id = 0;
    this->_recommended_level = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
    {
        this->set_error(disable_error);
        return (disable_error);
    }
    if (description_error != FT_ERR_SUCCESS)
    {
        this->set_error(description_error);
        return (description_error);
    }
    if (name_error != FT_ERR_SUCCESS)
    {
        this->set_error(name_error);
        return (name_error);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_region_definition::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::enable_thread_safety");
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

int32_t game_region_definition::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_region_definition::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_region_definition::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_region_definition::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (this->_mutex != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_region_definition::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_region_definition::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_region_definition::get_region_id() const noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::get_region_id");
    const int32_t lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (0);
    const int32_t region_id = this->_region_id;
    (void)this->unlock_internal(lock_acquired);
    return (region_id);
}

void game_region_definition::set_region_id(int32_t region_id) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::set_region_id");
    const int32_t lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    this->_region_id = region_id;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

const ft_string &game_region_definition::get_name() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::get_name");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_name);
}

void game_region_definition::set_name(const ft_string &name) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::set_name");
    const int32_t lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    int32_t result = FT_ERR_INVALID_ARGUMENT;
    if (name.is_initialised())
        result = this->_name.assign(name);
    (void)this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
}

const ft_string &game_region_definition::get_description() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::get_description");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_description);
}

void game_region_definition::set_description(const ft_string &description) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::set_description");
    const int32_t lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    int32_t result = FT_ERR_INVALID_ARGUMENT;
    if (description.is_initialised())
        result = this->_description.assign(description);
    (void)this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
}

int32_t game_region_definition::get_recommended_level() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::get_recommended_level");
    ft_bool lock_acquired;
    if (this->lock_internal(&lock_acquired) != FT_ERR_SUCCESS)
        return (0);
    const int32_t recommended_level = this->_recommended_level;
    (void)this->unlock_internal(lock_acquired);
    return (recommended_level);
}

void game_region_definition::set_recommended_level(int32_t recommended_level) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_region_definition::set_recommended_level");
    const int32_t lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    this->_recommended_level = recommended_level;
    (void)this->unlock_internal(lock_acquired);
    return ;
}


int32_t game_region_definition::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_region_definition::get_error");
    return (game_region_definition::_last_error);
}

const char *game_region_definition::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_region_definition::get_error_str");
    return (ft_strerror(game_region_definition::_last_error));
}
