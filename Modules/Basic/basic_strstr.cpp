#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"

char    *ft_strstr(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
        return (ft_nullptr);
    const char *match = std::strstr(haystack, needle);
    return (const_cast<char *>(match));
}
