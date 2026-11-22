#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include <cstring>

int32_t    ft_memcmp(const void *pointer1, const void *pointer2, ft_size_t size)
{
    if (size == 0 || pointer1 == pointer2
        || pointer1 == ft_nullptr || pointer2 == ft_nullptr)
        return (0);

    const unsigned char *string1 = static_cast<const unsigned char *>(pointer1);
    const unsigned char *string2 = static_cast<const unsigned char *>(pointer2);

    ft_size_t index = 0;
    while (size - index >= 2U * sizeof(uint64_t))
    {
        uint64_t first_word;
        uint64_t second_word;

        std::memcpy(&first_word, string1 + index, sizeof(first_word));
        std::memcpy(&second_word, string2 + index, sizeof(second_word));
        if (first_word != second_word)
            break ;
        index += sizeof(first_word);
    }
    while (index < size)
    {
        if (string1[index] != string2[index])
            return (static_cast<int32_t>(string1[index])
                    - static_cast<int32_t>(string2[index]));
        ++index;
    }
    return (0);
}
