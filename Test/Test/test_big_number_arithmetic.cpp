#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/CPP_class/class_big_number.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Template/move.hpp"
#include <cstring>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#define INIT_BIG_NUMBER(number) FT_ASSERT_EQ(FT_ERR_SUCCESS, number.initialize())

FT_TEST(test_big_number_addition_large_values)
{
    ft_big_number left_number;
    ft_big_number right_number;

    INIT_BIG_NUMBER(left_number);
    INIT_BIG_NUMBER(right_number);

    left_number.assign("12345678901234567890");
    right_number.assign("98765432109876543210");
    ft_big_number sum_number;
    sum_number = left_number + right_number;
    FT_ASSERT_EQ(0, std::strcmp(sum_number.c_str(), "111111111011111111100"));
    FT_ASSERT(!sum_number.is_negative());
    FT_ASSERT(sum_number.is_positive());
    return (1);
}

FT_TEST(test_big_number_signed_addition)
{
    ft_big_number negative_five_hundred;
    ft_big_number negative_two_hundred;
    ft_big_number positive_two_hundred;
    ft_big_number positive_five_hundred;

    INIT_BIG_NUMBER(negative_five_hundred);
    INIT_BIG_NUMBER(negative_two_hundred);
    INIT_BIG_NUMBER(positive_two_hundred);
    INIT_BIG_NUMBER(positive_five_hundred);

    negative_five_hundred.assign("-500");
    negative_two_hundred.assign("-200");
    positive_two_hundred.assign("200");
    positive_five_hundred.assign("500");

    ft_big_number sum_neg_pos;
    sum_neg_pos = negative_five_hundred + positive_two_hundred;
    FT_ASSERT_EQ(0, std::strcmp(sum_neg_pos.c_str(), "300"));
    FT_ASSERT(sum_neg_pos.is_negative());
    FT_ASSERT(!sum_neg_pos.is_positive());

    ft_big_number sum_pos_neg;
    sum_pos_neg = positive_five_hundred + negative_two_hundred;
    FT_ASSERT_EQ(0, std::strcmp(sum_pos_neg.c_str(), "300"));
    FT_ASSERT(!sum_pos_neg.is_negative());
    FT_ASSERT(sum_pos_neg.is_positive());

    ft_big_number sum_neg_neg;
    sum_neg_neg = negative_five_hundred + negative_two_hundred;
    FT_ASSERT_EQ(0, std::strcmp(sum_neg_neg.c_str(), "700"));
    FT_ASSERT(sum_neg_neg.is_negative());
    FT_ASSERT(!sum_neg_neg.is_positive());

    ft_big_number sum_cancel;
    sum_cancel = positive_two_hundred + negative_two_hundred;
    FT_ASSERT_EQ(0, std::strcmp(sum_cancel.c_str(), "0"));
    FT_ASSERT(!sum_cancel.is_negative());
    FT_ASSERT(!sum_cancel.is_positive());
    return (1);
}

FT_TEST(test_big_number_subtraction_basic)
{
    ft_big_number left_number;
    ft_big_number right_number;

    INIT_BIG_NUMBER(left_number);
    INIT_BIG_NUMBER(right_number);

    left_number.assign("1000");
    right_number.assign("1");
    ft_big_number difference_number;
    difference_number = left_number - right_number;
    FT_ASSERT_EQ(0, std::strcmp(difference_number.c_str(), "999"));
    FT_ASSERT(!difference_number.is_negative());
    FT_ASSERT(difference_number.is_positive());
    return (1);
}

