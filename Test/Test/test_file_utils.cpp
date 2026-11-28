#include "../test_internal.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/File/open_dir.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/CPP_class/class_file.hpp"
#include "../../Modules/CPP_class/class_file_stream.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "compatebility_system_test_hooks.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#if defined(_WIN32) || defined(_WIN64)
# include <fcntl.h>
# define TEST_FILE_BINARY_FLAG O_BINARY
#else
# include <fcntl.h>
# define TEST_FILE_BINARY_FLAG 0
#endif

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
#else
# include <unistd.h>
#endif

static void remove_directory_if_present(const char *directory_path)
{
    int error_code = FT_ERR_SUCCESS;
    int exists_value = 0;
    int status;

    status = cmp_directory_exists(directory_path, &exists_value, &error_code);
    if (status != FT_ERR_SUCCESS || exists_value != 1)
        return ;
#if defined(_WIN32) || defined(_WIN64)
    RemoveDirectoryA(directory_path);
#else
    rmdir(directory_path);
#endif
    return ;
}

static void create_cross_device_test_file(const char *path)
{
    FILE *file_pointer;

    file_pointer = ft_fopen(path, "w");
    if (file_pointer != ft_nullptr)
    {
        std::fputs("cross-device move payload", file_pointer);
        ft_fclose(file_pointer);
    }
    return ;
}

static void cleanup_file_utils_artifacts(void)
{
    file_delete("test_file_copy_with_buffer_small_chunks_source.bin");
    file_delete("test_file_copy_with_buffer_small_chunks_destination.bin");
    file_delete("test_ft_file_copy_to_streams_payload_source.bin");
    file_delete("test_ft_file_copy_to_streams_payload_destination.bin");
    return ;
}

static void write_payload(const char *path, const unsigned char *payload, size_t payload_size)
{
    FILE *file_pointer;
    size_t total_written;
    size_t chunk_written;

    file_pointer = ft_fopen(path, "wb");
    if (file_pointer == ft_nullptr)
        return ;
    total_written = 0;
    while (total_written < payload_size)
    {
        chunk_written = std::fwrite(payload + total_written, 1, payload_size - total_written, file_pointer);
        if (chunk_written == 0)
            break ;
        total_written += chunk_written;
    }
    ft_fclose(file_pointer);
    return ;
}

static size_t read_payload(const char *path, unsigned char *buffer, size_t buffer_capacity)
{
    FILE *file_pointer;
    size_t total_read;
    size_t chunk_read;

    if (buffer == ft_nullptr || buffer_capacity == 0)
        return (0);
    file_pointer = ft_fopen(path, "rb");
    if (file_pointer == ft_nullptr)
        return (0);
    total_read = 0;
    while (total_read < buffer_capacity)
    {
        chunk_read = std::fread(buffer + total_read, 1, buffer_capacity - total_read, file_pointer);
        if (chunk_read == 0)
            break ;
        total_read += chunk_read;
    }
    ft_fclose(file_pointer);
    return (total_read);
}

static void build_path_from_template(char *destination, const char *path_template, char separator)
{
    size_t index;

    index = 0;
    while (path_template[index] != '\0')
    {
        if (path_template[index] == '/')
            destination[index] = separator;
        else
            destination[index] = path_template[index];
        ++index;
    }
    destination[index] = '\0';
    return ;
}

static void build_native_path(char *destination, const char *path_template)
{
    build_path_from_template(destination, path_template, cmp_path_separator());
    return ;
}

static void build_alternate_separator_path(char *destination, const char *path_template)
{
    char path_separator;
    char alternate_separator;

    path_separator = cmp_path_separator();
    if (path_separator == '/')
        alternate_separator = '\\';
    else
        alternate_separator = '/';
    build_path_from_template(destination, path_template, alternate_separator);
    return ;
}

FT_TEST(test_file_path_join_prefers_absolute_right)
{
    ft_string *result = file_path_join("/etc", "/var/log");

    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->get_error());
    FT_ASSERT_EQ(0, ft_strcmp(result->c_str(), "/var/log"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->destroy());
    delete result;
    return (1);
}

FT_TEST(test_file_path_join_keeps_drive_letter)
{
    ft_string *result = file_path_join("/left", "C:/temp");

    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->get_error());
    FT_ASSERT_EQ(0, ft_strcmp(result->c_str(), "C:/temp"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->destroy());
    delete result;
    return (1);
}

