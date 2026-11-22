#include "byte_buffer.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t ft_byte_buffer::_last_error = FT_ERR_SUCCESS;

static constexpr ft_bool buffer_add_overflows(ft_size_t left,
    ft_size_t right) noexcept
{
    if (left > FT_SYSTEM_SIZE_MAX - right)
        return (FT_TRUE);
    return (FT_FALSE);
}

static int32_t buffer_encode_varuint64(uint64_t value, uint8_t *buffer,
    ft_size_t buffer_size, ft_size_t *encoded_size) noexcept
{
    ft_size_t index;

    if (buffer == ft_nullptr || encoded_size == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0;
    while (value >= 0x80U)
    {
        if (index >= buffer_size)
            return (FT_ERR_OUT_OF_RANGE);
        buffer[index] = static_cast<uint8_t>((value & 0x7FU) | 0x80U);
        value >>= 7;
        index += 1;
    }
    if (index >= buffer_size)
        return (FT_ERR_OUT_OF_RANGE);
    buffer[index] = static_cast<uint8_t>(value & 0x7FU);
    *encoded_size = index + 1;
    return (FT_ERR_SUCCESS);
}

static int32_t buffer_encode_varint64(int64_t value, uint8_t *buffer,
    ft_size_t buffer_size, ft_size_t *encoded_size) noexcept
{
    ft_size_t index;
    ft_bool more_bytes;
    ft_bool sign_bit_set;
    uint8_t byte_value;

    if (buffer == ft_nullptr || encoded_size == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0;
    more_bytes = FT_TRUE;
    while (more_bytes == FT_TRUE)
    {
        byte_value = static_cast<uint8_t>(value & 0x7F);
        value >>= 7;
        sign_bit_set = (byte_value & 0x40U) != 0;
        if ((value == 0 && sign_bit_set == FT_FALSE)
            || (value == -1 && sign_bit_set == FT_TRUE))
            more_bytes = FT_FALSE;
        else
            byte_value = static_cast<uint8_t>(byte_value | 0x80U);
        if (index >= buffer_size)
            return (FT_ERR_OUT_OF_RANGE);
        buffer[index] = byte_value;
        index += 1;
    }
    *encoded_size = index;
    return (FT_ERR_SUCCESS);
}

static int32_t buffer_decode_varuint64(const uint8_t *buffer, ft_size_t buffer_size,
    uint64_t *value_out, ft_size_t *consumed_size) noexcept
{
    ft_size_t index;
    uint64_t value;
    uint32_t shift;

    if (buffer == ft_nullptr || value_out == ft_nullptr || consumed_size == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    value = 0;
    shift = 0;
    index = 0;
    while (index < buffer_size)
    {
        if (shift >= 64U)
            return (FT_ERR_OUT_OF_RANGE);
        value |= static_cast<uint64_t>(buffer[index] & 0x7FU) << shift;
        index += 1;
        if ((buffer[index - 1] & 0x80U) == 0)
        {
            *value_out = value;
            *consumed_size = index;
            return (FT_ERR_SUCCESS);
        }
        shift += 7U;
    }
    return (FT_ERR_OUT_OF_RANGE);
}

static int32_t buffer_decode_varint64(const uint8_t *buffer, ft_size_t buffer_size,
    int64_t *value_out, ft_size_t *consumed_size) noexcept
{
    ft_size_t index;
    uint64_t value;
    uint32_t shift;
    uint8_t byte_value;

    if (buffer == ft_nullptr || value_out == ft_nullptr || consumed_size == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    value = 0;
    shift = 0;
    index = 0;
    while (index < buffer_size)
    {
        if (shift >= 64U)
            return (FT_ERR_OUT_OF_RANGE);
        byte_value = buffer[index];
        value |= static_cast<uint64_t>(byte_value & 0x7FU) << shift;
        shift += 7U;
        index += 1;
        if ((byte_value & 0x80U) == 0)
        {
            if (shift < 64U && (byte_value & 0x40U) != 0)
                value |= (~static_cast<uint64_t>(0)) << shift;
            *value_out = static_cast<int64_t>(value);
            *consumed_size = index;
            return (FT_ERR_SUCCESS);
        }
    }
    return (FT_ERR_OUT_OF_RANGE);
}

int32_t ft_byte_buffer::set_error(int32_t error_code) noexcept
{
    ft_byte_buffer::_last_error = error_code;
    return (error_code);
}

void ft_byte_buffer::reset_fields(void) noexcept
{
    this->_data = ft_nullptr;
    this->_size = 0;
    this->_capacity = 0;
    this->_read_position = 0;
    this->_fixed_capacity = FT_FALSE;
    this->_mutex = ft_nullptr;
    return ;
}

int32_t ft_byte_buffer::lock_internal(void) const noexcept
{
    return (pt_recursive_mutex_lock_if_not_null(this->_mutex));
}

void ft_byte_buffer::unlock_internal(void) const noexcept
{
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t ft_byte_buffer::reserve_internal(ft_size_t required_capacity) noexcept
{
    uint8_t *new_data;
    ft_size_t new_capacity;

    if (required_capacity <= this->_capacity)
        return (FT_ERR_SUCCESS);
    if (this->_fixed_capacity == FT_TRUE)
        return (FT_ERR_FULL);
    new_capacity = this->_capacity;
    if (new_capacity == 0)
        new_capacity = 16;
    while (new_capacity < required_capacity)
    {
        if (new_capacity > FT_SYSTEM_SIZE_MAX / 2)
        {
            new_capacity = required_capacity;
            break ;
        }
        new_capacity = new_capacity * 2;
    }
    new_data = static_cast<uint8_t *>(cma_realloc(this->_data, new_capacity));
    if (new_data == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    this->_data = new_data;
    this->_capacity = new_capacity;
    return (FT_ERR_SUCCESS);
}

int32_t ft_byte_buffer::append_internal(const void *data, ft_size_t length) noexcept
{
    if (length == 0)
        return (FT_ERR_SUCCESS);
    if (data == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (buffer_add_overflows(this->_size, length) == FT_TRUE)
        return (FT_ERR_OUT_OF_RANGE);
    const ft_size_t required_capacity = this->_size + length;
    if (required_capacity > this->_capacity)
    {
        const uintptr_t source_address = reinterpret_cast<uintptr_t>(data);
        const uintptr_t buffer_address = reinterpret_cast<uintptr_t>(this->_data);
        const ft_bool source_is_internal = static_cast<ft_bool>(
                source_address >= buffer_address
                && source_address - buffer_address < this->_capacity);
        const ft_size_t source_offset = source_address - buffer_address;
        if (source_is_internal == FT_TRUE
            && length > this->_capacity - source_offset)
            return (FT_ERR_OUT_OF_RANGE);
        const int32_t reserve_error = this->reserve_internal(required_capacity);
        if (reserve_error != FT_ERR_SUCCESS)
            return (reserve_error);
        if (source_is_internal == FT_TRUE)
            data = this->_data + source_offset;
    }
    ft_memcpy(this->_data + this->_size, data, length);
    this->_size += length;
    return (FT_ERR_SUCCESS);
}

int32_t ft_byte_buffer::read_internal(void *data, ft_size_t length) noexcept
{
    if (length == 0)
        return (FT_ERR_SUCCESS);
    if (data == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_read_position > this->_size)
        return (FT_ERR_INVALID_STATE);
    if (length > this->_size - this->_read_position)
        return (FT_ERR_OUT_OF_RANGE);
    ft_memcpy(data, this->_data + this->_read_position, length);
    this->_read_position += length;
    return (FT_ERR_SUCCESS);
}

ft_byte_buffer::ft_byte_buffer() noexcept
    : _data(ft_nullptr), _size(0), _capacity(0), _read_position(0),
      _fixed_capacity(FT_FALSE),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
{
    return ;
}

ft_byte_buffer::~ft_byte_buffer() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    else
    {
        cma_free(this->_data);
        this->reset_fields();
        this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    }
    return ;
}

int32_t ft_byte_buffer::initialize() noexcept
{
    return (this->initialize(0, FT_FALSE));
}

int32_t ft_byte_buffer::initialize(ft_size_t capacity,
    ft_bool fixed_capacity) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_byte_buffer::initialize",
            "called while object is already initialised");
    this->reset_fields();
    this->_fixed_capacity = fixed_capacity;
    if (capacity > 0)
    {
        this->_data = static_cast<uint8_t *>(cma_malloc(capacity));
        if (this->_data == ft_nullptr)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (ft_byte_buffer::set_error(FT_ERR_NO_MEMORY));
        }
        this->_capacity = capacity;
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::initialize(const ft_byte_buffer &other) noexcept
{
    int32_t lock_error;
    int32_t initialize_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_byte_buffer::initialize(const ft_byte_buffer &)",
            "called with uninitialised source object");
    if (&other == this)
        return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        initialize_error = this->destroy();
        if (initialize_error != FT_ERR_SUCCESS)
            return (ft_byte_buffer::set_error(initialize_error));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->reset_fields();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
    }
    lock_error = other.lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    initialize_error = this->initialize(other._capacity, other._fixed_capacity);
    if (initialize_error == FT_ERR_SUCCESS && other._size > 0)
        initialize_error = this->append_internal(other._data, other._size);
    if (initialize_error == FT_ERR_SUCCESS)
        this->_read_position = other._read_position;
    other.unlock_internal();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    }
    return (ft_byte_buffer::set_error(initialize_error));
}

int32_t ft_byte_buffer::initialize(ft_byte_buffer &&other) noexcept
{
    return (this->move(other));
}

int32_t ft_byte_buffer::destroy() noexcept
{
    int32_t destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
    }
    destroy_error = this->disable_thread_safety();
    cma_free(this->_data);
    this->reset_fields();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (destroy_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(destroy_error));
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::move(ft_byte_buffer &other) noexcept
{
    int32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_byte_buffer::move",
            "called with uninitialised source object");
    if (&other == this)
        return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        lock_error = this->destroy();
        if (lock_error != FT_ERR_SUCCESS)
            return (ft_byte_buffer::set_error(lock_error));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->reset_fields();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
    }
    lock_error = other.lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    this->_data = other._data;
    this->_size = other._size;
    this->_capacity = other._capacity;
    this->_read_position = other._read_position;
    this->_fixed_capacity = other._fixed_capacity;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._data = ft_nullptr;
    other._size = 0;
    other._capacity = 0;
    other._read_position = 0;
    other._fixed_capacity = FT_FALSE;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->unlock_internal();
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::reserve(ft_size_t required_capacity) noexcept
{
    int32_t lock_error;
    int32_t reserve_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::reserve");
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    reserve_error = this->reserve_internal(required_capacity);
    this->unlock_internal();
    return (ft_byte_buffer::set_error(reserve_error));
}

int32_t ft_byte_buffer::clear() noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::clear");
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    this->_size = 0;
    this->_read_position = 0;
    this->unlock_internal();
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::reset_read_position() noexcept
{
    return (this->set_read_position(0));
}

int32_t ft_byte_buffer::set_read_position(ft_size_t position) noexcept
{
    int32_t lock_error;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::set_read_position");
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    result = FT_ERR_SUCCESS;
    if (position > this->_size)
        result = FT_ERR_OUT_OF_RANGE;
    else
        this->_read_position = position;
    this->unlock_internal();
    return (ft_byte_buffer::set_error(result));
}

int32_t ft_byte_buffer::consume(ft_size_t length) noexcept
{
    int32_t lock_error;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::consume");
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    result = FT_ERR_SUCCESS;
    if (this->_read_position > this->_size)
        result = FT_ERR_INVALID_STATE;
    else if (length > this->_size - this->_read_position)
        result = FT_ERR_OUT_OF_RANGE;
    else
        this->_read_position += length;
    this->unlock_internal();
    return (ft_byte_buffer::set_error(result));
}

int32_t ft_byte_buffer::append(const void *data, ft_size_t length) noexcept
{
    int32_t lock_error;
    int32_t append_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::append");
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    append_error = this->append_internal(data, length);
    this->unlock_internal();
    return (ft_byte_buffer::set_error(append_error));
}

int32_t ft_byte_buffer::append_buffer(const ft_byte_buffer &other) noexcept
{
    const ft_byte_buffer *first_buffer;
    const ft_byte_buffer *second_buffer;
    uintptr_t this_address;
    uintptr_t other_address;
    ft_size_t destination_size;
    ft_size_t source_size;
    int32_t lock_error;
    int32_t append_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::append_buffer");
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_byte_buffer::append_buffer",
            "called with uninitialised source object");
    if (&other == this)
    {
        lock_error = this->lock_internal();
        if (lock_error != FT_ERR_SUCCESS)
            return (ft_byte_buffer::set_error(lock_error));
        append_error = this->append_internal(this->_data, this->_size);
        this->unlock_internal();
        return (ft_byte_buffer::set_error(append_error));
    }
    this_address = reinterpret_cast<uintptr_t>(this);
    other_address = reinterpret_cast<uintptr_t>(&other);
    if (this_address < other_address)
    {
        first_buffer = this;
        second_buffer = &other;
    }
    else
    {
        first_buffer = &other;
        second_buffer = this;
    }
    lock_error = first_buffer->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    if (second_buffer != first_buffer)
    {
        lock_error = second_buffer->lock_internal();
        if (lock_error != FT_ERR_SUCCESS)
        {
            first_buffer->unlock_internal();
            return (ft_byte_buffer::set_error(lock_error));
        }
    }
    destination_size = this->_size;
    source_size = other._size;
    append_error = FT_ERR_SUCCESS;
    if (buffer_add_overflows(destination_size, source_size) == FT_TRUE)
        append_error = FT_ERR_OUT_OF_RANGE;
    else
    {
        append_error = this->reserve_internal(destination_size + source_size);
        if (append_error == FT_ERR_SUCCESS && source_size > 0)
        {
            if (other._data == ft_nullptr)
                append_error = FT_ERR_INVALID_STATE;
            else
            {
                ft_memcpy(this->_data + destination_size, other._data,
                    source_size);
                this->_size = destination_size + source_size;
            }
        }
        else if (append_error == FT_ERR_SUCCESS)
            this->_size = destination_size;
    }
    if (second_buffer != first_buffer)
        second_buffer->unlock_internal();
    first_buffer->unlock_internal();
    return (ft_byte_buffer::set_error(append_error));
}

