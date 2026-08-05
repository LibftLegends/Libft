#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_is_power_of_two)
{
    FT_ASSERT_EQ(FT_FALSE, ft_is_power_of_two(0));
    FT_ASSERT_EQ(FT_TRUE, ft_is_power_of_two(1));
    FT_ASSERT_EQ(FT_TRUE, ft_is_power_of_two(1024));
    FT_ASSERT_EQ(FT_FALSE, ft_is_power_of_two(1023));
    return (1);
}