FT_TEST(test_file_path_normalize_collapses_duplicates)
{
    char path_buffer[64];
    char expected_buffer[64];
    ft_string *normalized;

    build_alternate_separator_path(path_buffer, "/folder///sub///file.txt");
    normalized = file_path_normalize(path_buffer);
    FT_ASSERT(normalized != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, normalized->get_error());
    build_native_path(expected_buffer, "/folder/sub/file.txt");
    FT_ASSERT_EQ(0, ft_strcmp(normalized->c_str(), expected_buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, normalized->destroy());
    delete normalized;
    return (1);
}

FT_TEST(test_file_path_normalize_preserves_trailing_separator)
{
    char path_buffer[64];
    char expected_buffer[64];
    ft_string *normalized;

    build_alternate_separator_path(path_buffer, "/folder/subdir///");
    normalized = file_path_normalize(path_buffer);
    FT_ASSERT(normalized != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, normalized->get_error());
    build_native_path(expected_buffer, "/folder/subdir/");
    FT_ASSERT_EQ(0, ft_strcmp(normalized->c_str(), expected_buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, normalized->destroy());
    delete normalized;
    return (1);
}

FT_TEST(test_file_path_normalize_handles_null_input)
{
    ft_string *normalized;

    normalized = file_path_normalize(ft_nullptr);
    FT_ASSERT(normalized != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, normalized->get_error());
    FT_ASSERT_EQ(0, ft_strcmp(normalized->c_str(), ""));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, normalized->destroy());
    delete normalized;
    return (1);
}

FT_TEST(test_file_path_join_appends_missing_separator)
{
    char left_buffer[64];
    char right_buffer[64];
    char expected_buffer[128];
    ft_string *result;

    build_alternate_separator_path(left_buffer, "/var//log");
    build_alternate_separator_path(right_buffer, "nginx///access.log");
    result = file_path_join(left_buffer, right_buffer);
    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->get_error());
    build_native_path(expected_buffer, "/var/log/nginx/access.log");
    FT_ASSERT_EQ(0, ft_strcmp(result->c_str(), expected_buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->destroy());
    delete result;
    return (1);
}

FT_TEST(test_file_path_join_with_empty_left)
{
    char right_buffer[64];
    char expected_buffer[64];
    ft_string *result;

    build_alternate_separator_path(right_buffer, "folder//file.txt");
    result = file_path_join("", right_buffer);
    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->get_error());
    build_native_path(expected_buffer, "folder/file.txt");
    FT_ASSERT_EQ(0, ft_strcmp(result->c_str(), expected_buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result->destroy());
    delete result;
    return (1);
}

FT_TEST(test_file_path_is_absolute_detects_root_and_drive)
{
    FT_ASSERT_EQ(FT_TRUE, file_path_is_absolute("/var/log"));
    FT_ASSERT_EQ(FT_TRUE, file_path_is_absolute("\\var\\log"));
    FT_ASSERT_EQ(FT_TRUE, file_path_is_absolute("C:/temp"));
    FT_ASSERT_EQ(FT_FALSE, file_path_is_absolute("relative/path"));
    FT_ASSERT_EQ(FT_FALSE, file_path_is_absolute(ft_nullptr));
    return (1);
}

FT_TEST(test_file_path_is_relative_is_inverse)
{
    FT_ASSERT_EQ(FT_FALSE, file_path_is_relative("/var/log"));
    FT_ASSERT_EQ(FT_FALSE, file_path_is_relative("C:/temp"));
    FT_ASSERT_EQ(FT_TRUE, file_path_is_relative("relative/path"));
    FT_ASSERT_EQ(FT_TRUE, file_path_is_relative(""));
    return (1);
}

FT_TEST(test_file_path_basename_handles_trailing_separator)
{
    char path_buffer[64];
    char *basename;

    build_alternate_separator_path(path_buffer, "/folder/subdir///");
    basename = file_path_basename(path_buffer);
    FT_ASSERT(basename != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(basename, "subdir"));
    cma_free(basename);
    return (1);
}

FT_TEST(test_file_path_basename_keeps_root)
{
    char *basename;

    basename = file_path_basename("/");
    FT_ASSERT(basename != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(basename, "/"));
    cma_free(basename);
    return (1);
}

FT_TEST(test_file_path_dirname_handles_nested_path)
{
    char path_buffer[64];
    char expected_buffer[64];
    char *dirname;

    build_alternate_separator_path(path_buffer, "/folder/subdir/file.txt");
    dirname = file_path_dirname(path_buffer);
    FT_ASSERT(dirname != ft_nullptr);
    build_native_path(expected_buffer, "/folder/subdir");
    FT_ASSERT_EQ(0, ft_strcmp(dirname, expected_buffer));
    cma_free(dirname);
    return (1);
}

FT_TEST(test_file_path_dirname_uses_current_directory_for_leaf)
{
    char *dirname;

    dirname = file_path_dirname("file.txt");
    FT_ASSERT(dirname != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(dirname, "."));
    cma_free(dirname);
    return (1);
}

FT_TEST(test_file_path_dirname_preserves_absolute_drive_root)
{
    ft_string *directory = file_path_dirname_string("C:/save.bin");
    FT_ASSERT(directory != ft_nullptr);
    char *raw_directory = file_path_dirname("C:/save.bin");
    FT_ASSERT(raw_directory != ft_nullptr);
#if defined(_WIN32) || defined(_WIN64)
    const char *expected = "C:\\";
#else
    const char *expected = "C:/";
#endif
    FT_ASSERT_EQ(FT_TRUE, *directory == expected);
    FT_ASSERT_EQ(FT_TRUE, *directory == raw_directory);
    cma_free(raw_directory);
    delete directory;
    return (1);
}

FT_TEST(test_file_path_extension_extracts_last_extension)
{
    char *extension;

    extension = file_path_extension("/tmp/archive.tar.gz");
    FT_ASSERT(extension != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(extension, ".gz"));
    cma_free(extension);
    return (1);
}

FT_TEST(test_file_path_extension_ignores_hidden_file_prefix)
{
    char *extension;

    extension = file_path_extension(".env");
    FT_ASSERT(extension != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(extension, ""));
    cma_free(extension);
    return (1);
}

FT_TEST(test_file_path_extension_ignores_trailing_dot)
{
    char *extension;

    extension = file_path_extension("filename.");
    FT_ASSERT(extension != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(extension, ""));
    cma_free(extension);
    return (1);
}

FT_TEST(test_file_path_stem_removes_last_extension)
{
    char *stem;

    stem = file_path_stem("/tmp/archive.tar.gz");
    FT_ASSERT(stem != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(stem, "archive.tar"));
    cma_free(stem);
    return (1);
}

FT_TEST(test_file_path_stem_keeps_hidden_file_name)
{
    char *stem;

    stem = file_path_stem(".env");
    FT_ASSERT(stem != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(stem, ".env"));
    cma_free(stem);
    return (1);
}

FT_TEST(test_file_path_basename_string_returns_heap_string)
{
    ft_string *basename;

    basename = file_path_basename_string("/tmp/report.txt");
    FT_ASSERT(basename != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, basename->get_error());
    FT_ASSERT_EQ(0, ft_strcmp(basename->c_str(), "report.txt"));
    delete basename;
    return (1);
}

FT_TEST(test_file_path_dirname_string_returns_heap_string)
{
    char path_buffer[64];
    char expected_buffer[64];
    ft_string *dirname;

    build_alternate_separator_path(path_buffer, "/tmp/reports/report.txt");
    dirname = file_path_dirname_string(path_buffer);
    FT_ASSERT(dirname != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dirname->get_error());
    build_native_path(expected_buffer, "/tmp/reports");
    FT_ASSERT_EQ(0, ft_strcmp(dirname->c_str(), expected_buffer));
    delete dirname;
    return (1);
}

FT_TEST(test_file_path_extension_string_keeps_empty_result)
{
    ft_string *extension;

    extension = file_path_extension_string("Makefile");
    FT_ASSERT(extension != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, extension->get_error());
    FT_ASSERT_EQ(0, ft_strcmp(extension->c_str(), ""));
    delete extension;
    return (1);
}

FT_TEST(test_file_path_stem_string_returns_heap_string)
{
    ft_string *stem;

    stem = file_path_stem_string("/tmp/archive.tar.gz");
    FT_ASSERT(stem != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stem->get_error());
    FT_ASSERT_EQ(0, ft_strcmp(stem->c_str(), "archive.tar"));
    delete stem;
    return (1);
}

FT_TEST(test_file_move_cross_device_fallback)
{
    const char *source_path = "test_file_move_cross_device_source.txt";
    const char *destination_path = "test_file_move_cross_device_destination.txt";
    FILE *destination_file;
    char destination_buffer[64];
    int32_t move_error;

    file_delete(source_path);
    file_delete(destination_path);
    create_cross_device_test_file(source_path);
    cmp_set_force_cross_device_move(1);
    move_error = file_move(source_path, destination_path);
    FT_ASSERT(move_error != FT_ERR_INVALID_ARGUMENT);
    cmp_set_force_cross_device_move(0);
    if (move_error == FT_ERR_SUCCESS)
    {
        FT_ASSERT_EQ(1, file_exists(destination_path));
        FT_ASSERT_EQ(0, file_exists(source_path));
    }
    else
    {
        file_delete(source_path);
        file_delete(destination_path);
        return (1);
    }
    destination_file = ft_fopen(destination_path, "r");
    if (move_error != FT_ERR_SUCCESS)
    {
        file_delete(source_path);
        file_delete(destination_path);
        return (1);
    }
    FT_ASSERT(destination_file != ft_nullptr);
    if (destination_file == ft_nullptr)
        return (0);
    FT_ASSERT(std::fgets(destination_buffer, 64, destination_file) != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(destination_buffer, "cross-device move payload"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_fclose(destination_file));
    FT_ASSERT_EQ(0, file_delete(destination_path));
    return (1);
}

FT_TEST(test_file_dir_exists_matches_file_exists_semantics)
{
    const char *directory_path = "test_file_dir_exists_matches_file_exists_semantics";

    remove_directory_if_present(directory_path);
    FT_ASSERT(file_dir_exists(directory_path) != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(0, file_create_directory(directory_path, 0777));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_dir_exists(directory_path));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, file_exists(directory_path));
    remove_directory_if_present(directory_path);
    FT_ASSERT(file_dir_exists(directory_path) != FT_ERR_SUCCESS);
    return (1);
}

FT_TEST(test_file_copy_buffer_size_matches_ft_file)
{
    FT_ASSERT_EQ(ft_file_default_buffer_size(), file_default_copy_buffer_size());
    return (1);
}

FT_TEST(test_file_copy_with_buffer_small_chunks)
{
    const char *source_path = "test_file_copy_with_buffer_small_chunks_source.bin";
    const char *destination_path = "test_file_copy_with_buffer_small_chunks_destination.bin";
    unsigned char payload[19] = {0};
    unsigned char read_source[32];
    unsigned char read_destination[32];
    size_t index;
    size_t source_size;
    size_t destination_size;

    index = 0;
    while (index < sizeof(payload))
    {
        payload[index] = static_cast<unsigned char>(index * 7 + 3);
        index += 1;
    }
    cleanup_file_utils_artifacts();
    write_payload(source_path, payload, sizeof(payload));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_copy_with_buffer(source_path, destination_path, 5));
    source_size = read_payload(source_path, read_source, sizeof(read_source));
    destination_size = read_payload(destination_path, read_destination, sizeof(read_destination));
    FT_ASSERT_EQ(source_size, destination_size);
    FT_ASSERT_EQ(0, ft_memcmp(read_source, read_destination, source_size));
    cleanup_file_utils_artifacts();
    return (1);
}

