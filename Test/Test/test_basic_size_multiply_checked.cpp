#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_size_multiply_checked)
{
    ft_size_t result;

    result = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_size_multiply_checked(6, 7, &result));
    FT_ASSERT_EQ(static_cast<ft_size_t>(42), result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        ft_size_multiply_checked(0, FT_SYSTEM_SIZE_MAX, &result));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), result);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        ft_size_multiply_checked(FT_SYSTEM_SIZE_MAX, 2, &result));
    return (1);
}