FT_TEST(test_big_number_signed_subtraction)
{
    ft_big_number positive_five;
    ft_big_number positive_three;
    ft_big_number negative_five;
    ft_big_number negative_three;
    ft_big_number negative_ten;

    INIT_BIG_NUMBER(positive_five);
    INIT_BIG_NUMBER(positive_three);
    INIT_BIG_NUMBER(negative_five);
    INIT_BIG_NUMBER(negative_three);
    INIT_BIG_NUMBER(negative_ten);

    positive_five.assign("5");
    positive_three.assign("3");
    negative_five.assign("-5");
    negative_three.assign("-3");
    negative_ten.assign("-10");

    ft_big_number diff_one;
    diff_one = positive_five - negative_three;
    FT_ASSERT_EQ(0, std::strcmp(diff_one.c_str(), "8"));
    FT_ASSERT(!diff_one.is_negative());
    FT_ASSERT(diff_one.is_positive());

    ft_big_number diff_two;
    diff_two = negative_five - positive_three;
    FT_ASSERT_EQ(0, std::strcmp(diff_two.c_str(), "8"));
    FT_ASSERT(diff_two.is_negative());
    FT_ASSERT(!diff_two.is_positive());

    ft_big_number diff_three;
    diff_three = negative_ten - negative_three;
    FT_ASSERT_EQ(0, std::strcmp(diff_three.c_str(), "7"));
    FT_ASSERT(diff_three.is_negative());
    FT_ASSERT(!diff_three.is_positive());

    ft_big_number diff_four;
    diff_four = negative_three - negative_ten;
    FT_ASSERT_EQ(0, std::strcmp(diff_four.c_str(), "7"));
    FT_ASSERT(!diff_four.is_negative());
    FT_ASSERT(diff_four.is_positive());
    return (1);
}

FT_TEST(test_big_number_subtraction_negative_result)
{
    ft_big_number small_number;
    ft_big_number large_number;

    INIT_BIG_NUMBER(small_number);
    INIT_BIG_NUMBER(large_number);

    small_number.assign("5");
    large_number.assign("10");
    ft_big_number result_number;
    result_number = small_number - large_number;
    FT_ASSERT(result_number.is_negative());
    FT_ASSERT(!result_number.is_positive());
    FT_ASSERT_EQ(0, std::strcmp(result_number.c_str(), "5"));
    return (1);
}

FT_TEST(test_big_number_signed_sum_reports_allocation_failure_and_can_retry)
{
    ft_big_number left;
    ft_big_number right;
    ft_big_number result;
    INIT_BIG_NUMBER(left);
    INIT_BIG_NUMBER(right);
    INIT_BIG_NUMBER(result);
    left.assign("123456789012345678901234567890");
    right.assign("1");

    cma_set_alloc_limit(16U);
    const ft_big_number_proxy addition = left + right;
    const ft_big_number_proxy subtraction = right - left;
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, addition.get_error());
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, subtraction.get_error());

    result = right - left;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_big_number::get_error());
    FT_ASSERT_EQ(FT_TRUE, result.is_negative());
    FT_ASSERT_EQ(0, std::strcmp(result.c_str(), "123456789012345678901234567889"));
    result = left + right;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_big_number::get_error());
    FT_ASSERT_EQ(FT_FALSE, result.is_negative());
    FT_ASSERT_EQ(0, std::strcmp(result.c_str(), "123456789012345678901234567891"));
    return (1);
}

FT_TEST(test_big_number_multiplication_large_values)
{
    ft_big_number left_number;
    ft_big_number right_number;

    INIT_BIG_NUMBER(left_number);
    INIT_BIG_NUMBER(right_number);

    left_number.assign("99999");
    right_number.assign("88888");
    ft_big_number product_number;
    product_number = left_number * right_number;
    FT_ASSERT_EQ(0, std::strcmp(product_number.c_str(), "8888711112"));
    return (1);
}

FT_TEST(test_big_number_grouped_multiply_handles_sparse_digits_and_allocation_failure)
{
    ft_big_number left;
    ft_big_number right;
    ft_big_number result;
    INIT_BIG_NUMBER(left);
    INIT_BIG_NUMBER(right);
    INIT_BIG_NUMBER(result);
    left.assign("-900000000012000000000045678901235");
    right.assign("100000000000000006700000089123456789");
    cma_set_alloc_limit(16U);
    const ft_big_number_proxy failed_result = left * right;
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_result.get_error());
    result = left * right;
    FT_ASSERT_EQ(FT_TRUE, result.is_negative());
    FT_ASSERT_EQ(0, std::strcmp(result.c_str(),
        "90000000001200006030004648181634611169787530110345561580386521234415"));

    char operand[161];
    std::memset(operand, '0', sizeof(operand) - 1U);
    operand[0] = '1';
    operand[159] = '1';
    operand[160] = '\0';
    left.assign(operand);
    cma_set_alloc_limit(400U);
    const ft_big_number_proxy failed_scratch = left * left;
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_scratch.get_error());

    result = left * left;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_big_number::get_error());
    char expected[320];
    std::memset(expected, '0', sizeof(expected) - 1U);
    expected[0] = '1';
    expected[159] = '2';
    expected[318] = '1';
    expected[319] = '\0';
    FT_ASSERT_EQ(FT_FALSE, result.is_negative());
    FT_ASSERT_EQ(0, std::strcmp(expected, result.c_str()));
    right.assign("-1");
    result = right * left;
    FT_ASSERT_EQ(FT_TRUE, result.is_negative());
    FT_ASSERT_EQ(0, std::strcmp(operand, result.c_str()));
    return (1);
}