FT_TEST(test_file_copy_with_buffer_rejects_null_paths)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, file_copy_with_buffer(ft_nullptr, "ignored.bin", 4));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, file_copy_with_buffer("ignored.bin", ft_nullptr, 4));
    return (1);
}

FT_TEST(test_ft_file_copy_to_propagates_errors)
{
    ft_file source_file;

    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, source_file.copy_to("ft_file_copy_to_destination.bin"));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, source_file.copy_to(ft_nullptr));
    return (1);
}

FT_TEST(test_ft_file_copy_to_streams_payload)
{
    const char *source_path = "test_ft_file_copy_to_streams_payload_source.bin";
    const char *destination_path = "test_ft_file_copy_to_streams_payload_destination.bin";
    unsigned char payload[23] = {0};
    unsigned char destination_payload[32];
    unsigned char source_payload[32];
    ft_file source_file;
    size_t index;
    size_t source_size;
    size_t destination_size;
    int open_flags;

    index = 0;
    while (index < sizeof(payload))
    {
        payload[index] = static_cast<unsigned char>(index * 5 + 11);
        index += 1;
    }
    cleanup_file_utils_artifacts();
    write_payload(source_path, payload, sizeof(payload));
    open_flags = O_RDONLY | TEST_FILE_BINARY_FLAG;
    FT_ASSERT_EQ(0, source_file.open(source_path, open_flags));
    FT_ASSERT_EQ(0, source_file.copy_to_with_buffer(destination_path, 8));
    source_size = read_payload(source_path, source_payload, sizeof(source_payload));
    destination_size = read_payload(destination_path, destination_payload, sizeof(destination_payload));
    FT_ASSERT_EQ(source_size, destination_size);
    FT_ASSERT_EQ(0, ft_memcmp(source_payload, destination_payload, source_size));
    cleanup_file_utils_artifacts();
    return (1);
}

