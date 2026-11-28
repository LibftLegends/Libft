#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>
#include <random>

static const uint8_t *reference_search(const uint8_t *buffer, ft_size_t length,
    const uint8_t *pattern, ft_size_t pattern_length)
{
    if (pattern_length > length)
        return (ft_nullptr);
    ft_size_t index = 0U;
    while (index <= length - pattern_length)
    {
        if (std::memcmp(buffer + index, pattern, pattern_length) == 0)
            return (buffer + index);
        ++index;
    }
    return (ft_nullptr);
}

FT_TEST(test_basic_memmem)
{
    const unsigned char haystack[6] = {1, 2, 3, 4, 5, 6};
    const unsigned char needle[2] = {3, 4};

    FT_ASSERT_EQ(haystack + 2, ft_memmem(haystack, 6, needle, 2));
    FT_ASSERT_EQ(ft_nullptr, ft_memmem(haystack, 6, "x", 1));
    FT_ASSERT_EQ(haystack, ft_memmem(haystack, 6, ft_nullptr, 0));
    return (1);
}

FT_TEST(test_basic_memmem_finds_first_complete_binary_match)
{
    const unsigned char haystack[] = {0x7FU, 0U, 0xFFU, 0U, 0xFFU, 0U};
    const unsigned char needle[] = {0U, 0xFFU, 0U};

    FT_ASSERT_EQ(haystack + 1U,
        ft_memmem(haystack, sizeof(haystack), needle, sizeof(needle)));
    FT_ASSERT_EQ(haystack + 3U,
        ft_memmem(haystack + 2U, 4U, needle, sizeof(needle)));
    FT_ASSERT_EQ(haystack + 2U,
        ft_memmem(haystack, sizeof(haystack), needle + 1U, 1U));
    FT_ASSERT_EQ(ft_nullptr,
        ft_memmem(haystack, 2U, needle, sizeof(needle)));
    FT_ASSERT_EQ(ft_nullptr,
        ft_memmem(haystack, sizeof(haystack), needle, 1U + sizeof(haystack)));
    return (1);
}

FT_TEST(test_basic_memmem_matches_reference_for_seeded_binary_inputs)
{
    std::mt19937 random(1234567U);
    uint8_t buffer[256];
    uint8_t pattern[64];
    uint32_t sample = 0U;
    while (sample < 200000U)
    {
        const ft_size_t length = random() % (sizeof(buffer) + 1U);
        const ft_size_t pattern_length = random() % (sizeof(pattern) + 1U);
        const auto alphabet_size = 1U + random() % 256U;
        ft_size_t index = 0U;
        while (index < sizeof(buffer))
        {
            buffer[index] = static_cast<uint8_t>(random() % alphabet_size);
            ++index;
        }
        index = 0U;
        while (index < sizeof(pattern))
        {
            pattern[index] = static_cast<uint8_t>(random() % alphabet_size);
            ++index;
        }
        FT_ASSERT_EQ(reference_search(buffer, length, pattern, pattern_length),
            ft_memmem(buffer, length, pattern, pattern_length));
        ++sample;
    }
    return (1);
}