FT_TEST(test_big_number_signed_multiplication)
{
    ft_big_number positive_twelve;
    ft_big_number negative_three;
    ft_big_number negative_four;

    INIT_BIG_NUMBER(positive_twelve);
    INIT_BIG_NUMBER(negative_three);
    INIT_BIG_NUMBER(negative_four);

    positive_twelve.assign("12");
    negative_three.assign("-3");
    negative_four.assign("-4");

    ft_big_number product_one;
    product_one = positive_twelve * negative_three;
    FT_ASSERT_EQ(0, std::strcmp(product_one.c_str(), "36"));
    FT_ASSERT(product_one.is_negative());
    FT_ASSERT(!product_one.is_positive());

    ft_big_number product_two;
    product_two = negative_three * negative_four;
    FT_ASSERT_EQ(0, std::strcmp(product_two.c_str(), "12"));
    FT_ASSERT(!product_two.is_negative());
    FT_ASSERT(product_two.is_positive());

    ft_big_number product_three;
    product_three = negative_four * positive_twelve;
    FT_ASSERT_EQ(0, std::strcmp(product_three.c_str(), "48"));
    FT_ASSERT(product_three.is_negative());
    FT_ASSERT(!product_three.is_positive());
    return (1);
}

FT_TEST(test_big_number_division_even)
{
    ft_big_number numerator_number;
    ft_big_number denominator_number;

    INIT_BIG_NUMBER(numerator_number);
    INIT_BIG_NUMBER(denominator_number);

    numerator_number.assign("99999999999999999999");
    denominator_number.assign("3");
    ft_big_number quotient_number;
    quotient_number = numerator_number / denominator_number;
    FT_ASSERT_EQ(0, std::strcmp(quotient_number.c_str(), "33333333333333333333"));
    return (1);
}

FT_TEST(test_big_number_signed_division)
{
    ft_big_number numerator_positive;
    ft_big_number numerator_negative;
    ft_big_number denominator_positive;
    ft_big_number denominator_negative;

    INIT_BIG_NUMBER(numerator_positive);
    INIT_BIG_NUMBER(numerator_negative);
    INIT_BIG_NUMBER(denominator_positive);
    INIT_BIG_NUMBER(denominator_negative);

    numerator_positive.assign("100");
    numerator_negative.assign("-100");
    denominator_positive.assign("5");
    denominator_negative.assign("-5");

    ft_big_number quotient_one;
    quotient_one = numerator_negative / denominator_positive;
    FT_ASSERT_EQ(0, std::strcmp(quotient_one.c_str(), "20"));
    FT_ASSERT(quotient_one.is_negative());
    FT_ASSERT(!quotient_one.is_positive());

    ft_big_number quotient_two;
    quotient_two = numerator_positive / denominator_negative;
    FT_ASSERT_EQ(0, std::strcmp(quotient_two.c_str(), "20"));
    FT_ASSERT(quotient_two.is_negative());
    FT_ASSERT(!quotient_two.is_positive());

    ft_big_number quotient_three;
    quotient_three = numerator_negative / denominator_negative;
    FT_ASSERT_EQ(0, std::strcmp(quotient_three.c_str(), "20"));
    FT_ASSERT(!quotient_three.is_negative());
    FT_ASSERT(quotient_three.is_positive());
    return (1);
}

