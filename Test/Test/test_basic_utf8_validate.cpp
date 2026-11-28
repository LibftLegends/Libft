#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/utf8.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_utf8_validate)
{
    const char invalid_sequence[2] = {
        static_cast<char>(0xC0), static_cast<char>(0xAF)};
    const char valid_sequence[] = "A" "\xF0\x9F\x98\x80";

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        ft_utf8_validate(valid_sequence, sizeof(valid_sequence) - 1));
    FT_ASSERT_NEQ(FT_ERR_SUCCESS,
        ft_utf8_validate(invalid_sequence, sizeof(invalid_sequence)));
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, ft_utf8_validate(ft_nullptr, 1));
    return (1);
}

FT_TEST(test_basic_utf8_validate_ascii_and_multibyte_boundaries)
{
    const char mixed[] = "ASCII\0" "\xC3\xA9" " tail";
    const char counted[] = "ASCII " "\xC3\xA9" " tail";
    const char truncated[] = "ASCII " "\xC3";
    ft_size_t count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_validate(mixed, sizeof(mixed) - 1U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, ft_utf8_validate(mixed, 7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_utf8_count(counted, &count));
    FT_ASSERT_EQ(12U, count);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, ft_utf8_count(truncated, &count));
    FT_ASSERT_EQ(12U, count);
    return (1);
}
