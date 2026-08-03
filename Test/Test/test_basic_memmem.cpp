#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_memmem)
{
    const unsigned char haystack[6] = {1, 2, 3, 4, 5, 6};
    const unsigned char needle[2] = {3, 4};

    FT_ASSERT_EQ(haystack + 2, ft_memmem(haystack, 6, needle, 2));
    FT_ASSERT_EQ(ft_nullptr, ft_memmem(haystack, 6, "x", 1));
    FT_ASSERT_EQ(haystack, ft_memmem(haystack, 6, ft_nullptr, 0));
    return (1);
}
