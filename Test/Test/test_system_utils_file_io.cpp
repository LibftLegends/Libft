#include "../test_internal.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static void create_system_io_test_file(void)
{
    FILE *file_handle;

    file_handle = std::fopen("build_logs/test_cmp_system_io.txt", "w");
    if (file_handle != ft_nullptr)
    {
        std::fputs("data", file_handle);
        std::fclose(file_handle);
    }
    return ;
}

static int  g_su_write_hook_calls = 0;

static int64_t   su_write_zero_progress_hook(int file_descriptor,
    const void *buffer, ft_size_t count)
{
    (void)file_descriptor;
    (void)buffer;
    if (count == 0)
        return (0);
    g_su_write_hook_calls += 1;
    if (g_su_write_hook_calls == 1)
        return (0);
    return (static_cast<ssize_t>(count));
}

FT_TEST(test_cmp_open_failure_sets_errno)
{
    errno = 0;
    std::remove("missing_cmp_file.txt");
    FT_ASSERT_EQ(-1, cmp_open("missing_cmp_file.txt"));
    return (1);
}

FT_TEST(test_cmp_read_invalid_fd_sets_ft_einval)
{
    char buffer[4];
    int64_t bytes_read;

    bytes_read = 0;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        cmp_read(-1, buffer, sizeof(buffer), &bytes_read));
    FT_ASSERT_EQ(0, bytes_read);
    return (1);
}

FT_TEST(test_cmp_write_invalid_fd_sets_ft_einval)
{
    char buffer[4] = {0, 0, 0, 0};
    int64_t bytes_written;

    bytes_written = 0;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        cmp_write(-1, buffer, sizeof(buffer), &bytes_written));
    FT_ASSERT_EQ(0, bytes_written);
    return (1);
}

FT_TEST(test_cmp_close_invalid_fd_sets_ft_einval)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cmp_close(-1));
    return (1);
}

#if defined(_WIN32) || defined(_WIN64)
FT_TEST(test_cmp_read_untracked_fd_sets_ft_invalid_handle)
{
    char buffer[4];
    int64_t bytes_read;

    bytes_read = 0;
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE,
        cmp_read(100, buffer, sizeof(buffer), &bytes_read));
    FT_ASSERT_EQ(0, bytes_read);
    return (1);
}

FT_TEST(test_cmp_write_untracked_fd_sets_ft_invalid_handle)
{
    const char buffer[4] = {'t', 'e', 's', 't'};
    int64_t bytes_written;

    bytes_written = 0;
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE,
        cmp_write(101, buffer, sizeof(buffer), &bytes_written));
    FT_ASSERT_EQ(0, bytes_written);
    return (1);
}

FT_TEST(test_cmp_close_untracked_fd_sets_ft_invalid_handle)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, cmp_close(102));
    return (1);
}
#endif

FT_TEST(test_cmp_read_translates_errno)
{
    int file_descriptor;
    char buffer[4];
    int64_t bytes_read;

    create_system_io_test_file();
    file_descriptor = cmp_open("build_logs/test_cmp_system_io.txt", O_WRONLY);
    FT_ASSERT(file_descriptor >= 0);
    bytes_read = 0;
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE,
        cmp_read(file_descriptor, buffer, sizeof(buffer), &bytes_read));
    FT_ASSERT_EQ(0, bytes_read);
    FT_ASSERT_EQ(0, cmp_close(file_descriptor));
    return (1);
}

FT_TEST(test_cmp_write_translates_errno)
{
    int file_descriptor;
    const char buffer[4] = {'t', 'e', 's', 't'};
    int64_t bytes_written;

    create_system_io_test_file();
    file_descriptor = cmp_open("build_logs/test_cmp_system_io.txt", O_RDONLY);
    FT_ASSERT(file_descriptor >= 0);
    bytes_written = 0;
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE,
        cmp_write(file_descriptor, buffer, sizeof(buffer), &bytes_written));
    FT_ASSERT_EQ(0, bytes_written);
    FT_ASSERT_EQ(0, cmp_close(file_descriptor));
    return (1);
}

FT_TEST(test_su_write_reports_zero_progress)
{
    const char  buffer[4] = {'d', 'a', 't', 'a'};

    g_su_write_hook_calls = 0;
    su_set_write_syscall_hook(su_write_zero_progress_hook);
    FT_ASSERT_EQ(-1, su_write(42, buffer, sizeof(buffer)));
    FT_ASSERT_EQ(1, g_su_write_hook_calls);
    su_reset_write_syscall_hook();
    return (1);
}

FT_TEST(test_cmp_close_translates_errno)
{
    int file_descriptor;

    create_system_io_test_file();
    file_descriptor = cmp_open("build_logs/test_cmp_system_io.txt", O_RDONLY);
    FT_ASSERT(file_descriptor >= 0);
    FT_ASSERT_EQ(0, cmp_close(file_descriptor));
    errno = 0;
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, cmp_close(file_descriptor));
    return (1);
}
