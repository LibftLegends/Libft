#include "class_data_buffer.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"

thread_local int32_t DataBuffer::_last_error = FT_ERR_SUCCESS;

static int32_t data_buffer_prepare_storage(ft_vector<uint8_t> &storage) noexcept
{
    int32_t initialize_error;

    if (storage.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        initialize_error = storage.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
    }
    else
        storage.clear();
    return (FT_ERR_SUCCESS);
}

data_buffer_proxy::data_buffer_proxy() noexcept
    : _data_buffer(ft_nullptr), _error_code(FT_ERR_SUCCESS)
{
    return ;
}

data_buffer_proxy::data_buffer_proxy(DataBuffer *data_buffer, int32_t error_code) noexcept
    : _data_buffer(data_buffer), _error_code(error_code)
{
    return ;
}

data_buffer_proxy::data_buffer_proxy(const data_buffer_proxy &other) noexcept
    : _data_buffer(other._data_buffer), _error_code(other._error_code)
{
    return ;
}

data_buffer_proxy::data_buffer_proxy(data_buffer_proxy &&other) noexcept
    : _data_buffer(other._data_buffer), _error_code(other._error_code)
{
    other._data_buffer = ft_nullptr;
    other._error_code = FT_ERR_SUCCESS;
    return ;
}

data_buffer_proxy::~data_buffer_proxy()
{
    return ;
}

int32_t data_buffer_proxy::get_error() const noexcept
{
    return (this->_error_code);
}

int32_t DataBuffer::set_last_error(int32_t error_code) noexcept
{
    DataBuffer::_last_error = error_code;
    return (error_code);
}

void DataBuffer::set_operation_error(int32_t error_code) noexcept
{
    this->_operation_error = error_code;
    this->_ok = (error_code == FT_ERR_SUCCESS);
    DataBuffer::set_last_error(error_code);
    return ;
}

int32_t DataBuffer::write_record_locked(const char *bytes, ft_size_t length) noexcept
{
    const ft_size_t previous_size = this->_buffer.size();
    const int32_t header_error = this->_buffer.append(
        reinterpret_cast<const uint8_t *>(&length), sizeof(length));
    if (header_error != FT_ERR_SUCCESS)
        return (header_error);
    const int32_t payload_error = this->_buffer.append(
        reinterpret_cast<const uint8_t *>(bytes), length);
    if (payload_error != FT_ERR_SUCCESS)
        this->_buffer.resize(previous_size);
    return (payload_error);
}

int32_t DataBuffer::read_length_locked(ft_size_t &length) noexcept
{
    const ft_size_t buffer_size = this->_buffer.size();
    if (this->_read_pos > buffer_size || sizeof(length) > buffer_size - this->_read_pos)
        return (FT_ERR_INVALID_ARGUMENT);
    (void)ft_memcpy_fixed<sizeof(length)>(&length, this->_buffer.begin() + this->_read_pos);
    this->_read_pos += sizeof(length);
    return (FT_ERR_SUCCESS);
}

DataBuffer::DataBuffer() noexcept
    : _buffer(), _read_pos(0), _ok(FT_TRUE), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _operation_error(FT_ERR_SUCCESS)
{
    return ;
}

DataBuffer::~DataBuffer() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return ;
    }
    int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        DataBuffer::set_last_error(destroy_error);
    return ;
}

