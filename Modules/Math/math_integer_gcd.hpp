#ifndef MATH_INTEGER_GCD_HPP
# define MATH_INTEGER_GCD_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

template <typename Unsigned, typename Signed>
inline constexpr Unsigned math_integer_magnitude(Signed value)
{
    const Unsigned encoded_value = static_cast<Unsigned>(value);
    if (value < 0)
        return (Unsigned{0U} - encoded_value);
    return (encoded_value);
}

template <typename Unsigned>
inline constexpr Unsigned math_unsigned_gcd(Unsigned first_number,
    Unsigned second_number)
{
    while (second_number != 0U)
    {
        const Unsigned remainder = first_number % second_number;
        first_number = second_number;
        second_number = remainder;
    }
    return (first_number);
}

#endif
