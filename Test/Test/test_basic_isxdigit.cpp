#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_isxdigit)
{
    FT_ASSERT_NEQ(0, ft_isxdigit('0'));
    FT_ASSERT_NEQ(0, ft_isxdigit('F'));
    FT_ASSERT_NEQ(0, ft_isxdigit('a'));
    FT_ASSERT_EQ(0, ft_isxdigit('g'));
    FT_ASSERT_EQ(0, ft_isxdigit(-1));
    return (1);
}
