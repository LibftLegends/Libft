#include <cstddef>
#include "advanced.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"

static ft_size_t count_words(const char *string, char delimiter)
{
    ft_size_t count = 0;
    ft_bool in_word = FT_FALSE;
    ft_size_t index = 0;
    while (string[index] != '\0')
    {
        if (string[index] != delimiter && in_word == FT_FALSE)
        {
            in_word = FT_TRUE;
            ++count;
        }
        else if (string[index] == delimiter)
        {
            in_word = FT_FALSE;
        }
        ++index;
    }
    return (count);
}

static void free_strings(char **strings, ft_size_t filled)
{
    ft_size_t index = 0;
    while (index < filled)
    {
        if (strings[index])
            cma_free(strings[index]);
        ++index;
    }
    cma_free(strings);
    return ;
}

char **adv_split(char const *string, char delimiter)
{
    if (string == ft_nullptr)
        return (ft_nullptr);
    ft_size_t word_count = count_words(string, delimiter);
    if (word_count >= FT_SYSTEM_SIZE_MAX / sizeof(char *))
        return (ft_nullptr);
    char **result = static_cast<char **>(cma_malloc((word_count + 1) * sizeof(char *)));
    if (result == ft_nullptr)
        return (ft_nullptr);
    ft_size_t current_word = 0;
    ft_size_t index = 0;
    while (string[index] != '\0')
    {
        if (string[index] == delimiter)
        {
            ++index;
            continue ;
        }
        ft_size_t start = index;
        while (string[index] != '\0' && string[index] != delimiter)
            ++index;
        ft_size_t length = index - start;
        char *word = ft_span_dup(string + start, length);
        if (word == ft_nullptr)
        {
            free_strings(result, current_word);
            return (ft_nullptr);
        }
        result[current_word++] = word;
    }
    result[current_word] = ft_nullptr;
    return (result);
}
