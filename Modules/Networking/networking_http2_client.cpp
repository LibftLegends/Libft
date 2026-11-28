#include "http2_client.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "openssl_support.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "../Template/vector.hpp"
#if NETWORKING_HAS_OPENSSL
#include <openssl/ssl.h>
#include <cstdlib>

thread_local int32_t http2_frame::_last_error = FT_ERR_SUCCESS;

static int32_t http2_append_uint16(ft_string &target, uint16_t value)
{
    const char bytes[] = {
        static_cast<char>(value >> 8),
        static_cast<char>(value & 0xFF),
    };
    return (target.append(bytes, sizeof(bytes)));
}

http2_header_field::http2_header_field() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _name(), _value(), _mutex(ft_nullptr)
{
    return ;
}

http2_header_field::~http2_header_field() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t http2_header_field::move(http2_header_field &other) noexcept
{
    return (this->initialize(static_cast<http2_header_field &&>(other)));
}

int32_t http2_header_field::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "http2_header_field::initialize", "initialize called on initialised instance");
    int32_t name_error = this->_name.initialize();
    if (name_error != FT_ERR_SUCCESS)
        return (name_error);
    int32_t value_error = this->_value.initialize();
    if (value_error != FT_ERR_SUCCESS)
    {
        this->_name.destroy();
        return (value_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t http2_header_field::initialize(const http2_header_field &other) noexcept
{
    ft_bool lock_acquired;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "http2_header_field::initialize(const http2_header_field &)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (this->initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    lock_acquired = FT_FALSE;
    if (other.lock(&lock_acquired) != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INVALID_OPERATION);
    }
    this->_name = other._name;
    this->_value = other._value;
    const_cast<http2_header_field &>(other).unlock(lock_acquired);
    if (this->_name.get_error() != FT_ERR_SUCCESS
        || this->_value.get_error() != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INVALID_OPERATION);
    }
    return (FT_ERR_SUCCESS);
}

