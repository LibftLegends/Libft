#include "class_big_number.hpp"
#include "class_string.hpp"
#include <new>

ft_string *big_number_to_hex_string(const ft_big_number& number) noexcept
{
    if (number._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(number._initialised_state,
            "big_number_to_hex_string", "called with uninitialised source object");
        return (ft_string::from_error(ft_big_number::set_error(FT_ERR_INVALID_STATE)));
    }
    return (number.format_base(16));
}

ft_big_number *big_number_from_hex_string(const char* hex_digits) noexcept
{
    ft_big_number *result;
    int32_t result_initialization_error;

    result = new (std::nothrow) ft_big_number();
    if (result == ft_nullptr)
    {
        ft_big_number::set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    result_initialization_error = result->initialize();
    if (result_initialization_error != FT_ERR_SUCCESS)
    {
        delete result;
        ft_big_number::set_error(result_initialization_error);
        return (ft_nullptr);
    }

    if (!hex_digits)
    {
        (void)result->assign_base_digits(ft_nullptr, 16U, FT_FALSE);
        return (result);
    }
    const char* digits = hex_digits;
    ft_bool has_negative_sign = FT_FALSE;

    if (digits[0] == '-')
    {
        has_negative_sign = FT_TRUE;
        digits++;
    }
    else if (digits[0] == '+')
        digits++;
    if (digits[0] == '0')
    {
        if (digits[1] == 'x' || digits[1] == 'X')
            digits += 2;
    }
    if (digits[0] == '-')
    {
        has_negative_sign = has_negative_sign == FT_FALSE;
        digits += 1;
    }
    (void)result->assign_base_digits(digits, 16U, has_negative_sign);
    return (result);
}
