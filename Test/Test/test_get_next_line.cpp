#include "../test_internal.hpp"
#include "../../Modules/GetNextLine/get_next_line.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int  create_temp_fd_with_content(const char *content)
{
    char        temporary_path[1024];
    int         file_descriptor;
    ft_size_t   total_written;
    ft_size_t   content_length;
    ssize_t     write_result;

    file_descriptor = test_create_temp_file_from_template(temporary_path,
            sizeof(temporary_path), "gnl_testXXXXXX");
    if (file_descriptor < 0)
        return (-1);
    unlink(temporary_path);
    content_length = ft_strlen(content);
    total_written = 0;
    while (total_written < content_length)
    {
        write_result = write(file_descriptor, content + total_written,
                static_cast<unsigned int>(content_length - total_written));
        if (write_result <= 0)
        {
            close(file_descriptor);
            return (-1);
        }
        total_written += write_result;
    }
    if (lseek(file_descriptor, 0, SEEK_SET) < 0)
    {
        close(file_descriptor);
        return (-1);
    }
    return (file_descriptor);
}

static void close_temp_fd(int file_descriptor)
{
    gnl_clear_stream(file_descriptor);
    close(file_descriptor);
    return ;
}

static void    *failing_leftover_allocator(ft_size_t size)
{
    (void)size;
    return (ft_nullptr);
}

static void    *leftover_allocator_that_limits_map(ft_size_t size)
{
    void    *result;

    result = cma_malloc(size);
    if (result)
        cma_set_alloc_limit(1);
    return (result);
}

static int g_leftover_allocation_count = 0;

static void    *leftover_allocator_fail_after_one_success(ft_size_t size)
{
    g_leftover_allocation_count += 1;
    if (g_leftover_allocation_count >= 2)
        return (ft_nullptr);
    return (cma_malloc(size));
}

static void    *leftover_allocator_fail_after_two_successes(ft_size_t size)
{
    g_leftover_allocation_count += 1;
    if (g_leftover_allocation_count >= 3)
        return (ft_nullptr);
    return (cma_malloc(size));
}

static ft_size_t gnl_current_allocated_bytes(void)
{
    ft_size_t allocation_count;
    ft_size_t free_count;
    ft_size_t current_bytes;
    ft_size_t peak_bytes;
    int32_t stats_error;

    allocation_count = 0;
    free_count = 0;
    current_bytes = 0;
    peak_bytes = 0;
    stats_error = cma_get_extended_stats(&allocation_count, &free_count,
            &current_bytes, &peak_bytes);
    if (stats_error != FT_ERR_SUCCESS)
        return (0);
    return (current_bytes);
}

int test_get_next_line_basic(void)
{
    int     file_descriptor;
    char    *line_one;
    char    *line_two;
    char    *line_three;
    int     success;

    file_descriptor = create_temp_fd_with_content("Hello\nWorld\n");
    if (file_descriptor < 0)
        return (0);
    line_one = get_next_line(file_descriptor, 2);
    line_two = get_next_line(file_descriptor, 2);
    line_three = get_next_line(file_descriptor, 2);
success = line_one && line_two && !line_three
        && ft_strcmp(line_one, "Hello\n") == 0
        && ft_strcmp(line_two, "World\n") == 0;
    if (line_one)
        cma_free(line_one);
    if (line_two)
        cma_free(line_two);
    if (line_three)
        cma_free(line_three);
    close_temp_fd(file_descriptor);
    return (success);
}

int test_get_next_line_empty(void)
{
    int     file_descriptor;
    char    *line;

    file_descriptor = create_temp_fd_with_content("");
    if (file_descriptor < 0)
        return (0);
    line = get_next_line(file_descriptor, 4);
    if (line)
    {
        cma_free(line);
        close_temp_fd(file_descriptor);
        return (0);
    }
    close_temp_fd(file_descriptor);
    return (1);
}

