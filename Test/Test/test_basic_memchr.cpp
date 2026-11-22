#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_basic_memchr_large_unaligned_slice)
{
    uint8_t bytes[65] = {};

    bytes[32U] = 0xFEU;
    bytes[63U] = 0xFEU;
    FT_ASSERT_EQ(bytes + 32U, ft_memchr(bytes + 1U, 0x1FE, 63U));
    FT_ASSERT_EQ(bytes + 63U, ft_memchr(bytes + 33U, 0x1FE, 31U));
    FT_ASSERT_EQ(ft_nullptr, ft_memchr(bytes + 33U, 0x1FE, 30U));
    return (1);
}

FT_TEST(test_basic_memchr_found)
{
    char buffer[4];
    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = 'c';
    buffer[3] = 'd';
    FT_ASSERT_EQ(buffer + 2, ft_memchr(buffer, 'c', 4));
    return (1);
}

FT_TEST(test_basic_memchr_not_found)
{
    char buffer[3];
    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = 'c';
    FT_ASSERT_EQ(ft_nullptr, ft_memchr(buffer, 'x', 3));
    return (1);
}

FT_TEST(test_basic_memchr_null_char)
{
    char buffer[4];
    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = '\0';
    buffer[3] = 'c';
    FT_ASSERT_EQ(buffer + 2, ft_memchr(buffer, '\0', 4));
    return (1);
}

FT_TEST(test_basic_memchr_zero_length)
{
    char buffer[1];
    buffer[0] = 'a';
    FT_ASSERT_EQ(ft_nullptr, ft_memchr(buffer, 'a', 0));
    return (1);
}

FT_TEST(test_basic_memchr_nullptr_zero)
{
    FT_ASSERT_EQ(ft_nullptr, ft_memchr(ft_nullptr, 'a', 0));
    return (1);
}

FT_TEST(test_basic_memchr_zero_length_clears_errno)
{
    FT_ASSERT_EQ(ft_nullptr, ft_memchr(ft_nullptr, 'a', 0));
    return (1);
}

FT_TEST(test_basic_memchr_null_sets_errno)
{
    FT_ASSERT_EQ(ft_nullptr, ft_memchr(ft_nullptr, 'a', 1));
    return (1);
}

FT_TEST(test_basic_memchr_limit_stops_search)
{
    char buffer[4];

    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = 'c';
    buffer[3] = 'd';
    FT_ASSERT_EQ(ft_nullptr, ft_memchr(buffer, 'c', 2));
    return (1);
}

FT_TEST(test_basic_memchr_signed_byte)
{
    char buffer[3];

    buffer[0] = 'a';
    buffer[1] = static_cast<char>(0xF2);
    buffer[2] = '\0';
    FT_ASSERT_EQ(buffer + 1, ft_memchr(buffer, 0xF2, 3));
    return (1);
}

FT_TEST(test_basic_memchr_miss_clears_errno)
{
    char buffer[4];

    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = 'c';
    buffer[3] = '\0';
    FT_ASSERT_EQ(ft_nullptr, ft_memchr(buffer, 'z', 4));
    return (1);
}

FT_TEST(test_basic_memchr_recovers_after_null_failure)
{
    char buffer[3];

    buffer[0] = 'q';
    buffer[1] = 'w';
    buffer[2] = '\0';
    FT_ASSERT_EQ(ft_nullptr, ft_memchr(ft_nullptr, 'q', 2));
    FT_ASSERT_EQ(buffer, ft_memchr(buffer, 'q', 2));
    return (1);
}

FT_TEST(test_basic_memchr_matches_last_byte)
{
    char buffer[5];

    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = 'c';
    buffer[3] = 'd';
    buffer[4] = 'e';
    FT_ASSERT_EQ(buffer + 4, ft_memchr(buffer, 'e', 5));
    return (1);
}