int32_t http2_header_field::initialize(http2_header_field &&other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "http2_header_field::initialize(http2_header_field &&)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    ft_bool lock_acquired = FT_FALSE;
    int32_t move_error = other.lock(&lock_acquired);
    if (move_error != FT_ERR_SUCCESS)
        return (move_error);
    move_error = this->_name.move(other._name);
    if (move_error == FT_ERR_SUCCESS)
    {
        move_error = this->_value.move(other._value);
        if (move_error != FT_ERR_SUCCESS)
            (void)other._name.move(this->_name);
    }
    other.unlock(lock_acquired);
    if (move_error != FT_ERR_SUCCESS)
    {
        (void)this->_name.destroy();
        (void)this->_value.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (move_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t http2_header_field::destroy() noexcept
{
    int32_t first_error;
    int32_t disable_error;
    int32_t name_error;
    int32_t value_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    name_error = this->_name.destroy();
    if (first_error == FT_ERR_SUCCESS && name_error != FT_ERR_SUCCESS)
        first_error = name_error;
    value_error = this->_value.destroy();
    if (first_error == FT_ERR_SUCCESS && value_error != FT_ERR_SUCCESS)
        first_error = value_error;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

int32_t http2_header_field::enable_thread_safety() noexcept
{
    void *memory_pointer;
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_header_field::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    memory_pointer = std::malloc(sizeof(pt_recursive_mutex));
    if (memory_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_pointer = new(memory_pointer) pt_recursive_mutex();
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        mutex_pointer->~pt_recursive_mutex();
        std::free(memory_pointer);
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t http2_header_field::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_header_field::disable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        destroy_error = this->_mutex->destroy();
        this->_mutex->~pt_recursive_mutex();
        std::free(this->_mutex);
        this->_mutex = ft_nullptr;
        return (destroy_error);
    }
    return (FT_ERR_SUCCESS);
}

ft_bool http2_header_field::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t http2_header_field::lock(ft_bool *lock_acquired) const noexcept
{
    http2_header_field *mutable_field;
    ft_bool has_mutex;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    mutable_field = const_cast<http2_header_field *>(this);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_header_field::lock");
    has_mutex = (mutable_field->_mutex != ft_nullptr);
    if (pt_recursive_mutex_lock_if_not_null(mutable_field->_mutex) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    if (lock_acquired != ft_nullptr && has_mutex)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void http2_header_field::unlock(ft_bool lock_acquired) const noexcept
{
    http2_header_field *mutable_field;

    if (lock_acquired == FT_FALSE)
        return ;
    mutable_field = const_cast<http2_header_field *>(this);
    (void)pt_recursive_mutex_unlock_if_not_null(mutable_field->_mutex);
    return ;
}

ft_bool http2_header_field::set_name(const ft_string &name_value) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_name = name_value;
    this->unlock(lock_acquired);
    return (this->_name.get_error() == FT_ERR_SUCCESS);
}

ft_bool http2_header_field::set_name_from_cstr(const char *name_value) noexcept
{
    ft_string temporary_name;

    if (name_value == ft_nullptr)
        return (FT_FALSE);
    if (temporary_name.initialize() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    temporary_name = name_value;
    if (temporary_name.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (this->set_name(temporary_name));
}

ft_bool http2_header_field::set_name_from_buffer(const char *buffer, ft_size_t length) noexcept
{
    ft_bool lock_acquired;

    if (buffer == ft_nullptr && length > 0)
        return (FT_FALSE);
    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_name.assign(buffer, length);
    this->unlock(lock_acquired);
    return (this->_name.get_error() == FT_ERR_SUCCESS);
}

ft_bool http2_header_field::set_value(const ft_string &value_value) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_value = value_value;
    this->unlock(lock_acquired);
    return (this->_value.get_error() == FT_ERR_SUCCESS);
}

ft_bool http2_header_field::set_value_from_cstr(const char *value_value) noexcept
{
    ft_string temporary_value;

    if (value_value == ft_nullptr)
        return (FT_FALSE);
    if (temporary_value.initialize() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    temporary_value = value_value;
    if (temporary_value.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (this->set_value(temporary_value));
}

ft_bool http2_header_field::set_value_from_buffer(const char *buffer, ft_size_t length) noexcept
{
    ft_bool lock_acquired;

    if (buffer == ft_nullptr && length > 0)
        return (FT_FALSE);
    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_value.assign(buffer, length);
    this->unlock(lock_acquired);
    return (this->_value.get_error() == FT_ERR_SUCCESS);
}

ft_bool http2_header_field::assign(const ft_string &name_value, const ft_string &value_value) noexcept
{
    ft_bool lock_acquired;
    ft_bool success_state;

    success_state = FT_TRUE;
    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_name = name_value;
    if (this->_name.get_error() != FT_ERR_SUCCESS)
        success_state = FT_FALSE;
    if (success_state)
    {
        this->_value = value_value;
        if (this->_value.get_error() != FT_ERR_SUCCESS)
            success_state = FT_FALSE;
    }
    this->unlock(lock_acquired);
    return (success_state);
}

ft_bool http2_header_field::assign_from_cstr(const char *name_value, const char *value_value) noexcept
{
    ft_string name_string;
    ft_string value_string;
    int32_t initialize_error;

    if (name_value == ft_nullptr || value_value == ft_nullptr)
        return (FT_FALSE);
    initialize_error = name_string.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (FT_FALSE);
    initialize_error = value_string.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (FT_FALSE);
    name_string = name_value;
    if (name_string.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    value_string = value_value;
    if (value_string.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    return (this->assign(name_string, value_string));
}

ft_bool http2_header_field::assign_from_buffers(const char *name_buffer, ft_size_t name_length,
    const char *value_buffer, ft_size_t value_length) noexcept
{
    ft_bool lock_acquired;
    ft_bool success_state;

    if ((name_buffer == ft_nullptr && name_length > 0) || (value_buffer == ft_nullptr && value_length > 0))
        return (FT_FALSE);
    success_state = FT_TRUE;
    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_name.assign(name_buffer, name_length);
    if (this->_name.get_error() != FT_ERR_SUCCESS)
        success_state = FT_FALSE;
    if (success_state)
    {
        this->_value.assign(value_buffer, value_length);
        if (this->_value.get_error() != FT_ERR_SUCCESS)
            success_state = FT_FALSE;
    }
    this->unlock(lock_acquired);
    return (success_state);
}

ft_bool http2_header_field::copy_name(ft_string &out_name) const noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    out_name = this->_name;
    this->unlock(lock_acquired);
    return (out_name.get_error() == FT_ERR_SUCCESS);
}

ft_bool http2_header_field::copy_value(ft_string &out_value) const noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    out_value = this->_value;
    this->unlock(lock_acquired);
    return (out_value.get_error() == FT_ERR_SUCCESS);
}

void http2_header_field::clear() noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return ;
    this->_name.clear();
    this->_value.clear();
    this->unlock(lock_acquired);
    return ;
}

http2_frame::http2_frame() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _type(0), _flags(0),
      _stream_identifier(0), _payload(), _mutex(ft_nullptr)
{
    return ;
}

http2_frame::~http2_frame() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t http2_frame::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "http2_frame::initialize", "initialize called on initialised instance");
    this->_type = 0;
    this->_flags = 0;
    this->_stream_identifier = 0;
    int32_t payload_error = this->_payload.initialize();
    if (payload_error != FT_ERR_SUCCESS)
    {
        this->set_error(payload_error);
        return (payload_error);
    }
    this->_mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t http2_frame::destroy() noexcept
{
    int32_t disable_error;
    int32_t payload_error;
    int32_t first_error;
    int32_t previous_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    previous_error = http2_frame::_last_error;
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    payload_error = this->_payload.destroy();
    if (first_error == FT_ERR_SUCCESS && payload_error != FT_ERR_SUCCESS)
        first_error = payload_error;
    this->_type = 0;
    this->_flags = 0;
    this->_stream_identifier = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (first_error != FT_ERR_SUCCESS)
        return (this->set_error(first_error));
    this->set_error(previous_error);
    return (FT_ERR_SUCCESS);
}

int32_t http2_frame::move(http2_frame &other) noexcept
{
    return (this->initialize(static_cast<http2_frame &&>(other)));
}

int32_t http2_frame::initialize(const http2_frame &other) noexcept
{
    ft_bool lock_acquired;
    int32_t destroy_error;
    int32_t initialize_error;
    int32_t source_error;
    int32_t lock_error;
    int32_t enable_error;
    int32_t payload_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "http2_frame::initialize(const http2_frame &)",
            "source is uninitialised");
    source_error = http2_frame::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (this->set_error(destroy_error));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_type = 0;
        this->_flags = 0;
        this->_stream_identifier = 0;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(initialize_error));
    }
    lock_acquired = FT_FALSE;
    lock_error = other.lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(lock_error));
    }
    this->_type = other._type;
    this->_flags = other._flags;
    this->_stream_identifier = other._stream_identifier;
    this->_payload = other._payload;
    const_cast<http2_frame &>(other).unlock(lock_acquired);
    payload_error = this->_payload.get_error();
    if (payload_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(payload_error));
    }
    if (other.is_thread_safe() == FT_TRUE)
    {
        enable_error = this->enable_thread_safety();
        if (enable_error != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(enable_error));
        }
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t http2_frame::initialize(http2_frame &&other) noexcept
{
    int32_t initialize_error;
    int32_t destroy_error;
    int32_t source_error;

    initialize_error = this->initialize(other);
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    source_error = http2_frame::_last_error;
    destroy_error = other.destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (this->set_error(destroy_error));
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t http2_frame::enable_thread_safety() noexcept
{
    void *memory_pointer;
    pt_recursive_mutex *mutex_pointer;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_frame::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    memory_pointer = std::malloc(sizeof(pt_recursive_mutex));
    if (memory_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_pointer = new(memory_pointer) pt_recursive_mutex();
    if (mutex_pointer->initialize() != FT_ERR_SUCCESS)
    {
        (void)mutex_pointer->destroy();
        mutex_pointer->~pt_recursive_mutex();
        std::free(memory_pointer);
        return (FT_ERR_INVALID_OPERATION);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t http2_frame::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_frame::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    this->_mutex->~pt_recursive_mutex();
    std::free(this->_mutex);
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool http2_frame::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t http2_frame::lock(ft_bool *lock_acquired) const noexcept
{
    http2_frame *mutable_frame;
    ft_bool has_mutex;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    mutable_frame = const_cast<http2_frame *>(this);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_frame::lock");
    has_mutex = (mutable_frame->_mutex != ft_nullptr);
    if (pt_recursive_mutex_lock_if_not_null(mutable_frame->_mutex) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    if (lock_acquired != ft_nullptr && has_mutex)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void http2_frame::unlock(ft_bool lock_acquired) const noexcept
{
    http2_frame *mutable_frame;

    if (lock_acquired == FT_FALSE)
        return ;
    mutable_frame = const_cast<http2_frame *>(this);
    (void)pt_recursive_mutex_unlock_if_not_null(mutable_frame->_mutex);
    return ;
}

ft_bool http2_frame::set_type(uint8_t type_value) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_type = type_value;
    this->unlock(lock_acquired);
    return (FT_TRUE);
}

ft_bool http2_frame::get_type(uint8_t &out_type) const noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    out_type = this->_type;
    this->unlock(lock_acquired);
    return (FT_TRUE);
}

ft_bool http2_frame::set_flags(uint8_t flags_value) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_flags = flags_value;
    this->unlock(lock_acquired);
    return (FT_TRUE);
}

ft_bool http2_frame::get_flags(uint8_t &out_flags) const noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    out_flags = this->_flags;
    this->unlock(lock_acquired);
    return (FT_TRUE);
}

ft_bool http2_frame::set_stream_identifier(uint32_t stream_identifier_value) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_stream_identifier = stream_identifier_value;
    this->unlock(lock_acquired);
    return (FT_TRUE);
}

ft_bool http2_frame::get_stream_identifier(uint32_t &out_stream_identifier) const noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    out_stream_identifier = this->_stream_identifier;
    this->unlock(lock_acquired);
    return (FT_TRUE);
}

ft_bool http2_frame::set_payload(const ft_string &payload_value) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_payload = payload_value;
    this->unlock(lock_acquired);
    return (this->_payload.get_error() == FT_ERR_SUCCESS);
}

