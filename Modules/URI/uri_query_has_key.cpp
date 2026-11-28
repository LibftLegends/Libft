#include "uri_internal.hpp"
#include "../Basic/basic.hpp"

ft_bool uri_query_has_key(const char *query, ft_size_t query_size,
    const char *key, ft_size_t key_size)
{
    if ((query == ft_nullptr && query_size != 0U)
        || (key == ft_nullptr && key_size != 0U))
        return (FT_FALSE);
    const char *encoded;
    ft_size_t remaining;
    int32_t error = uri_query_find_value(query, query_size, key, key_size,
        encoded, remaining);
    if (error != FT_ERR_SUCCESS)
    {
        uri_set_error(error);
        return (FT_FALSE);
    }
    while (remaining != 0U)
    {
        const char *percent = static_cast<const char *>(ft_memchr(encoded, '%', remaining));
        if (percent == ft_nullptr)
            break ;
        const ft_size_t offset = static_cast<ft_size_t>(percent - encoded);
        uint8_t value;
        if (uri_read_percent_byte(percent, remaining - offset, value) == FT_FALSE)
        {
            uri_set_error(FT_ERR_INVALID_ARGUMENT);
            return (FT_FALSE);
        }
        encoded = percent + 3U;
        remaining -= offset + 3U;
    }
    uri_set_error(FT_ERR_SUCCESS);
    return (FT_TRUE);
}
