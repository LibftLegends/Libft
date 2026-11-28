#include "../test_internal.hpp"
#include "../../Modules/Printf/printf.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <unistd.h>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#if defined(_WIN32) || defined(_WIN64)
# include <io.h>
#endif

typedef int (*t_pf_snprintf_plain)(char *, ft_size_t, const char *, ...);
typedef int (*t_pf_printf_fd_plain)(int, const char *, ...);

static int create_pipe(int pipe_fds[2])
{
    if (pipe(pipe_fds) != 0)
        return (0);
    return (1);
}

static int close_pipe_end(int file_descriptor)
{
    if (close(file_descriptor) != 0)
        return (0);
    return (1);
}

static int read_pipe_into_buffer(int read_fd, char *buffer, size_t buffer_size, ssize_t *bytes_read)
{
    if (buffer == ft_nullptr || bytes_read == ft_nullptr)
        return (0);
    *bytes_read = read(read_fd, buffer,
        static_cast<unsigned int>(buffer_size));
    if (*bytes_read < 0)
        return (0);
    return (1);
}

static int pf_test_custom_handler(va_list *args, ft_string &output, void *context)
{
    const char  *prefix;
    const char  *value;

    prefix = static_cast<const char *>(context);
    if (args == ft_nullptr)
        return (-1);
    value = va_arg(*args, const char *);
    output.clear();
    if (prefix != ft_nullptr)
    {
        output.append(prefix);
    }
    if (value != ft_nullptr)
        output.append(value);
    else
        output.append("(null)");
    return (0);
}

static int pf_test_custom_handler_failure(va_list *args, ft_string &output, void *context)
{
    (void)args;
    (void)output;
    (void)context;
    return (-1);
}

static int call_ft_vfprintf(FILE *stream, const char *format, ...)
{
    va_list arguments;
    int result;

    va_start(arguments, format);
    result = ft_vfprintf(stream, format, arguments);
    va_end(arguments);
    return (result);
}

FT_TEST(test_ft_vfprintf_writes_output)
{
    int pipe_fds[2];
    FILE *stream;
    ssize_t bytes_read;
    char buffer[64];
    char word[] = "done";

    FT_ASSERT(create_pipe(pipe_fds));
    stream = fdopen(pipe_fds[1], "w");
    if (stream == ft_nullptr)
    {
        close_pipe_end(pipe_fds[0]);
        close_pipe_end(pipe_fds[1]);
        return (0);
    }
    int printed = call_ft_vfprintf(stream, "Value:%d %s!", 42, word);
    FT_ASSERT_EQ(14, printed);
    FT_ASSERT_EQ(0, fflush(stream));
    FT_ASSERT_EQ(0, fclose(stream));
    stream = static_cast<FILE *>(ft_nullptr);
    pipe_fds[1] = -1;
    FT_ASSERT(read_pipe_into_buffer(pipe_fds[0], buffer, sizeof(buffer) - 1, &bytes_read));
    FT_ASSERT(bytes_read >= 0);
    buffer[static_cast<size_t>(bytes_read)] = '\0';
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "Value:42 done!"));
    FT_ASSERT(close_pipe_end(pipe_fds[0]));
    return (1);
}

FT_TEST(test_ft_vfprintf_null_arguments_return_error)
{
    int pipe_fds[2];
    FILE *stream;

    FT_ASSERT_EQ(-1, call_ft_vfprintf(static_cast<FILE *>(ft_nullptr), "noop"));
    FT_ASSERT(create_pipe(pipe_fds));
    stream = fdopen(pipe_fds[1], "w");
    if (stream == ft_nullptr)
    {
        close_pipe_end(pipe_fds[0]);
        close_pipe_end(pipe_fds[1]);
        return (0);
    }
    FT_ASSERT_EQ(-1, call_ft_vfprintf(stream, static_cast<const char *>(ft_nullptr)));
    FT_ASSERT_EQ(0, fclose(stream));
    stream = static_cast<FILE *>(ft_nullptr);
    FT_ASSERT(close_pipe_end(pipe_fds[0]));
    return (1);
}

FT_TEST(test_ft_vfprintf_write_failure_sets_errno)
{
    FILE *stream;

    stream = std::tmpfile();
    if (stream == ft_nullptr)
        return (0);
#if defined(_WIN32) || defined(_WIN64)
    int descriptor = _fileno(stream);
    if (descriptor != -1)
        _close(descriptor);
#else
    int descriptor = fileno(stream);
    if (descriptor != -1)
        close(descriptor);
#endif
    int printed = call_ft_vfprintf(stream, "%s", "fail");
    FT_ASSERT_EQ(-1, printed);
    fclose(stream);
    return (1);
}

