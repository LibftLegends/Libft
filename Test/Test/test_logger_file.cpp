#include "../test_internal.hpp"
#include "../../Modules/Logger/logger.hpp"
#include "../../Modules/Logger/logger_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <thread>
#include <unistd.h>
#include <sys/stat.h>
#include <utime.h>
#include <ctime>
#include <string>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int  g_file_sink_hook_calls = 0;

static int create_logger_temp_file(char *template_path, size_t template_path_size)
{
    return (test_create_temp_file_from_template(template_path,
            template_path_size, ft_nullptr));
}

static int64_t    logger_partial_write_hook(int file_descriptor,
    const void *buffer, ft_size_t count)
{
    ft_size_t  chunk_size;
    int64_t    bytes_written;
    int64_t    write_result;

    g_file_sink_hook_calls += 1;
    chunk_size = 4;
    if (count < chunk_size)
        chunk_size = count;
    bytes_written = 0;
    write_result = cmp_write(file_descriptor, buffer, chunk_size, &bytes_written);
    if (write_result != FT_ERR_SUCCESS)
        return (write_result);
    return (bytes_written);
}

static int64_t logger_write_with_extra_bytes(int32_t descriptor,
    const void *message, ft_size_t count)
{
    char external[1024];
    ft_memset(external, 'e', sizeof(external));
    int64_t written = 0;
    if (cmp_write(descriptor, external, sizeof(external), &written) != FT_ERR_SUCCESS
        || written != static_cast<int64_t>(sizeof(external))
        || cmp_write(descriptor, message, count, &written) != FT_ERR_SUCCESS)
        return (-1);
    return (written);
}

FT_TEST(test_logger_rotation_checks_written_file_size_after_append)
{
    char path[256];
    const int32_t descriptor = create_logger_temp_file(path, sizeof(path));
    FT_ASSERT(descriptor >= 0);
    close(descriptor);
    ft_log_close();
    FT_ASSERT_EQ(0, ft_log_set_file(path, 512U));
    su_set_write_syscall_hook(logger_write_with_extra_bytes);
    ft_log_info("cross threshold after write");
    su_reset_write_syscall_hook();
    ft_log_close();

    const std::string rotated = std::string(path) + ".1";
    ft_size_t size = 0U;
    t_time modification = 0;
    const int32_t status = cmp_file_get_size_and_mtime(rotated.c_str(), &size,
        &modification, ft_nullptr);
    unlink(path);
    unlink(rotated.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status);
    FT_ASSERT(size > 1024U);
    FT_ASSERT(modification > 0);
    return (1);
}

FT_TEST(test_logger_file_sink_prepare_thread_safety_initializes_mutex)
{
    char        template_path[256];
    int         temp_fd;
    s_file_sink sink;
    ft_bool     lock_acquired;

    temp_fd = create_logger_temp_file(template_path, sizeof(template_path));
    FT_ASSERT(temp_fd >= 0);
    sink.file_descriptor = temp_fd;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.path.initialize(template_path));
    sink.max_size = 0;
    sink.retention_count = 1;
    sink.max_age_seconds = 0;
    FT_ASSERT_EQ(0, file_sink_prepare_thread_safety(&sink));
    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, file_sink_lock(&sink, &lock_acquired));
    FT_ASSERT(lock_acquired == FT_TRUE);
    file_sink_unlock(&sink, lock_acquired);
    file_sink_teardown_thread_safety(&sink);
    close(temp_fd);
    unlink(template_path);
    return (1);
}

