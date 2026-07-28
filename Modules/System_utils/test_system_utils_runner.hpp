#ifndef TEST_RUNNER_HPP
#define TEST_RUNNER_HPP

#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "../Basic/limits.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"

typedef int32_t (*t_test_func)(void);
#ifndef TEST_MODULE
#define TEST_MODULE "Libft"
#endif

int32_t ft_register_test(t_test_func func, const char *description, const char *module, const char *name);
// TEMP_DIAGNOSTIC_REMOVE: Delete after the Windows heap corruption is identified.
void ft_test_runner_write_startup_diagnostic(const char *message);
void ft_test_fail(const char *expression, const char *file, int32_t line);
void ft_test_fail_values(const char *expression, const char *file, int32_t line,
    const char *expected_value, const char *actual_value);
int32_t ft_run_registered_tests(void);

#ifdef LIBFT_TEST_BUILD
int32_t ft_test_runner_reserve_capacity(int32_t required_capacity);
int32_t ft_test_runner_registered_count(void);
int32_t ft_test_runner_registered_capacity(void);
void ft_test_runner_set_current_test_name(const char *name);
const char *ft_test_runner_current_test_name(void);
#endif

template <typename ValueType, typename = void>
struct ft_test_is_streamable
{
    static const bool value = false;
};

template <typename ValueType, bool IsEnum>
struct ft_test_numeric_type_helper
{
    typedef typename std::decay<ValueType>::type type;
};

template <typename ValueType>
struct ft_test_numeric_type_helper<ValueType, true>
{
    typedef typename std::underlying_type<
        typename std::decay<ValueType>::type>::type type;
};

template <typename ValueType>
static const char *ft_test_value_to_string(char *buffer, ft_size_t buffer_size,
    const ValueType &value)
{
    if constexpr (std::is_same<typename std::decay<ValueType>::type, bool>::value)
    {
        if (value)
            return ("true");
        return ("false");
    }
    else if constexpr (std::is_same<typename std::decay<ValueType>::type, ft_nullptr_t>::value)
        return ("ft_nullptr");
    else if constexpr (std::is_enum<typename std::decay<ValueType>::type>::value)
    {
        std::snprintf(buffer, buffer_size, FT_INT64_DECIMAL_FORMAT,
            static_cast<int64_t>(static_cast<typename ft_test_numeric_type_helper<
                ValueType, true>::type>(value)));
        return (buffer);
    }
    else if constexpr (std::is_same<typename std::decay<ValueType>::type, char>::value
        || std::is_same<typename std::decay<ValueType>::type, signed char>::value
        || std::is_same<typename std::decay<ValueType>::type, unsigned char>::value)
    {
        std::snprintf(buffer, buffer_size, "%d",
            static_cast<int32_t>(value));
        return (buffer);
    }
    else if constexpr (std::is_integral<typename std::decay<ValueType>::type>::value)
    {
        if constexpr (std::is_signed<typename std::decay<ValueType>::type>::value)
            std::snprintf(buffer, buffer_size, FT_INT64_DECIMAL_FORMAT,
                static_cast<int64_t>(value));
        else
            std::snprintf(buffer, buffer_size, FT_UINT64_DECIMAL_FORMAT,
                static_cast<uint64_t>(value));
        return (buffer);
    }
    else if constexpr (std::is_floating_point<typename std::decay<ValueType>::type>::value)
    {
        std::snprintf(buffer, buffer_size, "%.17g",
            static_cast<double>(value));
        return (buffer);
    }
    else if constexpr (std::is_pointer<typename std::decay<ValueType>::type>::value)
    {
        std::snprintf(buffer, buffer_size, "%p",
            static_cast<const void *>(value));
        return (buffer);
    }
    else
        return ("<unprintable>");
}

static inline const char *ft_test_value_to_string(char *buffer,
    ft_size_t buffer_size, const char *value)
{
    (void)buffer;
    (void)buffer_size;
    if (value == NULL)
        return ("(null)");
    return (value);
}