int test_get_next_line_custom_buffer(void)
{
    int     file_descriptor;
    char    *line;
    int     success;

    file_descriptor = create_temp_fd_with_content("A long line without newline");
    if (file_descriptor < 0)
        return (0);
    line = get_next_line(file_descriptor, 3);
    success = line && ft_strcmp(line, "A long line without newline") == 0;
    if (line)
        cma_free(line);
    close_temp_fd(file_descriptor);
    return (success);
}

FT_TEST(test_get_next_line_zero_buffer_sets_errno)
{
    int     file_descriptor;
    char    *line;

    file_descriptor = create_temp_fd_with_content("Hello\n");
    FT_ASSERT(file_descriptor >= 0);
    line = get_next_line(file_descriptor, 0);
    FT_ASSERT(line == ft_nullptr);
    close_temp_fd(file_descriptor);
    return (1);
}

FT_TEST(test_get_next_line_closed_fd_returns_null)
{
    int     file_descriptor;
    char    *line;

    file_descriptor = create_temp_fd_with_content("data\n");
    FT_ASSERT(file_descriptor >= 0);
    close(file_descriptor);
    line = get_next_line(file_descriptor, 4);
    FT_ASSERT(line == ft_nullptr);
    gnl_clear_stream(file_descriptor);
    return (1);
}

FT_TEST(test_get_next_line_allocator_failure_sets_errno)
{
    int     file_descriptor;
    char    *line;

    file_descriptor = create_temp_fd_with_content("data\n");
    FT_ASSERT(file_descriptor >= 0);
    cma_set_alloc_limit(4);
    line = get_next_line(file_descriptor, 4);
    cma_set_alloc_limit(0);
    FT_ASSERT(line == ft_nullptr);
    close_temp_fd(file_descriptor);
    return (1);
}

FT_TEST(test_get_next_line_leftover_allocation_failure_is_safe)
{
    int     file_descriptor;
    char    *line;

    file_descriptor = create_temp_fd_with_content("line one\nline two\n");
    FT_ASSERT(file_descriptor >= 0);
    gnl_set_leftover_alloc_hook(failing_leftover_allocator);
    line = get_next_line(file_descriptor, 8);
    gnl_reset_leftover_alloc_hook();
    FT_ASSERT(line == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(file_descriptor));
    close(file_descriptor);
    return (1);
}

FT_TEST(test_get_next_line_hash_map_allocation_failure_is_reported)
{
    int     file_descriptor;
    char    *line;
    int     filler_descriptors[6];
    int     filler_count;
    int     cleanup_index;
    char    *filler_line;

    gnl_reset_all_streams();
    filler_count = 0;
    while (filler_count < 5)
    {
        filler_descriptors[filler_count] = create_temp_fd_with_content("alpha\nbeta\n");
        FT_ASSERT(filler_descriptors[filler_count] >= 0);
        filler_line = get_next_line(filler_descriptors[filler_count], 8);
        FT_ASSERT(filler_line != ft_nullptr);
        cma_free(filler_line);
        filler_count++;
    }
    file_descriptor = create_temp_fd_with_content("line one\nline two\n");
    FT_ASSERT(file_descriptor >= 0);
    cma_set_alloc_limit(0);
    gnl_set_leftover_alloc_hook(leftover_allocator_that_limits_map);
    line = get_next_line(file_descriptor, 8);
    gnl_reset_leftover_alloc_hook();
    cma_set_alloc_limit(0);
    FT_ASSERT(line == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(file_descriptor));
    close(file_descriptor);
    cleanup_index = 0;
    while (cleanup_index < filler_count)
    {
        close_temp_fd(filler_descriptors[cleanup_index]);
        cleanup_index++;
    }
    gnl_reset_all_streams();
    cma_set_alloc_limit(0);
    return (1);
}

