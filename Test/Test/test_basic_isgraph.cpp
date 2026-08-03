#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_isgraph)
{
    FT_ASSERT_EQ(0, ft_isgraph(' '));
    FT_ASSERT_NEQ(0, ft_isgraph('!'));
    FT_ASSERT_NEQ(0, ft_isgraph('~'));
    FT_ASSERT_EQ(0, ft_isgraph(0x7F));
    return (1);
}
