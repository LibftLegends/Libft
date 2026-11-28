#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"

char    *ft_strrchr(const char *string, int32_t char_to_find)
{
    if (!string)
        return (ft_nullptr);

    const char *match = std::strrchr(string, char_to_find);
    return (const_cast<char *>(match));
}
