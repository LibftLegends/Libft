#include "../test_internal.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/File/open_dir.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstdio>
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <atomic>
#include <string>
#include <thread>
#include <vector>

struct filesystem_walk_test_context
{
    ft_size_t entries;
    ft_bool saw_file;
    ft_bool saw_directory;
};

struct filesystem_walk_failure_context
{
    ft_size_t entries;
};

static int32_t filesystem_walk_test_callback(const char *path, ft_bool is_directory,
    void *user_context)
{
    filesystem_walk_test_context *context;

    if (path == ft_nullptr || user_context == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    context = static_cast<filesystem_walk_test_context *>(user_context);
    context->entries++;
    if (is_directory == FT_TRUE)
    {
        context->saw_directory = FT_TRUE;
    }
    else
    {
        context->saw_file = FT_TRUE;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t filesystem_walk_failure_callback(const char *path, ft_bool is_directory,
    void *user_context)
{
    filesystem_walk_failure_context *context;

    (void)is_directory;
    if (path == ft_nullptr || user_context == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    context = static_cast<filesystem_walk_failure_context *>(user_context);
    context->entries++;
    return (FT_ERR_TERMINATED);
}

static int32_t filesystem_make_test_directory(ft_string *path, const char *name)
{
    int32_t error_code;

    error_code = filesystem_temp_path(name, ft_nullptr, path);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    (void)file_delete_recursive(path->c_str());
    return (file_create_directory(path->c_str(), 0700));
}

FT_TEST(test_filesystem_path_wrappers)
{
    ft_string *normalized;
    ft_string *joined;
    ft_string *extension;
    ft_string *stem;

    normalized = filesystem_normalize_path("alpha//beta/../gamma.txt");
    FT_ASSERT(normalized != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, normalized->get_error());
    joined = filesystem_join_path("alpha", "beta.txt");
    FT_ASSERT(joined != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, joined->get_error());
    extension = filesystem_extension(joined->c_str());
    FT_ASSERT(extension != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, extension->get_error());
    FT_ASSERT_EQ(FT_TRUE, *extension == ".txt");
    stem = filesystem_stem(joined->c_str());
    FT_ASSERT(stem != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stem->get_error());
    FT_ASSERT_EQ(FT_TRUE, filesystem_has_extension(joined->c_str(), ".txt"));
    FT_ASSERT_EQ(FT_TRUE, filesystem_is_relative("alpha/beta"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, normalized->destroy());
    delete normalized;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, joined->destroy());
    delete joined;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, extension->destroy());
    delete extension;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stem->destroy());
    delete stem;
    return (1);
}

FT_TEST(test_filesystem_temp_path_uses_prefix_and_extension)
{
    ft_string path;
    int32_t error_code;

    error_code = filesystem_temp_path("libft_fs_test", "tmp", &path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.get_error());
    FT_ASSERT_EQ(FT_TRUE, filesystem_has_extension(path.c_str(), ".tmp"));
    return (1);
}

FT_TEST(test_filesystem_atomic_write_creates_target_contents)
{
    ft_string path;
    FILE *file_stream;
    char buffer[8];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path("libft_atomic", "txt", &path));
    (void)file_delete(path.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_atomic_write(path.c_str(), "hello", 5));
    file_stream = ft_fopen(path.c_str(), "rb");
    FT_ASSERT(file_stream != ft_nullptr);
    FT_ASSERT_EQ(static_cast<size_t>(5), std::fread(buffer, 1, 5, file_stream));
    buffer[5] = '\0';
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_fclose(file_stream));
    FT_ASSERT_EQ('h', buffer[0]);
    FT_ASSERT_EQ('o', buffer[4]);
    (void)file_delete(path.c_str());
    return (1);
}

FT_TEST(test_filesystem_atomic_write_concurrent_writers_keep_complete_payloads)
{
    ft_string path;
    std::vector<std::thread> writers;
    std::atomic<int32_t> first_error(FT_ERR_SUCCESS);
    const int32_t writer_count = 8;
    const int32_t iterations = 32;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path("libft_atomic_race",
        "txt", &path));
    (void)file_delete(path.c_str());
    for (int32_t writer = 0; writer < writer_count; ++writer)
    {
        writers.emplace_back([&path, &first_error, writer, iterations]()
        {
            const std::string payload = "writer-" + std::to_string(writer)
                + ": complete atomic payload\n";
            for (int32_t iteration = 0; iteration < iterations; ++iteration)
            {
                const int32_t error_code = filesystem_atomic_write(path.c_str(),
                    payload.data(), payload.size());
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
                + ": complete atomic payload\n";
            if (contents == payload.c_str())
                matched = true;
        }
        FT_ASSERT_EQ(true, matched);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, contents.destroy());
    }
    (void)file_delete(path.c_str());
    return (1);
}

