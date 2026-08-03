#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_align_up_checked)
{
    ft_size_t result;

    result = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_align_up_checked(17, 8, &result));
    FT_ASSERT_EQ(static_cast<ft_size_t>(24), result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_align_up_checked(16, 8, &result));
    FT_ASSERT_EQ(static_cast<ft_size_t>(16), result);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        ft_align_up_checked(4, 3, &result));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        ft_align_up_checked(FT_SYSTEM_SIZE_MAX, 2, &result));
    return (1);
}