int32_t ft_byte_buffer::prepend_buffer(const ft_byte_buffer &other) noexcept
{
    const ft_byte_buffer *first_buffer;
    const ft_byte_buffer *second_buffer;
    uintptr_t this_address;
    uintptr_t other_address;
    ft_size_t destination_size;
    ft_size_t source_size;
    int32_t lock_error;
    int32_t prepend_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::prepend_buffer");
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_byte_buffer::prepend_buffer",
            "called with uninitialised source object");
    if (&other == this)
    {
        lock_error = this->lock_internal();
        if (lock_error != FT_ERR_SUCCESS)
            return (ft_byte_buffer::set_error(lock_error));
        const ft_size_t original_size = this->_size;
        prepend_error = this->append_internal(this->_data, original_size);
        if (prepend_error == FT_ERR_SUCCESS)
            this->_read_position += original_size;
        this->unlock_internal();
        return (ft_byte_buffer::set_error(prepend_error));
    }
    this_address = reinterpret_cast<uintptr_t>(this);
    other_address = reinterpret_cast<uintptr_t>(&other);
    if (this_address < other_address)
    {
        first_buffer = this;
        second_buffer = &other;
    }
    else
    {
        first_buffer = &other;
        second_buffer = this;
    }
    lock_error = first_buffer->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    if (second_buffer != first_buffer)
    {
        lock_error = second_buffer->lock_internal();
        if (lock_error != FT_ERR_SUCCESS)
        {
            first_buffer->unlock_internal();
            return (ft_byte_buffer::set_error(lock_error));
        }
    }
    destination_size = this->_size;
    source_size = other._size;
    prepend_error = FT_ERR_SUCCESS;
    if (buffer_add_overflows(destination_size, source_size) == FT_TRUE)
        prepend_error = FT_ERR_OUT_OF_RANGE;
    else
    {
        prepend_error = this->reserve_internal(destination_size + source_size);
        if (prepend_error == FT_ERR_SUCCESS && source_size > 0)
        {
            if (other._data == ft_nullptr)
                prepend_error = FT_ERR_INVALID_STATE;
            else
            {
                if (destination_size > 0)
                {
                    ft_memmove(this->_data + source_size, this->_data,
                        destination_size);
                }
                ft_memcpy(this->_data, other._data, source_size);
                this->_size = destination_size + source_size;
                this->_read_position += source_size;
            }
        }
    }
    if (second_buffer != first_buffer)
        second_buffer->unlock_internal();
    first_buffer->unlock_internal();
    return (ft_byte_buffer::set_error(prepend_error));
}