ft_bool http2_frame::set_payload_from_buffer(const char *buffer, ft_size_t length) noexcept
{
    ft_bool lock_acquired;

    if (buffer == ft_nullptr && length > 0)
        return (FT_FALSE);
    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    this->_payload.assign(buffer, length);
    this->unlock(lock_acquired);
    return (this->_payload.get_error() == FT_ERR_SUCCESS);
}

ft_bool http2_frame::copy_payload(ft_string &out_payload) const noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    out_payload = this->_payload;
    this->unlock(lock_acquired);
    return (out_payload.get_error() == FT_ERR_SUCCESS);
}

int32_t http2_frame::set_error(int32_t error_code) const noexcept
{
    http2_frame::_last_error = error_code;
    return (error_code);
}

int32_t http2_frame::get_error(void) const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "http2_frame::get_error");
    return (http2_frame::_last_error);
}

const char *http2_frame::get_error_str(void) const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "http2_frame::get_error_str");
    return (ft_strerror(http2_frame::_last_error));
}

void http2_frame::clear_payload() noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != 0)
        return ;
    this->_payload.clear();
    this->unlock(lock_acquired);
    return ;
}

http2_stream_state::http2_stream_state() noexcept
    : buffer(), dependency_identifier(0), weight(16), exclusive_dependency(FT_FALSE),
      remote_window(65535), local_window(65535)
{
    if (this->buffer.initialize() != FT_ERR_SUCCESS)
        (void)this->buffer.destroy();
    return ;
}

http2_stream_state::~http2_stream_state() noexcept
{
    (void)this->buffer.destroy();
    return ;
}

http2_stream_manager::http2_stream_manager() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _streams(),
      _initial_remote_window(65535),
      _initial_local_window(65535), _connection_remote_window(65535),
      _connection_local_window(65535), _mutex(ft_nullptr)
{
    return ;
}

thread_local int32_t http2_stream_manager::_last_error = FT_ERR_SUCCESS;

http2_stream_manager::~http2_stream_manager() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t http2_stream_manager::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "http2_stream_manager::initialize", "initialize called on initialised instance");
    int32_t streams_error = this->_streams.initialize();
    if (streams_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
        return (this->set_error(streams_error));
    }
    this->_initial_remote_window = 65535;
    this->_initial_local_window = 65535;
    this->_connection_remote_window = 65535;
    this->_connection_local_window = 65535;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t http2_stream_manager::move(http2_stream_manager &other) noexcept
{
    return (this->initialize(static_cast<http2_stream_manager &&>(other)));
}

int32_t http2_stream_manager::initialize(const http2_stream_manager &other) noexcept
{
    ft_bool lock_acquired;
    int32_t destroy_error;
    int32_t initialize_error;
    int32_t source_error;
    int32_t lock_error;
    int32_t streams_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "http2_stream_manager::initialize(const http2_stream_manager &)",
            "source is uninitialised");
    source_error = http2_stream_manager::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (this->set_error(destroy_error));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initial_remote_window = 65535;
        this->_initial_local_window = 65535;
        this->_connection_remote_window = 65535;
        this->_connection_local_window = 65535;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(initialize_error));
    }
    lock_acquired = FT_FALSE;
    lock_error = other.lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(lock_error));
    }
    this->_initial_remote_window = other._initial_remote_window;
    this->_initial_local_window = other._initial_local_window;
    this->_connection_remote_window = other._connection_remote_window;
    this->_connection_local_window = other._connection_local_window;
    streams_error = this->_streams.copy_from(other._streams);
    other.unlock(lock_acquired);
    if (streams_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(streams_error));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t http2_stream_manager::initialize(http2_stream_manager &&other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->initialize(static_cast<const http2_stream_manager &>(other)));
    const int32_t source_error = http2_stream_manager::_last_error;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = other.lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    const int32_t streams_error = this->_streams.move_from(other._streams);
    if (streams_error == FT_ERR_SUCCESS)
    {
        this->_initial_remote_window = other._initial_remote_window;
        this->_initial_local_window = other._initial_local_window;
        this->_connection_remote_window = other._connection_remote_window;
        this->_connection_local_window = other._connection_local_window;
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    }
    other.unlock(lock_acquired);
    if (streams_error != FT_ERR_SUCCESS)
    {
        (void)this->_streams.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(streams_error));
    }
    const int32_t source_destroy_error = other.destroy();
    if (source_destroy_error != FT_ERR_SUCCESS)
        return (this->set_error(source_destroy_error));
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t http2_stream_manager::destroy() noexcept
{
    int32_t first_error;
    int32_t disable_error;
    int32_t stream_error;
    int32_t previous_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    previous_error = http2_stream_manager::_last_error;
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        first_error = disable_error;
    stream_error = this->_streams.destroy();
    if (first_error == FT_ERR_SUCCESS && stream_error != FT_ERR_SUCCESS)
        first_error = stream_error;
    this->_initial_remote_window = 65535;
    this->_initial_local_window = 65535;
    this->_connection_remote_window = 65535;
    this->_connection_local_window = 65535;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (first_error != FT_ERR_SUCCESS)
        return (this->set_error(first_error));
    this->set_error(previous_error);
    return (FT_ERR_SUCCESS);
}

