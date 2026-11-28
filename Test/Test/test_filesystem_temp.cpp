#include "../test_internal.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

FT_TEST(test_filesystem_temp_path_keeps_parallel_requests_distinct)
{
    constexpr uint32_t THREAD_COUNT = 8;
    constexpr uint32_t PATHS_PER_THREAD = 32;
    std::vector<std::string> paths(THREAD_COUNT * PATHS_PER_THREAD);
    std::vector<std::thread> workers;
    std::atomic<uint32_t> ready{0};
    std::atomic<ft_bool> start{FT_FALSE};
    std::atomic<int32_t> error_code{FT_ERR_SUCCESS};
    uint32_t thread_index = 0;
    while (thread_index < THREAD_COUNT)
    {
        workers.emplace_back([&, thread_index]()
        {
            ready.fetch_add(1);
            while (start.load() == FT_FALSE)
                std::this_thread::yield();
            uint32_t path_index = 0;
            ft_string path;
            while (path_index < PATHS_PER_THREAD)
            {
                const int32_t result = filesystem_temp_path("parallel", ".tmp", &path);
                if (result != FT_ERR_SUCCESS)
                {
                    error_code.store(result);
                    return ;
                }
                paths[thread_index * PATHS_PER_THREAD + path_index] = path.c_str();
                ++path_index;
            }
            return ;
        });
        ++thread_index;
    }
    while (ready.load() < THREAD_COUNT)
        std::this_thread::yield();
    start.store(FT_TRUE);
    thread_index = 0;
    while (thread_index < THREAD_COUNT)
    {
        workers[thread_index].join();
        ++thread_index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code.load());
    std::sort(paths.begin(), paths.end());
    FT_ASSERT(std::adjacent_find(paths.begin(), paths.end()) == paths.end());
    return (1);
}

FT_TEST(test_filesystem_temp_path_supports_prefix_aliased_to_output)
{
    ft_string path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize("aliased"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path(path.c_str(), ".tmp", &path));
    FT_ASSERT(ft_strstr(path.c_str(), "aliased_") != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, filesystem_has_extension(path.c_str(), ".tmp"));
    return (1);
}
