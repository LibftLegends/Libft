#include "basic.hpp"
#include "../Basic/class_nullptr.hpp"

char    *ft_strnstr(const char *haystack, const char *needle, ft_size_t maximum_length)
{
    constexpr ft_size_t INITIAL_CANDIDATE_COUNT = 64U;

    if (needle == ft_nullptr || haystack == ft_nullptr)
        return (ft_nullptr);

    const ft_size_t needle_length = ft_strlen_size_t(needle);
    if (needle_length == 0)
        return (const_cast<char *>(haystack));

    if (needle_length > maximum_length)
        return (ft_nullptr);
    if (ft_strncmp(haystack, needle, needle_length) == 0)
        return (const_cast<char *>(haystack));
    if (maximum_length - needle_length >= INITIAL_CANDIDATE_COUNT)
    {
        /* Check the first few candidates before measuring the entire
         * bounded string. Include enough lookahead for a crossing match. */
        const ft_size_t prefix_length = needle_length + INITIAL_CANDIDATE_COUNT - 1U;
        const ft_size_t available = ft_strnlen(haystack, prefix_length);
        void *match = ft_memmem(haystack, available, needle, needle_length);
        if (match != ft_nullptr || available != prefix_length)
            return (static_cast<char *>(match));
        haystack += INITIAL_CANDIDATE_COUNT;
        maximum_length -= INITIAL_CANDIDATE_COUNT;
    }
    return (static_cast<char *>(ft_memmem(haystack,
        ft_strnlen(haystack, maximum_length), needle, needle_length)));
}