FT_TEST(test_logger_file_sink_lock_blocks_until_release)
{
    char                     template_path[256];
    int                      temp_fd;
    s_file_sink              sink;
    ft_bool                  main_lock_acquired;
    std::atomic<bool>        ready;
    std::atomic<bool>        worker_failed;
    std::atomic<long long>   wait_duration_ms;
    std::thread              worker;

    temp_fd = create_logger_temp_file(template_path, sizeof(template_path));
    FT_ASSERT(temp_fd >= 0);
    sink.file_descriptor = temp_fd;
    sink.max_size = 0;
    sink.retention_count = 1;
    sink.max_age_seconds = 0;
    FT_ASSERT_EQ(0, file_sink_prepare_thread_safety(&sink));
    main_lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, file_sink_lock(&sink, &main_lock_acquired));
    FT_ASSERT(main_lock_acquired == FT_TRUE);
    ready.store(false);
    worker_failed.store(false);
    wait_duration_ms.store(0);
    worker = std::thread([&sink, &ready, &worker_failed, &wait_duration_ms]() {
        ft_bool                                    worker_lock_acquired;
        std::chrono::steady_clock::time_point      start_time;
        std::chrono::steady_clock::time_point      end_time;

        worker_lock_acquired = FT_FALSE;
        ready.store(true);
        start_time = std::chrono::steady_clock::now();
        if (file_sink_lock(&sink, &worker_lock_acquired) != 0)
        {
            worker_failed.store(true);
            wait_duration_ms.store(-1);
            return ;
        }
        end_time = std::chrono::steady_clock::now();
        wait_duration_ms.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time).count());
        if (!worker_lock_acquired)
            worker_failed.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        file_sink_unlock(&sink, worker_lock_acquired);
    });
    while (!ready.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    file_sink_unlock(&sink, main_lock_acquired);
    worker.join();
    FT_ASSERT(worker_failed.load() == false);
    FT_ASSERT(wait_duration_ms.load() >= 40);
    file_sink_teardown_thread_safety(&sink);
    close(temp_fd);
    unlink(template_path);
    return (1);
}

FT_TEST(test_logger_file_sink_uses_system_utils_write)
{
    char    template_path[256];
    int     temp_fd;
    char    read_buffer[512];
    ssize_t read_count;
    int     log_fd;

    temp_fd = create_logger_temp_file(template_path, sizeof(template_path));
    FT_ASSERT(temp_fd >= 0);
    close(temp_fd);
    ft_log_close();
    FT_ASSERT_EQ(0, ft_log_set_file(template_path, 4096));
    g_file_sink_hook_calls = 0;
    su_set_write_syscall_hook(logger_partial_write_hook);
    ft_log_info("file sink test message");
    su_reset_write_syscall_hook();
    FT_ASSERT(g_file_sink_hook_calls > 1);
    ft_log_close();
    log_fd = open(template_path, O_RDONLY);
    FT_ASSERT(log_fd >= 0);
    read_count = read(log_fd, read_buffer, sizeof(read_buffer) - 1);
    FT_ASSERT(read_count > 0);
    read_buffer[read_count] = '\0';
    FT_ASSERT(ft_strstr(read_buffer, "file sink test message") != ft_nullptr);
    close(log_fd);
    unlink(template_path);
    return (1);
}

FT_TEST(test_logger_set_file_missing_directory)
{
    const char *directory_path;
    const char *file_path;
    int result;

    directory_path = "/tmp/libft_logger_missing_dir";
    file_path = "/tmp/libft_logger_missing_dir/log.txt";
    (void)rmdir(directory_path);
    errno = 0;
    result = ft_log_set_file(file_path, 1024);
    FT_ASSERT_EQ(-1, result);
    return (1);
}

FT_TEST(test_logger_rotate_fstat_failure_sets_errno)
{
    s_file_sink sink;

    sink.file_descriptor = -1;
    ft_string invalid_path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_path.initialize("/tmp/libft_logger_invalid_fd"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.path.initialize(invalid_path));
    sink.max_size = 1;
    errno = 0;
    ft_log_rotate(&sink);
    return (1);
}

