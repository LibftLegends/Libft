#include "class_istringstream.hpp"
#include "../Basic/basic.hpp"
#include "../Template/move.hpp"
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

ft_istringstream::ft_istringstream() noexcept
: ft_istream(), _buffer()
{
    return ;
}

ft_istringstream::~ft_istringstream() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_istringstream::initialize(const ft_string &string) noexcept
{
    uint32_t stream_initialize_error;
    uint32_t buffer_initialize_error;

    stream_initialize_error = ft_istream::initialize();
    if (stream_initialize_error != FT_ERR_SUCCESS)
        return (stream_initialize_error);
    buffer_initialize_error = this->_buffer.initialize(string);
    if (buffer_initialize_error != FT_ERR_SUCCESS)
    {
        (void)ft_istream::destroy();
        return (buffer_initialize_error);
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_istringstream::destroy() noexcept
{
    int32_t buffer_destroy_error;
    int32_t stream_destroy_error;

    buffer_destroy_error = this->_buffer.destroy();
    stream_destroy_error = ft_istream::destroy();
    if (buffer_destroy_error != FT_ERR_SUCCESS)
        return (buffer_destroy_error);
    return (stream_destroy_error);
}

int32_t ft_istringstream::move(ft_istringstream &other) noexcept
{
    if (&other == this)
        return (FT_ERR_SUCCESS);
    int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    int32_t stream_move_error = ft_istream::move(other);
    if (stream_move_error != FT_ERR_SUCCESS)
        return (stream_move_error);
    int32_t buffer_move_error = this->_buffer.move(other._buffer);
    if (buffer_move_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (buffer_move_error);
    }
    return (FT_ERR_SUCCESS);
}

ssize_t ft_istringstream::do_read(char *buffer, ft_size_t count)
{
    return (this->_buffer.read(buffer, count));
}

ft_string *ft_istringstream::get_string() const
{
    ft_string *value;
    int32_t initialize_error;

    value = new (std::nothrow) ft_string();
    if (value == ft_nullptr)
        return (ft_nullptr);
    initialize_error = value->initialize();
    if (initialize_error != FT_ERR_SUCCESS || this->_buffer.get_string(*value) != FT_ERR_SUCCESS)
    {
        delete value;
        return (ft_nullptr);
    }
    return (value);
}

ft_istringstream &operator>>(ft_istringstream &input, int32_t &value)
{
    ft_string content;
    if (content.initialize() == FT_ERR_SUCCESS
        && input._buffer.get_string(content) == FT_ERR_SUCCESS)
        value = ft_atoi(content.c_str());
    return (input);
}

ft_istringstream &operator>>(ft_istringstream &input, ft_string &value)
{
    ft_string content;
    if (content.initialize() == FT_ERR_SUCCESS
        && input._buffer.get_string(content) == FT_ERR_SUCCESS)
        value = ft_move(content);
    return (input);
}
