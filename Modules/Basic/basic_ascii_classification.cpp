#include "basic.hpp"

int32_t ft_isxdigit(int32_t character)
{
    if ((character >= '0' && character <= '9')
        || (character >= 'a' && character <= 'f')
        || (character >= 'A' && character <= 'F'))
        return (1);
    return (0);
}

int32_t ft_ispunct(int32_t character)
{
    if (ft_isgraph(character) == 0 || ft_isalnum(character) != 0)
        return (0);
    return (1);
}

int32_t ft_isgraph(int32_t character)
{
    if (character >= 0x21 && character <= 0x7E)
        return (1);
    return (0);
}

int32_t ft_iscntrl(int32_t character)
{
    if ((character >= 0 && character <= 0x1F) || character == 0x7F)
        return (1);
    return (0);
}

int32_t ft_isblank(int32_t character)
{
    if (character == ' ' || character == '\t')
        return (1);
    return (0);
}