FT_TEST(test_logger_rotate_success_clears_errno)
{
    char        template_path[256];
    int         temp_fd;
    ssize_t     write_result;
    s_file_sink sink;
    ft_string   rotated_path;

    temp_fd = create_logger_temp_file(template_path, sizeof(template_path));
    FT_ASSERT(temp_fd >= 0);
    write_result = write(temp_fd, "rotation-test", 13);
    FT_ASSERT_EQ(13, write_result);
    sink.file_descriptor = temp_fd;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.path.initialize(template_path));
    sink.max_size = 4;
    errno = 0;
    ft_log_rotate(&sink);
    FT_ASSERT(sink.file_descriptor >= 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_path.initialize(sink.path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_path.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_path.append(".1"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_path.get_error());
    FT_ASSERT_EQ(0, access(rotated_path.c_str(), F_OK));
    close(sink.file_descriptor);
    unlink(template_path);
    unlink(rotated_path.c_str());
    return (1);
}

FT_TEST(test_logger_rotate_rename_failure_reopens_file)
{
    char        directory_template[] = "/tmp/libft_logger_rotate_fail_XXXXXX";
    char       *directory_path;
    ft_string   file_path;
    ft_string   long_name;
    int         file_descriptor;
    ssize_t     write_result;
    s_file_sink sink;

    directory_path = mkdtemp(directory_template);
    FT_ASSERT(directory_path != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_path.initialize(directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_path.append("/"));
#if defined(_WIN32)
    ft_bool path_found;
    ft_size_t long_name_length;

    path_found = FT_FALSE;
    long_name_length = 255;
    while (long_name_length > 0 && path_found == FT_FALSE)
    {
        ft_string candidate_path;
        ft_string rotated_candidate_path;
        int rotated_file_descriptor;
        std::string long_name_src(long_name_length, 'a');

        FT_ASSERT_EQ(FT_ERR_SUCCESS, candidate_path.initialize(directory_path));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, candidate_path.append("/"));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, candidate_path.append(long_name_src.c_str()));
        file_descriptor = open(candidate_path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (file_descriptor >= 0)
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_candidate_path.initialize(candidate_path));
            FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_candidate_path.append(".1"));
            rotated_file_descriptor = open(rotated_candidate_path.c_str(),
                    O_CREAT | O_WRONLY | O_APPEND, 0644);
            if (rotated_file_descriptor < 0)
            {
                FT_ASSERT_EQ(FT_ERR_SUCCESS, file_path.initialize(candidate_path));
                path_found = FT_TRUE;
            }
            else
            {
                close(rotated_file_descriptor);
                close(file_descriptor);
                unlink(candidate_path.c_str());
            }
        }
        if (path_found == FT_FALSE)
            long_name_length -= 1;
    }
    FT_ASSERT(path_found == FT_TRUE);
#else
    std::string long_name_src(255, 'a');
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_path.append(long_name_src.c_str()));
    file_descriptor = open(file_path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    FT_ASSERT(file_descriptor >= 0);
#endif
    write_result = write(file_descriptor, "trigger", 7);
    FT_ASSERT_EQ(7, write_result);
    sink.file_descriptor = file_descriptor;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.path.initialize(file_path));
    sink.max_size = 4;
    errno = 0;
    ft_log_rotate(&sink);
    FT_ASSERT_EQ(ENAMETOOLONG, errno);
    FT_ASSERT_EQ(file_descriptor, sink.file_descriptor);
    write_result = write(sink.file_descriptor, "ok", 2);
    FT_ASSERT_EQ(2, write_result);
    close(sink.file_descriptor);
    FT_ASSERT_EQ(0, unlink(file_path.c_str()));
    FT_ASSERT_EQ(0, rmdir(directory_path));
    return (1);
}

FT_TEST(test_logger_set_rotation_without_file_sink)
{
    ft_log_close();
    FT_ASSERT_EQ(-1, ft_log_set_rotation(1024, 2, 60));
    return (1);
}