int32_t http2_stream_manager::enable_thread_safety() noexcept
{
    void *memory_pointer;
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_stream_manager::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        return (this->set_error(FT_ERR_SUCCESS));
    }
    memory_pointer = std::malloc(sizeof(pt_recursive_mutex));
    if (!memory_pointer)
    {
        return (this->set_error(FT_ERR_NO_MEMORY));
    }
    mutex_pointer = new(memory_pointer) pt_recursive_mutex();
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        mutex_pointer->~pt_recursive_mutex();
        std::free(memory_pointer);
        return (this->set_error(mutex_error));
    }
    this->_mutex = mutex_pointer;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t http2_stream_manager::lock(ft_bool *lock_acquired) const noexcept
{
    http2_stream_manager *mutable_manager;
    ft_bool has_mutex;
    int32_t lock_error;

    if (lock_acquired)
        *lock_acquired = FT_FALSE;
    mutable_manager = const_cast<http2_stream_manager *>(this);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_stream_manager::lock");
    has_mutex = (this->_mutex != ft_nullptr);
    lock_error = pt_recursive_mutex_lock_if_not_null(mutable_manager->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired && has_mutex)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void http2_stream_manager::unlock(ft_bool lock_acquired) const noexcept
{
    http2_stream_manager *mutable_manager;

    if (!lock_acquired)
        return ;
    mutable_manager = const_cast<http2_stream_manager *>(this);
    (void)pt_recursive_mutex_unlock_if_not_null(mutable_manager->_mutex);
    return ;
}

int32_t http2_stream_manager::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_stream_manager::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    destroy_error = this->_mutex->destroy();
    this->_mutex->~pt_recursive_mutex();
    std::free(this->_mutex);
    this->_mutex = ft_nullptr;
    return (this->set_error(destroy_error));
}

ft_bool http2_stream_manager::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t http2_stream_manager::set_error(int32_t error_code) const noexcept
{
    http2_stream_manager::_last_error = error_code;
    return (error_code);
}

int32_t http2_stream_manager::get_error(void) const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "http2_stream_manager::get_error");
    return (http2_stream_manager::_last_error);
}

const char *http2_stream_manager::get_error_str(void) const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "http2_stream_manager::get_error_str");
    return (ft_strerror(http2_stream_manager::_last_error));
}

ft_bool http2_stream_manager::reserve_remote_connection_window(uint32_t length) noexcept
{
    if (this->_connection_remote_window < length)
    {
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

ft_bool http2_stream_manager::record_connection_send(uint32_t length) noexcept
{
    if (this->_connection_remote_window < length)
    {
        return (FT_FALSE);
    }
    this->_connection_remote_window -= length;
    return (FT_TRUE);
}

ft_bool http2_stream_manager::open_stream(uint32_t stream_identifier) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (FT_FALSE);
    }
    int32_t error_code = FT_ERR_ALREADY_EXISTS;
    if (this->_streams.find(stream_identifier) == this->_streams.end())
    {
        http2_stream_state state;
        state.remote_window = this->_initial_remote_window;
        state.local_window = this->_initial_local_window;
        this->_streams.insert(stream_identifier, state);
        error_code = this->_streams.get_error();
    }
    this->unlock(lock_acquired);
    return (this->set_error(error_code) == FT_ERR_SUCCESS);
}

ft_bool http2_stream_manager::append_data(uint32_t stream_identifier, const char *data,
    ft_size_t length) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (FT_FALSE);
    }
    int32_t error_code = FT_ERR_SUCCESS;
    if (!data && length > 0)
        error_code = FT_ERR_INVALID_ARGUMENT;
    else
    {
        auto *stream_entry = this->_streams.find(stream_identifier);
        if (stream_entry == this->_streams.end())
            error_code = FT_ERR_NOT_FOUND;
        else if (length > UINT32_MAX)
            error_code = FT_ERR_OUT_OF_RANGE;
        else if (length > stream_entry->value.local_window
            || length > this->_connection_local_window)
            error_code = FT_ERR_INVALID_OPERATION;
        else if (length > 0)
        {
            error_code = stream_entry->value.buffer.append(data, length);
            if (error_code == FT_ERR_SUCCESS)
            {
                stream_entry->value.local_window -= static_cast<uint32_t>(length);
                this->_connection_local_window -= static_cast<uint32_t>(length);
            }
        }
    }
    this->unlock(lock_acquired);
    return (this->set_error(error_code) == FT_ERR_SUCCESS);
}

ft_bool http2_stream_manager::close_stream(uint32_t stream_identifier) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (FT_FALSE);
    }
    ft_bool removed = FT_FALSE;
    int32_t error_code = this->_streams.erase_if(
        [stream_identifier, &removed](const auto &entry, ft_size_t) noexcept -> ft_bool
        {
            if (entry.key != stream_identifier)
                return (FT_FALSE);
            removed = FT_TRUE;
            return (FT_TRUE);
        },
        [](auto &destination, auto &source) noexcept
        {
            destination.key = source.key;
            http2_stream_state &destination_state = destination.value;
            http2_stream_state &source_state = source.value;
            (void)destination_state.buffer.move(source_state.buffer);
            destination_state.dependency_identifier = source_state.dependency_identifier;
            destination_state.weight = source_state.weight;
            destination_state.exclusive_dependency = source_state.exclusive_dependency;
            destination_state.remote_window = source_state.remote_window;
            destination_state.local_window = source_state.local_window;
            return ;
        });
    if (error_code == FT_ERR_SUCCESS && !removed)
        error_code = FT_ERR_NOT_FOUND;
    this->unlock(lock_acquired);
    return (this->set_error(error_code) == FT_ERR_SUCCESS);
}

ft_bool http2_stream_manager::get_stream_buffer(uint32_t stream_identifier,
    ft_string &out_buffer) const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (FT_FALSE);
    }
    const auto *stream_entry = this->_streams.find(stream_identifier);
    int32_t error_code = FT_ERR_SUCCESS;
    if (stream_entry == this->_streams.end())
        error_code = FT_ERR_NOT_FOUND;
    else
    {
        const ft_string &source = stream_entry->value.buffer;
        if (!out_buffer.is_initialised())
            error_code = out_buffer.initialize();
        if (error_code == FT_ERR_SUCCESS && &out_buffer != &source)
            error_code = out_buffer.assign(source.c_str(), source.size());
    }
    this->unlock(lock_acquired);
    return (this->set_error(error_code) == FT_ERR_SUCCESS);
}

ft_bool http2_stream_manager::update_priority(uint32_t stream_identifier,
    uint32_t dependency_identifier, uint8_t weight, ft_bool exclusive) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    ft_bool success = FT_FALSE;
    if (stream_identifier != dependency_identifier && weight != 0)
    {
        auto *stream_entry = this->_streams.find(stream_identifier);
        if (stream_entry != this->_streams.end())
        {
            success = FT_TRUE;
            if (exclusive)
            {
                success = this->_streams.for_each(
                    [stream_identifier, dependency_identifier](uint32_t key,
                        http2_stream_state &state) noexcept
                    {
                        if (key != stream_identifier
                            && state.dependency_identifier == dependency_identifier)
                            state.dependency_identifier = stream_identifier;
                        return ;
                    }) == FT_ERR_SUCCESS;
            }
            if (success)
            {
                stream_entry->value.dependency_identifier = dependency_identifier;
                stream_entry->value.weight = weight;
                stream_entry->value.exclusive_dependency = exclusive;
            }
        }
    }
    this->unlock(lock_acquired);
    return (success);
}

