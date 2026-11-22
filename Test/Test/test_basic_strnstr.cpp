#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_basic_strnstr_basic)
{
    const char *haystack = "hello world";

    FT_ASSERT_EQ(haystack + 6, ft_strnstr(haystack, "world", 11));
    return (1);
}

FT_TEST(test_basic_strnstr_not_found)
{
    FT_ASSERT_EQ(ft_nullptr, ft_strnstr("hello", "xyz", 5));
    return (1);
}

FT_TEST(test_basic_strnstr_empty_needle)
{
    const char *haystack = "hello";

    FT_ASSERT_EQ(haystack, ft_strnstr(haystack, "", 5));
    return (1);
}

FT_TEST(test_basic_strnstr_size_limit)
{
    FT_ASSERT_EQ(ft_nullptr, ft_strnstr("hello", "lo", 3));
    return (1);
}

FT_TEST(test_basic_strnstr_zero_size)
{
    FT_ASSERT_EQ(ft_nullptr, ft_strnstr("hello", "he", 0));
    return (1);
}

FT_TEST(test_basic_strnstr_empty_needle_zero_size)
{
    const char *haystack = "hello";

    FT_ASSERT_EQ(haystack, ft_strnstr(haystack, "", 0));
    return (1);
}

FT_TEST(test_basic_strnstr_zero_size_allows_null_haystack)
{
    FT_ASSERT_EQ(ft_nullptr, ft_strnstr(ft_nullptr, "needle", 0));
    return (1);
}

FT_TEST(test_basic_strnstr_zero_size_null_haystack_empty_needle)
{
    FT_ASSERT_EQ(ft_nullptr, ft_strnstr(ft_nullptr, "", 0));
    return (1);
}

FT_TEST(test_basic_strnstr_null_arguments)
{
    FT_ASSERT_EQ(ft_nullptr, ft_strnstr(ft_nullptr, "abc", 3));
    FT_ASSERT_EQ(ft_nullptr, ft_strnstr("abc", ft_nullptr, 3));
    return (1);
}

FT_TEST(test_basic_strnstr_recovers_after_null_haystack)
{
    const char *haystack;

    haystack = "searchable";
    FT_ASSERT_EQ(ft_nullptr, ft_strnstr(ft_nullptr, "search", 7));
    FT_ASSERT_EQ(haystack + 4, ft_strnstr(haystack, "ch", 8));
    return (1);
}

FT_TEST(test_basic_strnstr_errno_resets_on_success)
{
    const char *haystack = "prefix";

    FT_ASSERT_EQ(haystack + 3, ft_strnstr(haystack, "fix", 6));
    return (1);
}

FT_TEST(test_basic_strnstr_restart_within_limit)
{
    const char *haystack = "abcabcd";

    FT_ASSERT_EQ(haystack + 3, ft_strnstr(haystack, "abcd", 7));
    return (1);
}

FT_TEST(test_basic_strnstr_same_buffer_with_length_limit)
{
    char buffer[] = "abcdef";

    FT_ASSERT_EQ(ft_nullptr, ft_strnstr(buffer, buffer, 3));
    return (1);
}

FT_TEST(test_basic_strnstr_match_at_limit_boundary)
{
    const char *haystack = "abcd";

    FT_ASSERT_EQ(haystack + 3, ft_strnstr(haystack, "d", 4));
    return (1);
}

FT_TEST(test_basic_strnstr_limit_blocks_partial_match)
{
    const char *haystack = "foobar";

    FT_ASSERT_EQ(ft_nullptr, ft_strnstr(haystack, "bar", 5));
    return (1);
}

FT_TEST(test_basic_strnstr_bounded_prefix_handoff)
{
    char buffer[96];
    ft_size_t index = 0U;

    while (index < sizeof(buffer))
    {
        buffer[index] = 'a';
        ++index;
    }
    buffer[63U] = 'b';
    buffer[64U] = 'c';
    buffer[65U] = 'd';
    FT_ASSERT_EQ(buffer + 63U, ft_strnstr(buffer, "bcd", sizeof(buffer)));
    FT_ASSERT_EQ(buffer + 64U, ft_strnstr(buffer, "cd", sizeof(buffer)));
    FT_ASSERT_EQ(ft_nullptr, ft_strnstr(buffer, "bcd", 65U));
    buffer[61U] = '\0';
    FT_ASSERT_EQ(ft_nullptr, ft_strnstr(buffer, "bcd", sizeof(buffer)));
    return (1);
}