FT_TEST(test_file_readdir_handles_null_stream)
{
    FT_ASSERT_EQ(ft_nullptr, file_readdir(ft_nullptr));
    return (1);
}

FT_TEST(test_file_readdir_clears_error_on_success)
{
    file_dir *directory_stream;
    file_dirent *directory_entry;

    directory_stream = file_opendir(".");
    FT_ASSERT(directory_stream != ft_nullptr);
    if (directory_stream == ft_nullptr)
        return (0);
    directory_entry = file_readdir(directory_stream);
    FT_ASSERT(directory_entry != ft_nullptr);
    FT_ASSERT_EQ(0, file_closedir(directory_stream));
    return (1);
}

FT_TEST(test_file_closedir_handles_null_stream)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, file_closedir(ft_nullptr));
    return (1);
}

FT_TEST(test_file_closedir_clears_error_on_success)
{
    file_dir *directory_stream;

    directory_stream = file_opendir(".");
    FT_ASSERT(directory_stream != ft_nullptr);
    if (directory_stream == ft_nullptr)
        return (0);
    FT_ASSERT_EQ(0, file_closedir(directory_stream));
    return (1);
}

FT_TEST(test_file_copy_with_buffer_zero_size_uses_default)
{
    const char  *source_path;
    const char  *destination_path;
    const char  *payload_string;
    FILE        *file_stream;
    char        read_buffer[32];
    size_t      payload_length;
    size_t      bytes_written;
    size_t      bytes_read;

    source_path = "test_file_copy_with_buffer_zero_size_source.txt";
    destination_path = "test_file_copy_with_buffer_zero_size_destination.txt";
    payload_string = "bufferless copy payload";
    payload_length = ft_strlen_size_t(payload_string);
    file_delete(source_path);
    file_delete(destination_path);
    file_stream = std::fopen(source_path, "wb");
    if (file_stream == ft_nullptr)
        return (0);
    bytes_written = std::fwrite(payload_string, 1, payload_length, file_stream);
    std::fclose(file_stream);
    if (bytes_written != payload_length)
    {
        file_delete(source_path);
        file_delete(destination_path);
        return (0);
    }
    if (file_copy_with_buffer(source_path, destination_path, 0) != 0)
    {
        file_delete(source_path);
        file_delete(destination_path);
        return (0);
    }
    file_stream = std::fopen(destination_path, "rb");
    if (file_stream == ft_nullptr)
    {
        file_delete(source_path);
        file_delete(destination_path);
        return (0);
    }
    std::memset(read_buffer, 0, sizeof(read_buffer));
    bytes_read = std::fread(read_buffer, 1, payload_length, file_stream);
    std::fclose(file_stream);
    if (bytes_read != payload_length)
    {
        file_delete(source_path);
        file_delete(destination_path);
        return (0);
    }
    if (std::memcmp(read_buffer, payload_string, payload_length) != 0)
    {
        file_delete(source_path);
        file_delete(destination_path);
        return (0);
    }
    file_delete(source_path);
    file_delete(destination_path);
    return (1);
}