int32_t DataBuffer::initialize() noexcept
{
    int32_t prepare_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "DataBuffer::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    prepare_error = data_buffer_prepare_storage(this->_buffer);
    if (prepare_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (prepare_error);
    }
    this->_read_pos = 0;
    this->_ok = FT_TRUE;
    this->_operation_error = FT_ERR_SUCCESS;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    DataBuffer::set_last_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t DataBuffer::initialize(const DataBuffer &other) noexcept
{
    int32_t prepare_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "DataBuffer::initialize(const DataBuffer &)",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            int32_t destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                return (destroy_error);
        }
        prepare_error = data_buffer_prepare_storage(this->_buffer);
        if (prepare_error != FT_ERR_SUCCESS)
            return (prepare_error);
        this->_read_pos = 0;
        this->_ok = FT_TRUE;
        this->_operation_error = FT_ERR_SUCCESS;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        DataBuffer::set_last_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (&other == this)
    {
        this->set_operation_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_read_pos = 0;
    this->_ok = FT_TRUE;
    this->_operation_error = FT_ERR_SUCCESS;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    const DataBuffer *first_locked = this;
    const DataBuffer *second_locked = &other;
    if (second_locked < first_locked)
    {
        const DataBuffer *temporary = first_locked;
        first_locked = second_locked;
        second_locked = temporary;
    }
    int32_t first_lock_error = pt_recursive_mutex_lock_if_not_null(first_locked->_mutex);
    if (first_lock_error != FT_ERR_SUCCESS)
        return (first_lock_error);
    int32_t second_lock_error = pt_recursive_mutex_lock_if_not_null(second_locked->_mutex);
    if (second_lock_error != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(first_locked->_mutex);
        return (second_lock_error);
    }
    prepare_error = data_buffer_prepare_storage(this->_buffer);
    if (prepare_error != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(second_locked->_mutex);
        (void)pt_recursive_mutex_unlock_if_not_null(first_locked->_mutex);
        this->set_operation_error(prepare_error);
        return (prepare_error);
    }
    const int32_t copy_error = this->_buffer.append(other._buffer.begin(),
        other._buffer.size());
    if (copy_error == FT_ERR_SUCCESS)
        this->_read_pos = other._read_pos;
    (void)pt_recursive_mutex_unlock_if_not_null(second_locked->_mutex);
    (void)pt_recursive_mutex_unlock_if_not_null(first_locked->_mutex);
    if (copy_error == FT_ERR_SUCCESS)
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_operation_error(copy_error);
    return (copy_error);
}

int32_t DataBuffer::move(DataBuffer &other) noexcept
{
    pt_recursive_mutex *moved_mutex;
    int32_t move_error;
    int32_t moved_operation_error;
    ft_bool moved_ok;
    ft_size_t moved_read_pos;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "DataBuffer::move(DataBuffer &)",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            int32_t destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
                return (destroy_error);
        }
        this->_read_pos = 0;
        this->_ok = FT_TRUE;
        this->_operation_error = FT_ERR_SUCCESS;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_operation_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (&other == this)
    {
        this->set_operation_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_read_pos = 0;
    this->_ok = FT_TRUE;
    this->_operation_error = FT_ERR_SUCCESS;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    DataBuffer *first_locked = this;
    DataBuffer *second_locked = &other;
    if (second_locked < first_locked)
    {
        DataBuffer *temporary = first_locked;
        first_locked = second_locked;
        second_locked = temporary;
    }
    int32_t first_lock_error = pt_recursive_mutex_lock_if_not_null(first_locked->_mutex);
    if (first_lock_error != FT_ERR_SUCCESS)
        return (first_lock_error);
    int32_t second_lock_error = pt_recursive_mutex_lock_if_not_null(second_locked->_mutex);
    if (second_lock_error != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(first_locked->_mutex);
        return (second_lock_error);
    }
    move_error = this->_buffer.move(other._buffer);
    if (move_error != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(second_locked->_mutex);
        (void)pt_recursive_mutex_unlock_if_not_null(first_locked->_mutex);
        this->set_operation_error(move_error);
        return (move_error);
    }
    moved_read_pos = other._read_pos;
    moved_ok = other._ok;
    moved_operation_error = other._operation_error;
    moved_mutex = other._mutex;
    other._read_pos = 0;
    other._ok = FT_TRUE;
    other._operation_error = FT_ERR_SUCCESS;
    (void)pt_recursive_mutex_unlock_if_not_null(second_locked->_mutex);
    (void)pt_recursive_mutex_unlock_if_not_null(first_locked->_mutex);
    this->_read_pos = moved_read_pos;
    this->_ok = moved_ok;
    this->_operation_error = moved_operation_error;
    this->_mutex = moved_mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    DataBuffer::set_last_error(this->_operation_error);
    return (FT_ERR_SUCCESS);
}

int32_t DataBuffer::destroy() noexcept
{
    int32_t destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    destroy_error = this->disable_thread_safety();
    this->_buffer.clear();
    this->_read_pos = 0;
    this->_ok = FT_TRUE;
    this->_operation_error = destroy_error;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    DataBuffer::set_last_error(this->_operation_error);
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    this->_operation_error = FT_ERR_SUCCESS;
    DataBuffer::set_last_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

void DataBuffer::clear() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::clear");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_operation_error(lock_error);
        return ;
    }
    this->_buffer.clear();
    this->_read_pos = 0;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->set_operation_error(FT_ERR_SUCCESS);
    return ;
}

