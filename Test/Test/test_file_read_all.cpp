#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/Observability/observability.hpp"
#include <cstring>

FT_TEST(test_file_read_all_round_trips_written_content)
{
    const char *file_path;
    ft_string content;

    file_path = "test_file_read_all.txt";
    (void)file_delete(file_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(file_path, "hello file", 10));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(file_path, content));
    FT_ASSERT_EQ(FT_TRUE, content == "hello file");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(file_path));
    return (1);
}

FT_TEST(test_file_read_all_rejects_null_path)
{
    ft_string content;

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, file_read_all(ft_nullptr, content));
    return (1);
}

FT_TEST(test_file_read_all_replaces_content_only_after_success)
{
    ft_string path;
    ft_string content;
    const char payload[] = {'a', '\0', 'b', '\0'};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.initialize("original"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path("libft_file_read_all", "bin", &path));
    FT_ASSERT_EQ(FT_ERR_FILE_OPEN_FAILED, file_read_all(path.c_str(), content));
    FT_ASSERT_EQ(FT_TRUE, content == "original");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path.c_str(), payload, sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path.c_str(), content));
    FT_ASSERT_EQ(sizeof(payload), content.size());
    FT_ASSERT_EQ(0, std::memcmp(payload, content.data(), sizeof(payload)));
    FT_ASSERT_EQ('\0', content.c_str()[sizeof(payload)]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path.c_str(), ft_nullptr, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path.c_str(), content));
    FT_ASSERT_EQ(FT_TRUE, content.empty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path.c_str()));
    return (1);
}

static constexpr char FILE_READ_ALL_ALIAS_PATH[] = "test_file_read_all_alias.txt";
static ft_bool g_file_read_all_trace_path_matches = FT_FALSE;

static void file_read_all_check_trace_path(const ft_observability_trace_event &event)
{
    if (event.phase == FT_OBSERVABILITY_TRACE_FINISH
        && event.module == FT_OBSERVABILITY_MODULE_FILE
        && std::strcmp(event.operation, "read_all") == 0
        && std::strcmp(event.resource, FILE_READ_ALL_ALIAS_PATH) == 0)
        g_file_read_all_trace_path_matches = FT_TRUE;
    return ;
}

FT_TEST(test_file_read_all_preserves_aliased_path_during_finish_trace)
{
    ft_string contents;
    const char payload[] = "new file contents";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(FILE_READ_ALL_ALIAS_PATH, payload, sizeof(payload) - 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, contents.initialize(FILE_READ_ALL_ALIAS_PATH));
    g_file_read_all_trace_path_matches = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_set_trace_hook(file_read_all_check_trace_path));
    const int32_t read_error = file_read_all(contents.c_str(), contents);
    observability_clear_trace_hook();
    const int32_t cleanup_error = file_delete(FILE_READ_ALL_ALIAS_PATH);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleanup_error);
    FT_ASSERT_EQ(FT_TRUE, g_file_read_all_trace_path_matches);
    FT_ASSERT_EQ(FT_TRUE, contents == payload);
    return (1);
}
