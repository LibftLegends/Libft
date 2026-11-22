#include "advanced.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

int64_t ft_hash_string31(const char *string)
{
    uint32_t hash_value;
    ft_size_t index;
    uint8_t character_value;

    if (string == ft_nullptr)
        return (0);
    hash_value = 0;
    index = 0;
    while (string[index] != '\0')
    {
        character_value = static_cast<uint8_t>(string[index]);
        hash_value = hash_value * 31U + static_cast<uint32_t>(character_value);
        index++;
    }
    return (static_cast<int64_t>(static_cast<int32_t>(hash_value)));
}

int64_t ft_hash_string31(const ft_string &string)
{
    return (ft_hash_string31(string.c_str()));
}

char *ft_span_dup(const char *buffer, ft_size_t length)
{
    char *duplicate;
    if ((!buffer && length != 0) || length == FT_SYSTEM_SIZE_MAX)
        return (ft_nullptr);
    duplicate = static_cast<char *>(cma_malloc(length + 1));
    if (duplicate == ft_nullptr)
        return (ft_nullptr);
    (void)ft_memcpy(duplicate, buffer, length);
    duplicate[length] = '\0';
    return (duplicate);
}

char *ft_strmapi(const char *string, char (*function)(uint32_t, char))
{
    char *result;
    ft_size_t length;
    uint32_t index;

    if (!string || !function)
        return (ft_nullptr);
    length = ft_strlen_size_t(string);
    result = static_cast<char *>(cma_malloc(length + 1));
    if (result == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < length)
    {
        result[index] = function(index, string[index]);
        ++index;
    }
    result[length] = '\0';
    return (result);
}