FT_TEST(test_big_number_division_by_zero_error)
{
    ft_big_number numerator_number;
    ft_big_number zero_number;

    INIT_BIG_NUMBER(numerator_number);
    INIT_BIG_NUMBER(zero_number);

    zero_number.assign("0");
    numerator_number.assign("12345");
    ft_big_number result_number;
    result_number = numerator_number / zero_number;
    return (1);
}

FT_TEST(test_big_number_division_reuses_multiples_for_signed_wide_operands)
{
    ft_big_number numerator;
    ft_big_number divisor;
    ft_big_number result;
    INIT_BIG_NUMBER(numerator);
    INIT_BIG_NUMBER(divisor);
    INIT_BIG_NUMBER(result);
    numerator.assign("-98765432101234567890987654321012345678909876543210123456789098765432101234567890");
    divisor.assign("-1000000000000000123");

    cma_set_alloc_limit(16U);
    const ft_big_number_proxy failed_quotient = numerator / divisor;
    const ft_big_number_proxy failed_remainder = numerator % divisor;
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_quotient.get_error());
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_remainder.get_error());

    result = numerator / divisor;
    FT_ASSERT_EQ(FT_FALSE, result.is_negative());
    FT_ASSERT_EQ(0, std::strcmp(result.c_str(),
        "98765432101234555742839505869161989309650654636285438369758578"));
    result = numerator % divisor;
    FT_ASSERT_EQ(FT_TRUE, result.is_negative());
    FT_ASSERT_EQ(0, std::strcmp(result.c_str(), "502323181754262796"));
    divisor.assign("7");
    result = numerator / divisor;
    FT_ASSERT_EQ(FT_TRUE, result.is_negative());
    FT_ASSERT_EQ(0, std::strcmp(result.c_str(),
        "14109347443033509698712522045858906525558553791887160493827014109347443033509698"));
    result = numerator % divisor;
    FT_ASSERT_EQ(FT_TRUE, result.is_negative());
    FT_ASSERT_EQ(0, std::strcmp(result.c_str(), "4"));
    divisor.assign("0");
    FT_ASSERT_EQ(FT_ERR_DIVIDE_BY_ZERO, (numerator % divisor).get_error());
    return (1);
}

FT_TEST(test_big_number_wide_divisor_table_can_retry_after_allocation_failure)
{
    ft_big_number numerator;
    ft_big_number divisor;
    ft_big_number result;
    INIT_BIG_NUMBER(numerator);
    INIT_BIG_NUMBER(divisor);
    INIT_BIG_NUMBER(result);
    char digits[161];
    std::memset(digits, '0', sizeof(digits) - 1U);
    digits[160] = '\0';
    digits[0] = '1';
    digits[159] = '1';
    divisor.assign(digits);
    digits[0] = '3';
    digits[159] = '9';
    numerator.assign(digits);

    cma_set_alloc_limit(500U);
    const ft_big_number_proxy failed_quotient = numerator / divisor;
    const ft_big_number_proxy failed_remainder = numerator % divisor;
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_quotient.get_error());
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_remainder.get_error());
    result = numerator / divisor;
    FT_ASSERT_EQ(0, std::strcmp(result.c_str(), "3"));
    result = numerator % divisor;
    FT_ASSERT_EQ(0, std::strcmp(result.c_str(), "6"));
    return (1);
}

