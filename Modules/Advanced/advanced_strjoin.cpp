#include <cstddef>
#include "advanced.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"

char *adv_strjoin(char const *string_1, char const *string_2)
{
    if (string_1 == ft_nullptr && string_2 == ft_nullptr)
        return (ft_nullptr);
    const ft_size_t first_string_length = ft_strlen_size_t(string_1);
    const ft_size_t second_string_length = ft_strlen_size_t(string_2);
    if (first_string_length >= FT_SYSTEM_SIZE_MAX - second_string_length)
        return (ft_nullptr);
    ft_size_t total_length = first_string_length + second_string_length;
    char *result = static_cast<char *>(cma_malloc(total_length + 1));
    if (result == ft_nullptr)
        return (ft_nullptr);
    (void)ft_memcpy(result, string_1, first_string_length);
    (void)ft_memcpy(result + first_string_length, string_2, second_string_length);
    result[total_length] = '\0';
    return (result);
}
