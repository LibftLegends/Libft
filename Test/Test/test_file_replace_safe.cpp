#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <atomic>
#include <string>
#include <thread>
#include <vector>

FT_TEST(test_file_replace_safe_writes_and_replaces_content)
{
    const char *path;
    ft_string content;

    path = "test_file_replace_safe.txt";
    (void)file_delete(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, "old", 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_replace_safe(path, "new-safe", 8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path, content));
    FT_ASSERT_EQ(FT_TRUE, content == "new-safe");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_file_replace_safe_rejects_invalid_arguments)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, file_replace_safe(ft_nullptr, "x", 1));
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, file_replace_safe("test_invalid.txt",
            ft_nullptr, 1));
    return (1);
}

FT_TEST(test_file_replace_safe_concurrent_writers_keep_complete_payloads)
{
    ft_string path;
    std::vector<std::thread> writers;
    std::atomic<int32_t> first_error(FT_ERR_SUCCESS);
    const int32_t writer_count = 8;
    const int32_t iterations = 16;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path(
            "libft_replace_safe_race", "txt", &path));
    (void)file_delete(path.c_str());
    for (int32_t writer = 0; writer < writer_count; ++writer)
    {
        writers.emplace_back([&path, &first_error, writer, iterations]()
        {
            const std::string payload = "writer-" + std::to_string(writer)
                + ": complete durable payload\n";
            for (int32_t iteration = 0; iteration < iterations; ++iteration)
            {
                const int32_t error_code = file_replace_safe(path.c_str(),
                    payload.c_str(), payload.size());
                if (error_code != FT_ERR_SUCCESS)
                {
                    int32_t expected = FT_ERR_SUCCESS;
                    first_error.compare_exchange_strong(expected, error_code);
                }
            }
        });
    }
    for (std::thread &writer : writers)
        writer.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_error.load());
    {
        ft_string contents;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, contents.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path.c_str(), contents));
        bool matched = false;
        for (int32_t writer = 0; writer < writer_count; ++writer)
        {
            const std::string payload = "writer-" + std::to_string(writer)
                + ": complete durable payload\n";
            if (contents == payload.c_str())
                matched = true;
        }
        FT_ASSERT_EQ(true, matched);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, contents.destroy());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.destroy());
    return (1);
}

FT_TEST(test_file_replace_safe_reports_file_sync_failure_before_replace)
{
    ft_string path;
    ft_string content;
    int32_t error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path(
            "libft_replace_sync_file", "txt", &path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path.c_str(), "old", 3));
    cmp_test_set_file_sync_failure(FT_ERR_IO);
    error_code = file_replace_safe(path.c_str(), "new", 3);
    cmp_test_clear_sync_failures();
    FT_ASSERT_EQ(FT_ERR_IO, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path.c_str(), content));
    FT_ASSERT_EQ(FT_TRUE, content == "old");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.destroy());
    return (1);
}

FT_TEST(test_file_replace_safe_reports_directory_sync_failure_after_replace)
{
    ft_string path;
    ft_string content;
    int32_t error_code;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path(
            "libft_replace_sync_directory", "txt", &path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path.c_str(), "old", 3));
    cmp_test_set_directory_sync_failure(FT_ERR_IO);
    error_code = file_replace_safe(path.c_str(), "new", 3);
    cmp_test_clear_sync_failures();
    FT_ASSERT_EQ(FT_ERR_IO, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path.c_str(), content));
    FT_ASSERT_EQ(FT_TRUE, content == "new");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.destroy());
    return (1);
}
