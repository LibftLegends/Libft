#include "uri_internal.hpp"
#include "../Basic/limits.hpp"

static ft_bool uri_query_key_matches(const char *encoded, ft_size_t encoded_size,
    const char *key, ft_size_t key_size) noexcept
{
    if (encoded_size < key_size)
        return (FT_FALSE);
    ft_size_t encoded_index = 0U;
    ft_size_t key_index = 0U;
    while (encoded_index < encoded_size && key_index < key_size)
    {
        uint8_t value = static_cast<uint8_t>(encoded[encoded_index]);
        if (value == '%')
        {
            if (uri_read_percent_byte(encoded + encoded_index,
                    encoded_size - encoded_index, value) == FT_FALSE)
                return (FT_FALSE);
            encoded_index += 2U;
        }
        if (value != static_cast<uint8_t>(key[key_index]))
            return (FT_FALSE);
        encoded_index += 1U;
        key_index += 1U;
    }
    return (encoded_index == encoded_size && key_index == key_size);
}

int32_t uri_query_find_value(const char *query, ft_size_t query_size,
    const char *key, ft_size_t key_size, const char *&value, ft_size_t &value_size) noexcept
{
    if ((query == ft_nullptr && query_size != 0U)
        || (key == ft_nullptr && key_size != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    if (query_size >= FT_SYSTEM_SIZE_MAX || key_size >= FT_SYSTEM_SIZE_MAX)
        return (FT_ERR_OUT_OF_RANGE);
    if (query == ft_nullptr)
        query = "";
    ft_size_t pair_start = 0U;
    while (FT_TRUE)
    {
        ft_size_t pair_end = pair_start;
        ft_size_t equals_index = query_size;
        while (pair_end < query_size && query[pair_end] != '&')
        {
            if (equals_index == query_size && query[pair_end] == '=')
                equals_index = pair_end;
            pair_end += 1U;
        }
        ft_size_t key_end = pair_end;
        ft_size_t value_start = pair_end;
        if (equals_index != query_size)
        {
            key_end = equals_index;
            value_start = equals_index + 1U;
        }
        if (uri_query_key_matches(query + pair_start, key_end - pair_start,
                key, key_size) == FT_TRUE)
        {
            value = query + value_start;
            value_size = pair_end - value_start;
            return (FT_ERR_SUCCESS);
        }
        if (pair_end == query_size)
            return (FT_ERR_NOT_FOUND);
        pair_start = pair_end + 1U;
    }
}

char *uri_query_get_value(const char *query, ft_size_t query_size,
    const char *key, ft_size_t key_size)
{
    const char *value;
    ft_size_t value_size;
    const int32_t error = uri_query_find_value(query, query_size, key, key_size,
        value, value_size);
    if (error != FT_ERR_SUCCESS)
    {
        uri_set_error(error);
        return (ft_nullptr);
    }
    return (reinterpret_cast<char *>(
        uri_percent_decode_component(value, value_size, ft_nullptr)));
}
