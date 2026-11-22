#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>

#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_basic_memcpy_basic)
{
    char source[6];
    char destination[6];

    source[0] = 'h';
    source[1] = 'e';
    source[2] = 'l';
    source[3] = 'l';
    source[4] = 'o';
    source[5] = '\0';
    FT_ASSERT_EQ(destination, ft_memcpy(destination, source, 6));
    FT_ASSERT_EQ(0, ft_strcmp(destination, source));
    return (1);
}

FT_TEST(test_basic_memcpy_zero_length)
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
    FT_ASSERT_EQ(destination, ft_memcpy(destination, source, 0));
    FT_ASSERT_EQ(0, ft_strcmp(destination, "abc"));
    return (1);
}

FT_TEST(test_basic_memcpy_zero_length_null)
{
    FT_ASSERT_EQ(ft_nullptr, ft_memcpy(ft_nullptr, ft_nullptr, 0));
    return (1);
}

FT_TEST(test_basic_memcpy_null_argument)
{
    char source[2];

    source[0] = 'q';
    source[1] = '\0';
    FT_ASSERT_EQ(ft_nullptr, ft_memcpy(ft_nullptr, source, 1));
    FT_ASSERT_EQ(ft_nullptr, ft_memcpy(source, ft_nullptr, 1));
    return (1);
}

FT_TEST(test_basic_memcpy_same_pointer)
{
    char buffer[4];

    buffer[0] = 'a';
    buffer[1] = 'b';
    buffer[2] = 'c';
    buffer[3] = 'd';
    FT_ASSERT_EQ(buffer, ft_memcpy(buffer, buffer, 4));
    FT_ASSERT_EQ('a', buffer[0]);
    FT_ASSERT_EQ('b', buffer[1]);
    FT_ASSERT_EQ('c', buffer[2]);
    FT_ASSERT_EQ('d', buffer[3]);
    return (1);
}

FT_TEST(test_basic_memcpy_overlap_behavior)
{
    char buffer[6];
    char source[4];
    char destination[4];

    buffer[0] = '1';
    buffer[1] = '2';
    buffer[2] = '3';
    buffer[3] = '4';
    buffer[4] = '5';
    buffer[5] = '\0';
    FT_ASSERT_EQ(buffer + 1, ft_memcpy(buffer + 1, buffer, 4));
    FT_ASSERT_EQ('1', buffer[1]);
    FT_ASSERT_EQ('1', buffer[2]);
    FT_ASSERT_EQ('1', buffer[3]);
    FT_ASSERT_EQ('1', buffer[4]);
    source[0] = 'd';
    source[1] = 'a';
    source[2] = 't';
    source[3] = 'a';
    destination[0] = 'x';
    destination[1] = 'x';
    destination[2] = 'x';
    destination[3] = '\0';
    FT_ASSERT_EQ(destination, ft_memcpy(destination, source, 3));
    FT_ASSERT_EQ('d', destination[0]);
    FT_ASSERT_EQ('a', destination[1]);
    FT_ASSERT_EQ('t', destination[2]);
    FT_ASSERT_EQ('\0', destination[3]);
    return (1);
}

FT_TEST(test_basic_memcpy_partial_copy_preserves_tail)
{
    char source[6];
    char destination[6];

    source[0] = 's';
    source[1] = 'o';
    source[2] = 'u';
    source[3] = 'r';
    source[4] = 'c';
    source[5] = 'e';
    destination[0] = 'd';
    destination[1] = 'e';
    destination[2] = 's';
    destination[3] = 't';
    destination[4] = '!';
    destination[5] = '\0';
    FT_ASSERT_EQ(destination, ft_memcpy(destination, source, 3));
    FT_ASSERT_EQ('s', destination[0]);
    FT_ASSERT_EQ('o', destination[1]);
    FT_ASSERT_EQ('u', destination[2]);
    FT_ASSERT_EQ('t', destination[3]);
    FT_ASSERT_EQ('!', destination[4]);
    FT_ASSERT_EQ('\0', destination[5]);
    return (1);
}

static int32_t check_forward_copy(uint32_t source, uint32_t destination,
    uint32_t size)
{
    uint8_t actual[64];
    uint8_t expected[64];
    uint32_t offset = 0U;
    while (offset < sizeof(actual))
    {
        actual[offset] = static_cast<uint8_t>(offset);
        expected[offset] = actual[offset];
        ++offset;
    }
    offset = 0U;
    while (offset < size)
    {
        expected[destination + offset] = expected[source + offset];
        ++offset;
    }
    FT_ASSERT_EQ(actual + destination,
        ft_memcpy(actual + destination, actual + source, size));
    FT_ASSERT_EQ(0, std::memcmp(actual, expected, sizeof(actual)));
    return (1);
}

FT_TEST(test_basic_memcpy_matches_forward_copy_across_overlaps)
{
    uint32_t source = 0U;
    while (source < 32U)
    {
        uint32_t destination = 0U;
        while (destination < 32U)
        {
            uint32_t size = 0U;
            while (size <= 32U)
            {
                FT_ASSERT_EQ(1, check_forward_copy(source, destination, size));
                ++size;
            }
            ++destination;
        }
        ++source;
    }
    return (1);
}

FT_TEST(test_basic_memcpy_fixed_matches_runtime_copy)
{
    uint8_t actual[40];
    uint8_t expected[40];
    FT_ASSERT_EQ(actual, ft_memcpy_fixed<0>(actual, ft_nullptr));
    FT_ASSERT_EQ(ft_nullptr, ft_memcpy_fixed<16>(actual, ft_nullptr));
    FT_ASSERT_EQ(ft_nullptr, ft_memcpy_fixed<16>(ft_nullptr, actual));
    uint32_t source = 0;
    while (source <= 24U)
    {
        uint32_t destination = 0;
        while (destination <= 24U)
        {
            uint32_t index = 0;
            while (index < sizeof(actual))
            {
                actual[index] = static_cast<uint8_t>(index);
                expected[index] = actual[index];
                ++index;
            }
            ft_memcpy(expected + destination, expected + source, 16U);
            FT_ASSERT_EQ(actual + destination,
                ft_memcpy_fixed<16>(actual + destination, actual + source));
            FT_ASSERT_EQ(0, std::memcmp(actual, expected, sizeof(actual)));
            ++destination;
        }
        ++source;
    }
    return (1);
}
