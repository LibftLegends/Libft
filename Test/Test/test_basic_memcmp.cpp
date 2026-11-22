#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_basic_memcmp_exact_difference_after_unaligned_prefix)
{
    uint8_t left[33] = {};
    uint8_t right[33] = {};

    left[24U] = 0xE2U;
    right[24U] = 0x12U;
    left[25U] = 0x04U;
    right[25U] = 0xF0U;
    FT_ASSERT_EQ(208, ft_memcmp(left + 1U, right + 1U, 32U));
    FT_ASSERT_EQ(-208, ft_memcmp(right + 1U, left + 1U, 32U));
    FT_ASSERT_EQ(0, ft_memcmp(left + 1U, right + 1U, 23U));
    return (1);
}

FT_TEST(test_basic_memcmp_equal)
{
    char a[3];
    char b[3];
    a[0] = 'a';
    a[1] = 'b';
    a[2] = 'c';
    b[0] = 'a';
    b[1] = 'b';
    b[2] = 'c';
    FT_ASSERT_EQ(0, ft_memcmp(a, b, 3));
    return (1);
}

FT_TEST(test_basic_memcmp_less)
{
    char a[3];
    char b[3];
    a[0] = 'a';
    a[1] = 'b';
    a[2] = 'c';
    b[0] = 'a';
    b[1] = 'b';
    b[2] = 'd';
    FT_ASSERT(ft_memcmp(a, b, 3) < 0);
    return (1);
}

FT_TEST(test_basic_memcmp_greater)
{
    char a[3];
    char b[3];
    a[0] = 'a';
    a[1] = 'b';
    a[2] = 'e';
    b[0] = 'a';
    b[1] = 'b';
    b[2] = 'c';
    FT_ASSERT(ft_memcmp(a, b, 3) > 0);
    return (1);
}

FT_TEST(test_basic_memcmp_zero_length)
{
    char a[1];
    char b[1];
    a[0] = 'x';
    b[0] = 'y';
    FT_ASSERT_EQ(0, ft_memcmp(a, b, 0));
    return (1);
}

FT_TEST(test_basic_memcmp_zero_length_clears_errno)
{
    FT_ASSERT_EQ(0, ft_memcmp(ft_nullptr, ft_nullptr, 0));
    return (1);
}

FT_TEST(test_basic_memcmp_high_bit)
{
    char a[1];
    char b[1];
    a[0] = static_cast<char>(0x80);
    b[0] = 0;
    FT_ASSERT(ft_memcmp(a, b, 1) > 0);
    return (1);
}

FT_TEST(test_basic_memcmp_limit_hides_late_difference)
{
    char first[4];
    char second[4];

    first[0] = 'a';
    first[1] = 'b';
    first[2] = 'c';
    first[3] = '\0';
    second[0] = 'a';
    second[1] = 'b';
    second[2] = 'x';
    second[3] = '\0';
    FT_ASSERT_EQ(0, ft_memcmp(first, second, 2));
    return (1);
}

FT_TEST(test_basic_memcmp_null_pointer_sets_errno)
{
    char buffer[2];

    buffer[0] = 'x';
    buffer[1] = '\0';
    FT_ASSERT_EQ(0, ft_memcmp(buffer, ft_nullptr, 1));
    FT_ASSERT_EQ(0, ft_memcmp(ft_nullptr, buffer, 1));
    return (1);
}

FT_TEST(test_basic_memcmp_errno_recovers_after_null_pointer)
{
    char buffer[3];

    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = '\0';
    FT_ASSERT_EQ(0, ft_memcmp(buffer, ft_nullptr, 1));
    FT_ASSERT_EQ(0, ft_memcmp(buffer, buffer, 1));
    return (1);
}
