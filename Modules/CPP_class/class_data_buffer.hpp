#ifndef DATA_BUFFER
# define DATA_BUFFER

#include "../Template/vector.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Advanced/advanced.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Template/move.hpp"
#include "class_string.hpp"
#include "class_istringstream.hpp"
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>

class DataBuffer;

class data_buffer_proxy
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        DataBuffer *_data_buffer;
        int32_t _error_code;

    public:
        data_buffer_proxy() noexcept;
        data_buffer_proxy(DataBuffer *data_buffer, int32_t error_code) noexcept;
        data_buffer_proxy(const data_buffer_proxy &other) noexcept;
        data_buffer_proxy(data_buffer_proxy &&other) noexcept;
        ~data_buffer_proxy();

        data_buffer_proxy &operator=(const data_buffer_proxy &other) noexcept = delete;
        data_buffer_proxy &operator=(data_buffer_proxy &&other) noexcept = delete;

        template <typename ValueType>
        data_buffer_proxy operator<<(const ValueType &value) const noexcept;

        template <typename ValueType>
        data_buffer_proxy operator>>(ValueType &value) const noexcept;

        int32_t get_error() const noexcept;
};

class DataBuffer
{
    friend class data_buffer_proxy;

#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_vector<uint8_t> _buffer;
        ft_size_t _read_pos;
        ft_bool _ok;
        mutable pt_recursive_mutex *_mutex;
        uint8_t _initialised_state;
        int32_t _operation_error;
        static thread_local int32_t _last_error;

        static int32_t set_last_error(int32_t error_code) noexcept;
        void set_operation_error(int32_t error_code) noexcept;
        int32_t write_record_locked(const char *bytes, ft_size_t length) noexcept;
        int32_t read_length_locked(ft_size_t &length) noexcept;

        template <typename ValueType>
        int32_t write_value(const ValueType &value) noexcept;

        template <typename ValueType>
        int32_t read_value(ValueType &value) noexcept;

    public:
        DataBuffer() noexcept;
        DataBuffer(const DataBuffer& other) noexcept = delete;
        DataBuffer(DataBuffer&& other) noexcept = delete;
        ~DataBuffer() noexcept;

        DataBuffer& operator=(const DataBuffer& other) noexcept = delete;
        DataBuffer& operator=(DataBuffer&& other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const DataBuffer &other) noexcept;
        int32_t move(DataBuffer &other) noexcept;
        int32_t destroy() noexcept;

        void clear() noexcept;
        ft_size_t size() const noexcept;
        const ft_vector<uint8_t>& data() const noexcept;
        ft_size_t tell() const noexcept;
        ft_bool seek(ft_size_t position) noexcept;

        explicit operator ft_bool() const noexcept;
        ft_bool good() const noexcept;
        ft_bool bad() const noexcept;

        template <typename ValueType>
        data_buffer_proxy operator<<(const ValueType &value) noexcept;

        template <typename ValueType>
        data_buffer_proxy operator>>(ValueType &value) noexcept;

        int32_t enable_thread_safety(void) noexcept;
        int32_t disable_thread_safety(void) noexcept;
        ft_bool is_thread_safe(void) const noexcept;

        static int32_t get_error() noexcept;
        static const char *get_error_str() noexcept;
        int32_t get_operation_error() const noexcept;
        const char *get_operation_error_str() const noexcept;

};

template <typename ValueType>
int32_t DataBuffer::write_value(const ValueType &value) noexcept
{
    std::ostringstream output_stream;
    output_stream << value;
    const std::string bytes = output_stream.str();
    const std::string::size_type terminator = bytes.find('\0');
    ft_size_t length = bytes.size();
    if (terminator != std::string::npos)
        length = terminator;
    return (this->write_record_locked(bytes.data(), length));
}

template <typename ValueType>
int32_t DataBuffer::read_value(ValueType &value) noexcept
{
    ft_size_t length = 0;
    int32_t length_error = this->read_length_locked(length);
    if (length_error != FT_ERR_SUCCESS)
        return (length_error);
    if (length > this->_buffer.size() - this->_read_pos)
        return (FT_ERR_INVALID_ARGUMENT);
    const char *bytes = reinterpret_cast<const char *>(this->_buffer.begin() + this->_read_pos);
    this->_read_pos += length;
    const char *terminator = static_cast<const char *>(ft_memchr(bytes, '\0', length));
    if (terminator != ft_nullptr)
        length = static_cast<ft_size_t>(terminator - bytes);
    ft_string string_value;
    int32_t string_initialize_error = string_value.initialize();
    if (string_initialize_error != FT_ERR_SUCCESS)
        return (string_initialize_error);
    const int32_t string_assign_error = string_value.assign(bytes, length);
    if (string_assign_error != FT_ERR_SUCCESS)
        return (string_assign_error);
    if constexpr (std::is_same_v<ValueType, ft_string>)
    {
        value = ft_move(string_value);
        return (value.get_error());
    }
    else if constexpr (std::is_same_v<ValueType, int32_t>)
    {
        value = ft_atoi(string_value.c_str());
        return (FT_ERR_SUCCESS);
    }
    else
    {
        ft_istringstream input_stream;
        const int32_t input_stream_initialize_error = input_stream.initialize(string_value);
        if (input_stream_initialize_error != FT_ERR_SUCCESS)
            return (input_stream_initialize_error);
        input_stream >> value;
        return (FT_ERR_SUCCESS);
    }
}

template <typename ValueType>
data_buffer_proxy DataBuffer::operator<<(const ValueType &value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::operator<<");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_operation_error(lock_error);
        return (data_buffer_proxy(this, lock_error));
    }
    int32_t write_error = this->write_value(value);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    int32_t final_error = write_error;
    this->set_operation_error(final_error);
    return (data_buffer_proxy(this, final_error));
}

template <typename ValueType>
data_buffer_proxy DataBuffer::operator>>(ValueType &value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "DataBuffer::operator>>");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_operation_error(lock_error);
        return (data_buffer_proxy(this, lock_error));
    }
    int32_t read_error = this->read_value(value);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    int32_t final_error = read_error;
    this->set_operation_error(final_error);
    return (data_buffer_proxy(this, final_error));
}

template <typename ValueType>
data_buffer_proxy data_buffer_proxy::operator<<(const ValueType &value) const noexcept
{
    if (this->_error_code != FT_ERR_SUCCESS)
    {
        DataBuffer::set_last_error(this->_error_code);
        return (data_buffer_proxy(this->_data_buffer, this->_error_code));
    }
    if (this->_data_buffer == ft_nullptr)
    {
        DataBuffer::set_last_error(FT_ERR_INVALID_ARGUMENT);
        return (data_buffer_proxy(this->_data_buffer, FT_ERR_INVALID_ARGUMENT));
    }
    return (this->_data_buffer->operator<<(value));
}

template <typename ValueType>
data_buffer_proxy data_buffer_proxy::operator>>(ValueType &value) const noexcept
{
    if (this->_error_code != FT_ERR_SUCCESS)
    {
        DataBuffer::set_last_error(this->_error_code);
        return (data_buffer_proxy(this->_data_buffer, this->_error_code));
    }
    if (this->_data_buffer == ft_nullptr)
    {
        DataBuffer::set_last_error(FT_ERR_INVALID_ARGUMENT);
        return (data_buffer_proxy(this->_data_buffer, FT_ERR_INVALID_ARGUMENT));
    }
    return (this->_data_buffer->operator>>(value));
}

#endif
