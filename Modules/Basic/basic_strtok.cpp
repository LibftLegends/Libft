#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include <cstring>

char *ft_strtok(char *string, const char *delimiters)
{
    static thread_local char *saved_string = ft_nullptr;

    if (string != ft_nullptr)
        saved_string = string;
    if (delimiters == ft_nullptr || saved_string == ft_nullptr)
        return (ft_nullptr);
    char *token_start = saved_string + std::strspn(saved_string, delimiters);
    if (*token_start == '\0')
    {
        saved_string = ft_nullptr;
        return (ft_nullptr);
    }
    char *token_end = token_start + std::strcspn(token_start, delimiters);
    if (*token_end == '\0')
        saved_string = ft_nullptr;
    else
    {
        *token_end = '\0';
        saved_string = token_end + 1;
    }
    return (token_start);
}
