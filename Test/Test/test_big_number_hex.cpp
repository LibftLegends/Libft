#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_big_number.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/CMA/CMA.hpp"
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

FT_TEST(test_big_number_hex_serialization_round_trip)
{
    ft_big_number decimal_value;
    INIT_BIG_NUMBER(decimal_value);

    decimal_value.assign("3735928559");

    ft_string *hex_string = big_number_to_hex_string(decimal_value);
    FT_ASSERT(hex_string != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hex_string->get_error());
    FT_ASSERT(*hex_string == "DEADBEEF");

    ft_big_number *parsed_value = big_number_from_hex_string(hex_string->c_str());

    FT_ASSERT(parsed_value != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(parsed_value->c_str(), decimal_value.c_str()));
    FT_ASSERT(!parsed_value->is_negative());
    FT_ASSERT(parsed_value->is_positive());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, parsed_value->destroy());
    delete parsed_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hex_string->destroy());
    delete hex_string;
    return (1);
}

FT_TEST(test_big_number_hex_serialization_prefix_support)
{
    ft_big_number negative_decimal;
    INIT_BIG_NUMBER(negative_decimal);

    negative_decimal.assign("-4660");

    ft_string *negative_hex = big_number_to_hex_string(negative_decimal);
    FT_ASSERT(negative_hex != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, negative_hex->get_error());
    FT_ASSERT(*negative_hex == "-1234");

    ft_big_number *round_trip_negative = big_number_from_hex_string(negative_hex->c_str());

    FT_ASSERT(round_trip_negative != ft_nullptr);
    FT_ASSERT(round_trip_negative->is_negative());
    FT_ASSERT_EQ(0, std::strcmp(round_trip_negative->c_str(), "4660"));

    ft_big_number *prefixed_negative = big_number_from_hex_string("-0x1234");

    FT_ASSERT(prefixed_negative != ft_nullptr);
    FT_ASSERT(prefixed_negative->is_negative());
    FT_ASSERT_EQ(0, std::strcmp(prefixed_negative->c_str(), "4660"));

    ft_big_number *prefixed_positive = big_number_from_hex_string("0XCAFEBABE");

    FT_ASSERT(prefixed_positive != ft_nullptr);
    FT_ASSERT(!prefixed_positive->is_negative());
    FT_ASSERT(prefixed_positive->is_positive());
    FT_ASSERT_EQ(0, std::strcmp(prefixed_positive->c_str(), "3405691582"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, prefixed_positive->destroy());
    delete prefixed_positive;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, prefixed_negative->destroy());
    delete prefixed_negative;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, round_trip_negative->destroy());
    delete round_trip_negative;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, negative_hex->destroy());
    delete negative_hex;
    return (1);
}

FT_TEST(test_big_number_base_conversion_retries_after_failed_workspace)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);
    number.assign("42");
    char hexadecimal[600];
    const char pattern[] = "F000ABCD0005123456789";
    uint32_t index = 0U;
    while (index < sizeof(hexadecimal) - 1U)
    {
        hexadecimal[index] = pattern[index % (sizeof(pattern) - 1U)];
        index += 1U;
    }
    hexadecimal[index] = '\0';

    cma_set_alloc_limit(100U);
    const int32_t parse_error = number.assign_base(hexadecimal, 16);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, parse_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, ft_big_number::get_error());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "42"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.assign_base(hexadecimal, 16));

    cma_set_alloc_limit(500U);
    ft_string *failed_format = number.to_string_base(16);
    const int32_t format_error = ft_big_number::get_error();
    cma_set_alloc_limit(0U);
    delete failed_format;
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, format_error);
    ft_string *formatted = big_number_to_hex_string(number);
    FT_ASSERT(formatted != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, formatted->get_error());
    FT_ASSERT_EQ(0, std::strcmp(hexadecimal, formatted->c_str()));
    delete formatted;
    return (1);
}

FT_TEST(test_big_number_grouped_conversion_pads_intermediate_groups)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);
    const char ternary[] = "-1021020021200100002120102100012001200212212010200122012020001";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.assign_base(ternary, 3));
    FT_ASSERT_EQ(FT_TRUE, number.is_negative());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "53746895514668707227182233015"));
    ft_string *formatted = number.to_string_base(3);
    FT_ASSERT(formatted != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(formatted->c_str(), ternary));
    delete formatted;
    return (1);
}

FT_TEST(test_big_number_base_conversion_matches_unsigned_limits_in_every_base)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);
    constexpr char symbols[] = "0123456789ABCDEF";
    uint32_t base = 2U;
    while (base <= 16U)
    {
        char representation[65];
        ft_size_t first = sizeof(representation) - 1U;
        representation[first] = '\0';
        uint64_t value = UINT64_MAX;
        while (value != 0U)
        {
            first -= 1U;
            representation[first] = symbols[value % base];
            value /= base;
        }
        const char *expected = representation + first;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, number.assign_base(expected, static_cast<int32_t>(base)));
        FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "18446744073709551615"));
        ft_string *formatted = number.to_string_base(static_cast<int32_t>(base));
        FT_ASSERT(formatted != ft_nullptr);
        const int32_t error_code = formatted->get_error();
        const int32_t comparison = std::strcmp(formatted->c_str(), expected);
        delete formatted;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
        FT_ASSERT_EQ(0, comparison);
        base += 1U;
    }
    return (1);
}

#undef INIT_BIG_NUMBER
