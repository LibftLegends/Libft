#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_basic_parse_int64)
{
    char *end_pointer;
    int64_t value;

    end_pointer = ft_nullptr;
    value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        ft_parse_int64("-9223372036854775808!", &end_pointer, &value));
    FT_ASSERT_EQ(FT_LLONG_MIN, value);
    FT_ASSERT_EQ('!', *end_pointer);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        ft_parse_int64("9223372036854775808", ft_nullptr, &value));
    return (1);
}
