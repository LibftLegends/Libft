#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
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
