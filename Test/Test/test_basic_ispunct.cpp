#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_ispunct)
{
    FT_ASSERT_NEQ(0, ft_ispunct('!'));
    FT_ASSERT_NEQ(0, ft_ispunct('_'));
    FT_ASSERT_EQ(0, ft_ispunct('A'));
    FT_ASSERT_EQ(0, ft_ispunct(' '));
    return (1);
}
