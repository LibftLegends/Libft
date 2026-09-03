#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/File/file_utils.hpp"
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

FT_TEST(test_file_write_all_atomic_replaces_content)
{
    const char *file_path;
    ft_string content;

    file_path = "test_file_write_all_atomic.txt";
    (void)file_delete(file_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(file_path, "old", 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all_atomic(file_path, "new-value", 9));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(file_path, content));
    FT_ASSERT_EQ(FT_TRUE, content == "new-value");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(file_path));
    return (1);
}

FT_TEST(test_file_write_all_atomic_cleans_up_temp_file_after_open_failure)
{
    FT_ASSERT_NE(FT_ERR_ALREADY_EXISTS, file_write_all_atomic(
            "test_file_failure_missing_parent/value.txt", "x", 1));
    FT_ASSERT_EQ(FILE_TYPE_MISSING, file_get_type(
            "test_file_failure_missing_parent/value.txt.tmp"));
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, file_write_all_atomic("test_invalid.txt",
            ft_nullptr, 1));
    return (1);
}

FT_TEST(test_file_write_all_atomic_concurrent_writers_keep_complete_payloads)
{
    ft_string file_path;
    std::vector<std::thread> writers;
    std::atomic<int32_t> first_error(FT_ERR_SUCCESS);
    const int32_t writer_count = 8;
    const int32_t iterations = 32;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path(
            "libft_file_atomic_race", "txt", &file_path));
    (void)file_delete(file_path.c_str());
    for (int32_t writer = 0; writer < writer_count; ++writer)
    {
        writers.emplace_back([&file_path, &first_error, writer, iterations]()
        {
            const std::string payload = "writer-" + std::to_string(writer)
                + ": complete atomic payload\n";
            for (int32_t iteration = 0; iteration < iterations; ++iteration)
            {
                const int32_t error_code = file_write_all_atomic(file_path.c_str(),
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
        FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(file_path.c_str(), contents));
        bool matched = false;
        for (int32_t writer = 0; writer < writer_count; ++writer)
        {
            const std::string payload = "writer-" + std::to_string(writer)
                + ": complete atomic payload\n";
            if (contents == payload.c_str())
                matched = true;
        }
        FT_ASSERT_EQ(true, matched);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, contents.destroy());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(file_path.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_path.destroy());
    return (1);
}