FT_TEST(test_ft_fprintf_writes_and_counts)
{
    int pipe_fds[2];
    FILE *stream;
    ssize_t bytes_read;
    char buffer[64];
    char name[] = "world";

    FT_ASSERT(create_pipe(pipe_fds));
    stream = fdopen(pipe_fds[1], "w");
    if (stream == ft_nullptr)
    {
        close_pipe_end(pipe_fds[0]);
        close_pipe_end(pipe_fds[1]);
        return (0);
    }
    int printed = ft_fprintf(stream, "Hello %s", name);
    FT_ASSERT_EQ(11, printed);
    FT_ASSERT_EQ(0, fflush(stream));
    FT_ASSERT_EQ(0, fclose(stream));
    stream = static_cast<FILE *>(ft_nullptr);
    pipe_fds[1] = -1;
    FT_ASSERT(read_pipe_into_buffer(pipe_fds[0], buffer, sizeof(buffer) - 1, &bytes_read));
    FT_ASSERT(bytes_read >= 0);
    buffer[static_cast<size_t>(bytes_read)] = '\0';
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "Hello world"));
    FT_ASSERT(close_pipe_end(pipe_fds[0]));
    return (1);
}

FT_TEST(test_ft_fprintf_null_arguments_return_error)
{
    int pipe_fds[2];
    FILE *stream;
    typedef int (*t_ft_fprintf_plain)(FILE *, const char *, ...);
    t_ft_fprintf_plain plain_ft_fprintf;

    FT_ASSERT_EQ(-1, ft_fprintf(static_cast<FILE *>(ft_nullptr), "noop"));
    FT_ASSERT(create_pipe(pipe_fds));
    stream = fdopen(pipe_fds[1], "w");
    if (stream == ft_nullptr)
    {
        close_pipe_end(pipe_fds[0]);
        close_pipe_end(pipe_fds[1]);
        return (0);
    }
    plain_ft_fprintf = ft_fprintf;
    FT_ASSERT_EQ(-1, plain_ft_fprintf(stream, static_cast<const char *>(ft_nullptr)));
    FT_ASSERT_EQ(0, fclose(stream));
    stream = static_cast<FILE *>(ft_nullptr);
    FT_ASSERT(close_pipe_end(pipe_fds[0]));
    return (1);
}

FT_TEST(test_pf_printf_writes_to_stdout)
{
    int pipe_fds[2];
    int stdout_backup;
    ssize_t bytes_read;
    char buffer[64];
    char status[] = "done";

    FT_ASSERT(create_pipe(pipe_fds));
    stdout_backup = dup(STDOUT_FILENO);
    if (stdout_backup < 0)
    {
        close_pipe_end(pipe_fds[0]);
        close_pipe_end(pipe_fds[1]);
        return (0);
    }
    if (dup2(pipe_fds[1], STDOUT_FILENO) < 0)
    {
        close_pipe_end(pipe_fds[0]);
        close_pipe_end(pipe_fds[1]);
        close(stdout_backup);
        return (0);
    }
    int printed = pf_printf("Sum=%d %s", 7, status);
    FT_ASSERT_EQ(10, printed);
    FT_ASSERT_EQ(0, fflush(stdout));
    FT_ASSERT(close_pipe_end(pipe_fds[1]));
    pipe_fds[1] = -1;
    FT_ASSERT(dup2(stdout_backup, STDOUT_FILENO) >= 0);
    close(stdout_backup);
    FT_ASSERT(read_pipe_into_buffer(pipe_fds[0], buffer, sizeof(buffer) - 1, &bytes_read));
    FT_ASSERT(bytes_read >= 0);
    buffer[static_cast<size_t>(bytes_read)] = '\0';
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "Sum=7 done"));
    FT_ASSERT(close_pipe_end(pipe_fds[0]));
    return (1);
}

FT_TEST(test_pf_custom_specifier_formats_stream)
{
    char    buffer[64];
    char    context[] = "tag:";

    FT_ASSERT_EQ(0, pf_register_custom_specifier('T', pf_test_custom_handler, context));
    t_pf_snprintf_plain plain_pf_snprintf = pf_snprintf;
    int length = plain_pf_snprintf(buffer, sizeof(buffer), "%T", "value");
    int unregister_status = pf_unregister_custom_specifier('T');
    FT_ASSERT_EQ(0, unregister_status);
    FT_ASSERT_EQ(9, length);
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "tag:value"));
    return (1);
}

