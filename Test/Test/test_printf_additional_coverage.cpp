#include "../test_internal.hpp"
#include "../../Modules/Printf/printf.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <thread>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

typedef int (*t_pf_printf_fd_plain)(int, const char *, ...);
typedef int (*t_pf_printf_plain)(const char *, ...);
typedef int (*t_pf_snprintf_plain)(char *, ft_size_t, const char *, ...);

static int pf_vsnprintf_wrapper(char *string, size_t size, const char *format, ...)
{
    va_list arguments;
    int result;

    va_start(arguments, format);
    result = pf_vsnprintf(string, size, format, arguments);
    va_end(arguments);
    return (result);
}

static FILE *pf_tmpfile_failure(void)
{
    return (static_cast<FILE *>(ft_nullptr));
}

static int pf_fflush_failure(FILE *stream)
{
    (void)stream;
    errno = EIO;
    return (-1);
}

static int64_t pf_ftell_failure(FILE *stream)
{
    (void)stream;
    errno = EIO;
    return (-1);
}

static int read_file_content(const char *file_name, char *buffer, size_t buffer_size)
{
    int file_descriptor;
    ssize_t read_size;

    if (file_name == ft_nullptr || buffer == ft_nullptr || buffer_size == 0)
        return (0);
    file_descriptor = open(file_name, O_RDONLY);
    if (file_descriptor < 0)
        return (0);
    read_size = read(file_descriptor, buffer,
        static_cast<unsigned int>(buffer_size - 1));
    close(file_descriptor);
    if (read_size < 0)
        return (0);
    buffer[static_cast<size_t>(read_size)] = '\0';
    return (1);
}

static int pf_custom_ok_handler(va_list *arguments, ft_string &output, void *context)
{
    const char *prefix;
    const char *value;

    if (arguments == ft_nullptr)
        return (-1);
    prefix = static_cast<const char *>(context);
    value = va_arg(*arguments, const char *);
    output.clear();
    if (prefix != ft_nullptr)
    {
        if (output.append(prefix) != FT_ERR_SUCCESS)
            return (-1);
    }
    if (value == ft_nullptr)
    {
        if (output.append("(null)") != FT_ERR_SUCCESS)
            return (-1);
        return (0);
    }
    if (output.append(value) != FT_ERR_SUCCESS)
        return (-1);
    return (0);
}

static int pf_custom_fail_handler(va_list *arguments, ft_string &output, void *context)
{
    (void)arguments;
    (void)output;
    (void)context;
    return (-1);
}

FT_TEST(test_pf_printf_fd_null_format_returns_error)
{
    t_pf_printf_fd_plain call_pf_printf_fd;

    call_pf_printf_fd = pf_printf_fd;
    FT_ASSERT_EQ(-1, call_pf_printf_fd(1, static_cast<const char *>(ft_nullptr)));
    return (1);
}

FT_TEST(test_pf_printf_fd_invalid_fd_returns_error)
{
    FT_ASSERT_EQ(-1, pf_printf_fd(-1, "%s", "x"));
    return (1);
}

FT_TEST(test_pf_printf_null_format_returns_error)
{
    t_pf_printf_plain call_pf_printf;

    call_pf_printf = pf_printf;
    FT_ASSERT_EQ(-1, call_pf_printf(static_cast<const char *>(ft_nullptr)));
    return (1);
}

FT_TEST(test_pf_snprintf_null_format_returns_error)
{
    char buffer[8];
    t_pf_snprintf_plain call_pf_snprintf;

    buffer[0] = 'A';
    call_pf_snprintf = pf_snprintf;
    FT_ASSERT_EQ(-1, call_pf_snprintf(buffer, sizeof(buffer), static_cast<const char *>(ft_nullptr)));
    FT_ASSERT_EQ('\0', buffer[0]);
    return (1);
}

FT_TEST(test_pf_snprintf_null_buffer_nonzero_size_returns_error)
{
    FT_ASSERT_EQ(-1, pf_snprintf(static_cast<char *>(ft_nullptr), 4, "%s", "x"));
    return (1);
}

FT_TEST(test_pf_snprintf_null_buffer_zero_size_returns_length)
{
    FT_ASSERT_EQ(3, pf_snprintf(static_cast<char *>(ft_nullptr), 0, "%s", "abc"));
    return (1);
}

FT_TEST(test_pf_snprintf_zero_size_keeps_buffer)
{
    char buffer[8];

    buffer[0] = 'Q';
    FT_ASSERT_EQ(3, pf_snprintf(buffer, 0, "%s", "abc"));
    FT_ASSERT_EQ('Q', buffer[0]);
    return (1);
}

FT_TEST(test_pf_snprintf_truncates_and_null_terminates)
{
    char buffer[5];

    FT_ASSERT_EQ(9, pf_snprintf(buffer, sizeof(buffer), "%s", "truncate!"));
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "trun"));
    return (1);
}