ft_bool http2_stream_manager::get_priority(uint32_t stream_identifier,
    uint32_t &dependency_identifier, uint8_t &weight, ft_bool &exclusive) const noexcept
{
    const Pair<uint32_t, http2_stream_state> *stream_entry;
    ft_bool lock_acquired;
    ft_bool success_state;

    lock_acquired = FT_FALSE;
    success_state = FT_TRUE;
    if (this->lock(&lock_acquired) != 0)
        return (FT_FALSE);
    stream_entry = this->_streams.find(stream_identifier);
    if (stream_entry == this->_streams.end())
    {
        success_state = FT_FALSE;
    }
    if (success_state)
    {
        dependency_identifier = stream_entry->value.dependency_identifier;
        weight = stream_entry->value.weight;
        exclusive = stream_entry->value.exclusive_dependency;
    }
    this->unlock(lock_acquired);
    return (success_state);
}

ft_bool http2_stream_manager::update_initial_window(uint32_t new_window,
    uint32_t &initial_window, uint32_t http2_stream_state::*stream_window) noexcept
{
    constexpr uint32_t MAX_WINDOW = 0x7FFFFFFFU;
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (FT_FALSE);
    }
    int32_t error_code = FT_ERR_SUCCESS;
    if (new_window > MAX_WINDOW)
        error_code = FT_ERR_OUT_OF_RANGE;
    else if (new_window != initial_window)
    {
        const int64_t delta = static_cast<int64_t>(new_window) - initial_window;
        error_code = this->_streams.for_each(
            [delta, stream_window](uint32_t, http2_stream_state &state) noexcept
            {
                uint32_t &window = state.*stream_window;
                const int64_t updated = static_cast<int64_t>(window) + delta;
                if (updated > MAX_WINDOW)
                    window = MAX_WINDOW;
                else if (updated < 0)
                    window = 0;
                else
                    window = static_cast<uint32_t>(updated);
                return ;
            });
        if (error_code == FT_ERR_SUCCESS)
            initial_window = new_window;
    }
    this->unlock(lock_acquired);
    return (this->set_error(error_code) == FT_ERR_SUCCESS);
}

ft_bool http2_stream_manager::update_remote_initial_window(uint32_t new_window) noexcept
{
    return (this->update_initial_window(new_window, this->_initial_remote_window,
        &http2_stream_state::remote_window));
}

ft_bool http2_stream_manager::update_local_initial_window(uint32_t new_window) noexcept
{
    return (this->update_initial_window(new_window, this->_initial_local_window,
        &http2_stream_state::local_window));
}

uint32_t http2_stream_manager::get_local_window(uint32_t stream_identifier) const noexcept
{
    const Pair<uint32_t, http2_stream_state> *stream_entry;
    ft_bool lock_acquired;
    ft_bool success_state;
    uint32_t window_value;

    lock_acquired = FT_FALSE;
    success_state = FT_TRUE;
    window_value = 0;
    if (this->lock(&lock_acquired) != 0)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (0);
    }
    stream_entry = this->_streams.find(stream_identifier);
    if (stream_entry == this->_streams.end())
    {
        this->unlock(lock_acquired);
        this->set_error(FT_ERR_NOT_FOUND);
        return (0);
    }
    if (success_state)
    {
        window_value = stream_entry->value.local_window;
    }
    this->unlock(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (window_value);
}

uint32_t http2_stream_manager::get_remote_window(uint32_t stream_identifier) const noexcept
{
    const Pair<uint32_t, http2_stream_state> *stream_entry;
    ft_bool lock_acquired;
    ft_bool success_state;
    uint32_t window_value;

    lock_acquired = FT_FALSE;
    success_state = FT_TRUE;
    window_value = 0;
    if (this->lock(&lock_acquired) != 0)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (0);
    }
    stream_entry = this->_streams.find(stream_identifier);
    if (stream_entry == this->_streams.end())
    {
        this->unlock(lock_acquired);
        this->set_error(FT_ERR_NOT_FOUND);
        return (0);
    }
    if (success_state)
    {
        window_value = stream_entry->value.remote_window;
    }
    this->unlock(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (window_value);
}

ft_bool http2_stream_manager::increase_local_window(uint32_t stream_identifier,
    uint32_t increment) noexcept
{
    Pair<uint32_t, http2_stream_state> *stream_entry;
    uint64_t updated_window;
    ft_bool lock_acquired;
    ft_bool success_state;

    lock_acquired = FT_FALSE;
    success_state = FT_TRUE;
    stream_entry = ft_nullptr;
    if (this->lock(&lock_acquired) != 0)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (FT_FALSE);
    }
    if (increment > 0x7FFFFFFFu)
    {
        this->unlock(lock_acquired);
        this->set_error(FT_ERR_OUT_OF_RANGE);
        return (FT_FALSE);
    }
    if (success_state)
    {
        stream_entry = this->_streams.find(stream_identifier);
        if (stream_entry == this->_streams.end())
        {
            this->unlock(lock_acquired);
            this->set_error(FT_ERR_NOT_FOUND);
            return (FT_FALSE);
        }
    }
    if (success_state)
    {
        updated_window = static_cast<uint64_t>(stream_entry->value.local_window)
            + increment;
        if (updated_window > 0x7FFFFFFFul)
        {
            this->unlock(lock_acquired);
            this->set_error(FT_ERR_OUT_OF_RANGE);
            return (FT_FALSE);
        }
        else
        {
            stream_entry->value.local_window = static_cast<uint32_t>(updated_window);
        }
    }
    this->unlock(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (success_state);
}

ft_bool http2_stream_manager::increase_remote_window(uint32_t stream_identifier,
    uint32_t increment) noexcept
{
    Pair<uint32_t, http2_stream_state> *stream_entry;
    uint64_t updated_window;
    ft_bool lock_acquired;
    ft_bool success_state;

    lock_acquired = FT_FALSE;
    success_state = FT_TRUE;
    stream_entry = ft_nullptr;
    if (this->lock(&lock_acquired) != 0)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (FT_FALSE);
    }
    if (increment > 0x7FFFFFFFu)
    {
        this->unlock(lock_acquired);
        this->set_error(FT_ERR_OUT_OF_RANGE);
        return (FT_FALSE);
    }
    if (success_state)
    {
        stream_entry = this->_streams.find(stream_identifier);
        if (stream_entry == this->_streams.end())
        {
            this->unlock(lock_acquired);
            this->set_error(FT_ERR_NOT_FOUND);
            return (FT_FALSE);
        }
    }
    if (success_state)
    {
        updated_window = static_cast<uint64_t>(stream_entry->value.remote_window)
            + increment;
        if (updated_window > 0x7FFFFFFFul)
        {
            this->unlock(lock_acquired);
            this->set_error(FT_ERR_OUT_OF_RANGE);
            return (FT_FALSE);
        }
        else
        {
            stream_entry->value.remote_window = static_cast<uint32_t>(updated_window);
        }
    }
    this->unlock(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (success_state);
}