static inline const char *ft_test_value_to_string(char *buffer,
    ft_size_t buffer_size, char *value)
{
    (void)buffer;
    (void)buffer_size;
    if (value == NULL)
        return ("(null)");
    return (value);
}

template <typename LeftType, typename RightType>
static bool ft_test_values_equal(const LeftType &left_value,
    const RightType &right_value)
{
    using left_decay_type = typename std::decay<LeftType>::type;
    using right_decay_type = typename std::decay<RightType>::type;
    using left_compare_type = typename ft_test_numeric_type_helper<
        LeftType, std::is_enum<left_decay_type>::value>::type;
    using right_compare_type = typename ft_test_numeric_type_helper<
        RightType, std::is_enum<right_decay_type>::value>::type;

    if constexpr ((std::is_arithmetic<left_decay_type>::value
            || std::is_enum<left_decay_type>::value)
        && (std::is_arithmetic<right_decay_type>::value
            || std::is_enum<right_decay_type>::value))
    {
        left_compare_type left_numeric_value;
        right_compare_type right_numeric_value;

        left_numeric_value = static_cast<left_compare_type>(left_value);
        right_numeric_value = static_cast<right_compare_type>(right_value);
        if constexpr (std::is_signed<left_compare_type>::value
            == std::is_signed<right_compare_type>::value)
        {
            if constexpr (std::is_floating_point<left_compare_type>::value
                || std::is_floating_point<right_compare_type>::value)
            {
                using common_t = std::common_type_t<
                    left_compare_type, right_compare_type>;
                const common_t lf = static_cast<common_t>(left_numeric_value);
                const common_t rf = static_cast<common_t>(right_numeric_value);
                return (std::memcmp(&lf, &rf, sizeof(common_t)) == 0);
            }
            else
                return (left_numeric_value == right_numeric_value);
        }
        else if constexpr (std::is_signed<left_compare_type>::value)
        {
            if (left_numeric_value < 0)
                return (false);
            using unsigned_left_type =
                typename std::make_unsigned<left_compare_type>::type;

            return (static_cast<unsigned_left_type>(left_numeric_value)
                == right_numeric_value);
        }
        else
        {
            if (right_numeric_value < 0)
                return (false);
            using unsigned_right_type =
                typename std::make_unsigned<right_compare_type>::type;

            return (left_numeric_value
                == static_cast<unsigned_right_type>(right_numeric_value));
        }
    }
    else
        return (left_value == right_value);
}

