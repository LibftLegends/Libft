#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_isblank)
{
    FT_ASSERT_NEQ(0, ft_isblank(' '));
    FT_ASSERT_NEQ(0, ft_isblank('\t'));
    FT_ASSERT_EQ(0, ft_isblank('\n'));
    FT_ASSERT_EQ(0, ft_isblank('A'));
    return (1);
}
