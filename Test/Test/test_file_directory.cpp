#include "../test_internal.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/File/open_dir.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#if defined(_WIN32) || defined(_WIN64)
# include <direct.h>
# include <windows.h>
#else
# include <sys/stat.h>
# include <unistd.h>
#endif

FT_TEST(test_file_cmp_dir_open_null_path)
{
    int error_code = FT_ERR_SUCCESS;

    FT_ASSERT_EQ(ft_nullptr, cmp_dir_open(ft_nullptr, &error_code));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, error_code);
    return (1);
}

FT_TEST(test_file_cmp_dir_open_allocation_failure)
{
    file_dir *directory_stream;
    int error_code = FT_ERR_SUCCESS;

    cma_set_alloc_limit(1);
    directory_stream = cmp_dir_open(".", &error_code);
    FT_ASSERT_EQ(ft_nullptr, directory_stream);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error_code);
    cma_set_alloc_limit(0);
    return (1);
}

FT_TEST(test_file_cmp_dir_open_os_error)
{
    int error_code = FT_ERR_SUCCESS;

    FT_ASSERT_EQ(ft_nullptr, cmp_dir_open("cmp_dir_open_os_error_missing", &error_code));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, error_code);
    return (1);
}

FT_TEST(test_file_cmp_dir_read_null_stream)
{
    int error_code = FT_ERR_SUCCESS;

    FT_ASSERT_EQ(ft_nullptr, cmp_dir_read(ft_nullptr, &error_code));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, error_code);
    return (1);
}

FT_TEST(test_file_cmp_dir_read_success_resets_errno)
{
    file_dir *directory_stream;
    file_dirent *directory_entry;
    int error_code = FT_ERR_INVALID_ARGUMENT;

    directory_stream = cmp_dir_open(".", &error_code);
    if (directory_stream == ft_nullptr)
        return (0);
    directory_entry = cmp_dir_read(directory_stream, &error_code);
    FT_ASSERT(directory_entry != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    cmp_dir_close(directory_stream, &error_code);
    return (1);
}

FT_TEST(test_file_cmp_directory_exists_null_path)
{
    int error_code = FT_ERR_SUCCESS;
    int exists_value = 1;

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cmp_directory_exists(ft_nullptr,
        &exists_value, &error_code));
    FT_ASSERT_EQ(0, exists_value);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, error_code);
    return (1);
}

FT_TEST(test_file_cmp_directory_exists_existing_directory)
{
    int error_code = FT_ERR_INVALID_ARGUMENT;
    int exists_value = 0;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cmp_directory_exists(".", &exists_value,
        &error_code));
    FT_ASSERT_EQ(1, exists_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    return (1);
}

FT_TEST(test_file_cmp_directory_exists_missing_directory)
{
    int error_code = FT_ERR_SUCCESS;
    int exists_value = 1;

    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, cmp_directory_exists(
        "cmp_directory_exists_missing_directory", &exists_value, &error_code));
    FT_ASSERT_EQ(0, exists_value);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, error_code);
    return (1);
}

FT_TEST(test_file_cmp_directory_exists_file_path)
{
    const char *file_path = "cmp_directory_exists_file_path.txt";
    std::FILE *file_handle;
    int error_code = FT_ERR_INVALID_ARGUMENT;
    int exists_value = 1;

    file_handle = std::fopen(file_path, "w");
    if (file_handle == ft_nullptr)
        return (0);
    std::fclose(file_handle);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cmp_directory_exists(file_path, &exists_value,
        &error_code));
    FT_ASSERT_EQ(0, exists_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    std::remove(file_path);
    return (1);
}

