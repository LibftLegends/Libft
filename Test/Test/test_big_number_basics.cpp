#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/CPP_class/class_big_number.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
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

FT_TEST(test_big_number_default_state)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);

    FT_ASSERT(number.empty());
    FT_ASSERT_EQ(0, number.size());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "0"));
    FT_ASSERT(!number.is_negative());
    FT_ASSERT(!number.is_positive());
    return (1);
}

FT_TEST(test_big_number_decimal_assign_reports_invalid_input_and_failed_growth)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);
    number.assign("123");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_big_number::get_error());
    number.assign("4x");
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_big_number::get_error());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "123"));
    cma_set_alloc_limit(16U);
    number.assign("12345678901234567890123456789012345678901234567890");
    const int32_t error_code = ft_big_number::get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "123"));
    cma_set_alloc_limit(16U);
    number.assign("000000000000000000000000000000000000000000000042");
    const int32_t padded_error = ft_big_number::get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, padded_error);
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "42"));
    return (1);
}

FT_TEST(test_big_number_copy_preserves_values_on_failure_and_reuses_storage)
{
    constexpr char source_digits[] = "12345678901234567890123456789012345678901234567890";
    ft_big_number source;
    ft_big_number destination;
    ft_big_number fresh;
    ft_big_number empty_value;
    INIT_BIG_NUMBER(source);
    INIT_BIG_NUMBER(destination);
    INIT_BIG_NUMBER(empty_value);
    source.assign(source_digits);
    destination.assign("-7");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());

    cma_set_alloc_limit(16U);
    destination = source;
    const int32_t assignment_error = ft_big_number::get_error();
    const ft_bool assignment_preserved_mutex = destination.is_thread_safe();
    const ft_bool assignment_preserved_value = destination.is_negative()
        && std::strcmp(destination.c_str(), "7") == 0;
    const int32_t reinitialize_error = destination.initialize(source);
    const int32_t initialize_error = fresh.initialize(source);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, assignment_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, reinitialize_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, initialize_error);
    FT_ASSERT_EQ(FT_TRUE, assignment_preserved_mutex);
    FT_ASSERT_EQ(FT_TRUE, assignment_preserved_value);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, destination._mutex);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fresh.initialize(source));
    destination = empty_value;
    FT_ASSERT_EQ(FT_TRUE, destination.empty());
    cma_set_alloc_limit(16U);
    destination = source;
    const int32_t reuse_error = ft_big_number::get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reuse_error);
    source.assign("11");
    FT_ASSERT_EQ(0, std::strcmp(destination.c_str(), source_digits));
    FT_ASSERT_EQ(0, std::strcmp(fresh.c_str(), source_digits));
    FT_ASSERT_EQ(FT_FALSE, destination.is_negative());
    return (1);
}

FT_TEST(test_big_number_assign_trim)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);

    number.assign("0001234500");
    FT_ASSERT_EQ(7, number.size());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "1234500"));
    FT_ASSERT(!number.is_negative());
    FT_ASSERT(number.is_positive());
    return (1);
}

FT_TEST(test_big_number_assign_negative)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);

    number.assign("-001234");
    FT_ASSERT_EQ(4, number.size());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "1234"));
    FT_ASSERT(number.is_negative());
    FT_ASSERT(!number.is_positive());

    number.assign("-0");
    FT_ASSERT_EQ(1, number.size());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "0"));
    FT_ASSERT(!number.is_negative());
    FT_ASSERT(!number.is_positive());
    return (1);
}

FT_TEST(test_big_number_assign_base_binary)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.assign_base("101010", 2));
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "42"));
    FT_ASSERT(!number.is_negative());
    FT_ASSERT(number.is_positive());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.assign_base("-1111111", 2));
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "127"));
    FT_ASSERT(number.is_negative());
    FT_ASSERT(!number.is_positive());
    return (1);
}

FT_TEST(test_big_number_assign_base_octal_hex)
{
    ft_big_number octal_number;
    ft_big_number hex_number;
    ft_big_number invalid_number;

    INIT_BIG_NUMBER(octal_number);
    INIT_BIG_NUMBER(hex_number);
    INIT_BIG_NUMBER(invalid_number);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, octal_number.assign_base("755", 8));
    FT_ASSERT_EQ(0, std::strcmp(octal_number.c_str(), "493"));
    FT_ASSERT(!octal_number.is_negative());
    FT_ASSERT(octal_number.is_positive());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, hex_number.assign_base("1a3f", 16));
    FT_ASSERT_EQ(0, std::strcmp(hex_number.c_str(), "6719"));
    FT_ASSERT(!hex_number.is_negative());
    FT_ASSERT(hex_number.is_positive());

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, invalid_number.assign_base("19", 8));
    return (1);
}