FT_TEST(test_logger_rotation_by_age)
{
    char            template_path[256];
    int             temp_fd;
    struct utimbuf  timestamps;
    time_t          current_time;
    ft_string       rotated_path;
    int             log_fd;
    ssize_t         read_count;
    char            buffer[512];

    temp_fd = create_logger_temp_file(template_path, sizeof(template_path));
    FT_ASSERT(temp_fd >= 0);
    close(temp_fd);
    ft_log_close();
    FT_ASSERT_EQ(0, ft_log_set_file(template_path, 4096));
    FT_ASSERT_EQ(0, ft_log_set_rotation(4096, 1, 1));
    current_time = ::time(ft_nullptr);
    FT_ASSERT(current_time != static_cast<time_t>(-1));
    timestamps.actime = current_time - 5;
    timestamps.modtime = current_time - 5;
    FT_ASSERT_EQ(0, utime(template_path, &timestamps));
    ft_log_info("age-rotation-test");
    ft_log_close();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_path.initialize(template_path));
    rotated_path += ".1";
    log_fd = open(rotated_path.c_str(), O_RDONLY);
    FT_ASSERT(log_fd >= 0);
    read_count = read(log_fd, buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(ft_strstr(buffer, "age-rotation-test") != ft_nullptr);
    close(log_fd);
    log_fd = open(template_path, O_RDONLY);
    FT_ASSERT(log_fd >= 0);
    read_count = read(log_fd, buffer, sizeof(buffer));
    FT_ASSERT_EQ(0, read_count);
    close(log_fd);
    unlink(rotated_path.c_str());
    unlink(template_path);
    return (1);
}

FT_TEST(test_logger_rotation_getter_reports_config)
{
    char            template_path[256];
    int             temp_fd;
    ft_size_t       max_size;
    ft_size_t       retention_count;
    unsigned int    max_age_seconds;

    temp_fd = create_logger_temp_file(template_path, sizeof(template_path));
    FT_ASSERT(temp_fd >= 0);
    close(temp_fd);
    ft_log_close();
    FT_ASSERT_EQ(0, ft_log_set_file(template_path, 64));
    FT_ASSERT_EQ(0, ft_log_set_rotation(128, 3, 42));
    max_size = 0;
    retention_count = 0;
    max_age_seconds = 0;
    FT_ASSERT_EQ(0, ft_log_get_rotation(&max_size, &retention_count, &max_age_seconds));
    FT_ASSERT_EQ(static_cast<ft_size_t>(128), max_size);
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), retention_count);
    FT_ASSERT_EQ(static_cast<unsigned int>(42), max_age_seconds);
    ft_log_close();
    unlink(template_path);
    return (1);
}

FT_TEST(test_logger_rotation_retention_limit)
{
    char        template_path[256];
    int         temp_fd;
    ft_string   rotated_one_path;
    ft_string   rotated_two_path;
    ft_string   rotated_three_path;
    int         fd;
    ssize_t     read_count;
    char        buffer[512];

    temp_fd = create_logger_temp_file(template_path, sizeof(template_path));
    FT_ASSERT(temp_fd >= 0);
    close(temp_fd);
    ft_log_close();
    FT_ASSERT_EQ(0, ft_log_set_file(template_path, 8));
    FT_ASSERT_EQ(0, ft_log_set_rotation(8, 2, 0));
    ft_log_info("retention-first");
    ft_log_info("retention-second");
    ft_log_info("retention-third");
    ft_log_close();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_one_path.initialize(template_path));
    rotated_one_path += ".1";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_one_path.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_two_path.initialize(template_path));
    rotated_two_path += ".2";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_two_path.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_three_path.initialize(template_path));
    rotated_three_path += ".3";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rotated_three_path.get_error());
    fd = open(rotated_one_path.c_str(), O_RDONLY);
    FT_ASSERT(fd >= 0);
    read_count = read(fd, buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(ft_strstr(buffer, "retention-third") != ft_nullptr);
    close(fd);
    fd = open(rotated_two_path.c_str(), O_RDONLY);
    FT_ASSERT(fd >= 0);
    read_count = read(fd, buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(ft_strstr(buffer, "retention-second") != ft_nullptr);
    close(fd);
    errno = 0;
    FT_ASSERT_EQ(-1, access(rotated_three_path.c_str(), F_OK));
    FT_ASSERT(cmp_map_system_error_to_ft(errno) == FT_ERR_IO
        || cmp_map_system_error_to_ft(errno) == FT_ERR_NOT_FOUND);
    unlink(rotated_one_path.c_str());
    unlink(rotated_two_path.c_str());
    unlink(template_path);
    return (1);
}
