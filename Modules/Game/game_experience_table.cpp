#include "../PThread/pthread_internal.hpp"
#include "game_experience_table.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_experience_table::_last_error = FT_ERR_SUCCESS;

game_experience_table::game_experience_table() noexcept
    : _levels(ft_nullptr), _count(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_experience_table::~game_experience_table() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_experience_table::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_experience_table::initialize", "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->_levels = ft_nullptr;
    this->_count = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_experience_table::destroy() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t disable_error;
    int32_t first_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    first_error = FT_ERR_SUCCESS;
    if (lock_error != FT_ERR_SUCCESS)
    {
        first_error = lock_error;
    }
    if (lock_error == FT_ERR_SUCCESS && this->_levels)
        cma_free(this->_levels);
    this->_levels = ft_nullptr;
    this->_count = 0;
    this->unlock_internal(lock_acquired);
    disable_error = this->disable_thread_safety();
    if (first_error == FT_ERR_SUCCESS && disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(first_error);
    return (first_error);
}

int32_t game_experience_table::move(game_experience_table &other) noexcept
{
    int32_t destroy_error;
    int32_t initialize_error;
    int32_t source_error;
    int32_t source_disable_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_experience_table::move",
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
    this->_levels = other._levels;
    this->_count = other._count;
    source_error = other.get_error();
    other._levels = ft_nullptr;
    other._count = 0;
    source_disable_error = other.disable_thread_safety();
    if (source_disable_error != FT_ERR_SUCCESS)
    {
        this->set_error(source_disable_error);
        return (source_disable_error);
    }
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.set_error(source_error);
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_experience_table::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::lock_internal");
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void game_experience_table::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}


int32_t game_experience_table::set_error(int32_t error_code) noexcept
{
    game_experience_table::_last_error = error_code;
    return (error_code);
}

ft_bool game_experience_table::is_valid(int32_t count, const int32_t *array) const noexcept
{
    if (!array || count <= 1)
        return (FT_TRUE);
    int32_t index;

    index = 1;
    while (index < count)
    {
        if (array[index] <= array[index - 1])
            return (FT_FALSE);
        ++index;
    }
    return (FT_TRUE);
}

int32_t game_experience_table::resize_locked(int32_t new_count,
        ft_bool validate_existing) noexcept
{
    this->set_error(FT_ERR_SUCCESS);
    if (new_count <= 0)
    {
        if (this->_levels)
            cma_free(this->_levels);
        this->_levels = ft_nullptr;
        this->_count = 0;
        return (FT_ERR_SUCCESS);
    }
    int32_t old_count;
    int32_t *new_levels;

    old_count = this->_count;
    new_levels = static_cast<int32_t*>(cma_realloc(this->_levels,
                sizeof(int32_t) * new_count));
    if (!new_levels)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (this->get_error());
    }
    if (new_count > old_count)
    {
        int32_t index;

        index = old_count;
        while (index < new_count)
        {
            new_levels[index] = 0;
            ++index;
        }
    }
    this->_levels = new_levels;
    this->_count = new_count;
    if (validate_existing)
    {
        int32_t check_count;

        check_count = old_count;
        if (old_count > new_count)
            check_count = new_count;
        if (!this->is_valid(check_count, this->_levels))
        {
            this->set_error(FT_ERR_CONFIGURATION);
            return (this->get_error());
        }
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_experience_table::get_count() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t count_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::get_count");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_experience_table *>(this)->set_error(lock_error);
        return (0);
    }
    count_value = this->_count;
    const_cast<game_experience_table *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (count_value);
}

int32_t game_experience_table::resize(int32_t new_count) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::resize");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    result = this->resize_locked(new_count);
    this->unlock_internal(lock_acquired);
    return (result);
}

int32_t game_experience_table::get_level(int32_t experience) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t level;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::get_level");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_experience_table *>(this)->set_error(lock_error);
        return (0);
    }
    if (!this->_levels || this->_count == 0)
    {
        const_cast<game_experience_table *>(this)->set_error(FT_ERR_SUCCESS);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    level = 0;
    while (level < this->_count && experience >= this->_levels[level])
        ++level;
    const_cast<game_experience_table *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (level);
}

