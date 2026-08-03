#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_iscntrl)
{
    FT_ASSERT_NEQ(0, ft_iscntrl(0));
    FT_ASSERT_NEQ(0, ft_iscntrl(0x7F));
    FT_ASSERT_EQ(0, ft_iscntrl('A'));
    return (1);
}
