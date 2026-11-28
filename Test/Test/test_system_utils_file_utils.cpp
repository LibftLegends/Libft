#include "../test_internal.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#if defined(_WIN32) || defined(_WIN64)
# include <direct.h>
#else
# include <unistd.h>
#endif

static void su_cleanup_path(const char *path)
{
    if (path == ft_nullptr)
        return ;
    std::remove(path);
    return ;
}

static void su_remove_directory(const char *path)
{
#if defined(_WIN32) || defined(_WIN64)
    _rmdir(path);
#else
    rmdir(path);
#endif
    return ;
}

static int su_write_text_file(const char *path, const char *contents)
{
    su_file *file_stream;
    size_t text_length;
    size_t written;

    file_stream = su_fopen(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_stream == ft_nullptr)
        return (-1);
    text_length = std::strlen(contents);
    if (text_length == 0)
    {
        su_fclose(file_stream);
        return (0);
    }
    written = su_fwrite(contents, 1, text_length, file_stream);
    if (written != text_length)
    {
        su_fclose(file_stream);
        return (-1);
    }
    if (su_fclose(file_stream) != 0)
        return (-1);
    return (0);
}

static int su_read_text_file(const char *path, ft_string &result)
{
    su_file *file_stream;
    char buffer[128];
    int initialize_error;

    file_stream = su_fopen(path, O_RDONLY);
    if (file_stream == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);

    initialize_error = result.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)su_fclose(file_stream);
        return (initialize_error);
    }
    while (true)
    {
        size_t bytes_read = su_fread(buffer, 1, sizeof(buffer), file_stream);
        if (bytes_read == 0)
            break ;
        size_t index = 0;
        while (index < bytes_read)
        {
            if (result.append(buffer[index]) != FT_ERR_SUCCESS)
                break ;
            index += 1;
        }
    }
    su_fclose(file_stream);
    return (FT_ERR_SUCCESS);
}

static void su_prepare_directory_fixture(void)
{
    int error_code = FT_ERR_SUCCESS;

    cmp_file_create_directory("su_copy_dir_source", 0755, &error_code);
    cmp_file_create_directory("su_copy_dir_source/nested", 0755, &error_code);
    su_write_text_file("su_copy_dir_source/root.txt", "root payload\n");
    su_write_text_file("su_copy_dir_source/nested/item.txt", "nested payload\n");
    return ;
}

static void su_cleanup_directory_fixture(void)
{
    su_cleanup_path("su_copy_dir_source/root.txt");
    su_cleanup_path("su_copy_dir_source/nested/item.txt");
    su_remove_directory("su_copy_dir_source/nested");
    su_remove_directory("su_copy_dir_source");
    su_cleanup_path("su_copy_dir_destination/root.txt");
    su_cleanup_path("su_copy_dir_destination/nested/item.txt");
    su_remove_directory("su_copy_dir_destination/nested");
    su_remove_directory("su_copy_dir_destination");
    return ;
}

