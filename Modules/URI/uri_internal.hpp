#ifndef URI_INTERNAL_HPP
# define URI_INTERNAL_HPP

#include "uri.hpp"
#include "../Basic/class_nullptr.hpp"

int32_t     uri_set_error(int32_t error_code) noexcept;

static constexpr ft_bool uri_is_alpha(char character) noexcept
{
    if (character >= 'A' && character <= 'Z')
        return (FT_TRUE);
    if (character >= 'a' && character <= 'z')
        return (FT_TRUE);
    return (FT_FALSE);
}

static constexpr ft_bool uri_is_digit(char character) noexcept
{
    if (character >= '0' && character <= '9')
        return (FT_TRUE);
    return (FT_FALSE);
}

static constexpr ft_bool uri_is_scheme_character(char character) noexcept
{
    if (uri_is_alpha(character) == FT_TRUE)
        return (FT_TRUE);
    if (uri_is_digit(character) == FT_TRUE)
        return (FT_TRUE);
    if (character == '+' || character == '-' || character == '.')
        return (FT_TRUE);
    return (FT_FALSE);
}

static constexpr ft_bool uri_is_unreserved(uint8_t value) noexcept
{
    if (value >= 'A' && value <= 'Z')
        return (FT_TRUE);
    if (value >= 'a' && value <= 'z')
        return (FT_TRUE);
    if (value >= '0' && value <= '9')
        return (FT_TRUE);
    if (value == '-' || value == '_' || value == '.' || value == '~')
        return (FT_TRUE);
    return (FT_FALSE);
}

static constexpr char uri_lower_character(char character) noexcept
{
    if (character >= 'A' && character <= 'Z')
        return (static_cast<char>(character + ('a' - 'A')));
    return (character);
}

static constexpr char uri_hex_character(uint8_t value) noexcept
{
    if (value < 10)
        return (static_cast<char>('0' + value));
    return (static_cast<char>('A' + (value - 10)));
}

static constexpr int32_t uri_hex_value(char character) noexcept
{
    if (character >= '0' && character <= '9')
        return (character - '0');
    if (character >= 'a' && character <= 'f')
        return (character - 'a' + 10);
    if (character >= 'A' && character <= 'F')
        return (character - 'A' + 10);
    return (-1);
}

static inline ft_bool uri_read_percent_byte(const char *input, ft_size_t remaining,
    uint8_t &value) noexcept
{
    if (remaining < 3U)
        return (FT_FALSE);
    const int32_t high = uri_hex_value(input[1]);
    const int32_t low = uri_hex_value(input[2]);
    if (high < 0 || low < 0)
        return (FT_FALSE);
    value = static_cast<uint8_t>((high << 4U) | low);
    return (FT_TRUE);
}

int32_t uri_query_find_value(const char *query, ft_size_t query_size,
    const char *key, ft_size_t key_size, const char *&value, ft_size_t &value_size) noexcept;
char        *uri_duplicate_span(const char *input, ft_size_t start,
                ft_size_t end) noexcept;
int32_t     uri_assign_component(char **destination, const char *input,
                ft_size_t start, ft_size_t end) noexcept;
void        uri_lower_string(char *string) noexcept;
void        uri_delete_components(uri_components *components) noexcept;

#endif