FT_TEST(test_pf_snprintf_empty_format_returns_zero)
{
    char buffer[4];

    buffer[0] = 'X';
    FT_ASSERT_EQ(0, pf_snprintf(buffer, sizeof(buffer), "%s", ""));
    FT_ASSERT_EQ('\0', buffer[0]);
    return (1);
}

FT_TEST(test_pf_vsnprintf_basic_formatting)
{
    char buffer[32];

    FT_ASSERT_EQ(6, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%s-%d", "abc", 12));
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "abc-12"));
    return (1);
}

FT_TEST(test_pf_vsnprintf_null_format_returns_error)
{
    char buffer[8];

    buffer[0] = 'A';
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), static_cast<const char *>(ft_nullptr)));
    return (1);
}

FT_TEST(test_pf_set_tmpfile_function_null_resets_default)
{
    char buffer[8];

    pf_set_tmpfile_function(static_cast<t_pf_tmpfile_function>(ft_nullptr));
    FT_ASSERT_EQ(2, pf_snprintf(buffer, sizeof(buffer), "%s", "ok"));
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "ok"));
    return (1);
}

FT_TEST(test_pf_snprintf_tmpfile_failure_returns_error)
{
    char buffer[8];

    buffer[0] = 'A';
    pf_set_tmpfile_function(pf_tmpfile_failure);
    FT_ASSERT_EQ(-1, pf_snprintf(buffer, sizeof(buffer), "%s", "abc"));
    pf_reset_tmpfile_function();
    FT_ASSERT_EQ('\0', buffer[0]);
    return (1);
}

FT_TEST(test_pf_snprintf_fflush_failure_returns_error)
{
    char buffer[8];

    pf_set_fflush_function(pf_fflush_failure);
    FT_ASSERT_EQ(-1, pf_snprintf(buffer, sizeof(buffer), "%s", "abc"));
    pf_reset_fflush_function();
    FT_ASSERT_EQ('\0', buffer[0]);
    return (1);
}

FT_TEST(test_pf_snprintf_ftell_failure_returns_error)
{
    char buffer[8];

    pf_set_ftell_function(pf_ftell_failure);
    FT_ASSERT_EQ(-1, pf_snprintf(buffer, sizeof(buffer), "%s", "abc"));
    pf_reset_ftell_function();
    FT_ASSERT_EQ('\0', buffer[0]);
    return (1);
}

FT_TEST(test_ft_fprintf_null_stream_returns_error)
{
    FT_ASSERT_EQ(-1, ft_fprintf(static_cast<FILE *>(ft_nullptr), "%s", "x"));
    return (1);
}

FT_TEST(test_ft_fprintf_null_format_returns_error)
{
    FILE *stream;
    int result;
    typedef int (*t_ft_fprintf_plain)(FILE *, const char *, ...);
    t_ft_fprintf_plain call_ft_fprintf;

    stream = tmpfile();
    if (stream == ft_nullptr)
        return (0);
    call_ft_fprintf = ft_fprintf;
    result = call_ft_fprintf(stream, static_cast<const char *>(ft_nullptr));
    fclose(stream);
    FT_ASSERT_EQ(-1, result);
    return (1);
}

FT_TEST(test_ft_fprintf_writes_expected_text)
{
    const char *file_name;
    FILE *stream;
    char buffer[64];
    int printed;

    file_name = "tmp_test_ft_fprintf.txt";
    stream = fopen(file_name, "w+");
    if (stream == ft_nullptr)
        return (0);
    printed = ft_fprintf(stream, "Value=%d %s", 42, "ok");
    fflush(stream);
    fclose(stream);
    FT_ASSERT_EQ(11, printed);
    FT_ASSERT(read_file_content(file_name, buffer, sizeof(buffer)));
    unlink(file_name);
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "Value=42 ok"));
    return (1);
}

FT_TEST(test_pf_printf_fd_percent_escape)
{
    const char *file_name;
    int file_descriptor;
    char buffer[16];
    int printed;

    file_name = "tmp_test_pf_printf_percent.txt";
    file_descriptor = open(file_name, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (file_descriptor < 0)
        return (0);
    printed = pf_printf_fd(file_descriptor, "%%");
    close(file_descriptor);
    FT_ASSERT_EQ(1, printed);
    FT_ASSERT(read_file_content(file_name, buffer, sizeof(buffer)));
    unlink(file_name);
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "%"));
    return (1);
}

#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif

FT_TEST(test_pf_custom_specifier_register_and_format)
{
    char buffer[64];
    char format[3];
    char context[] = "pre:";
    int register_result;

    format[0] = '%';
    format[1] = 'Q';
    format[2] = '\0';
    pf_unregister_custom_specifier('Q');
    register_result = pf_register_custom_specifier('Q', pf_custom_ok_handler, context);
    FT_ASSERT_EQ(0, register_result);
    FT_ASSERT_EQ(9, pf_snprintf(buffer, sizeof(buffer), format, "value"));
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "pre:value"));
    FT_ASSERT_EQ(0, pf_unregister_custom_specifier('Q'));
    return (1);
}