FT_TEST(test_get_next_line_late_allocation_failure_releases_memory_after_cleanup)
{
    int         warmup_file_descriptor;
    int         file_descriptor;
    char        *line;
    ft_size_t   baseline_bytes;
    ft_size_t   bytes_after_failure;
    ft_size_t   bytes_after_cleanup;

    gnl_reset_all_streams();
    cma_set_alloc_limit(0);
    warmup_file_descriptor = create_temp_fd_with_content("warmup line\n");
    FT_ASSERT(warmup_file_descriptor >= 0);
    line = get_next_line(warmup_file_descriptor, 8);
    FT_ASSERT(line != ft_nullptr);
    cma_free(line);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(warmup_file_descriptor));
    close(warmup_file_descriptor);
    baseline_bytes = gnl_current_allocated_bytes();
    file_descriptor = create_temp_fd_with_content(
            "abcdefghijklmnopq\nabcdefghijklmnopq\n");
    FT_ASSERT(file_descriptor >= 0);
    cma_set_alloc_limit(16);
    line = get_next_line(file_descriptor, 8);
    cma_set_alloc_limit(0);
    FT_ASSERT(line == ft_nullptr);
    bytes_after_failure = gnl_current_allocated_bytes();
    FT_ASSERT(bytes_after_failure >= baseline_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(file_descriptor));
    close(file_descriptor);
    bytes_after_cleanup = gnl_current_allocated_bytes();
    FT_ASSERT(bytes_after_cleanup >= baseline_bytes);
    FT_ASSERT(bytes_after_cleanup <= bytes_after_failure);
    gnl_reset_all_streams();
    return (1);
}

FT_TEST(test_get_next_line_late_leftover_hook_failure_releases_memory_after_cleanup)
{
    int         warmup_file_descriptor;
    int         file_descriptor;
    char        *line;
    ft_size_t   baseline_bytes;
    ft_size_t   bytes_after_failure;
    ft_size_t   bytes_after_cleanup;

    gnl_reset_all_streams();
    cma_set_alloc_limit(0);
    warmup_file_descriptor = create_temp_fd_with_content("warmup line\n");
    FT_ASSERT(warmup_file_descriptor >= 0);
    line = get_next_line(warmup_file_descriptor, 8);
    FT_ASSERT(line != ft_nullptr);
    cma_free(line);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(warmup_file_descriptor));
    close(warmup_file_descriptor);
    baseline_bytes = gnl_current_allocated_bytes();
    file_descriptor = create_temp_fd_with_content("line one\nline two\nline three\n");
    FT_ASSERT(file_descriptor >= 0);
    g_leftover_allocation_count = 0;
    gnl_set_leftover_alloc_hook(leftover_allocator_fail_after_one_success);
    line = get_next_line(file_descriptor, 8);
    FT_ASSERT(line != ft_nullptr);
    cma_free(line);
    line = get_next_line(file_descriptor, 8);
    gnl_reset_leftover_alloc_hook();
    FT_ASSERT(line == ft_nullptr);
    bytes_after_failure = gnl_current_allocated_bytes();
    FT_ASSERT(bytes_after_failure >= baseline_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(file_descriptor));
    close(file_descriptor);
    bytes_after_cleanup = gnl_current_allocated_bytes();
    FT_ASSERT(bytes_after_cleanup >= baseline_bytes);
    FT_ASSERT(bytes_after_cleanup <= bytes_after_failure);
    gnl_reset_all_streams();
    return (1);
}

FT_TEST(test_get_next_line_very_late_growth_failure_releases_memory_after_cleanup)
{
    int         warmup_file_descriptor;
    int         file_descriptor;
    char        *line;
    ft_size_t   baseline_bytes;
    ft_size_t   bytes_after_failure;
    ft_size_t   bytes_after_cleanup;

    gnl_reset_all_streams();
    cma_set_alloc_limit(0);
    warmup_file_descriptor = create_temp_fd_with_content("warmup line\n");
    FT_ASSERT(warmup_file_descriptor >= 0);
    line = get_next_line(warmup_file_descriptor, 8);
    FT_ASSERT(line != ft_nullptr);
    cma_free(line);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(warmup_file_descriptor));
    close(warmup_file_descriptor);
    baseline_bytes = gnl_current_allocated_bytes();
    file_descriptor = create_temp_fd_with_content(
            "01234567890123456789012345678901234567890123456789\n");
    FT_ASSERT(file_descriptor >= 0);
    cma_set_alloc_limit(40);
    line = get_next_line(file_descriptor, 4);
    cma_set_alloc_limit(0);
    FT_ASSERT(line == ft_nullptr);
    bytes_after_failure = gnl_current_allocated_bytes();
    FT_ASSERT(bytes_after_failure >= baseline_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(file_descriptor));
    close(file_descriptor);
    bytes_after_cleanup = gnl_current_allocated_bytes();
    FT_ASSERT(bytes_after_cleanup >= baseline_bytes);
    FT_ASSERT(bytes_after_cleanup <= bytes_after_failure);
    return (1);
}