FT_TEST(test_file_cmp_dir_independent_entry_buffers)
{
    struct cmp_dir_thread_result
    {
        uintptr_t entry_address;
        char entry_name[256];
        bool read_success;
    };
    const char *directory_path_string;
    char file_path[512];
    std::FILE *file_handle;
    file_dir *directory_one;
    file_dir *directory_two;
    cmp_dir_thread_result result_one;
    cmp_dir_thread_result result_two;
    std::atomic<bool> thread_one_ready;
    std::atomic<bool> thread_two_ready;
    std::atomic<bool> start_reads;
    std::thread thread_one;
    std::thread thread_two;
    file_dirent *next_entry;
    bool cleanup_directory_one;
    bool cleanup_directory_two;
    bool test_success;
    int error_code;

    directory_path_string = ft_nullptr;
    file_handle = ft_nullptr;
    directory_one = ft_nullptr;
    directory_two = ft_nullptr;
    cleanup_directory_one = false;
    cleanup_directory_two = false;
    test_success = false;
    next_entry = ft_nullptr;
    ft_bzero(file_path, sizeof(file_path));
    ft_bzero(&result_one, sizeof(result_one));
    ft_bzero(&result_two, sizeof(result_two));
    error_code = FT_ERR_SUCCESS;
#if defined(_WIN32) || defined(_WIN64)
    char directory_path_buffer[MAX_PATH];
    DWORD process_id;

    process_id = GetCurrentProcessId();
    std::snprintf(directory_path_buffer, sizeof(directory_path_buffer),
        "cmp_dir_thread_safe_%lu", static_cast<unsigned long>(process_id));
    if (_mkdir(directory_path_buffer) != 0)
        return (0);
    directory_path_string = directory_path_buffer;
#else
    char directory_template[] = "cmp_dir_thread_safeXXXXXX";

    if (mkdtemp(directory_template) == ft_nullptr)
        return (0);
    directory_path_string = directory_template;
#endif
    std::snprintf(file_path, sizeof(file_path), "%s/thread_entry.txt",
        directory_path_string);
    file_handle = std::fopen(file_path, "w");
    if (file_handle == ft_nullptr)
        goto cleanup;
    if (std::fputs("entry\n", file_handle) == EOF)
    {
        std::fclose(file_handle);
        file_handle = ft_nullptr;
        goto cleanup;
    }
    std::fclose(file_handle);
    file_handle = ft_nullptr;
    directory_one = cmp_dir_open(directory_path_string, &error_code);
    if (directory_one == ft_nullptr)
        goto cleanup;
    cleanup_directory_one = true;
    directory_two = cmp_dir_open(directory_path_string, &error_code);
    if (directory_two == ft_nullptr)
        goto cleanup;
    cleanup_directory_two = true;
    thread_one_ready.store(false);
    thread_two_ready.store(false);
    start_reads.store(false);
    thread_one = std::thread([&result_one, directory_one, &thread_one_ready,
            &start_reads]() {
        thread_one_ready.store(true);
        while (start_reads.load() == false)
            std::this_thread::yield();
        int local_error = FT_ERR_SUCCESS;
        file_dirent *entry = cmp_dir_read(directory_one, &local_error);
        if (entry == ft_nullptr)
            return ;
        result_one.entry_address = reinterpret_cast<uintptr_t>(entry);
        ft_strlcpy(result_one.entry_name, entry->d_name,
            sizeof(result_one.entry_name));
        result_one.read_success = true;
    });
    thread_two = std::thread([&result_two, directory_two, &thread_two_ready,
            &start_reads]() {
        thread_two_ready.store(true);
        while (start_reads.load() == false)
            std::this_thread::yield();
        int local_error = FT_ERR_SUCCESS;
        file_dirent *entry = cmp_dir_read(directory_two, &local_error);
        if (entry == ft_nullptr)
            return ;
        result_two.entry_address = reinterpret_cast<uintptr_t>(entry);
        ft_strlcpy(result_two.entry_name, entry->d_name,
            sizeof(result_two.entry_name));
        result_two.read_success = true;
    });
    while (thread_one_ready.load() == false || thread_two_ready.load() == false)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    start_reads.store(true);
    thread_one.join();
    thread_two.join();
    FT_ASSERT(result_one.read_success);
    FT_ASSERT(result_two.read_success);
    FT_ASSERT(result_one.entry_address != 0);
    FT_ASSERT(result_two.entry_address != 0);
    FT_ASSERT(result_one.entry_address != result_two.entry_address);
    FT_ASSERT(result_one.entry_name[0] != '\0');
    FT_ASSERT(result_two.entry_name[0] != '\0');
    char preserved_entry_name[256];
    ft_strlcpy(preserved_entry_name, result_two.entry_name,
        sizeof(preserved_entry_name));
    next_entry = cmp_dir_read(directory_one, &error_code);
    if (next_entry != ft_nullptr)
        FT_ASSERT(next_entry->d_name[0] != '\0');
    FT_ASSERT_EQ(0, ft_strncmp(result_two.entry_name, preserved_entry_name,
            sizeof(preserved_entry_name)));
    test_success = true;
cleanup:
    if (directory_two != ft_nullptr && cleanup_directory_two == true)
        cmp_dir_close(directory_two, &error_code);
    if (directory_one != ft_nullptr && cleanup_directory_one == true)
        cmp_dir_close(directory_one, &error_code);
    if (file_handle != ft_nullptr)
        std::fclose(file_handle);
    if (file_path[0] != '\0')
        std::remove(file_path);
    if (directory_path_string != ft_nullptr)
    {
#if defined(_WIN32) || defined(_WIN64)
        _rmdir(directory_path_string);
#else
        rmdir(directory_path_string);
#endif
    }
    if (test_success == false)
        return (0);
    return (1);
}
