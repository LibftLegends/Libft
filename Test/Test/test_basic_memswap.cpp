#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_memswap)
{
    unsigned char left[3];
    unsigned char right[3];

    left[0] = 1;
    left[1] = 2;
    left[2] = 3;
    right[0] = 4;
    right[1] = 5;
    right[2] = 6;
    FT_ASSERT_EQ(left, ft_memswap(left, right, 3));
    FT_ASSERT_EQ(4, left[0]);
    FT_ASSERT_EQ(6, left[2]);
    FT_ASSERT_EQ(1, right[0]);
    FT_ASSERT_EQ(3, right[2]);
    return (1);
}