int32_t game_experience_table::get_value(int32_t index) const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::get_value");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_experience_table *>(this)->set_error(lock_error);
        return (0);
    }
    if (index < 0 || index >= this->_count || !this->_levels)
    {
        const_cast<game_experience_table *>(this)->set_error(FT_ERR_OUT_OF_RANGE);
        this->unlock_internal(lock_acquired);
        return (0);
    }
    value = this->_levels[index];
    const_cast<game_experience_table *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (value);
}

void game_experience_table::set_value(int32_t index, int32_t value) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::set_value");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    if (index < 0 || index >= this->_count || !this->_levels)
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->_levels[index] = value;
    if (!this->is_valid(this->_count, this->_levels))
    {
        this->set_error(FT_ERR_CONFIGURATION);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_experience_table::set_levels(const int32_t *levels, int32_t count) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t resize_result;
    int32_t level_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::set_levels");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->set_error(FT_ERR_SUCCESS);
    if (count <= 0 || !levels)
    {
        resize_result = this->resize_locked(0);
        this->unlock_internal(lock_acquired);
        return (resize_result);
    }
    resize_result = this->resize_locked(count, FT_FALSE);
    if (resize_result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        return (this->get_error());
    }
    level_index = 0;
    while (level_index < count)
    {
        this->_levels[level_index] = levels[level_index];
        ++level_index;
    }
    if (!this->is_valid(this->_count, this->_levels))
    {
        this->set_error(FT_ERR_CONFIGURATION);
        this->unlock_internal(lock_acquired);
        return (this->get_error());
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_experience_table::generate_levels_total(int32_t count, int32_t base,
        double multiplier) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t resize_result;
    double value;
    int32_t level_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::generate_levels_total");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->set_error(FT_ERR_SUCCESS);
    if (count <= 0)
    {
        resize_result = this->resize_locked(0);
        if (resize_result == FT_ERR_SUCCESS)
            this->set_error(FT_ERR_SUCCESS);
        this->unlock_internal(lock_acquired);
        return (resize_result);
    }
    resize_result = this->resize_locked(count, FT_FALSE);
    if (resize_result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        return (this->get_error());
    }
    value = static_cast<double>(base);
    level_index = 0;
    while (level_index < count)
    {
        this->_levels[level_index] = static_cast<int32_t>(value);
        value *= multiplier;
        ++level_index;
    }
    if (!this->is_valid(this->_count, this->_levels))
    {
        this->set_error(FT_ERR_CONFIGURATION);
        this->unlock_internal(lock_acquired);
        return (this->get_error());
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_experience_table::generate_levels_scaled(int32_t count, int32_t base,
        double multiplier) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t resize_result;
    double increment;
    double total;
    int32_t index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::generate_levels_scaled");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    this->set_error(FT_ERR_SUCCESS);
    if (count <= 0)
    {
        resize_result = this->resize_locked(0);
        this->unlock_internal(lock_acquired);
        return (resize_result);
    }
    resize_result = this->resize_locked(count, FT_FALSE);
    if (resize_result != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        return (this->get_error());
    }
    increment = static_cast<double>(base);
    total = static_cast<double>(base);
    this->_levels[0] = static_cast<int32_t>(total);
    index = 1;
    while (index < count)
    {
        increment *= multiplier;
        total += increment;
        this->_levels[index] = static_cast<int32_t>(total);
        ++index;
    }
    if (!this->is_valid(this->_count, this->_levels))
    {
        this->set_error(FT_ERR_CONFIGURATION);
        this->unlock_internal(lock_acquired);
        return (this->get_error());
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_experience_table::check_for_error() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::check_for_error");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_experience_table *>(this)->set_error(lock_error);
        return (lock_error);
    }
    if (!this->_levels)
    {
        const_cast<game_experience_table *>(this)->set_error(FT_ERR_SUCCESS);
        this->unlock_internal(lock_acquired);
        return (FT_ERR_SUCCESS);
    }
    index = 1;
    while (index < this->_count)
    {
        if (this->_levels[index] <= this->_levels[index - 1])
        {
            const_cast<game_experience_table *>(this)->set_error(FT_ERR_CONFIGURATION);
            this->unlock_internal(lock_acquired);
            return (this->_levels[index]);
        }
        ++index;
    }
    const_cast<game_experience_table *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_experience_table::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::get_error");
    return (game_experience_table::_last_error);
}

const char *game_experience_table::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::get_error_str");
    return (ft_strerror(this->get_error()));
}

int32_t game_experience_table::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_experience_table::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
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

int32_t game_experience_table::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_experience_table::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}