ft_size_t DataBuffer::size() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::size");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<DataBuffer *>(this)->set_operation_error(lock_error);
        return (0);
    }
    ft_size_t value = this->_buffer.size();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    const_cast<DataBuffer *>(this)->set_operation_error(FT_ERR_SUCCESS);
    return (value);
}

const ft_vector<uint8_t>& DataBuffer::data() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::data");
    const_cast<DataBuffer *>(this)->set_operation_error(FT_ERR_SUCCESS);
    return (this->_buffer);
}

ft_size_t DataBuffer::tell() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::tell");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<DataBuffer *>(this)->set_operation_error(lock_error);
        return (0);
    }
    ft_size_t value = this->_read_pos;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    const_cast<DataBuffer *>(this)->set_operation_error(FT_ERR_SUCCESS);
    return (value);
}

ft_bool DataBuffer::seek(ft_size_t position) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::seek");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_operation_error(lock_error);
        return (FT_FALSE);
    }
    int32_t operation_error = FT_ERR_SUCCESS;
    if (position > this->_buffer.size())
        operation_error = FT_ERR_INVALID_ARGUMENT;
    if (operation_error == FT_ERR_SUCCESS)
        this->_read_pos = position;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->set_operation_error(operation_error);
    return (operation_error == FT_ERR_SUCCESS);
}

DataBuffer::operator ft_bool() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::operator ft_bool");
    const_cast<DataBuffer *>(this)->set_operation_error(FT_ERR_SUCCESS);
    return (this->_ok);
}

ft_bool DataBuffer::good() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::good");
    const_cast<DataBuffer *>(this)->set_operation_error(FT_ERR_SUCCESS);
    return (this->_ok);
}

ft_bool DataBuffer::bad() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::bad");
    const_cast<DataBuffer *>(this)->set_operation_error(FT_ERR_SUCCESS);
    return (!this->_ok);
}

int32_t DataBuffer::enable_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "DataBuffer::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        this->set_operation_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    pt_recursive_mutex *mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
    {
        this->set_operation_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    int32_t initialization_error = mutex_pointer->initialize();
    if (initialization_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        this->set_operation_error(initialization_error);
        return (initialization_error);
    }
    this->_mutex = mutex_pointer;
    this->set_operation_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t DataBuffer::disable_thread_safety(void) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "DataBuffer::disable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        int32_t destroy_error = this->_mutex->destroy();
        delete this->_mutex;
        this->_mutex = ft_nullptr;
        this->set_operation_error(destroy_error);
        return (destroy_error);
    }
    this->set_operation_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

ft_bool DataBuffer::is_thread_safe(void) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::is_thread_safe");
    const_cast<DataBuffer *>(this)->set_operation_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

int32_t DataBuffer::get_error() noexcept
{
    return (DataBuffer::_last_error);
}

const char *DataBuffer::get_error_str() noexcept
{
    return (ft_strerror(DataBuffer::get_error()));
}

int32_t DataBuffer::get_operation_error() const noexcept
{
    return (this->_operation_error);
}

const char *DataBuffer::get_operation_error_str() const noexcept
{
    return (ft_strerror(this->_operation_error));
}
