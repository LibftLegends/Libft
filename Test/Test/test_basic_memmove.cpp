#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>

#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_basic_memmove_overlap_forward)
{
    char buffer[8];

    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = 'c';
    buffer[3] = 'd';
    buffer[4] = 'e';
    buffer[5] = 'f';
    buffer[6] = 'g';
    buffer[7] = '\0';
    FT_ASSERT_EQ(buffer + 2, ft_memmove(buffer + 2, buffer, 5));
    FT_ASSERT_EQ('a', buffer[0]);
    FT_ASSERT_EQ('a', buffer[2]);
    FT_ASSERT_EQ('e', buffer[6]);
    FT_ASSERT_EQ('\0', buffer[7]);
    return (1);
}

FT_TEST(test_basic_memmove_overlap_backward)
{
    char buffer[8];

    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = 'c';
    buffer[3] = 'd';
    buffer[4] = 'e';
    buffer[5] = 'f';
    buffer[6] = 'g';
    buffer[7] = '\0';
    FT_ASSERT_EQ(buffer, ft_memmove(buffer, buffer + 2, 5));
    FT_ASSERT_EQ('c', buffer[0]);
    FT_ASSERT_EQ('g', buffer[4]);
    FT_ASSERT_EQ('\0', buffer[7]);
    return (1);
}

FT_TEST(test_basic_memmove_zero_length)
{
    char source[4];
    char destination[4];

    source[0] = 'x';
    source[1] = 'y';
    source[2] = 'z';
    source[3] = '\0';
    destination[0] = 'a';
    destination[1] = 'b';
    destination[2] = 'c';
    destination[3] = '\0';
    FT_ASSERT_EQ(destination, ft_memmove(destination, source, 0));
    FT_ASSERT_EQ('a', destination[0]);
    FT_ASSERT_EQ('c', destination[2]);
    return (1);
}

FT_TEST(test_basic_memmove_null_zero_length_clears_errno)
{
    FT_ASSERT_EQ(ft_nullptr, ft_memmove(ft_nullptr, ft_nullptr, 0));
    return (1);
}

FT_TEST(test_basic_memmove_null)
{
    char source[1];
    source[0] = 'a';
    FT_ASSERT_EQ(ft_nullptr, ft_memmove(ft_nullptr, source, 1));
    FT_ASSERT_EQ(ft_nullptr, ft_memmove(source, ft_nullptr, 1));
    return (1);
}

FT_TEST(test_basic_memmove_null_both_pointers)
{
    FT_ASSERT_EQ(ft_nullptr, ft_memmove(ft_nullptr, ft_nullptr, 1));
    return (1);
}

FT_TEST(test_basic_memmove_same_pointer)
{
    char buffer[4];

    buffer[0] = 'h';
    buffer[1] = 'i';
    buffer[2] = 'j';
    buffer[3] = 'k';
    FT_ASSERT_EQ(buffer, ft_memmove(buffer, buffer, 4));
    FT_ASSERT_EQ('h', buffer[0]);
    FT_ASSERT_EQ('k', buffer[3]);
    return (1);
}

FT_TEST(test_basic_memmove_errno_recovers_after_failure)
{
    char source[5];
    char destination[5];

    source[0] = 'a';
    source[1] = 'b';
    source[2] = 'c';
    source[3] = 'd';
    source[4] = '\0';
    destination[0] = 'x';
    destination[1] = 'x';
    destination[2] = 'x';
    destination[3] = 'x';
    destination[4] = '\0';
    FT_ASSERT_EQ(ft_nullptr, ft_memmove(ft_nullptr, source, 3));
    FT_ASSERT_EQ(destination, ft_memmove(destination, source, 4));
    FT_ASSERT_EQ('a', destination[0]);
    FT_ASSERT_EQ('b', destination[1]);
    FT_ASSERT_EQ('c', destination[2]);
    FT_ASSERT_EQ('d', destination[3]);
    FT_ASSERT_EQ('\0', destination[4]);
    return (1);
}

FT_TEST(test_basic_memmove_single_byte_overlap)
{
    char buffer[5];

    buffer[0] = 'x';
    buffer[1] = 'y';
    buffer[2] = 'z';
    buffer[3] = 'w';
    buffer[4] = '\0';
    FT_ASSERT_EQ(buffer + 1, ft_memmove(buffer + 1, buffer, 3));
    FT_ASSERT_EQ('x', buffer[1]);
    FT_ASSERT_EQ('y', buffer[2]);
    FT_ASSERT_EQ('z', buffer[3]);
    FT_ASSERT_EQ('\0', buffer[4]);
    return (1);
}

FT_TEST(test_basic_memmove_forward_overlap_across_copy_blocks)
{
    uint8_t actual[20017];
    uint8_t expected[20017];
    ft_size_t index;

    index = 0U;
    while (index < sizeof(actual))
    {
        actual[index] = static_cast<uint8_t>(index % 251U);
        expected[index] = actual[index];
        ++index;
    }
    (void)std::memmove(expected, expected + 11U, sizeof(expected) - 11U);
    FT_ASSERT_EQ(actual + 0U, ft_memmove(actual, actual + 11U, sizeof(actual) - 11U));
    FT_ASSERT_EQ(0, std::memcmp(expected, actual, sizeof(actual)));
    return (1);
}

template <ft_size_t Size>
static int32_t check_fixed_memmove()
{
    constexpr ft_size_t OFFSET_COUNT = 36;
    uint8_t actual[80];
    uint8_t expected[80];
    ft_size_t source = 0;
    while (source < OFFSET_COUNT)
    {
        ft_size_t destination = 0;
        while (destination < OFFSET_COUNT)
        {
            ft_size_t index = 0;
            while (index < sizeof(actual))
            {
                actual[index] = static_cast<uint8_t>(index);
                expected[index] = actual[index];
                ++index;
            }
            std::memmove(expected + destination, expected + source, Size);
            FT_ASSERT_EQ(actual + destination,
                ft_memmove_fixed<Size>(actual + destination, actual + source));
            FT_ASSERT_EQ(0, std::memcmp(expected, actual, sizeof(actual)));
            ++destination;
        }
        ++source;
    }
    return (1);
}

FT_TEST(test_basic_fixed_memmove_matches_standard_for_overlaps)
{
    uint8_t byte = 0;
    FT_ASSERT_EQ(&byte, ft_memmove_fixed<0>(&byte, ft_nullptr));
    FT_ASSERT_EQ(ft_nullptr, ft_memmove_fixed<1>(&byte, ft_nullptr));
    FT_ASSERT_EQ(ft_nullptr, ft_memmove_fixed<1>(ft_nullptr, &byte));
    FT_ASSERT_EQ(1, check_fixed_memmove<1>());
    FT_ASSERT_EQ(1, check_fixed_memmove<7>());
    FT_ASSERT_EQ(1, check_fixed_memmove<16>());
    FT_ASSERT_EQ(1, check_fixed_memmove<32>());
    FT_ASSERT_EQ(1, check_fixed_memmove<33>());
    return (1);
}