ft_bool http2_stream_manager::reserve_send_window(uint32_t stream_identifier,
    uint32_t length) noexcept
{
    Pair<uint32_t, http2_stream_state> *stream_entry;
    ft_bool lock_acquired;
    ft_bool success_state;

    lock_acquired = FT_FALSE;
    success_state = FT_TRUE;
    if (this->lock(&lock_acquired) != 0)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (FT_FALSE);
    }
    stream_entry = this->_streams.find(stream_identifier);
    if (stream_entry == this->_streams.end())
    {
        this->unlock(lock_acquired);
        this->set_error(FT_ERR_NOT_FOUND);
        return (FT_FALSE);
    }
    if (success_state)
    {
        if (!this->reserve_remote_connection_window(length))
        {
            this->unlock(lock_acquired);
            this->set_error(FT_ERR_INVALID_OPERATION);
            return (FT_FALSE);
        }
    }
    if (success_state && stream_entry->value.remote_window < length)
    {
        this->unlock(lock_acquired);
        this->set_error(FT_ERR_OUT_OF_RANGE);
        return (FT_FALSE);
    }
    if (success_state)
    {
        stream_entry->value.remote_window -= length;
        if (!this->record_connection_send(length))
        {
            this->unlock(lock_acquired);
            this->set_error(FT_ERR_INVALID_OPERATION);
            return (FT_FALSE);
        }
    }
    this->unlock(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (success_state);
}

ft_bool http2_stream_manager::update_connection_local_window(uint32_t increment) noexcept
{
    uint64_t updated_window;
    ft_bool lock_acquired;
    ft_bool success_state;

    lock_acquired = FT_FALSE;
    success_state = FT_TRUE;
    if (this->lock(&lock_acquired) != 0)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (FT_FALSE);
    }
    if (increment > 0x7FFFFFFFu)
    {
        this->unlock(lock_acquired);
        this->set_error(FT_ERR_OUT_OF_RANGE);
        return (FT_FALSE);
    }
    if (success_state)
    {
        updated_window = static_cast<uint64_t>(this->_connection_local_window)
            + increment;
        if (updated_window > 0x7FFFFFFFul)
        {
            this->unlock(lock_acquired);
            this->set_error(FT_ERR_OUT_OF_RANGE);
            return (FT_FALSE);
        }
        else
        {
            this->_connection_local_window = static_cast<uint32_t>(updated_window);
        }
    }
    this->unlock(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (success_state);
}

ft_bool http2_stream_manager::update_connection_remote_window(uint32_t increment) noexcept
{
    uint64_t updated_window;
    ft_bool lock_acquired;
    ft_bool success_state;

    lock_acquired = FT_FALSE;
    success_state = FT_TRUE;
    if (this->lock(&lock_acquired) != 0)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (FT_FALSE);
    }
    if (increment > 0x7FFFFFFFu)
    {
        this->unlock(lock_acquired);
        this->set_error(FT_ERR_OUT_OF_RANGE);
        return (FT_FALSE);
    }
    if (success_state)
    {
        updated_window = static_cast<uint64_t>(this->_connection_remote_window)
            + increment;
        if (updated_window > 0x7FFFFFFFul)
        {
            this->unlock(lock_acquired);
            this->set_error(FT_ERR_OUT_OF_RANGE);
            return (FT_FALSE);
        }
        else
        {
            this->_connection_remote_window = static_cast<uint32_t>(updated_window);
        }
    }
    this->unlock(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (success_state);
}

uint32_t http2_stream_manager::get_connection_local_window() const noexcept
{
    ft_bool lock_acquired;
    uint32_t window_value;

    lock_acquired = FT_FALSE;
    window_value = 0;
    if (this->lock(&lock_acquired) != 0)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (0);
    }
    window_value = this->_connection_local_window;
    this->unlock(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (window_value);
}

uint32_t http2_stream_manager::get_connection_remote_window() const noexcept
{
    ft_bool lock_acquired;
    uint32_t window_value;

    lock_acquired = FT_FALSE;
    window_value = 0;
    if (this->lock(&lock_acquired) != 0)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        return (0);
    }
    window_value = this->_connection_remote_window;
    this->unlock(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (window_value);
}

ft_bool http2_settings_state::apply_single_setting(uint16_t identifier, uint32_t value,
    http2_stream_manager &streams) noexcept
{
    if (identifier == 0x1)
    {
        this->_header_table_size = value;
        return (FT_TRUE);
    }
    if (identifier == 0x2)
    {
        this->_enable_push = (value != 0);
        return (FT_TRUE);
    }
    if (identifier == 0x3)
    {
        this->_max_concurrent_streams = value;
        return (FT_TRUE);
    }
    if (identifier == 0x4)
    {
        if (!streams.update_remote_initial_window(value))
        {
            return (FT_FALSE);
        }
        this->_initial_remote_window = value;
        return (FT_TRUE);
    }
    if (identifier == 0x5)
    {
        this->_max_frame_size = value;
        return (FT_TRUE);
    }
    if (identifier == 0x6)
    {
        this->_max_header_list_size = value;
        return (FT_TRUE);
    }
    return (FT_TRUE);
}

http2_settings_state::http2_settings_state() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _header_table_size(4096),
      _enable_push(FT_TRUE), _max_concurrent_streams(0xFFFFFFFFu),
      _initial_local_window(65535), _initial_remote_window(65535),
      _max_frame_size(16384), _max_header_list_size(0)
{
    return ;
}

http2_settings_state::~http2_settings_state() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t http2_settings_state::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "http2_settings_state::initialize", "initialize called on initialised instance");
    this->_header_table_size = 4096;
    this->_enable_push = FT_TRUE;
    this->_max_concurrent_streams = 0xFFFFFFFFu;
    this->_initial_local_window = 65535;
    this->_initial_remote_window = 65535;
    this->_max_frame_size = 16384;
    this->_max_header_list_size = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t http2_settings_state::move(http2_settings_state &other) noexcept
{
    return (this->initialize(static_cast<http2_settings_state &&>(other)));
}