FT_TEST(test_big_number_modulus_basic)
{
    ft_big_number positive_dividend;
    ft_big_number divisor;

    INIT_BIG_NUMBER(positive_dividend);
    INIT_BIG_NUMBER(divisor);

    positive_dividend.assign("100");
    divisor.assign("7");

    ft_big_number positive_remainder;
    positive_remainder = positive_dividend % divisor;
    FT_ASSERT_EQ(0, std::strcmp(positive_remainder.c_str(), "2"));
    FT_ASSERT(!positive_remainder.is_negative());
    FT_ASSERT(positive_remainder.is_positive());

    ft_big_number negative_dividend;
    INIT_BIG_NUMBER(negative_dividend);

    negative_dividend.assign("-100");
    ft_big_number negative_remainder;
    negative_remainder = negative_dividend % divisor;
    FT_ASSERT_EQ(0, std::strcmp(negative_remainder.c_str(), "2"));
    FT_ASSERT(negative_remainder.is_negative());
    FT_ASSERT(!negative_remainder.is_positive());

    ft_big_number small_dividend;
    ft_big_number large_divisor;

    INIT_BIG_NUMBER(small_dividend);
    INIT_BIG_NUMBER(large_divisor);

    small_dividend.assign("3");
    large_divisor.assign("10");
    ft_big_number small_remainder;
    small_remainder = small_dividend % large_divisor;
    FT_ASSERT_EQ(0, std::strcmp(small_remainder.c_str(), "3"));
    FT_ASSERT(!small_remainder.is_negative());
    FT_ASSERT(small_remainder.is_positive());

    ft_big_number zero_divisor;
    INIT_BIG_NUMBER(zero_divisor);

    zero_divisor.assign("0");
    ft_big_number error_remainder;
    error_remainder = positive_dividend % zero_divisor;
    return (1);
}

FT_TEST(test_big_number_modular_exponentiation)
{
    ft_big_number base_number;
    ft_big_number exponent_number;
    ft_big_number modulus_number;

    INIT_BIG_NUMBER(base_number);
    INIT_BIG_NUMBER(exponent_number);
    INIT_BIG_NUMBER(modulus_number);

    base_number.assign("7");
    exponent_number.assign("4");
    modulus_number.assign("13");
    ft_big_number *power_result = base_number.mod_pow(exponent_number, modulus_number);
    FT_ASSERT(power_result != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(power_result->c_str(), "9"));
    FT_ASSERT(!power_result->is_negative());
    FT_ASSERT(power_result->is_positive());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, power_result->destroy());
    delete power_result;

    ft_big_number zero_exponent;
    INIT_BIG_NUMBER(zero_exponent);

    zero_exponent.assign("0");
    ft_big_number *identity_result = base_number.mod_pow(zero_exponent, modulus_number);
    FT_ASSERT(identity_result != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(identity_result->c_str(), "1"));
    FT_ASSERT(!identity_result->is_negative());
    FT_ASSERT(identity_result->is_positive());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, identity_result->destroy());
    delete identity_result;

    ft_big_number negative_base;
    ft_big_number cube_exponent;
    ft_big_number modulus_five;

    INIT_BIG_NUMBER(negative_base);
    INIT_BIG_NUMBER(cube_exponent);
    INIT_BIG_NUMBER(modulus_five);

    negative_base.assign("-2");
    cube_exponent.assign("3");
    modulus_five.assign("5");
    ft_big_number *wrapped_result = negative_base.mod_pow(cube_exponent, modulus_five);
    FT_ASSERT(wrapped_result != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(wrapped_result->c_str(), "2"));
    FT_ASSERT(!wrapped_result->is_negative());
    FT_ASSERT(wrapped_result->is_positive());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, wrapped_result->destroy());
    delete wrapped_result;

    ft_big_number zero_modulus;
    ft_big_number exponent_one;

    INIT_BIG_NUMBER(zero_modulus);
    INIT_BIG_NUMBER(exponent_one);

    zero_modulus.assign("0");
    exponent_one.assign("1");
    ft_big_number *modulus_error = base_number.mod_pow(exponent_one, zero_modulus);
    FT_ASSERT(modulus_error != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_DIVIDE_BY_ZERO, ft_big_number::get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, modulus_error->destroy());
    delete modulus_error;

    ft_big_number negative_exponent;
    INIT_BIG_NUMBER(negative_exponent);

    negative_exponent.assign("-1");
    ft_big_number *exponent_error
        = base_number.mod_pow(negative_exponent, modulus_number);
    FT_ASSERT(exponent_error != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_big_number::get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, exponent_error->destroy());
    delete exponent_error;
    return (1);
}

