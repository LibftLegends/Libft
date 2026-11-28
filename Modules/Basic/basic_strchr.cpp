#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"

char    *ft_strchr(const char *string, int32_t char_to_find)
{
    if (!string)
        return (ft_nullptr);

    const char *match = std::strchr(string, char_to_find);
    return (const_cast<char *>(match));
}
