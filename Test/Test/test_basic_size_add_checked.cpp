#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_size_add_checked)
{
    ft_size_t result;

    result = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_size_add_checked(4, 5, &result));
    FT_ASSERT_EQ(static_cast<ft_size_t>(9), result);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        ft_size_add_checked(FT_SYSTEM_SIZE_MAX, 1, &result));
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, ft_size_add_checked(1, 1, ft_nullptr));
    return (1);
}