FT_TEST(test_filesystem_walk_recursive_visits_children)
{
    ft_string root_path;
    ft_string *child_directory;
    ft_string *child_file;
    filesystem_walk_test_context context;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_make_test_directory(&root_path,
            "libft_walk_root"));
    child_directory = filesystem_join_path(root_path.c_str(), "child");
    FT_ASSERT(child_directory != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_directory->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(child_directory->c_str(), 0700));
    child_file = filesystem_join_path(child_directory->c_str(), "entry.txt");
    FT_ASSERT(child_file != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_file->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_atomic_write(child_file->c_str(), "x", 1));
    context.entries = 0;
    context.saw_file = FT_FALSE;
    context.saw_directory = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_walk_recursive(root_path.c_str(),
            filesystem_walk_test_callback, &context));
    FT_ASSERT_EQ(FT_TRUE, context.saw_file);
    FT_ASSERT_EQ(FT_TRUE, context.saw_directory);
    (void)file_delete(child_file->c_str());
    (void)file_delete(child_directory->c_str());
    (void)file_delete(root_path.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_file->destroy());
    delete child_file;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_directory->destroy());
    delete child_directory;
    return (1);
}

FT_TEST(test_filesystem_basename_and_dirname_edge_cases)
{
    ft_string *basename;
    ft_string *dirname;

    basename = filesystem_basename("/tmp/example.txt");
    FT_ASSERT(basename != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, basename->get_error());
    FT_ASSERT_EQ(FT_TRUE, *basename == "example.txt");
    dirname = filesystem_dirname("/tmp/example.txt");
    FT_ASSERT(dirname != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dirname->get_error());
    FT_ASSERT_EQ(FT_TRUE, *dirname == "/tmp");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, basename->destroy());
    delete basename;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dirname->destroy());
    delete dirname;
    return (1);
}

FT_TEST(test_filesystem_extension_rejects_hidden_file_without_suffix)
{
    ft_string *extension;
    ft_string *stem;

    extension = filesystem_extension(".env");
    FT_ASSERT(extension != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, extension->get_error());
    FT_ASSERT_EQ(FT_TRUE, *extension == "");
    stem = filesystem_stem(".env");
    FT_ASSERT(stem != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stem->get_error());
    FT_ASSERT_EQ(FT_TRUE, *stem == ".env");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, extension->destroy());
    delete extension;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stem->destroy());
    delete stem;
    return (1);
}

FT_TEST(test_filesystem_has_extension_rejects_null_arguments)
{
    FT_ASSERT_EQ(FT_FALSE, filesystem_has_extension(ft_nullptr, ".txt"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_has_extension("file.txt", ft_nullptr));
    return (1);
}

FT_TEST(test_filesystem_temp_path_rejects_null_output)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER,
        filesystem_temp_path("libft_null", "tmp", ft_nullptr));
    return (1);
}

FT_TEST(test_filesystem_atomic_write_accepts_empty_payload)
{
    ft_string path;
    FILE *file_stream;
    char byte_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path("libft_empty", "bin", &path));
    (void)file_delete(path.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_atomic_write(path.c_str(), ft_nullptr, 0));
    file_stream = ft_fopen(path.c_str(), "rb");
    FT_ASSERT(file_stream != ft_nullptr);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), std::fread(&byte_value, 1, 1, file_stream));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_fclose(file_stream));
    (void)file_delete(path.c_str());
    return (1);
}

FT_TEST(test_filesystem_walk_recursive_propagates_callback_error)
{
    ft_string root_path;
    ft_string *child_file;
    filesystem_walk_failure_context context;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_make_test_directory(&root_path,
            "libft_walk_fail"));
    child_file = filesystem_join_path(root_path.c_str(), "entry.txt");
    FT_ASSERT(child_file != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_file->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_atomic_write(child_file->c_str(), "x", 1));
    context.entries = 0;
    FT_ASSERT_EQ(FT_ERR_TERMINATED, filesystem_walk_recursive(root_path.c_str(),
            filesystem_walk_failure_callback, &context));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), context.entries);
    (void)file_delete(child_file->c_str());
    (void)file_delete(root_path.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_file->destroy());
    delete child_file;
    return (1);
}

FT_TEST(test_filesystem_atomic_write_rejects_null_data_with_size)
{
    ft_string path;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path("libft_null_payload", "txt",
            &path));
    (void)file_delete(path.c_str());
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, filesystem_atomic_write(path.c_str(),
            ft_nullptr, 4));
    return (1);
}

FT_TEST(test_filesystem_atomic_write_reports_open_failure_for_missing_parent)
{
    FT_ASSERT_NE(FT_ERR_ALREADY_EXISTS, filesystem_atomic_write(
            "/tmp/libft_missing_parent_for_atomic_write/value.txt", "x", 1));
    return (1);
}

FT_TEST(test_filesystem_walk_recursive_reports_open_failure_for_missing_root)
{
    filesystem_walk_test_context context;

    context.entries = 0;
    context.saw_file = FT_FALSE;
    context.saw_directory = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_FILE_OPEN_FAILED, filesystem_walk_recursive(
            "/tmp/libft_missing_walk_root", filesystem_walk_test_callback, &context));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), context.entries);
    return (1);
}

FT_TEST(test_filesystem_walk_recursive_rejects_null_callback)
{
    ft_string root_path;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_make_test_directory(&root_path,
            "libft_walk_null_callback"));
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, filesystem_walk_recursive(root_path.c_str(),
            ft_nullptr, ft_nullptr));
    (void)file_delete(root_path.c_str());
    return (1);
}

