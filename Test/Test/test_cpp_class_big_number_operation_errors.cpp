#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_big_number.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#define INIT_BIG_NUMBER(number) FT_ASSERT_EQ(FT_ERR_SUCCESS, number.initialize())

FT_TEST(test_ft_big_number_assign_base_invalid_input_keeps_value)
{
    ft_big_number number_value;
    INIT_BIG_NUMBER(number_value);

    number_value.assign("42");
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, number_value.assign_base("1G", 16));
    FT_ASSERT_EQ(0, std::strcmp(number_value.c_str(), "42"));
    return (1);
}

FT_TEST(test_ft_big_number_divide_by_zero_returns_zero_result)
{
    ft_big_number numerator;
    ft_big_number denominator;
    ft_big_number quotient;

    INIT_BIG_NUMBER(numerator);
    INIT_BIG_NUMBER(denominator);
    INIT_BIG_NUMBER(quotient);

    numerator.assign("25");
    denominator.assign("0");
    quotient = numerator / denominator;
    FT_ASSERT_EQ(FT_ERR_DIVIDE_BY_ZERO, ft_big_number::get_error());
    FT_ASSERT_EQ(0, std::strcmp(quotient.c_str(), "0"));
    return (1);
}

FT_TEST(test_big_number_temporary_proxy_transfers_digits_without_allocation)
{
    ft_big_number left;
    ft_big_number right;
    ft_big_number destination;
    INIT_BIG_NUMBER(left);
    INIT_BIG_NUMBER(right);
    INIT_BIG_NUMBER(destination);
    left.assign("123456789012345678901234567890");
    right.assign("1");
    ft_big_number_proxy sum = left + right;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sum.get_error());
    cma_set_alloc_limit(16U);
    destination = static_cast<ft_big_number_proxy &&>(sum);
    const int32_t assignment_error = ft_big_number::get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, assignment_error);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, sum.get_error());
    FT_ASSERT_EQ(0, std::strcmp(destination.c_str(), "123456789012345678901234567891"));
    return (1);
}

FT_TEST(test_big_number_proxy_preserves_failed_chain_status_at_assignment)
{
    ft_big_number numerator;
    ft_big_number zero;
    ft_big_number destination;
    INIT_BIG_NUMBER(numerator);
    INIT_BIG_NUMBER(zero);
    INIT_BIG_NUMBER(destination);
    numerator.assign("25");
    destination.assign("7");
    destination = (numerator / zero).operator+(numerator);
    FT_ASSERT_EQ(FT_ERR_DIVIDE_BY_ZERO, ft_big_number::get_error());
    FT_ASSERT_EQ(0, std::strcmp(destination.c_str(), "0"));
    ft_big_number_proxy empty;
    destination = empty;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, ft_big_number::get_error());
    destination = numerator + numerator;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_big_number::get_error());
    FT_ASSERT_EQ(0, std::strcmp(destination.c_str(), "50"));
    return (1);
}

#undef INIT_BIG_NUMBER