#define FT_TEST(name) \
    static int32_t name(void); \
    static void register_##name(void) __attribute__((constructor)); \
    static void register_##name(void) \
    { \
        ft_register_test(name, #name, TEST_MODULE, #name); \
        return ; \
    } \
    static int32_t name(void)

#define FT_ASSERT(expression) \
    do \
    { \
        if (!(expression)) \
        { \
            ft_test_fail(#expression, __FILE__, __LINE__); \
            return (0); \
        } \
    } while (0)

#define FT_ASSERT_EQ(expected, actual) \
    do \
    { \
        auto ft_expected_value = (expected); \
        auto ft_actual_value = (actual); \
        if (!ft_test_values_equal(ft_expected_value, ft_actual_value)) \
        { \
            char ft_expected_buffer[128]; \
            char ft_actual_buffer[128]; \
            ft_test_fail_values(#expected " == " #actual, __FILE__, __LINE__, \
                ft_test_value_to_string(ft_expected_buffer, sizeof(ft_expected_buffer), ft_expected_value), \
                ft_test_value_to_string(ft_actual_buffer, sizeof(ft_actual_buffer), ft_actual_value)); \
            return (0); \
        } \
    } while (0)

#define FT_ASSERT_DOUBLE_EQ(expected, actual) \
    do \
    { \
        double ft_expected_value = (expected); \
        double ft_actual_value = (actual); \
        if (std::fabs(ft_expected_value - ft_actual_value) > 0.0000001) \
        { \
            char ft_expected_buffer[128]; \
            char ft_actual_buffer[128]; \
            ft_test_fail_values(#expected " ~= " #actual, __FILE__, __LINE__, \
                ft_test_value_to_string(ft_expected_buffer, sizeof(ft_expected_buffer), ft_expected_value), \
                ft_test_value_to_string(ft_actual_buffer, sizeof(ft_actual_buffer), ft_actual_value)); \
            return (0); \
        } \
    } while (0)

#define FT_ASSERT_NEQ(expected, actual) \
    do \
    { \
        auto ft_expected_value = (expected); \
        auto ft_actual_value = (actual); \
        if (ft_test_values_equal(ft_expected_value, ft_actual_value)) \
        { \
            char ft_expected_buffer[128]; \
            char ft_actual_buffer[128]; \
            ft_test_fail_values(#expected " != " #actual, __FILE__, __LINE__, \
                ft_test_value_to_string(ft_expected_buffer, sizeof(ft_expected_buffer), ft_expected_value), \
                ft_test_value_to_string(ft_actual_buffer, sizeof(ft_actual_buffer), ft_actual_value)); \
            return (0); \
        } \
    } while (0)

#define FT_ASSERT_NE(unexpected, actual) \
    do \
    { \
        auto ft_unexpected_value = (unexpected); \
        auto ft_actual_value = (actual); \
        if (ft_test_values_equal(ft_unexpected_value, ft_actual_value)) \
        { \
            char ft_unexpected_buffer[128]; \
            char ft_actual_buffer[128]; \
            ft_test_fail_values(#unexpected " != " #actual, __FILE__, __LINE__, \
                ft_test_value_to_string(ft_unexpected_buffer, sizeof(ft_unexpected_buffer), ft_unexpected_value), \
                ft_test_value_to_string(ft_actual_buffer, sizeof(ft_actual_buffer), ft_actual_value)); \
            return (0); \
        } \
    } while (0)

#define FT_ASSERT_STR_EQ(expected, actual) \
    do \
    { \
        const char *ft_expected_value = (expected); \
        const char *ft_actual_value = (actual); \
        if (std::strcmp(ft_expected_value, ft_actual_value) != 0) \
        { \
            char ft_expected_buffer[128]; \
            char ft_actual_buffer[128]; \
            ft_test_fail_values(#expected " == " #actual, __FILE__, __LINE__, \
                ft_test_value_to_string(ft_expected_buffer, sizeof(ft_expected_buffer), ft_expected_value), \
                ft_test_value_to_string(ft_actual_buffer, sizeof(ft_actual_buffer), ft_actual_value)); \
            return (0); \
        } \
    } while (0)

#define FT_ASSERT_SINGLE_GLOBAL_ERROR(expression) \
    do \
    { \
        expression; \
    } while (0)

#define FT_ASSERT_SINGLE_ERROR_READ(stream, buffer, count) \
    do \
    { \
        (stream).read((buffer), (count)); \
    } while (0)

#define FT_ASSERT_SINGLE_ERROR_BAD(result_variable, stream) \
    do \
    { \
        result_variable = !(stream).is_valid(); \
    } while (0)

#define FT_ASSERT_SINGLE_ERROR_GCOUNT(result_variable, stream) \
    do \
    { \
        result_variable = (stream).gcount(); \
    } while (0)

#define FT_ASSERT_SINGLE_ERROR_STR(result_variable, stream) \
    do \
    { \
        result_variable = (stream).str(); \
    } while (0)

#define FT_ASSERT_STREAM_BAD_FALSE(stream) \
    do \
    { \
        ft_bool ft_stream_bad_result__ = FT_FALSE; \
        ft_stream_bad_result__ = !(stream).is_valid(); \
        FT_ASSERT_EQ(FT_FALSE, ft_stream_bad_result__); \
    } while (0)

#endif
