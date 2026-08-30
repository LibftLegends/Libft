#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int assert_buffer_zeroed(const char *buffer, size_t buffer_size)
{
    size_t index;

    index = 0;
    while (index < buffer_size)
    {
        FT_ASSERT(buffer[index] == '\0');
        index++;
    }
    return (1);
}

FT_TEST(test_strncpy_s_copies_within_bounds)
{
    char destination[16];
    const char *source;

    source = "hello";
    ft_memset(destination, 'X', sizeof(destination));
    FT_ASSERT_EQ(0, ft_strncpy_s(destination, sizeof(destination), source, 10));
    FT_ASSERT_EQ(0, ft_strcmp(source, destination));
    return (1);
}

FT_TEST(test_strncpy_s_detects_truncation)
{
    char destination[8];
    const char *source;

    source = "toolong";
    ft_memset(destination, 'Y', sizeof(destination));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
            ft_strncpy_s(destination, sizeof(destination), source, 4));
    if (!assert_buffer_zeroed(destination, sizeof(destination)))
        return (0);
    return (1);
}

FT_TEST(test_strncat_s_appends_without_truncation)
{
    char buffer[16];

    ft_memset(buffer, 0, sizeof(buffer));
    FT_ASSERT_EQ(0, ft_strcpy_s(buffer, sizeof(buffer), "foo"));
    FT_ASSERT_EQ(0, ft_strncat_s(buffer, sizeof(buffer), "bar", 8));
    FT_ASSERT_EQ(0, ft_strcmp("foobar", buffer));
    return (1);
}

FT_TEST(test_strncat_s_detects_capacity_overflow)
{
    char buffer[8];

    FT_ASSERT_EQ(0, ft_strcpy_s(buffer, sizeof(buffer), "data"));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
            ft_strncat_s(buffer, sizeof(buffer), "more", 4));
    if (!assert_buffer_zeroed(buffer, sizeof(buffer)))
        return (0);
    return (1);
}

FT_TEST(test_strncat_s_respects_append_length_limit)
{
    char buffer[16];

    FT_ASSERT_EQ(0, ft_strcpy_s(buffer, sizeof(buffer), "base"));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
            ft_strncat_s(buffer, sizeof(buffer), "suffix", 2));
    if (!assert_buffer_zeroed(buffer, sizeof(buffer)))
        return (0);
    return (1);
}

FT_TEST(test_strncpy_s_null_source_zeroes_destination)
{
    char destination[5];

    destination[0] = 'h';
    destination[1] = 'e';
    destination[2] = 'l';
    destination[3] = 'l';
    destination[4] = 'o';
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, ft_strncpy_s(destination,
            sizeof(destination), ft_nullptr, 3));
    if (!assert_buffer_zeroed(destination, sizeof(destination)))
        return (0);
    return (1);
}

FT_TEST(test_strncpy_s_zero_destination_size_preserves_buffer)
{
    char destination[4];

    destination[0] = 'a';
    destination[1] = 'b';
    destination[2] = 'c';
    destination[3] = '\0';
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_strncpy_s(destination, 0, "x", 1));
    FT_ASSERT_EQ('a', destination[0]);
    FT_ASSERT_EQ('b', destination[1]);
    FT_ASSERT_EQ('c', destination[2]);
    FT_ASSERT_EQ('\0', destination[3]);
    return (1);
}

FT_TEST(test_strncpy_s_empty_source_writes_terminator)
{
    char destination[3];

    destination[0] = 'x';
    destination[1] = 'y';
    destination[2] = 'z';
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strncpy_s(destination, sizeof(destination),
            "", 0));
    FT_ASSERT_EQ('\0', destination[0]);
    FT_ASSERT_EQ('y', destination[1]);
    FT_ASSERT_EQ('z', destination[2]);
    return (1);
}

FT_TEST(test_strncpy_s_exact_fit_succeeds)
{
    char destination[4];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strncpy_s(destination, sizeof(destination),
            "cat", 3));
    FT_ASSERT_EQ(0, ft_strcmp(destination, "cat"));
    return (1);
}

FT_TEST(test_strncpy_s_destination_too_small_for_terminator_zeroes_buffer)
{
    char destination[3];

    destination[0] = 'x';
    destination[1] = 'y';
    destination[2] = 'z';
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_strncpy_s(destination,
            sizeof(destination), "abc", 3));
    if (!assert_buffer_zeroed(destination, sizeof(destination)))
        return (0);
    return (1);
}

FT_TEST(test_strncpy_s_maximum_equal_source_length_succeeds)
{
    char destination[6];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strncpy_s(destination, sizeof(destination),
            "hello", 5));
    FT_ASSERT_EQ(0, ft_strcmp(destination, "hello"));
    return (1);
}

FT_TEST(test_strncat_s_empty_source_succeeds)
{
    char destination[8];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strcpy_s(destination, sizeof(destination), "abc"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strncat_s(destination, sizeof(destination), "", 0));
    FT_ASSERT_EQ(0, ft_strcmp(destination, "abc"));
    return (1);
}

FT_TEST(test_strncat_s_zero_destination_size_returns_invalid_argument)
{
    char destination[8];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strcpy_s(destination, sizeof(destination), "base"));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_strncat_s(destination,
            0, "x", 1));
    FT_ASSERT_EQ(0, ft_strcmp(destination, "base"));
    return (1);
}

FT_TEST(test_strncat_s_maximum_equal_source_length_succeeds)
{
    char destination[8];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strcpy_s(destination, sizeof(destination), "a"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strncat_s(destination, sizeof(destination), "bc", 2));
    FT_ASSERT_EQ(0, ft_strcmp(destination, "abc"));
    return (1);
}

FT_TEST(test_strncat_s_invalid_existing_length_preserves_buffer)
{
    char destination[5];

    destination[0] = 'a';
    destination[1] = 'b';
    destination[2] = 'c';
    destination[3] = 'd';
    destination[4] = '\0';
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_strncat_s(destination,
            4U, "z", 1));
    FT_ASSERT_EQ('a', destination[0]);
    FT_ASSERT_EQ('b', destination[1]);
    FT_ASSERT_EQ('c', destination[2]);
    FT_ASSERT_EQ('d', destination[3]);
    return (1);
}

FT_TEST(test_strncat_s_null_source_zeroes_destination)
{
    char destination[5];

    destination[0] = 't';
    destination[1] = 'e';
    destination[2] = 's';
    destination[3] = 't';
    destination[4] = '\0';
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, ft_strncat_s(destination,
            sizeof(destination), ft_nullptr, 1));
    if (!assert_buffer_zeroed(destination, sizeof(destination)))
        return (0);
    return (1);
}

FT_TEST(test_strncat_s_exact_fit_succeeds)
{
    char destination[6];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strcpy_s(destination, sizeof(destination), "ab"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strncat_s(destination, sizeof(destination), "cde", 3));
    FT_ASSERT_EQ(0, ft_strcmp(destination, "abcde"));
    return (1);
}

FT_TEST(test_strncat_s_destination_too_small_for_terminator_zeroes_buffer)
{
    char destination[5];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_strcpy_s(destination, sizeof(destination), "abc"));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_strncat_s(destination,
            sizeof(destination), "de", 2));
    if (!assert_buffer_zeroed(destination, sizeof(destination)))
        return (0);
    return (1);
}