int32_t ft_byte_buffer::read(void *data, ft_size_t length) noexcept
{
    int32_t lock_error;
    int32_t read_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::read");
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    read_error = this->read_internal(data, length);
    this->unlock_internal();
    return (ft_byte_buffer::set_error(read_error));
}

int32_t ft_byte_buffer::shrink_to_fit() noexcept
{
    uint8_t *new_data;
    int32_t lock_error;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::shrink_to_fit");
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    result = FT_ERR_SUCCESS;
    if (this->_capacity > this->_size)
    {
        if (this->_size == 0)
        {
            cma_free(this->_data);
            this->_data = ft_nullptr;
            this->_capacity = 0;
        }
        else
        {
            new_data = static_cast<uint8_t *>(cma_realloc(this->_data,
                this->_size));
            if (new_data == ft_nullptr)
                result = FT_ERR_NO_MEMORY;
            else
            {
                this->_data = new_data;
                this->_capacity = this->_size;
            }
        }
    }
    this->unlock_internal();
    return (ft_byte_buffer::set_error(result));
}

int32_t ft_byte_buffer::view(ft_size_t offset, ft_size_t length,
    const uint8_t **view_out) const noexcept
{
    int32_t lock_error;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::view");
    if (view_out == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    *view_out = ft_nullptr;
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    result = FT_ERR_SUCCESS;
    if (offset > this->_size)
        result = FT_ERR_OUT_OF_RANGE;
    else if (length > this->_size - offset)
        result = FT_ERR_OUT_OF_RANGE;
    else
        *view_out = this->_data + offset;
    this->unlock_internal();
    return (ft_byte_buffer::set_error(result));
}

int32_t ft_byte_buffer::append_u8(uint8_t value) noexcept
{
    return (this->append(&value, 1));
}

int32_t ft_byte_buffer::append_u16_be(uint16_t value) noexcept
{
    uint8_t bytes[2];

    bytes[0] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    bytes[1] = static_cast<uint8_t>(value & 0xFFU);
    return (this->append(bytes, 2));
}

int32_t ft_byte_buffer::append_u16_le(uint16_t value) noexcept
{
    uint8_t bytes[2];

    bytes[0] = static_cast<uint8_t>(value & 0xFFU);
    bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    return (this->append(bytes, 2));
}

int32_t ft_byte_buffer::append_u32_be(uint32_t value) noexcept
{
    uint8_t bytes[4];

    bytes[0] = static_cast<uint8_t>((value >> 24) & 0xFFU);
    bytes[1] = static_cast<uint8_t>((value >> 16) & 0xFFU);
    bytes[2] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    bytes[3] = static_cast<uint8_t>(value & 0xFFU);
    return (this->append(bytes, 4));
}

int32_t ft_byte_buffer::append_u32_le(uint32_t value) noexcept
{
    uint8_t bytes[4];

    bytes[0] = static_cast<uint8_t>(value & 0xFFU);
    bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
    bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
    return (this->append(bytes, 4));
}

int32_t ft_byte_buffer::append_u64_be(uint64_t value) noexcept
{
    uint8_t bytes[8];

    bytes[0] = static_cast<uint8_t>((value >> 56) & 0xFFU);
    bytes[1] = static_cast<uint8_t>((value >> 48) & 0xFFU);
    bytes[2] = static_cast<uint8_t>((value >> 40) & 0xFFU);
    bytes[3] = static_cast<uint8_t>((value >> 32) & 0xFFU);
    bytes[4] = static_cast<uint8_t>((value >> 24) & 0xFFU);
    bytes[5] = static_cast<uint8_t>((value >> 16) & 0xFFU);
    bytes[6] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    bytes[7] = static_cast<uint8_t>(value & 0xFFU);
    return (this->append(bytes, 8));
}

int32_t ft_byte_buffer::append_u64_le(uint64_t value) noexcept
{
    uint8_t bytes[8];

    bytes[0] = static_cast<uint8_t>(value & 0xFFU);
    bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    bytes[2] = static_cast<uint8_t>((value >> 16) & 0xFFU);
    bytes[3] = static_cast<uint8_t>((value >> 24) & 0xFFU);
    bytes[4] = static_cast<uint8_t>((value >> 32) & 0xFFU);
    bytes[5] = static_cast<uint8_t>((value >> 40) & 0xFFU);
    bytes[6] = static_cast<uint8_t>((value >> 48) & 0xFFU);
    bytes[7] = static_cast<uint8_t>((value >> 56) & 0xFFU);
    return (this->append(bytes, 8));
}

int32_t ft_byte_buffer::append_varuint64(uint64_t value) noexcept
{
    uint8_t bytes[10];
    ft_size_t encoded_size;
    int32_t error_code;

    error_code = buffer_encode_varuint64(value, bytes, 10, &encoded_size);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(error_code));
    return (this->append(bytes, encoded_size));
}