int32_t http2_settings_state::initialize(const http2_settings_state &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "http2_settings_state::initialize(const http2_settings_state &)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    this->_header_table_size = other._header_table_size;
    this->_enable_push = other._enable_push;
    this->_max_concurrent_streams = other._max_concurrent_streams;
    this->_initial_local_window = other._initial_local_window;
    this->_initial_remote_window = other._initial_remote_window;
    this->_max_frame_size = other._max_frame_size;
    this->_max_header_list_size = other._max_header_list_size;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t http2_settings_state::initialize(http2_settings_state &&other) noexcept
{
    int32_t initialize_error;

    initialize_error = this->initialize(other);
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t http2_settings_state::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

static uint16_t http2_setting_identifier(const unsigned char *entry)
{
    return (static_cast<uint16_t>((static_cast<uint16_t>(entry[0]) << 8) | entry[1]));
}

static uint32_t http2_setting_value(const unsigned char *entry)
{
    return ((static_cast<uint32_t>(entry[2]) << 24)
        | (static_cast<uint32_t>(entry[3]) << 16)
        | (static_cast<uint32_t>(entry[4]) << 8)
        | static_cast<uint32_t>(entry[5]));
}

static ft_bool http2_setting_valid(uint16_t identifier, uint32_t value)
{
    if (identifier == 0x2)
        return (value <= 1U);
    if (identifier == 0x4)
        return (value <= 0x7FFFFFFFU);
    if (identifier == 0x5)
        return (value >= 16384U && value <= 16777215U);
    return (FT_TRUE);
}

ft_bool http2_settings_state::apply_remote_settings(const http2_frame &frame,
    http2_stream_manager &streams) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_settings_state::apply_remote_settings");
    ft_bool lock_acquired = FT_FALSE;
    if (frame.lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    constexpr ft_size_t SETTING_SIZE = 6;
    const ft_size_t payload_length = frame._payload.size();
    const ft_bool acknowledge = (frame._flags & 0x1U) != 0;
    ft_bool success = frame._type == 0x4U && frame._stream_identifier == 0
        && payload_length % SETTING_SIZE == 0 && (!acknowledge || payload_length == 0);
    const auto *bytes = reinterpret_cast<const unsigned char *>(frame._payload.c_str());
    ft_size_t offset = 0;
    while (success && offset < payload_length)
    {
        success = http2_setting_valid(http2_setting_identifier(bytes + offset),
            http2_setting_value(bytes + offset));
        offset += SETTING_SIZE;
    }
    offset = 0;
    while (success && offset < payload_length)
    {
        success = this->apply_single_setting(http2_setting_identifier(bytes + offset),
            http2_setting_value(bytes + offset), streams);
        offset += SETTING_SIZE;
    }
    frame.unlock(lock_acquired);
    return (success);
}

ft_bool http2_settings_state::update_local_initial_window(uint32_t new_window,
    http2_stream_manager &streams) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_settings_state::update_local_initial_window");
    if (new_window > 0x7FFFFFFFu)
    {
        return (FT_FALSE);
    }
    if (!streams.update_local_initial_window(new_window))
    {
        return (FT_FALSE);
    }
    this->_initial_local_window = new_window;
    return (FT_TRUE);
}

ft_bool http2_settings_state::update_remote_initial_window(uint32_t new_window,
    http2_stream_manager &streams) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_settings_state::update_remote_initial_window");
    if (new_window > 0x7FFFFFFFu)
    {
        return (FT_FALSE);
    }
    if (!streams.update_remote_initial_window(new_window))
    {
        return (FT_FALSE);
    }
    this->_initial_remote_window = new_window;
    return (FT_TRUE);
}

uint32_t http2_settings_state::get_header_table_size() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_settings_state::get_header_table_size");
    return (this->_header_table_size);
}

ft_bool http2_settings_state::get_enable_push() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_settings_state::get_enable_push");
    return (this->_enable_push);
}

uint32_t http2_settings_state::get_max_concurrent_streams() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_settings_state::get_max_concurrent_streams");
    return (this->_max_concurrent_streams);
}

uint32_t http2_settings_state::get_initial_local_window() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_settings_state::get_initial_local_window");
    return (this->_initial_local_window);
}

uint32_t http2_settings_state::get_initial_remote_window() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_settings_state::get_initial_remote_window");
    return (this->_initial_remote_window);
}

uint32_t http2_settings_state::get_max_frame_size() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_settings_state::get_max_frame_size");
    return (this->_max_frame_size);
}

uint32_t http2_settings_state::get_max_header_list_size() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "http2_settings_state::get_max_header_list_size");
    return (this->_max_header_list_size);
}