FT_TEST(test_get_next_line_second_call_late_failure_releases_memory_after_cleanup)
{
    int         warmup_file_descriptor;
    int         file_descriptor;
    char        *line;
    ft_size_t   baseline_bytes;
    ft_size_t   bytes_after_failure;
    ft_size_t   bytes_after_cleanup;

    gnl_reset_all_streams();
    cma_set_alloc_limit(0);
    warmup_file_descriptor = create_temp_fd_with_content("warmup line\n");
    FT_ASSERT(warmup_file_descriptor >= 0);
    line = get_next_line(warmup_file_descriptor, 8);
    FT_ASSERT(line != ft_nullptr);
    cma_free(line);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(warmup_file_descriptor));
    close(warmup_file_descriptor);
    baseline_bytes = gnl_current_allocated_bytes();
    file_descriptor = create_temp_fd_with_content(
            "first line\nabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\n");
    FT_ASSERT(file_descriptor >= 0);
    line = get_next_line(file_descriptor, 8);
    FT_ASSERT(line != ft_nullptr);
    cma_free(line);
    cma_set_alloc_limit(24);
    line = get_next_line(file_descriptor, 8);
    cma_set_alloc_limit(0);
    FT_ASSERT(line == ft_nullptr);
    bytes_after_failure = gnl_current_allocated_bytes();
    FT_ASSERT(bytes_after_failure >= baseline_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(file_descriptor));
    close(file_descriptor);
    bytes_after_cleanup = gnl_current_allocated_bytes();
    FT_ASSERT(bytes_after_cleanup >= baseline_bytes);
    FT_ASSERT(bytes_after_cleanup <= bytes_after_failure);
    return (1);
}

FT_TEST(test_get_next_line_late_leftover_failure_after_multiple_successes_is_cleaned)
{
    int         warmup_file_descriptor;
    int         file_descriptor;
    char        *line;
    ft_size_t   baseline_bytes;
    ft_size_t   bytes_after_failure;
    ft_size_t   bytes_after_cleanup;

    gnl_reset_all_streams();
    cma_set_alloc_limit(0);
    warmup_file_descriptor = create_temp_fd_with_content("warmup line\n");
    FT_ASSERT(warmup_file_descriptor >= 0);
    line = get_next_line(warmup_file_descriptor, 8);
    FT_ASSERT(line != ft_nullptr);
    cma_free(line);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(warmup_file_descriptor));
    close(warmup_file_descriptor);
    baseline_bytes = gnl_current_allocated_bytes();
    file_descriptor = create_temp_fd_with_content("line one\nline two\nline three\nline four\n");
    FT_ASSERT(file_descriptor >= 0);
    g_leftover_allocation_count = 0;
    gnl_set_leftover_alloc_hook(leftover_allocator_fail_after_two_successes);
    line = get_next_line(file_descriptor, 8);
    FT_ASSERT(line != ft_nullptr);
    cma_free(line);
    line = get_next_line(file_descriptor, 8);
    FT_ASSERT(line != ft_nullptr);
    cma_free(line);
    line = get_next_line(file_descriptor, 8);
    gnl_reset_leftover_alloc_hook();
    FT_ASSERT(line == ft_nullptr);
    bytes_after_failure = gnl_current_allocated_bytes();
    FT_ASSERT(bytes_after_failure >= baseline_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, gnl_clear_stream(file_descriptor));
    close(file_descriptor);
    bytes_after_cleanup = gnl_current_allocated_bytes();
    FT_ASSERT(bytes_after_cleanup >= baseline_bytes);
    FT_ASSERT(bytes_after_cleanup <= bytes_after_failure);
    return (1);
}
