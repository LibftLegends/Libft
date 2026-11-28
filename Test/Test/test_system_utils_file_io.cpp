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
#if defined(_WIN32) || defined(_WIN64)
# include <chrono>
# include <future>
# include <io.h>
# include <thread>
#endif

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

static int64_t su_write_would_block_hook(int32_t file_descriptor,
    const void *buffer, ft_size_t count)
{
    (void)file_descriptor;
    (void)buffer;
    g_su_write_hook_calls += 1;
    if (g_su_write_hook_calls <= 11)
    {
        errno = EAGAIN;
        return (-1);
    }
    return (static_cast<int64_t>(count));
}

static int64_t su_write_one_byte_hook(int32_t file_descriptor,
    const void *buffer, ft_size_t count)
{
    (void)file_descriptor;
    (void)buffer;
    (void)count;
    g_su_write_hook_calls += 1;
    return (1);
}

FT_TEST(test_cmp_open_failure_sets_errno)
{
    errno = 0;
    std::remove("missing_cmp_file.txt");
    FT_ASSERT_EQ(-1, cmp_open("missing_cmp_file.txt"));
    return (1);
}

FT_TEST(test_cmp_file_stream_size_preserves_position)
{
    create_system_io_test_file();
    FILE *stream = std::fopen("build_logs/test_cmp_system_io.txt", "rb");
    FT_ASSERT(stream != ft_nullptr);
    const int32_t first = std::fgetc(stream);
    ft_size_t size = 0U;
    const int32_t status = cmp_file_stream_size(stream, &size);
    const int32_t second = std::fgetc(stream);
    const int32_t closed = std::fclose(stream);
    FT_ASSERT_EQ('d', first);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status);
    FT_ASSERT_EQ(4U, size);
    FT_ASSERT_EQ('a', second);
    FT_ASSERT_EQ(0, closed);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cmp_file_stream_size(ft_nullptr, &size));
    FT_ASSERT_EQ(0U, size);
    return (1);
}

FT_TEST(test_cmp_read_invalid_fd_sets_ft_einval)
{
    char buffer[4];
    int64_t bytes_read;
    int32_t system_error = EINTR;

    bytes_read = 0;
    errno = EINTR;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        cmp_read(-1, buffer, sizeof(buffer), &bytes_read, &system_error));
    FT_ASSERT_EQ(0, bytes_read);
    FT_ASSERT_EQ(0, system_error);
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

FT_TEST(test_cmp_windows_blocked_read_allows_unrelated_file_io)
{
    int32_t descriptors[2];
    FT_ASSERT_EQ(0, _pipe(descriptors, 4096, _O_BINARY));
    std::promise<DWORD> reader_id;
    auto reader_started = reader_id.get_future();
    char received = '\0';
    auto reader = std::async(std::launch::async, [&]()
    {
        reader_id.set_value(GetCurrentThreadId());
        return (su_read(descriptors[0], &received, 1U));
    });
    const HANDLE reader_handle = OpenThread(THREAD_QUERY_INFORMATION, FALSE,
        reader_started.get());
    BOOL pending = FALSE;
    if (reader_handle != ft_nullptr)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (GetThreadIOPendingFlag(reader_handle, &pending) != 0 && pending == FALSE
            && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        (void)CloseHandle(reader_handle);
    }
    const char *path = "build_logs/test_cmp_blocked_read.txt";
    auto writer = std::async(std::launch::async, [path]()
    {
        const int32_t descriptor = su_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (descriptor < 0)
            return (FT_FALSE);
        const int64_t written = su_write(descriptor, "other", 5U);
        const int32_t closed = su_close(descriptor);
        return (static_cast<ft_bool>(written == 5 && closed == FT_ERR_SUCCESS));
    });
    const auto writer_status = writer.wait_for(std::chrono::seconds(2));
    const int32_t unblocked = _write(descriptors[1], "x", 1U);
    (void)_close(descriptors[1]);
    const int64_t bytes_read = reader.get();
    const ft_bool writer_succeeded = writer.get();
    (void)_close(descriptors[0]);
    (void)std::remove(path);
    FT_ASSERT(pending != FALSE);
    FT_ASSERT(writer_status == std::future_status::ready);
    FT_ASSERT_EQ(FT_TRUE, writer_succeeded);
    FT_ASSERT_EQ(1, unblocked);
    FT_ASSERT_EQ(1, bytes_read);
    FT_ASSERT_EQ('x', received);
    return (1);
}
#endif

FT_TEST(test_cmp_read_translates_errno)
{
    int file_descriptor;
    char buffer[4];
    int64_t bytes_read;
    int32_t system_error = 0;

    create_system_io_test_file();
    file_descriptor = cmp_open("build_logs/test_cmp_system_io.txt", O_WRONLY);
    FT_ASSERT(file_descriptor >= 0);
    bytes_read = 0;
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE,
        cmp_read(file_descriptor, buffer, sizeof(buffer), &bytes_read, &system_error));
    FT_ASSERT_EQ(0, bytes_read);
    FT_ASSERT_EQ(EBADF, system_error);
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

FT_TEST(test_su_write_limits_would_block_retries)
{
    const char byte = 'a';
    int32_t error = FT_ERR_SUCCESS;
    g_su_write_hook_calls = 0;
    su_set_write_syscall_hook(su_write_would_block_hook);
    const int64_t written = su_write_with_error(42, &byte, 1U, &error);
    su_reset_write_syscall_hook();
    FT_ASSERT_EQ(-1, written);
    FT_ASSERT_EQ(11, g_su_write_hook_calls);
    FT_ASSERT_EQ(cmp_map_system_error_to_ft(EAGAIN), error);
    return (1);
}

FT_TEST(test_su_write_retries_interruptions_and_short_writes)
{
    const char bytes[] = {'a', 'b'};
    int32_t error = FT_ERR_IO;
    g_su_write_hook_calls = 0;
    su_set_write_syscall_hook(su_write_one_byte_hook);
    su_test_inject_write_interrupts(12U);
    const int64_t written = su_write_with_error(42, bytes, sizeof(bytes), &error);
    su_test_clear_write_interrupts();
    su_reset_write_syscall_hook();
    FT_ASSERT_EQ(2, written);
    FT_ASSERT_EQ(2, g_su_write_hook_calls);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error);
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
