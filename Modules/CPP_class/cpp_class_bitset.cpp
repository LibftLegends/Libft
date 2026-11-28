#include "bitset.hpp"
#include "../Basic/limits.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

int32_t ft_bitset::set_error(int32_t error_code)
{
    _last_error = error_code;
    return (error_code);
}

ft_size_t ft_bitset::block_index(ft_size_t position) const
{
    return (position / BITS_PER_BLOCK);
}

ft_size_t ft_bitset::bit_mask(ft_size_t position) const
{
    return (static_cast<ft_size_t>(1) << (position % BITS_PER_BLOCK));
}

void ft_bitset::destroy_data()
{
    if (this->_data != ft_nullptr)
    {
        cma_free(this->_data);
        this->_data = ft_nullptr;
    }
    this->_size = 0;
    this->_block_count = 0;
    return ;
}

int32_t ft_bitset::lock_internal(ft_bool *lock_acquired) const
{
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (set_error(FT_ERR_SYS_MUTEX_LOCK_FAILED));
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t ft_bitset::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE || this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

ft_bitset::ft_bitset(ft_size_t bits)
    : _size(0)
    , _configured_bits(bits)
    , _block_count(0)
    , _data(ft_nullptr)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_bitset::~ft_bitset()
{
    uint32_t previous_error;

    previous_error = _last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    (void)this->disable_thread_safety();
    (void)set_error(previous_error);
    return ;
}

int32_t ft_bitset::initialize_data(const ft_size_t *source_data)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_bitset::initialize",
            "called while object is already initialised");
        return (set_error(FT_ERR_INVALID_STATE));
    }
    this->_size = this->_configured_bits;
    this->_block_count = this->_configured_bits / BITS_PER_BLOCK
        + (this->_configured_bits % BITS_PER_BLOCK != 0);
    this->_data = ft_nullptr;
    if (this->_block_count > 0)
    {
        const ft_size_t byte_count = this->_block_count * sizeof(ft_size_t);
        this->_data = static_cast<ft_size_t *>(cma_malloc(byte_count));
        if (this->_data == ft_nullptr)
        {
            this->_size = 0;
            this->_block_count = 0;
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (set_error(FT_ERR_BITSET_NO_MEMORY));
        }
        if (source_data == ft_nullptr)
        {
            ft_size_t index = 0;
            while (index < this->_block_count)
            {
                this->_data[index] = 0;
                ++index;
            }
        }
        else
            (void)ft_memcpy(this->_data, source_data, byte_count);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_bitset::initialize()
{
    return (this->initialize_data(ft_nullptr));
}

int32_t ft_bitset::initialize(ft_size_t bits)
{
    this->_configured_bits = bits;
    return (this->initialize());
}

int32_t ft_bitset::initialize(const ft_bitset &other)
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_bitset::initialize(copy)", "source object is not initialised");
    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(other._last_error));
    }
    this->_configured_bits = other._size;
    return (this->initialize_data(other._data));
}

int32_t ft_bitset::initialize(ft_bitset &&other)
{
    return (this->move(other));
}

int32_t ft_bitset::move(ft_bitset &other)
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_bitset::move", "source object is not initialised");
    if (this == &other)
        return (set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (set_error(other._last_error));
    }
    this->_size = other._size;
    this->_configured_bits = other._configured_bits;
    this->_block_count = other._block_count;
    this->_data = other._data;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._size = 0;
    other._configured_bits = 0;
    other._block_count = 0;
    other._data = ft_nullptr;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (set_error(other._last_error));
}

int32_t ft_bitset::destroy()
{
    ft_bool lock_acquired;
    int32_t lock_result;
    int32_t disable_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (set_error(FT_ERR_SUCCESS));
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (set_error(lock_result));
    this->destroy_data();
    (void)this->unlock_internal(lock_acquired);
    disable_result = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_result != FT_ERR_SUCCESS)
        return (set_error(disable_result));
    return (set_error(FT_ERR_SUCCESS));
}

void ft_bitset::set(ft_size_t position)
{
    ft_bool lock_acquired;
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_bitset::set");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    if (position >= this->_size)
    {
        set_error(FT_ERR_OUT_OF_RANGE);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    this->_data[this->block_index(position)] |= this->bit_mask(position);
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

void ft_bitset::reset(ft_size_t position)
{
    ft_bool lock_acquired;
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_bitset::reset");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    if (position >= this->_size)
    {
        set_error(FT_ERR_OUT_OF_RANGE);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    this->_data[this->block_index(position)] &= ~this->bit_mask(position);
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

void ft_bitset::flip(ft_size_t position)
{
    ft_bool lock_acquired;
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_bitset::flip");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    if (position >= this->_size)
    {
        set_error(FT_ERR_OUT_OF_RANGE);
        (void)this->unlock_internal(lock_acquired);
        return ;
    }
    this->_data[this->block_index(position)] ^= this->bit_mask(position);
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_bool ft_bitset::test(ft_size_t position) const
{
    ft_bool lock_acquired;
    int32_t lock_result;
    ft_bool result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_bitset::test");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (position >= this->_size)
    {
        set_error(FT_ERR_OUT_OF_RANGE);
        (void)this->unlock_internal(lock_acquired);
        return (FT_FALSE);
    }
    result = (this->_data[this->block_index(position)] & this->bit_mask(position)) != 0;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (result);
}

ft_size_t ft_bitset::size() const
{
    ft_size_t current_size;
    ft_bool lock_acquired;
    int32_t lock_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_bitset::size");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return (0);
    current_size = this->_size;
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return (current_size);
}

void ft_bitset::clear()
{
    ft_bool lock_acquired;
    int32_t lock_result;
    ft_size_t index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_bitset::clear");
    lock_acquired = FT_FALSE;
    lock_result = this->lock_internal(&lock_acquired);
    if (lock_result != FT_ERR_SUCCESS)
        return ;
    index = 0;
    while (index < this->_block_count)
    {
        this->_data[index] = 0;
        index += 1;
    }
    set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t ft_bitset::enable_thread_safety()
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_bitset::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
        return (set_error(FT_ERR_NO_MEMORY));
    initialize_result = new_mutex->initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        return (set_error(initialize_result));
    }
    this->_mutex = new_mutex;
    return (set_error(FT_ERR_SUCCESS));
}

int32_t ft_bitset::disable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        && this->_initialised_state != FT_CLASS_STATE_DESTROYED)
        return (set_error(FT_ERR_INVALID_STATE));
    mutex_pointer = this->_mutex;
    if (mutex_pointer == ft_nullptr)
        return (set_error(FT_ERR_SUCCESS));
    this->_mutex = ft_nullptr;
    destroy_result = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_result != FT_ERR_SUCCESS)
        return (set_error(destroy_result));
    return (set_error(FT_ERR_SUCCESS));
}

ft_bool ft_bitset::is_thread_safe() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_bitset::is_thread_safe");
    set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

int32_t ft_bitset::lock(ft_bool *lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_bitset::lock");
    return (set_error(this->lock_internal(lock_acquired)));
}

void ft_bitset::unlock(ft_bool lock_acquired) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_bitset::unlock");
    (void)this->unlock_internal(lock_acquired);
    set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t ft_bitset::get_error() const
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_bitset::get_error");
    return (_last_error);
}

const char *ft_bitset::get_error_str() const
{
    errno_abort_if_uninitialised(this->_initialised_state, "ft_bitset::get_error_str");
    return (ft_strerror(_last_error));
}

thread_local int32_t ft_bitset::_last_error = FT_ERR_SUCCESS;
