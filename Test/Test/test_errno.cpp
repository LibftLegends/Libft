#include "../test_internal.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <cerrno>
#include <cstring>
#include <string>
#include <thread>
#include <unistd.h>
#if !defined(_WIN32) && !defined(_WIN64)
# include <sys/wait.h>
#endif

#ifndef LIBFT_TEST_BUILD
#endif

#if defined(_WIN32) || defined(_WIN64)
# include <winsock2.h>
# include <windows.h>
#endif

FT_TEST(test_ft_strerror_sets_success_errno)
{
    const char *expected_message;
    const char *actual_message;

    expected_message = strerror(EINVAL);
    actual_message = ft_strerror(FT_ERR_INVALID_ARGUMENT);
    FT_ASSERT(expected_message != NULL);
    FT_ASSERT(actual_message != NULL);
    FT_ASSERT_EQ(0, std::strcmp(expected_message, actual_message));
    return (1);
}

FT_TEST(test_ft_errno_memcpy_tracks_error_state)
{
    char        source_buffer[] = "data";
    char        destination_buffer[5];
    void       *copy_result;

    copy_result = ft_memcpy(ft_nullptr, source_buffer, sizeof(source_buffer));
    FT_ASSERT(copy_result == ft_nullptr);

    copy_result = ft_memcpy(destination_buffer, source_buffer, sizeof(source_buffer));
    FT_ASSERT(copy_result == destination_buffer);
    FT_ASSERT_EQ(0, std::memcmp(destination_buffer, source_buffer, sizeof(source_buffer)));
    return (1);
}

FT_TEST(test_ft_errno_strlen_resets_after_failure)
{
    const char *valid_string;
    int         string_length;

    valid_string = "example";
    string_length = ft_strlen(ft_nullptr);
    FT_ASSERT_EQ(0, string_length);

    string_length = ft_strlen(valid_string);
    FT_ASSERT_EQ(7, string_length);
    return (1);
}

FT_TEST(test_ft_errno_is_thread_local)
{
    std::atomic<int> thread_string_length;
    std::thread      worker_thread;

    thread_string_length.store(0);
    worker_thread = std::thread(
        [&thread_string_length]()
        {
            thread_string_length.store(ft_strlen("worker"));
            return ;
        });
    worker_thread.join();
    FT_ASSERT_EQ(6, thread_string_length.load());
    return (1);
}

FT_TEST(test_ft_perror_null_message_resets_errno)
{
    int     pipe_fds[2];
    int     saved_stderr;
    ssize_t read_count;
    char    buffer[256];
    const char *expected_message;

    expected_message = ft_strerror(FT_ERR_INVALID_ARGUMENT);
    FT_ASSERT_EQ(0, pipe(pipe_fds));
    saved_stderr = dup(2);
    FT_ASSERT(saved_stderr >= 0);
    FT_ASSERT(dup2(pipe_fds[1], 2) >= 0);
    FT_ASSERT_EQ(0, close(pipe_fds[1]));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_perror(ft_nullptr, FT_ERR_INVALID_ARGUMENT));
    FT_ASSERT(dup2(saved_stderr, 2) >= 0);
    FT_ASSERT_EQ(0, close(saved_stderr));

    read_count = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(std::strstr(buffer, expected_message) != ft_nullptr);
    FT_ASSERT_EQ(0, close(pipe_fds[0]));
    return (1);
}

FT_TEST(test_ft_perror_prefixes_custom_message_resets_errno)
{
    int     pipe_fds[2];
    int     saved_stderr;
    ssize_t read_count;
    char    buffer[256];
    const char *expected_message;

    expected_message = ft_strerror(FT_ERR_IO);
    FT_ASSERT_EQ(0, pipe(pipe_fds));
    saved_stderr = dup(2);
    FT_ASSERT(saved_stderr >= 0);
    FT_ASSERT(dup2(pipe_fds[1], 2) >= 0);
    FT_ASSERT_EQ(0, close(pipe_fds[1]));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_perror("custom context", FT_ERR_IO));
    FT_ASSERT(dup2(saved_stderr, 2) >= 0);
    FT_ASSERT_EQ(0, close(saved_stderr));

    read_count = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(std::strstr(buffer, "custom context: ") != ft_nullptr);
    FT_ASSERT(std::strstr(buffer, expected_message) != ft_nullptr);
    FT_ASSERT_EQ(0, close(pipe_fds[0]));
    return (1);
}

FT_TEST(test_errno_perror_preserves_long_prefix_and_final_newline)
{
    const std::string prefix(700U, 'p');
    const std::string expected = prefix + ": I/O error";
    int32_t pipe_descriptors[2];
    FT_ASSERT_EQ(0, pipe(pipe_descriptors));
    const int32_t saved_stderr = dup(2);
    FT_ASSERT(saved_stderr >= 0);
    FT_ASSERT(dup2(pipe_descriptors[1], 2) >= 0);
    FT_ASSERT_EQ(0, close(pipe_descriptors[1]));
    const int32_t write_status = ft_perror(prefix.c_str(), FT_ERR_IO);
    FT_ASSERT(dup2(saved_stderr, 2) >= 0);
    FT_ASSERT_EQ(0, close(saved_stderr));
    std::string output;
    char chunk[256];
    ssize_t count = read(pipe_descriptors[0], chunk, sizeof(chunk));
    while (count > 0)
    {
        output.append(chunk, static_cast<std::size_t>(count));
        count = read(pipe_descriptors[0], chunk, sizeof(chunk));
    }
    FT_ASSERT_EQ(0, close(pipe_descriptors[0]));
    FT_ASSERT_EQ(0, count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, write_status);
    FT_ASSERT(output == expected + "\n" || output == expected + "\r\n");
    return (1);
}
