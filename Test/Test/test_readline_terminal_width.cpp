#include "../test_internal.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_readline_terminal_width_matches_current_platform_width_or_falls_back)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, rl_get_terminal_width(ft_nullptr));
    int32_t expected_width = 0;
    if (cmp_readline_terminal_width(&expected_width) != FT_ERR_SUCCESS || expected_width <= 0)
        expected_width = 80;
    int32_t actual_width = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_get_terminal_width(&actual_width));
    FT_ASSERT_EQ(expected_width, actual_width);
    return (1);
}