FT_TEST(test_pf_custom_specifier_duplicate_register_fails)
{
    char context[] = "x:";
    FT_ASSERT_EQ(0, pf_register_custom_specifier('R', pf_custom_ok_handler, context));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, pf_register_custom_specifier('R', pf_custom_ok_handler, context));
    FT_ASSERT_EQ(0, pf_unregister_custom_specifier('R'));
    return (1);
}

FT_TEST(test_pf_custom_specifier_null_handler_fails)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, pf_register_custom_specifier('S', static_cast<t_pf_custom_formatter>(ft_nullptr), ft_nullptr));
    return (1);
}

FT_TEST(test_pf_custom_specifier_handler_failure_propagates)
{
    char buffer[16];
    char format[3];

    format[0] = '%';
    format[1] = 'Y';
    format[2] = '\0';
    FT_ASSERT_EQ(0, pf_register_custom_specifier('Y', pf_custom_fail_handler, ft_nullptr));
    FT_ASSERT_EQ(-1, pf_snprintf(buffer, sizeof(buffer), format, "x"));
    FT_ASSERT_EQ(0, pf_unregister_custom_specifier('Y'));
    return (1);
}

#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

FT_TEST(test_pf_enable_thread_safety_idempotent)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pf_enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pf_enable_thread_safety());
    pf_disable_thread_safety();
    return (1);
}

FT_TEST(test_pf_disable_thread_safety_without_enable_is_safe)
{
    char context[] = "a:";
    pf_disable_thread_safety();
    FT_ASSERT_EQ(0, pf_register_custom_specifier('Z', pf_custom_ok_handler, context));
    FT_ASSERT_EQ(0, pf_unregister_custom_specifier('Z'));
    return (1);
}

static int32_t pf_custom_marker_first(va_list *, ft_string &output, void *context)
{
    if (context == ft_nullptr || *static_cast<const char *>(context) != 'A')
        return (FT_ERR_INVALID_STATE);
    return (output.append('A'));
}

static int32_t pf_custom_marker_second(va_list *, ft_string &output, void *context)
{
    if (context == ft_nullptr || *static_cast<const char *>(context) != 'B')
        return (FT_ERR_INVALID_STATE);
    return (output.append('B'));
}

FT_TEST(test_pf_custom_registry_keeps_handler_context_pairs_during_replacement)
{
    char first = 'A';
    char second = 'B';
    std::atomic<ft_bool> success{FT_TRUE};
    std::atomic<ft_bool> ready{FT_FALSE};
    std::thread updater([&first, &second, &ready, &success]()
    {
        while (ready.load(std::memory_order_acquire) == FT_FALSE)
            std::this_thread::yield();
        uint32_t iteration = 0U;
        while (iteration < 600U)
        {
            int32_t result;
            if (iteration % 2U == 0U)
                result = pf_register_custom_specifier('Q', pf_custom_marker_first, &first);
            else
                result = pf_register_custom_specifier('Q', pf_custom_marker_second, &second);
            if (result != FT_ERR_SUCCESS || pf_unregister_custom_specifier('Q') != FT_ERR_SUCCESS)
                success.store(FT_FALSE, std::memory_order_relaxed);
            iteration += 1U;
        }
        return ;
    });

    ready.store(FT_TRUE, std::memory_order_release);
    uint32_t iteration = 0U;
    while (iteration < 1200U)
    {
        char output[4];
        const int32_t length = pf_vsnprintf_wrapper(output, sizeof(output), "%Q");
        if ((length != 1 || (output[0] != 'A' && output[0] != 'B'))
            && (length != 2 || ft_strcmp(output, "%Q") != 0))
            success.store(FT_FALSE, std::memory_order_relaxed);
        iteration += 1U;
    }
    updater.join();
    (void)pf_unregister_custom_specifier('Q');
    FT_ASSERT_EQ(FT_TRUE, success.load(std::memory_order_relaxed));
    return (1);
}

FT_TEST(test_pf_custom_registry_overrides_a_native_sequential_specifier)
{
    char marker = 'A';
    char output[8];
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pf_register_custom_specifier('d', pf_custom_marker_first, &marker));
    const int32_t custom_count = pf_vsnprintf_wrapper(output, sizeof(output), "%d", 42);
    const char custom_value = output[0];
    (void)pf_unregister_custom_specifier('d');
    FT_ASSERT_EQ(1, custom_count);
    FT_ASSERT_EQ('A', custom_value);
    FT_ASSERT_EQ(2, pf_vsnprintf_wrapper(output, sizeof(output), "%d", 42));
    FT_ASSERT_EQ(0, ft_strcmp(output, "42"));
    return (1);
}