FT_TEST(test_big_number_append_helpers)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);

    number.append_digit('4');
    number.append_digit('2');
    number.append_unsigned(12345);
    FT_ASSERT_EQ(7, number.size());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "4212345"));
    FT_ASSERT(!number.is_negative());
    FT_ASSERT(number.is_positive());
    return (1);
}

FT_TEST(test_big_number_assign_invalid_digit)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);

    number.assign("12a3");
    FT_ASSERT_EQ(0, number.size());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "0"));
    return (1);
}

FT_TEST(test_big_number_append_invalid_digit)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);

    number.append_digit('x');
    return (1);
}

FT_TEST(test_big_number_append_is_atomic_and_supports_borrowed_digits)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);
    number.assign("123456789012345");
    cma_set_alloc_limit(16U);
    number.append_digit('6');
    const int32_t digit_error = ft_big_number::get_error();
    number.append("a67b8");
    const int32_t text_error = ft_big_number::get_error();
    number.append_unsigned(1234U);
    const int32_t unsigned_error = ft_big_number::get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, digit_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, text_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, unsigned_error);
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "123456789012345"));

    cma_set_alloc_limit(18U);
    number.append_digit('6');
    const int32_t exact_capacity_error = ft_big_number::get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, exact_capacity_error);
    number.append(number.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_big_number::get_error());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "12345678901234561234567890123456"));
    return (1);
}

FT_TEST(test_big_number_append_filters_text_and_normalizes_leading_zeros)
{
    ft_big_number number;
    INIT_BIG_NUMBER(number);
    number.append("a0b000c");
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "0"));
    number.append("a0001b2c3");
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "123"));
    number.append_unsigned(UINT64_MAX);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_big_number::get_error());
    FT_ASSERT_EQ(0, std::strcmp(number.c_str(), "12318446744073709551615"));
    return (1);
}

FT_TEST(test_big_number_to_string_base_conversions)
{
    ft_big_number decimal_value;
    ft_big_number negative_value;

    INIT_BIG_NUMBER(decimal_value);
    INIT_BIG_NUMBER(negative_value);

    decimal_value.assign("255");
    negative_value.assign("-240");

    ft_string *hex_output = decimal_value.to_string_base(16);
    FT_ASSERT(hex_output != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hex_output->get_error());
    FT_ASSERT(*hex_output == "FF");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hex_output->destroy());
    delete hex_output;

    ft_string *octal_output = decimal_value.to_string_base(8);
    FT_ASSERT(octal_output != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, octal_output->get_error());
    FT_ASSERT(*octal_output == "377");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, octal_output->destroy());
    delete octal_output;

    ft_string *binary_output = negative_value.to_string_base(2);
    FT_ASSERT(binary_output != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, binary_output->get_error());
    FT_ASSERT(*binary_output == "-11110000");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, binary_output->destroy());
    delete binary_output;

    ft_string *invalid_output = decimal_value.to_string_base(1);
    FT_ASSERT(invalid_output != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, invalid_output->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_output->destroy());
    delete invalid_output;
    return (1);
}

#undef INIT_BIG_NUMBER
FT_TEST(test_big_number_decimal_format_preserves_sign_zero_and_allocation_errors)
{
    ft_big_number number;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.initialize());
    number.assign("-0009223372036854775808123");
    ft_string *decimal = number.to_string_base(10);
    FT_ASSERT(decimal != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, decimal->get_error());
    FT_ASSERT_EQ(0, ft_strcmp(decimal->c_str(), "-9223372036854775808123"));
    delete decimal;
    number.assign("-000");
    ft_string *zero = number.to_string_base(10);
    FT_ASSERT(zero != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(zero->c_str(), "0"));
    delete zero;
    cma_set_alloc_limit(1U);
    ft_string *failure = number.to_string_base(10);
    const int32_t error = ft_big_number::get_error();
    cma_set_alloc_limit(0U);
    if (failure != ft_nullptr)
        delete failure;
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error);
    return (1);
}