int32_t ft_byte_buffer::append_varint64(int64_t value) noexcept
{
    uint8_t bytes[10];
    ft_size_t encoded_size;
    int32_t error_code;

    error_code = buffer_encode_varint64(value, bytes, 10, &encoded_size);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(error_code));
    return (this->append(bytes, encoded_size));
}

int32_t ft_byte_buffer::read_u8(uint8_t *value_out) noexcept
{
    if (value_out == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    return (this->read(value_out, 1));
}

int32_t ft_byte_buffer::read_u16_be(uint16_t *value_out) noexcept
{
    uint8_t bytes[2];
    int32_t read_error;

    if (value_out == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    read_error = this->read(bytes, 2);
    if (read_error != FT_ERR_SUCCESS)
        return (read_error);
    *value_out = static_cast<uint16_t>(
        (static_cast<uint16_t>(bytes[0]) << 8)
        | static_cast<uint16_t>(bytes[1]));
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::read_u16_le(uint16_t *value_out) noexcept
{
    uint8_t bytes[2];
    int32_t read_error;

    if (value_out == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    read_error = this->read(bytes, 2);
    if (read_error != FT_ERR_SUCCESS)
        return (read_error);
    *value_out = static_cast<uint16_t>(
        static_cast<uint16_t>(bytes[0])
        | (static_cast<uint16_t>(bytes[1]) << 8));
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::read_u32_be(uint32_t *value_out) noexcept
{
    uint8_t bytes[4];
    int32_t read_error;

    if (value_out == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    read_error = this->read(bytes, 4);
    if (read_error != FT_ERR_SUCCESS)
        return (read_error);
    *value_out = (static_cast<uint32_t>(bytes[0]) << 24)
        | (static_cast<uint32_t>(bytes[1]) << 16)
        | (static_cast<uint32_t>(bytes[2]) << 8)
        | static_cast<uint32_t>(bytes[3]);
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::read_u32_le(uint32_t *value_out) noexcept
{
    uint8_t bytes[4];
    int32_t read_error;

    if (value_out == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    read_error = this->read(bytes, 4);
    if (read_error != FT_ERR_SUCCESS)
        return (read_error);
    *value_out = static_cast<uint32_t>(bytes[0])
        | (static_cast<uint32_t>(bytes[1]) << 8)
        | (static_cast<uint32_t>(bytes[2]) << 16)
        | (static_cast<uint32_t>(bytes[3]) << 24);
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::read_u64_be(uint64_t *value_out) noexcept
{
    uint8_t bytes[8];
    int32_t read_error;

    if (value_out == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    read_error = this->read(bytes, 8);
    if (read_error != FT_ERR_SUCCESS)
        return (read_error);
    *value_out = (static_cast<uint64_t>(bytes[0]) << 56)
        | (static_cast<uint64_t>(bytes[1]) << 48)
        | (static_cast<uint64_t>(bytes[2]) << 40)
        | (static_cast<uint64_t>(bytes[3]) << 32)
        | (static_cast<uint64_t>(bytes[4]) << 24)
        | (static_cast<uint64_t>(bytes[5]) << 16)
        | (static_cast<uint64_t>(bytes[6]) << 8)
        | static_cast<uint64_t>(bytes[7]);
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::read_u64_le(uint64_t *value_out) noexcept
{
    uint8_t bytes[8];
    int32_t read_error;

    if (value_out == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    read_error = this->read(bytes, 8);
    if (read_error != FT_ERR_SUCCESS)
        return (read_error);
    *value_out = static_cast<uint64_t>(bytes[0])
        | (static_cast<uint64_t>(bytes[1]) << 8)
        | (static_cast<uint64_t>(bytes[2]) << 16)
        | (static_cast<uint64_t>(bytes[3]) << 24)
        | (static_cast<uint64_t>(bytes[4]) << 32)
        | (static_cast<uint64_t>(bytes[5]) << 40)
        | (static_cast<uint64_t>(bytes[6]) << 48)
        | (static_cast<uint64_t>(bytes[7]) << 56);
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::read_varuint64(uint64_t *value_out) noexcept
{
    ft_size_t consumed_size;
    int32_t read_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::read_varuint64");
    if (value_out == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    read_error = this->lock_internal();
    if (read_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(read_error));
    if (this->_read_position > this->_size)
    {
        this->unlock_internal();
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_STATE));
    }
    read_error = buffer_decode_varuint64(this->_data + this->_read_position,
            this->_size - this->_read_position, value_out, &consumed_size);
    if (read_error == FT_ERR_SUCCESS)
        this->_read_position += consumed_size;
    this->unlock_internal();
    return (ft_byte_buffer::set_error(read_error));
}

int32_t ft_byte_buffer::read_varint64(int64_t *value_out) noexcept
{
    ft_size_t consumed_size;
    int32_t read_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::read_varint64");
    if (value_out == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    read_error = this->lock_internal();
    if (read_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(read_error));
    if (this->_read_position > this->_size)
    {
        this->unlock_internal();
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_STATE));
    }
    read_error = buffer_decode_varint64(this->_data + this->_read_position,
            this->_size - this->_read_position, value_out, &consumed_size);
    if (read_error == FT_ERR_SUCCESS)
        this->_read_position += consumed_size;
    this->unlock_internal();
    return (ft_byte_buffer::set_error(read_error));
}

ft_size_t ft_byte_buffer::find(const void *data, ft_size_t length) const noexcept
{
    ft_size_t index;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::find");
    if (length > 0 && data == ft_nullptr)
    {
        (void)ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT);
        return (static_cast<ft_size_t>(-1));
    }
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)ft_byte_buffer::set_error(lock_error);
        return (static_cast<ft_size_t>(-1));
    }
    if (this->_read_position > this->_size)
    {
        this->unlock_internal();
        (void)ft_byte_buffer::set_error(FT_ERR_INVALID_STATE);
        return (static_cast<ft_size_t>(-1));
    }
    if (length == 0)
    {
        index = this->_read_position;
        this->unlock_internal();
        (void)ft_byte_buffer::set_error(FT_ERR_SUCCESS);
        return (index);
    }
    const uint8_t *match = ft_nullptr;
    const ft_size_t remaining = this->_size - this->_read_position;
    if (length <= remaining)
        match = static_cast<const uint8_t *>(ft_memmem(
            this->_data + this->_read_position, remaining, data, length));
    if (match == ft_nullptr)
    {
        this->unlock_internal();
        (void)ft_byte_buffer::set_error(FT_ERR_NOT_FOUND);
        return (static_cast<ft_size_t>(-1));
    }
    index = static_cast<ft_size_t>(match - this->_data);
    this->unlock_internal();
    (void)ft_byte_buffer::set_error(FT_ERR_SUCCESS);
    return (index);
}

int32_t ft_byte_buffer::peek(ft_size_t offset, void *data, ft_size_t length) const noexcept
{
    int32_t lock_error;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::peek");
    if (data == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    result = FT_ERR_SUCCESS;
    if (this->_read_position > this->_size)
        result = FT_ERR_INVALID_STATE;
    else if (offset > this->_size - this->_read_position)
        result = FT_ERR_OUT_OF_RANGE;
    else if (length > this->_size - this->_read_position - offset)
        result = FT_ERR_OUT_OF_RANGE;
    else if (length > 0)
        ft_memcpy(data, this->_data + this->_read_position + offset, length);
    this->unlock_internal();
    return (ft_byte_buffer::set_error(result));
}

int32_t ft_byte_buffer::skip(ft_size_t length) noexcept
{
    int32_t lock_error;
    int32_t result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::skip");
    lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error));
    result = FT_ERR_SUCCESS;
    if (this->_read_position > this->_size)
        result = FT_ERR_INVALID_STATE;
    else if (length > this->_size - this->_read_position)
        result = FT_ERR_OUT_OF_RANGE;
    else
        this->_read_position += length;
    this->unlock_internal();
    return (ft_byte_buffer::set_error(result));
}

int32_t ft_byte_buffer::slice(ft_size_t offset, ft_size_t length,
    ft_byte_buffer &output) const noexcept
{
    int32_t lock_error_source;
    int32_t result;
    ft_byte_buffer temporary_buffer;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::slice");
    if (&output == this)
        return (ft_byte_buffer::set_error(FT_ERR_INVALID_ARGUMENT));
    lock_error_source = this->lock_internal();
    if (lock_error_source != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(lock_error_source));
    result = FT_ERR_SUCCESS;
    if (offset > this->_size)
        result = FT_ERR_OUT_OF_RANGE;
    else if (length > this->_size - offset)
        result = FT_ERR_OUT_OF_RANGE;
    else
    {
        result = temporary_buffer.initialize(length, FT_TRUE);
        if (result == FT_ERR_SUCCESS && length > 0)
            result = temporary_buffer.append(this->_data + offset, length);
    }
    this->unlock_internal();
    if (result == FT_ERR_SUCCESS)
        result = output.move(temporary_buffer);
    (void)temporary_buffer.destroy();
    return (ft_byte_buffer::set_error(result));
}

const uint8_t *ft_byte_buffer::data() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::data");
    ft_byte_buffer::set_error(FT_ERR_SUCCESS);
    return (this->_data);
}

ft_size_t ft_byte_buffer::size() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::size");
    ft_byte_buffer::set_error(FT_ERR_SUCCESS);
    return (this->_size);
}

ft_size_t ft_byte_buffer::capacity() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::capacity");
    ft_byte_buffer::set_error(FT_ERR_SUCCESS);
    return (this->_capacity);
}

ft_size_t ft_byte_buffer::read_position() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::read_position");
    ft_byte_buffer::set_error(FT_ERR_SUCCESS);
    return (this->_read_position);
}

ft_size_t ft_byte_buffer::remaining() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::remaining");
    ft_byte_buffer::set_error(FT_ERR_SUCCESS);
    if (this->_read_position > this->_size)
        return (0);
    return (this->_size - this->_read_position);
}

ft_bool ft_byte_buffer::is_fixed_capacity() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::is_fixed_capacity");
    ft_byte_buffer::set_error(FT_ERR_SUCCESS);
    return (this->_fixed_capacity);
}

ft_bool ft_byte_buffer::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t ft_byte_buffer::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialization_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_NO_MEMORY));
    initialization_error = mutex_pointer->initialize();
    if (initialization_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (ft_byte_buffer::set_error(initialization_error));
    }
    this->_mutex = mutex_pointer;
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

int32_t ft_byte_buffer::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    if (destroy_error != FT_ERR_SUCCESS)
        return (ft_byte_buffer::set_error(destroy_error));
    return (ft_byte_buffer::set_error(FT_ERR_SUCCESS));
}

ft_bool ft_byte_buffer::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_byte_buffer::is_thread_safe");
    ft_byte_buffer::set_error(FT_ERR_SUCCESS);
    if (this->_mutex != ft_nullptr)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t ft_byte_buffer::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_byte_buffer::get_error",
            "called on uninitialised object");
    return (ft_byte_buffer::_last_error);
}

const char *ft_byte_buffer::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "ft_byte_buffer::get_error_str",
            "called on uninitialised object");
    return (ft_strerror(ft_byte_buffer::_last_error));
}
