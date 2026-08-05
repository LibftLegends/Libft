#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_parse_uint32)
{
    char *end_pointer;
    uint32_t value;

    end_pointer = ft_nullptr;
    value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        ft_parse_uint32(" 4294967295x", &end_pointer, &value));
    FT_ASSERT_EQ(4294967295U, value);
    FT_ASSERT_EQ('x', *end_pointer);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        ft_parse_uint32("4294967296", ft_nullptr, &value));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        ft_parse_uint32("-1", ft_nullptr, &value));
    return (1);
}
