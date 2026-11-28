#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"

static unsigned char ft_ascii_to_lower(unsigned char character)
{
    if (character >= 'A' && character <= 'Z')
        return (static_cast<unsigned char>(character + ('a' - 'A')));
    return (character);
}

int32_t ft_strcasecmp(const char *left, const char *right)
{
    unsigned char left_character;
    unsigned char right_character;

    if (left == ft_nullptr || right == ft_nullptr)
        return (-1);
    while (*left != '\0' && *right != '\0')
    {
        left_character = ft_ascii_to_lower(static_cast<unsigned char>(*left));
        right_character = ft_ascii_to_lower(static_cast<unsigned char>(*right));
        if (left_character != right_character)
            return (static_cast<int32_t>(left_character)
                    - static_cast<int32_t>(right_character));
        ++left;
        ++right;
    }
    left_character = ft_ascii_to_lower(static_cast<unsigned char>(*left));
    right_character = ft_ascii_to_lower(static_cast<unsigned char>(*right));
    return (static_cast<int32_t>(left_character) - static_cast<int32_t>(right_character));
}

int32_t ft_strncasecmp(const char *left, const char *right, ft_size_t maximum_length)
{
    ft_size_t index;
    unsigned char left_character;
    unsigned char right_character;

    if (maximum_length == 0)
        return (0);
    if (left == ft_nullptr || right == ft_nullptr)
        return (-1);
    index = 0;
    while (index < maximum_length)
    {
        left_character = ft_ascii_to_lower(static_cast<unsigned char>(left[index]));
        right_character = ft_ascii_to_lower(static_cast<unsigned char>(right[index]));
        if (left_character != right_character)
            return (static_cast<int32_t>(left_character)
                    - static_cast<int32_t>(right_character));
        if (left_character == '\0')
            return (0);
        ++index;
    }
    return (0);
}

ft_bool ft_str_starts_with(const char *string, const char *prefix)
{
    ft_size_t index;

    if (string == ft_nullptr || prefix == ft_nullptr)
        return (FT_FALSE);
    index = 0;
    while (prefix[index] != '\0')
    {
        if (string[index] == '\0' || string[index] != prefix[index])
            return (FT_FALSE);
        ++index;
    }
    return (FT_TRUE);
}

ft_bool ft_str_ends_with(const char *string, const char *suffix)
{
    ft_size_t string_length;
    ft_size_t suffix_length;
    ft_size_t index;

    if (string == ft_nullptr || suffix == ft_nullptr)
        return (FT_FALSE);
    string_length = ft_strlen_size_t(string);
    suffix_length = ft_strlen_size_t(suffix);
    if (suffix_length > string_length)
        return (FT_FALSE);
    index = 0;
    while (index < suffix_length)
    {
        if (string[string_length - suffix_length + index] != suffix[index])
            return (FT_FALSE);
        ++index;
    }
    return (FT_TRUE);
}

ft_bool ft_str_contains(const char *haystack, const char *needle)
{
    return (static_cast<ft_bool>(ft_strstr(haystack, needle) != ft_nullptr));
}