FT_TEST(test_big_number_mod_pow_large_exponent_and_allocation_retry)
{
    ft_big_number base;
    ft_big_number exponent;
    ft_big_number modulus;
    INIT_BIG_NUMBER(base);
    INIT_BIG_NUMBER(exponent);
    INIT_BIG_NUMBER(modulus);
    base.assign("-123456789012345678901234567890");
    modulus.assign("987654321098765432109876543211");
    char oversized[161];
    std::memset(oversized, '9', sizeof(oversized) - 1U);
    oversized[160] = '\0';
    exponent.assign(oversized);

    cma_set_alloc_limit(128U);
    ft_big_number *failure = base.mod_pow(exponent, modulus);
    const int32_t failure_error = ft_big_number::get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT(failure != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failure_error);
    FT_ASSERT(failure->empty());
    delete failure;

    exponent.assign("100000000000000000000000000000000000000000000000123");
    ft_big_number *result = base.mod_pow(exponent, modulus);
    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_big_number::get_error());
    FT_ASSERT_EQ(FT_FALSE, result->is_negative());
    FT_ASSERT_EQ(0, std::strcmp(result->c_str(), "895680982089804569934957139271"));
    delete result;
    exponent.assign("0");
    modulus.assign("1");
    result = base.mod_pow(exponent, modulus);
    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_big_number::get_error());
    FT_ASSERT_EQ(0, std::strcmp(result->c_str(), "0"));
    delete result;
    return (1);
}

FT_TEST(test_big_number_comparisons)
{
    ft_big_number first_number;
    ft_big_number second_number;
    ft_big_number third_number;
    ft_big_number zero_number;

    INIT_BIG_NUMBER(first_number);
    INIT_BIG_NUMBER(second_number);
    INIT_BIG_NUMBER(third_number);
    INIT_BIG_NUMBER(zero_number);

    first_number.assign("12345");
    second_number.assign("12345");
    third_number.assign("12346");
    zero_number.assign("0");
    FT_ASSERT(first_number == second_number);
    FT_ASSERT(first_number <= second_number);
    FT_ASSERT(first_number >= second_number);
    FT_ASSERT(first_number != third_number);
    FT_ASSERT(first_number < third_number);
    FT_ASSERT(third_number > first_number);
    FT_ASSERT(zero_number <= first_number);
    FT_ASSERT(first_number >= zero_number);
    return (1);
}

FT_TEST(test_big_number_relations_treat_reduced_negative_zero_as_zero)
{
    ft_big_number reduced;
    ft_big_number zero;
    ft_big_number negative;
    INIT_BIG_NUMBER(reduced);
    INIT_BIG_NUMBER(zero);
    INIT_BIG_NUMBER(negative);
    reduced.assign("-7");
    reduced.reduce_to(0U);
    zero.assign("0");
    negative.assign("-1");
    FT_ASSERT(reduced == zero);
    FT_ASSERT(reduced >= zero);
    FT_ASSERT(reduced <= zero);
    FT_ASSERT(reduced > negative);
    FT_ASSERT(negative < reduced);
    return (1);
}

FT_TEST(test_big_number_signed_comparisons)
{
    ft_big_number negative_large;
    ft_big_number negative_small;
    ft_big_number positive_small;
    ft_big_number positive_large;
    ft_big_number negative_small_copy;

    INIT_BIG_NUMBER(negative_large);
    INIT_BIG_NUMBER(negative_small);
    INIT_BIG_NUMBER(positive_small);
    INIT_BIG_NUMBER(positive_large);
    INIT_BIG_NUMBER(negative_small_copy);

    negative_large.assign("-200");
    negative_small.assign("-50");
    positive_small.assign("50");
    positive_large.assign("200");
    negative_small_copy.assign("-50");

    FT_ASSERT(negative_large.is_negative());
    FT_ASSERT(!negative_large.is_positive());
    FT_ASSERT(positive_large.is_positive());
    FT_ASSERT(!positive_large.is_negative());

    FT_ASSERT(negative_large < negative_small);
    FT_ASSERT(!(negative_large > negative_small));
    FT_ASSERT(negative_small < positive_small);
    FT_ASSERT(!(positive_small < negative_large));
    FT_ASSERT(positive_large > negative_small);
    FT_ASSERT(positive_small >= negative_small);
    FT_ASSERT(negative_small <= positive_small);
    FT_ASSERT(negative_small == negative_small_copy);
    FT_ASSERT(negative_small != positive_small);
    return (1);
}

#undef INIT_BIG_NUMBER
