#ifndef PTHREAD_BUFFER_HPP
# define PTHREAD_BUFFER_HPP

# include <cstdlib>
# include <cstring>
# include <limits>
# include <type_traits>
# include "../Basic/limits.hpp"
# include "../Basic/class_nullptr.hpp"
# include "../Errno/errno.hpp"

template <typename t_type>
struct pt_buffer
{
    static_assert(std::is_trivially_copyable<t_type>::value,
        "pt_buffer requires trivially copyable elements");

    t_type    *data;
    ft_size_t  size;
    ft_size_t  capacity;
};

template <typename t_type>
void pt_buffer_init(pt_buffer<t_type> &buffer)
{
    buffer.data = ft_nullptr;
    buffer.size = 0;
    buffer.capacity = 0;
    return ;
}

template <typename t_type>
void pt_buffer_destroy(pt_buffer<t_type> &buffer)
{
    if (buffer.data != ft_nullptr)
    {
        std::free(buffer.data);
        buffer.data = ft_nullptr;
    }
    buffer.size = 0;
    buffer.capacity = 0;
    return ;
}

template <typename t_type>
int pt_buffer_reserve(pt_buffer<t_type> &buffer, ft_size_t required)
{
    if (required <= buffer.capacity)
        return (FT_ERR_SUCCESS);
    constexpr ft_size_t MAX_CAPACITY =
        std::numeric_limits<std::size_t>::max() / sizeof(t_type);
    if (required > MAX_CAPACITY)
        return (FT_ERR_OUT_OF_RANGE);
    ft_size_t new_capacity = required;
    if (buffer.capacity == 0U && required < 4U && MAX_CAPACITY >= 4U)
        new_capacity = 4U;
    else if (buffer.capacity <= MAX_CAPACITY / 2U && required < buffer.capacity * 2U)
        new_capacity = buffer.capacity * 2U;
    void *new_data = std::realloc(buffer.data,
        static_cast<std::size_t>(new_capacity * sizeof(t_type)));
    if (new_data == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    buffer.data = static_cast<t_type *>(new_data);
    buffer.capacity = new_capacity;
    return (FT_ERR_SUCCESS);
}

template <typename t_type>
int pt_buffer_push(pt_buffer<t_type> &buffer, const t_type &value)
{
    if (buffer.size < buffer.capacity)
        buffer.data[buffer.size] = value;
    else
    {
        if (buffer.size == FT_SYSTEM_SIZE_MAX)
            return (FT_ERR_OUT_OF_RANGE);
        const t_type saved_value = value;
        const int reserve_error = pt_buffer_reserve(buffer, buffer.size + 1U);
        if (reserve_error != FT_ERR_SUCCESS)
            return (reserve_error);
        buffer.data[buffer.size] = saved_value;
    }
    buffer.size += 1;
    return (FT_ERR_SUCCESS);
}

template <typename t_type>
void pt_buffer_clear(pt_buffer<t_type> &buffer)
{
    buffer.size = 0;
    return ;
}

template <typename t_type>
int pt_buffer_copy(pt_buffer<t_type> &dest,
        const pt_buffer<t_type> &source)
{
    int reserve_error = pt_buffer_reserve(dest, source.size);
    if (reserve_error != FT_ERR_SUCCESS)
        return (reserve_error);
    if (source.size != 0U && dest.data != source.data)
        std::memmove(dest.data, source.data,
            static_cast<std::size_t>(source.size * sizeof(t_type)));
    dest.size = source.size;
    return (FT_ERR_SUCCESS);
}

template <typename t_type>
void pt_buffer_erase(pt_buffer<t_type> &buffer, ft_size_t index)
{
    if (index >= buffer.size)
        return ;
    const ft_size_t count = buffer.size - index - 1U;
    if (count != 0U)
        std::memmove(buffer.data + index, buffer.data + index + 1U,
            static_cast<std::size_t>(count * sizeof(t_type)));
    --buffer.size;
    return ;
}

#endif
