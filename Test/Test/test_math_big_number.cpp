#include "../test_internal.hpp"
#include "../../Modules/Math/math.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#define INIT_BIG_NUMBER(number) FT_ASSERT_EQ(FT_ERR_SUCCESS, number.initialize())

static void destroy_big_number_result(ft_big_number *number)
{
    if (number == ft_nullptr)
        return ;
    (void)number->destroy();
    delete number;
    return ;
}

FT_TEST(test_math_big_gcd_large_operands)
{
    ft_big_number first_number;
    ft_big_number second_number;

    INIT_BIG_NUMBER(first_number);
    INIT_BIG_NUMBER(second_number);

    first_number.assign("123456789012345678901234567890");
    second_number.assign("987654321098765432109876543210");

    ft_big_number *gcd_value;

    gcd_value = math_big_gcd(first_number, second_number);
    FT_ASSERT(gcd_value != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(gcd_value->c_str(), "9000000000900000000090"));

    ft_big_number negative_first_number;
    ft_big_number negative_second_number;

    INIT_BIG_NUMBER(negative_first_number);
    INIT_BIG_NUMBER(negative_second_number);

    negative_first_number.assign("-123456789012345678901234567890");
    negative_second_number.assign("-987654321098765432109876543210");

    ft_big_number *negative_gcd_value;

    negative_gcd_value = math_big_gcd(negative_first_number,
        negative_second_number);
    FT_ASSERT(negative_gcd_value != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(negative_gcd_value->c_str(),
        "9000000000900000000090"));
    destroy_big_number_result(gcd_value);
    destroy_big_number_result(negative_gcd_value);
    return (1);
}

FT_TEST(test_math_big_lcm_large_operands)
{
    ft_big_number first_number;
    ft_big_number second_number;

    INIT_BIG_NUMBER(first_number);
    INIT_BIG_NUMBER(second_number);

    first_number.assign("123456789012345678901234567890");
    second_number.assign("987654321098765432109876543210");

    ft_big_number *lcm_value;

    lcm_value = math_big_lcm(first_number, second_number);
    FT_ASSERT(lcm_value != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(lcm_value->c_str(),
        "13548070124980948012498094801236261410"));

    ft_big_number negative_second_number;
    INIT_BIG_NUMBER(negative_second_number);

    negative_second_number.assign("-987654321098765432109876543210");

    ft_big_number *mixed_lcm_value;

    mixed_lcm_value = math_big_lcm(first_number, negative_second_number);
    FT_ASSERT(mixed_lcm_value != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(mixed_lcm_value->c_str(),
        "13548070124980948012498094801236261410"));

    ft_big_number zero_number;
    INIT_BIG_NUMBER(zero_number);

    ft_big_number *zero_lcm_value;

    zero_lcm_value = math_big_lcm(first_number, zero_number);
    FT_ASSERT(zero_lcm_value != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(zero_lcm_value->c_str(), "0"));
    destroy_big_number_result(lcm_value);
    destroy_big_number_result(mixed_lcm_value);
    destroy_big_number_result(zero_lcm_value);
    return (1);
}

FT_TEST(test_math_big_gcd_and_lcm_normalize_signed_results)
{
    ft_big_number first;
    ft_big_number second;
    ft_big_number zero;
    INIT_BIG_NUMBER(first);
    INIT_BIG_NUMBER(second);
    INIT_BIG_NUMBER(zero);
    first.assign("-3647175416613920351978753101033344378685538538950467392342133666030");
    second.assign("5901253787014381058843742121224782001416869458843457462882945365313");
    ft_big_number *gcd = math_big_gcd(first, second);
    ft_big_number *lcm = math_big_lcm(first, second);
    ft_big_number *zero_gcd = math_big_gcd(zero, first);
    FT_ASSERT(gcd != ft_nullptr && lcm != ft_nullptr && zero_gcd != ft_nullptr);
    FT_ASSERT_EQ(FT_FALSE, gcd->is_negative());
    FT_ASSERT_EQ(FT_FALSE, lcm->is_negative());
    FT_ASSERT_EQ(FT_FALSE, zero_gcd->is_negative());
    FT_ASSERT_EQ(0, std::strcmp(gcd->c_str(), "928409183745670123450005432109876543219999111"));
    FT_ASSERT_EQ(0, std::strcmp(lcm->c_str(),
        "23182566605345719782496645043740923659288683912476847898713444982039038628515401016323490"));
    FT_ASSERT_EQ(0, std::strcmp(zero_gcd->c_str(), first.c_str()));
    destroy_big_number_result(gcd);
    destroy_big_number_result(lcm);
    destroy_big_number_result(zero_gcd);
    return (1);
}

FT_TEST(test_math_big_gcd_and_lcm_retry_after_failed_operand_copy)
{
    ft_big_number first;
    ft_big_number second;
    INIT_BIG_NUMBER(first);
    INIT_BIG_NUMBER(second);
    first.assign("3");
    char large_digits[161];
    std::memset(large_digits, '9', sizeof(large_digits) - 1U);
    large_digits[160] = '\0';
    second.assign(large_digits);
    cma_set_alloc_limit(128U);
    ft_big_number *failed_gcd = math_big_gcd(first, second);
    ft_big_number *failed_lcm = math_big_lcm(first, second);
    cma_set_alloc_limit(0U);
    FT_ASSERT(failed_gcd != ft_nullptr && failed_lcm != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(failed_gcd->c_str(), "0"));
    FT_ASSERT_EQ(0, std::strcmp(failed_lcm->c_str(), "0"));
    destroy_big_number_result(failed_gcd);
    destroy_big_number_result(failed_lcm);
    ft_big_number *gcd = math_big_gcd(first, second);
    ft_big_number *lcm = math_big_lcm(first, second);
    FT_ASSERT(gcd != ft_nullptr && lcm != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(gcd->c_str(), "3"));
    FT_ASSERT_EQ(0, std::strcmp(lcm->c_str(), second.c_str()));
    destroy_big_number_result(gcd);
    destroy_big_number_result(lcm);
    return (1);
}

#undef INIT_BIG_NUMBER