ft_bool http2_encode_frame(const http2_frame &frame, ft_string &out_buffer,
    int32_t &error_code) noexcept
{
    if (out_buffer.clear() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    ft_bool lock_acquired = FT_FALSE;
    if (frame.lock(&lock_acquired) != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_INVALID_STATE;
        return (FT_FALSE);
    }
    const ft_size_t payload_length = frame._payload.size();
    if (payload_length > 0xFFFFFF)
    {
        frame.unlock(lock_acquired);
        error_code = FT_ERR_OUT_OF_RANGE;
        return (FT_FALSE);
    }
    const uint32_t stream_identifier = frame._stream_identifier;
    const unsigned char header[] = {
        static_cast<unsigned char>((payload_length >> 16) & 0xFF),
        static_cast<unsigned char>((payload_length >> 8) & 0xFF),
        static_cast<unsigned char>(payload_length & 0xFF),
        frame._type,
        frame._flags,
        static_cast<unsigned char>((stream_identifier >> 24) & 0x7F),
        static_cast<unsigned char>((stream_identifier >> 16) & 0xFF),
        static_cast<unsigned char>((stream_identifier >> 8) & 0xFF),
        static_cast<unsigned char>(stream_identifier & 0xFF),
    };
    const ft_bool success = out_buffer.append(reinterpret_cast<const char *>(header),
        sizeof(header)) == FT_ERR_SUCCESS
        && (payload_length == 0
            || out_buffer.append(frame._payload.c_str(), payload_length) == FT_ERR_SUCCESS);
    frame.unlock(lock_acquired);
    if (!success)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    error_code = FT_ERR_SUCCESS;
    return (FT_TRUE);
}

ft_bool http2_decode_frame(const unsigned char *buffer, ft_size_t buffer_size,
    ft_size_t &offset, http2_frame &out_frame, int32_t &error_code) noexcept
{
    if (!buffer || buffer_size < offset)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    constexpr ft_size_t HEADER_SIZE = 9;
    const ft_size_t remaining = buffer_size - offset;
    if (remaining < HEADER_SIZE)
    {
        error_code = FT_ERR_OUT_OF_RANGE;
        return (FT_FALSE);
    }
    const unsigned char *header = buffer + offset;
    const ft_size_t payload_length = (static_cast<ft_size_t>(header[0]) << 16)
        | (static_cast<ft_size_t>(header[1]) << 8)
        | static_cast<ft_size_t>(header[2]);
    if (remaining - HEADER_SIZE < payload_length)
    {
        error_code = FT_ERR_OUT_OF_RANGE;
        return (FT_FALSE);
    }
    const uint8_t type = header[3];
    const uint8_t flags = header[4];
    const uint32_t stream_identifier = (static_cast<uint32_t>(header[5] & 0x7F) << 24)
        | (static_cast<uint32_t>(header[6]) << 16)
        | (static_cast<uint32_t>(header[7]) << 8)
        | static_cast<uint32_t>(header[8]);
    ft_bool lock_acquired = FT_FALSE;
    if (out_frame.lock(&lock_acquired) != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_INVALID_STATE;
        return (FT_FALSE);
    }
    error_code = out_frame._payload.assign(
        reinterpret_cast<const char *>(header + HEADER_SIZE), payload_length);
    if (error_code == FT_ERR_SUCCESS)
    {
        out_frame._type = type;
        out_frame._flags = flags;
        out_frame._stream_identifier = stream_identifier;
        offset += HEADER_SIZE + payload_length;
    }
    out_frame.unlock(lock_acquired);
    return (error_code == FT_ERR_SUCCESS);
}

ft_bool http2_compress_headers(const ft_vector<http2_header_field> &headers,
    ft_string &out_block, int32_t &error_code) noexcept
{
    error_code = out_block.clear();
    if (error_code != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const ft_size_t header_count = headers.size();
    error_code = headers.get_error();
    if (error_code != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (header_count > UINT16_MAX)
    {
        error_code = FT_ERR_OUT_OF_RANGE;
        return (FT_FALSE);
    }
    error_code = http2_append_uint16(out_block, static_cast<uint16_t>(header_count));
    ft_size_t index = 0;
    while (error_code == FT_ERR_SUCCESS && index < header_count)
    {
        const http2_header_field &field = headers[index];
        ft_bool lock_acquired = FT_FALSE;
        if (field.lock(&lock_acquired) != FT_ERR_SUCCESS)
        {
            error_code = FT_ERR_INVALID_STATE;
            return (FT_FALSE);
        }
        const ft_size_t name_length = field._name.size();
        const ft_size_t value_length = field._value.size();
        if (name_length > UINT16_MAX || value_length > UINT16_MAX)
            error_code = FT_ERR_OUT_OF_RANGE;
        else
        {
            error_code = http2_append_uint16(out_block, static_cast<uint16_t>(name_length));
            if (error_code == FT_ERR_SUCCESS)
                error_code = out_block.append(field._name.c_str(), name_length);
            if (error_code == FT_ERR_SUCCESS)
                error_code = http2_append_uint16(out_block, static_cast<uint16_t>(value_length));
            if (error_code == FT_ERR_SUCCESS)
                error_code = out_block.append(field._value.c_str(), value_length);
        }
        field.unlock(lock_acquired);
        ++index;
    }
    return (error_code == FT_ERR_SUCCESS);
}

ft_bool http2_decompress_headers(const ft_string &block,
    ft_vector<http2_header_field> &out_headers, int32_t &error_code) noexcept
{
    out_headers.clear();
    error_code = out_headers.get_error();
    if (error_code != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const ft_size_t buffer_length = block.size();
    if (buffer_length < 2U)
    {
        error_code = FT_ERR_OUT_OF_RANGE;
        return (FT_FALSE);
    }
    const auto *buffer = reinterpret_cast<const unsigned char *>(block.c_str());
    const ft_size_t header_count = (static_cast<ft_size_t>(buffer[0]) << 8) | buffer[1];
    if (header_count > (buffer_length - 2U) / 4U)
    {
        error_code = FT_ERR_OUT_OF_RANGE;
        return (FT_FALSE);
    }
    out_headers.reserve(header_count);
    error_code = out_headers.get_error();
    if (error_code != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const unsigned char *cursor = buffer + 2;
    const unsigned char *end = buffer + buffer_length;
    ft_size_t index = 0;
    while (index < header_count)
    {
        if (end - cursor < 4)
        {
            error_code = FT_ERR_OUT_OF_RANGE;
            return (FT_FALSE);
        }
        const ft_size_t name_length = (static_cast<ft_size_t>(cursor[0]) << 8) | cursor[1];
        cursor += 2;
        if (static_cast<ft_size_t>(end - cursor) - 2U < name_length)
        {
            error_code = FT_ERR_OUT_OF_RANGE;
            return (FT_FALSE);
        }
        const char *name = reinterpret_cast<const char *>(cursor);
        cursor += name_length;
        const ft_size_t value_length = (static_cast<ft_size_t>(cursor[0]) << 8) | cursor[1];
        cursor += 2;
        if (static_cast<ft_size_t>(end - cursor) < value_length)
        {
            error_code = FT_ERR_OUT_OF_RANGE;
            return (FT_FALSE);
        }
        http2_header_field entry;
        error_code = entry.initialize();
        if (error_code != FT_ERR_SUCCESS)
            return (FT_FALSE);
        if (!entry.assign_from_buffers(name, name_length,
                reinterpret_cast<const char *>(cursor), value_length))
        {
            error_code = FT_ERR_INVALID_STATE;
            return (FT_FALSE);
        }
        error_code = out_headers.push_back(ft_move(entry));
        if (error_code != FT_ERR_SUCCESS)
            return (FT_FALSE);
        cursor += value_length;
        ++index;
    }
    return (FT_TRUE);
}

ft_bool http2_select_alpn_protocol(SSL *ssl_session, ft_bool &selected_http2,
    int32_t &error_code) noexcept
{
    selected_http2 = FT_FALSE;
    if (!ssl_session)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    static constexpr unsigned char PROTOCOLS[] = {
        2, 'h', '2', 8, 'h', 't', 't', 'p', '/', '1', '.', '1'
    };
    if (!SSL_is_init_finished(ssl_session)
        && SSL_set_alpn_protos(ssl_session, PROTOCOLS, sizeof(PROTOCOLS)) != 0)
    {
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    const unsigned char *selected_protocol = ft_nullptr;
    uint32_t selected_length = 0;
    SSL_get0_alpn_selected(ssl_session, &selected_protocol, &selected_length);
    selected_http2 = selected_protocol && selected_length == 2
        && selected_protocol[0] == 'h' && selected_protocol[1] == '2';
    error_code = FT_ERR_SUCCESS;
    return (FT_TRUE);
}

#endif