FT_TEST(test_pf_custom_specifier_formats_fd)
{
    int     pipe_fds[2];
    ssize_t bytes_read;
    char    buffer[64];
    char    context[] = "fd:";

    FT_ASSERT(create_pipe(pipe_fds));
    FT_ASSERT_EQ(0, pf_register_custom_specifier('Y', pf_test_custom_handler, context));
    t_pf_printf_fd_plain plain_pf_printf_fd = pf_printf_fd;
    int printed = plain_pf_printf_fd(pipe_fds[1], "%Y!", "output");
    FT_ASSERT(close_pipe_end(pipe_fds[1]));
    pipe_fds[1] = -1;
    FT_ASSERT(read_pipe_into_buffer(pipe_fds[0], buffer, sizeof(buffer) - 1, &bytes_read));
    FT_ASSERT(bytes_read >= 0);
    buffer[static_cast<size_t>(bytes_read)] = '\0';
    FT_ASSERT(close_pipe_end(pipe_fds[0]));
    int unregister_status = pf_unregister_custom_specifier('Y');
    FT_ASSERT_EQ(0, unregister_status);
    FT_ASSERT_EQ(10, printed);
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "fd:output!"));
    return (1);
}

FT_TEST(test_pf_custom_specifier_duplicate_registration)
{
    (void)pf_unregister_custom_specifier('Q');
    FT_ASSERT_EQ(0, pf_register_custom_specifier('Q', pf_test_custom_handler, ft_nullptr));
    int duplicate_status = pf_register_custom_specifier('Q', pf_test_custom_handler, ft_nullptr);
    int unregister_status = pf_unregister_custom_specifier('Q');
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, duplicate_status);
    FT_ASSERT_EQ(0, unregister_status);
    return (1);
}

FT_TEST(test_pf_custom_specifier_handler_error_propagates)
{
    char buffer[32];

    (void)pf_unregister_custom_specifier('R');
    FT_ASSERT_EQ(0, pf_register_custom_specifier('R', pf_test_custom_handler_failure, ft_nullptr));
    t_pf_snprintf_plain plain_pf_snprintf = pf_snprintf;
    int result = plain_pf_snprintf(buffer, sizeof(buffer), "%R");
    int unregister_status = pf_unregister_custom_specifier('R');
    FT_ASSERT_EQ(0, unregister_status);
    FT_ASSERT_EQ(-1, result);
    return (1);
}

FT_TEST(test_pf_printf_null_format_sets_errno)
{
    typedef int (*t_pf_printf_plain)(const char *, ...);
    t_pf_printf_plain plain_pf_printf;

    plain_pf_printf = pf_printf;
    FT_ASSERT_EQ(-1, plain_pf_printf(static_cast<const char *>(ft_nullptr)));
    return (1);
}

#ifdef LIBFT_TEST_BUILD
struct s_printf_write_capture
{
    char output[32];
    ft_size_t length;
    ft_size_t chunk_limit;
    uint32_t writes;
    ft_bool fail;
};

static s_printf_write_capture g_printf_capture{};

static int64_t test_printf_capture_write(int32_t, const void *data, ft_size_t length)
{
    g_printf_capture.writes += 1U;
    if (g_printf_capture.fail != FT_FALSE || length > sizeof(g_printf_capture.output) - g_printf_capture.length)
    {
        errno = EIO;
        return (-1);
    }
    if (g_printf_capture.chunk_limit != 0U && length > g_printf_capture.chunk_limit)
        length = g_printf_capture.chunk_limit;
    (void)ft_memcpy(g_printf_capture.output + g_printf_capture.length, data, length);
    g_printf_capture.length += length;
    return (static_cast<int64_t>(length));
}

FT_TEST(test_pf_printf_fd_combines_spans_and_retries_short_writes)
{
    g_printf_capture = {};
    su_set_write_syscall_hook(test_printf_capture_write);
    const int32_t first_result = pf_printf_fd(17, "prefix %s %d!", "abc", 7);
    const uint32_t first_writes = g_printf_capture.writes;
    g_printf_capture = {};
    g_printf_capture.chunk_limit = 3U;
    const int32_t second_result = pf_printf_fd(17, "prefix %s %d!", "abc", 7);
    su_set_write_syscall_hook(ft_nullptr);
    FT_ASSERT_EQ(13, first_result);
    FT_ASSERT_EQ(1U, first_writes);
    FT_ASSERT_EQ(13, second_result);
    FT_ASSERT_EQ(5U, g_printf_capture.writes);
    FT_ASSERT_EQ(0, ft_strcmp("prefix abc 7!", g_printf_capture.output));
    return (1);
}

FT_TEST(test_pf_printf_fd_failure_before_percent_n_leaves_count_untouched)
{
    g_printf_capture = {};
    g_printf_capture.fail = FT_TRUE;
    int32_t count = -1;
    const t_pf_printf_fd_plain format = pf_printf_fd;
    su_set_write_syscall_hook(test_printf_capture_write);
    const int32_t sequential = format(17, "prefix%n", &count);
    const int32_t positional = format(17, "prefix%1$n", &count);
    su_set_write_syscall_hook(ft_nullptr);
    FT_ASSERT_EQ(-1, sequential);
    FT_ASSERT_EQ(-1, positional);
    FT_ASSERT_EQ(-1, count);
    FT_ASSERT_EQ(2U, g_printf_capture.writes);
    return (1);
}
#endif