FT_TEST(test_su_copy_file_copies_contents)
{
    const char *source_path = "su_copy_file_source.txt";
    const char *destination_path = "su_copy_file_destination.txt";
    ft_string contents;

    su_cleanup_path(source_path);
    su_cleanup_path(destination_path);
    FT_ASSERT_EQ(0, su_write_text_file(source_path, "example payload"));
    FT_ASSERT_EQ(0, su_write_text_file(destination_path, "a longer previous payload"));
    FT_ASSERT_EQ(0, su_copy_file(source_path, destination_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_read_text_file(destination_path, contents));
    FT_ASSERT(strcmp(contents.c_str(), "example payload") == 0);
    su_cleanup_path(source_path);
    su_cleanup_path(destination_path);
    return (1);
}

FT_TEST(test_su_copy_file_refuses_source_and_hard_link_destinations)
{
    const char *source = "su_copy_alias_source.txt";
    const char *alias = "su_copy_alias_destination.txt";
    su_cleanup_path(source);
    su_cleanup_path(alias);
    FT_ASSERT_EQ(0, su_write_text_file(source, "retain original bytes"));
    std::error_code error;
    std::filesystem::create_hard_link(source, alias, error);
    if (error)
    {
        su_cleanup_path(source);
        FT_ASSERT(!error);
    }
    const int32_t same_result = su_copy_file(source, source);
    const int32_t alias_result = su_copy_file(source, alias);
    ft_string bytes;
    const int32_t read_result = su_read_text_file(source, bytes);
    su_cleanup_path(alias);
    su_cleanup_path(source);
    FT_ASSERT_EQ(-1, same_result);
    FT_ASSERT_EQ(-1, alias_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_result);
    FT_ASSERT_EQ(0, std::strcmp("retain original bytes", bytes.c_str()));
    return (1);
}

FT_TEST(test_su_copy_file_copies_binary_chunks_without_stream_allocations)
{
    const char *source_path = "su_copy_file_binary_source.bin";
    const char *destination_path = "su_copy_file_binary_destination.bin";
    std::array<uint8_t, 70001U> source{};
    std::array<uint8_t, 70001U> destination{};
    ft_size_t index = 0U;
    while (index < source.size())
    {
        source[index] = static_cast<uint8_t>(index);
        ++index;
    }
    FILE *input = std::fopen(source_path, "wb");
    FT_ASSERT(input != ft_nullptr);
    const auto written = std::fwrite(source.data(), 1U, source.size(), input);
    const int32_t source_closed = std::fclose(input);
    FT_ASSERT_EQ(source.size(), written);
    FT_ASSERT_EQ(0, source_closed);

    su_force_file_stream_allocation_failure(FT_TRUE);
    const int32_t copied = su_copy_file(source_path, destination_path);
    su_force_file_stream_allocation_failure(FT_FALSE);
    FT_ASSERT_EQ(0, copied);
    FILE *output = std::fopen(destination_path, "rb");
    FT_ASSERT(output != ft_nullptr);
    const auto bytes_read = std::fread(destination.data(), 1U, destination.size(), output);
    const int32_t trailing = std::fgetc(output);
    const int32_t destination_closed = std::fclose(output);
    su_cleanup_path(source_path);
    su_cleanup_path(destination_path);
    FT_ASSERT_EQ(destination.size(), bytes_read);
    FT_ASSERT_EQ(EOF, trailing);
    FT_ASSERT_EQ(0, destination_closed);
    FT_ASSERT(source == destination);
    return (1);
}

FT_TEST(test_su_copy_directory_recursive_replicates_structure)
{
    ft_string contents;

    su_cleanup_directory_fixture();
    su_prepare_directory_fixture();
    su_remove_directory("su_copy_dir_destination");
    FT_ASSERT_EQ(0, su_copy_directory_recursive("su_copy_dir_source", "su_copy_dir_destination"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_read_text_file(
        "su_copy_dir_destination/root.txt", contents));
    FT_ASSERT(strcmp(contents.c_str(), "root payload\n") == 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, contents.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_read_text_file(
        "su_copy_dir_destination/nested/item.txt", contents));
    FT_ASSERT(strcmp(contents.c_str(), "nested payload\n") == 0);
    su_cleanup_directory_fixture();
    return (1);
}

FT_TEST(test_su_inspect_permissions_reports_mode_bits)
{
    const char *path = "su_permissions_target.txt";
    mode_t permissions;

    su_cleanup_path(path);
    FT_ASSERT_EQ(0, su_write_text_file(path, "permissions"));
    FT_ASSERT_EQ(0, su_inspect_permissions(path, &permissions));
#if defined(_WIN32) || defined(_WIN64)
    FT_ASSERT((permissions & _S_IFREG) != 0);
#else
    FT_ASSERT((permissions & S_IFREG) != 0);
#endif
    su_cleanup_path(path);
    return (1);
}

FT_TEST(test_su_copy_directory_handles_invalid_arguments)
{
    FT_ASSERT_EQ(-1, su_copy_directory_recursive(ft_nullptr, "target"));
    FT_ASSERT_EQ(-1, su_copy_directory_recursive("missing", "target"));
    return (1);
}

FT_TEST(test_su_copy_directory_refuses_destination_inside_source)
{
    const char *source = "su_copy_ancestor_source";
    const char *destination = "su_copy_ancestor_source/./inside";
    int32_t error = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cmp_file_create_directory(source, 0755, &error));
    const int32_t same = su_copy_directory_recursive(source, source);
    const int32_t nested = su_copy_directory_recursive(source, destination);
    const ft_bool created = std::filesystem::exists(destination);
    (void)cmp_file_remove_directory(source);
    FT_ASSERT_EQ(-1, same);
    FT_ASSERT_EQ(-1, nested);
    FT_ASSERT_EQ(FT_FALSE, created);
    return (1);
}

FT_TEST(test_su_inspect_permissions_rejects_null_output)
{
    FT_ASSERT_EQ(-1, su_inspect_permissions("any", ft_nullptr));
    return (1);
}
