#include <cstddef>
#include <cstdarg>
#include "advanced.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"

char *adv_strjoin_multiple(int32_t count, ...)
{
    constexpr int32_t STACK_LENGTH_COUNT = 16;
    ft_size_t stack_lengths[STACK_LENGTH_COUNT];
    ft_size_t *length_cache = stack_lengths;

    if (count <= 0)
        return (ft_nullptr);
    if (static_cast<ft_size_t>(count) > FT_SYSTEM_SIZE_MAX / sizeof(ft_size_t))
        return (ft_nullptr);
    if (count > STACK_LENGTH_COUNT)
    {
        length_cache = static_cast<ft_size_t *>(
                cma_malloc(static_cast<ft_size_t>(count) * sizeof(ft_size_t)));
        if (length_cache == ft_nullptr)
            return (ft_nullptr);
    }
    va_list args;
    va_start(args, count);
    ft_size_t total_length = 0;
    int32_t index_argument = 0;
    while (index_argument < count)
    {
        const char *current = va_arg(args, const char *);
        const ft_size_t length = ft_strlen_size_t(current);
        if (total_length >= FT_SYSTEM_SIZE_MAX - length)
        {
            va_end(args);
            if (length_cache != stack_lengths)
                cma_free(length_cache);
            return (ft_nullptr);
        }
        length_cache[index_argument] = length;
        total_length += length;
        ++index_argument;
    }
    va_end(args);
    ft_size_t allocation_size = total_length + 1;
    char *result = static_cast<char *>(cma_malloc(allocation_size));
    if (result == ft_nullptr)
    {
        if (length_cache != stack_lengths)
            cma_free(length_cache);
        return (ft_nullptr);
    }
    va_start(args, count);
    ft_size_t write_index = 0;
    index_argument = 0;
    while (index_argument < count)
    {
        const char *current = va_arg(args, const char *);
        ft_size_t length = length_cache[index_argument];
        (void)ft_memcpy(result + write_index, current, length);
        write_index += length;
        ++index_argument;
    }
    va_end(args);
    if (length_cache != stack_lengths)
        cma_free(length_cache);
    result[write_index] = '\0';
    return (result);
}
