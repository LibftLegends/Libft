#include "math.hpp"
#include <new>

static int32_t math_big_assign_proxy(ft_big_number &destination,
    const ft_big_number_proxy &value)
{
    const int32_t error_code = value.get_error();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    destination = value;
    return (ft_big_number::get_error());
}

static int32_t math_big_store_absolute(ft_big_number &destination,
    const ft_big_number &value, const ft_big_number &zero)
{
    const ft_bool negative = value.is_negative();
    const int32_t error_code = ft_big_number::get_error();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (negative == FT_TRUE)
        return (math_big_assign_proxy(destination, zero - value));
    destination = value;
    return (ft_big_number::get_error());
}

static int32_t math_big_gcd_value(const ft_big_number &first_input,
    const ft_big_number &second_input, const ft_big_number &zero, ft_big_number &result)
{
    ft_big_number remainders[3];
    uint32_t index = 0U;
    while (index < 3U)
    {
        const int32_t error_code = remainders[index].initialize();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    const ft_big_number *first = &first_input;
    const ft_big_number *second = &second_input;
    index = 0U;
    while (FT_TRUE)
    {
        const ft_bool finished = *second == zero;
        const int32_t comparison_error = ft_big_number::get_error();
        if (comparison_error != FT_ERR_SUCCESS)
            return (comparison_error);
        if (finished == FT_TRUE)
            break ;
        const int32_t remainder_error = math_big_assign_proxy(remainders[index], *first % *second);
        if (remainder_error != FT_ERR_SUCCESS)
            return (remainder_error);
        first = second;
        second = &remainders[index];
        index = (index + 1U) % 3U;
    }
    return (math_big_store_absolute(result, *first, zero));
}

static int32_t math_big_lcm_value(const ft_big_number &first,
    const ft_big_number &second, const ft_big_number &zero, ft_big_number &result)
{
    if (first == zero || second == zero)
        return (FT_ERR_SUCCESS);

    ft_big_number gcd;
    int32_t error_code = gcd.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = math_big_gcd_value(first, second, zero, gcd);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);

    ft_big_number quotient;
    error_code = quotient.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = math_big_assign_proxy(quotient, first / gcd);
    if (error_code == FT_ERR_SUCCESS && quotient.is_negative() != second.is_negative())
        error_code = math_big_assign_proxy(quotient, zero - quotient);
    if (error_code == FT_ERR_SUCCESS)
        error_code = math_big_assign_proxy(result, quotient * second);
    return (error_code);
}

static int32_t math_big_calculate(const ft_big_number &first_number,
    const ft_big_number &second_number, ft_bool calculate_lcm, ft_big_number &result)
{
    ft_big_number first;
    ft_big_number second;
    ft_big_number zero;
    int32_t error_code = first.initialize(first_number);
    if (error_code == FT_ERR_SUCCESS)
        error_code = second.initialize(second_number);
    if (error_code == FT_ERR_SUCCESS)
        error_code = zero.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (calculate_lcm == FT_TRUE)
        return (math_big_lcm_value(first, second, zero, result));
    return (math_big_gcd_value(first, second, zero, result));
}

static ft_big_number *math_big_result(const ft_big_number &first,
    const ft_big_number &second, ft_bool calculate_lcm)
{
    ft_big_number *result = new (std::nothrow) ft_big_number();
    if (result == ft_nullptr)
        return (ft_nullptr);
    if (result->initialize() != FT_ERR_SUCCESS)
    {
        delete result;
        return (ft_nullptr);
    }
    if (math_big_calculate(first, second, calculate_lcm, *result) != FT_ERR_SUCCESS)
        result->clear();
    return (result);
}

ft_big_number *math_big_gcd(const ft_big_number &first_number,
    const ft_big_number &second_number)
{
    return (math_big_result(first_number, second_number, FT_FALSE));
}

ft_big_number *math_big_lcm(const ft_big_number &first_number,
    const ft_big_number &second_number)
{
    return (math_big_result(first_number, second_number, FT_TRUE));
}
