#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_basic_strtol_decimal)
{
    char *end;
    FT_ASSERT_EQ(123, ft_strtol("123abc", &end, 10));
    FT_ASSERT_EQ('a', *end);
    return (1);
}

FT_TEST(test_basic_strtol_exact_negative_limit)
{
    const char input[] = "-9223372036854775808!";
    char *end_pointer = ft_nullptr;

    FT_ASSERT_EQ(FT_LLONG_MIN, ft_strtol(input, &end_pointer, 10));
    FT_ASSERT_EQ(input + 20U, end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_hex_prefix)
{
    char *end;
    FT_ASSERT_EQ(26, ft_strtol("0x1a", &end, 0));
    FT_ASSERT_EQ('\0', *end);
    return (1);
}

FT_TEST(test_basic_strtol_octal_auto)
{
    char *end;
    FT_ASSERT_EQ(9, ft_strtol("011", &end, 0));
    FT_ASSERT_EQ('\0', *end);
    return (1);
}

FT_TEST(test_basic_strtol_negative)
{
    FT_ASSERT_EQ(-42, ft_strtol("-42", ft_nullptr, 10));
    return (1);
}

FT_TEST(test_basic_strtol_base16_skip)
{
    char *end;
    FT_ASSERT_EQ(26, ft_strtol("0x1a", &end, 16));
    FT_ASSERT_EQ('\0', *end);
    return (1);
}

FT_TEST(test_basic_strtol_invalid)
{
    char *end;
    FT_ASSERT_EQ(0, ft_strtol("xyz", &end, 10));
    FT_ASSERT_EQ('x', *end);
    return (1);
}

FT_TEST(test_basic_strtol_sign_only_sets_error_and_input_pointer)
{
    const char *plus_input = "+";
    const char *minus_input = "-";
    char *end_pointer;

    end_pointer = reinterpret_cast<char *>(0x1);
    FT_ASSERT_EQ(0L, ft_strtol(plus_input, &end_pointer, 10));
    FT_ASSERT_EQ(const_cast<char *>(plus_input), end_pointer);

    end_pointer = reinterpret_cast<char *>(0x1);
    FT_ASSERT_EQ(0L, ft_strtol(minus_input, &end_pointer, 10));
    FT_ASSERT_EQ(const_cast<char *>(minus_input), end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_invalid_base)
{
    const char *input_string = "123";
    char *end_pointer;

    end_pointer = reinterpret_cast<char *>(0x1);
    FT_ASSERT_EQ(0, ft_strtol(input_string, &end_pointer, 1));
    FT_ASSERT_EQ(const_cast<char *>(input_string), end_pointer);

    end_pointer = reinterpret_cast<char *>(0x1);
    FT_ASSERT_EQ(0, ft_strtol(input_string, &end_pointer, 37));
    FT_ASSERT_EQ(const_cast<char *>(input_string), end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_maximum_in_range)
{
    char *end;

    FT_ASSERT_EQ(FT_LLONG_MAX, ft_strtol("9223372036854775807", &end, 10));
    FT_ASSERT_EQ('\0', *end);
    return (1);
}

FT_TEST(test_basic_strtol_positive_overflow)
{
    char *end;
    const char *overflow_string = "92233720368547758079";

    FT_ASSERT_EQ(FT_LLONG_MAX, ft_strtol(overflow_string, &end, 10));
    FT_ASSERT_EQ('\0', *end);
    FT_ASSERT_EQ(ft_strlen(overflow_string), end - overflow_string);
    return (1);
}

FT_TEST(test_basic_strtol_negative_overflow)
{
    char *end;
    const char *underflow_string = "-92233720368547758089";

    FT_ASSERT_EQ(FT_LLONG_MIN, ft_strtol(underflow_string, &end, 10));
    FT_ASSERT_EQ('\0', *end);
    FT_ASSERT_EQ(ft_strlen(underflow_string), end - underflow_string);
    return (1);
}

FT_TEST(test_basic_strtol_uppercase_hex_prefix)
{
    char *end_pointer;

    FT_ASSERT_EQ(255, ft_strtol("0Xff", &end_pointer, 0));
    FT_ASSERT_EQ('\0', *end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_base_auto_allows_leading_plus)
{
    char *end_pointer;

    FT_ASSERT_EQ(255, ft_strtol("+0XFF", &end_pointer, 0));
    FT_ASSERT_EQ('\0', *end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_base36_mixed_case)
{
    char *end_pointer;

    FT_ASSERT_EQ(1294L, ft_strtol("Zy", &end_pointer, 36));
    FT_ASSERT_EQ('\0', *end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_skips_leading_whitespace)
{
    char *end_pointer;

    FT_ASSERT_EQ(678L, ft_strtol("\t  678xyz", &end_pointer, 10));
    FT_ASSERT_EQ('x', *end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_null_input)
{
    char *end_pointer = reinterpret_cast<char *>(0x1);

    FT_ASSERT_EQ(0, ft_strtol(ft_nullptr, &end_pointer, 10));
    FT_ASSERT_EQ(ft_nullptr, end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_base_two_rejects_invalid_digit)
{
    char *end_pointer;
    int64_t parsed_value;

    parsed_value = ft_strtol("10102", &end_pointer, 2);
    FT_ASSERT_EQ(static_cast<int64_t>(10), parsed_value);
    FT_ASSERT_EQ('2', *end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_mixed_case_digits_custom_base)
{
    char *end_pointer;
    int64_t parsed_value;

    parsed_value = ft_strtol("aB19", &end_pointer, 20);
    FT_ASSERT_EQ(static_cast<int64_t>(84429), parsed_value);
    FT_ASSERT_EQ('\0', *end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_rejects_at_symbol_in_high_base)
{
    char *end_pointer;
    int64_t parsed_value;

    parsed_value = ft_strtol("@123", &end_pointer, 36);
    FT_ASSERT_EQ(static_cast<int64_t>(0), parsed_value);
    FT_ASSERT_EQ('@', *end_pointer);
    return (1);
}

FT_TEST(test_basic_strtol_stops_before_brace_character)
{
    char *end_pointer;
    int64_t parsed_value;

    parsed_value = ft_strtol("Zz{", &end_pointer, 36);
    FT_ASSERT_EQ(static_cast<int64_t>(1295), parsed_value);
    FT_ASSERT_EQ('{', *end_pointer);
    return (1);
}
