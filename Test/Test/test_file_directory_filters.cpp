#include "../test_internal.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

static ft_bool file_test_copy_filter(const char *path, ft_bool is_directory,
    void *user_context)
{
    (void)user_context;
    if (path == ft_nullptr)
        return (FT_FALSE);
    if (is_directory == FT_TRUE)
        return (FT_TRUE);
    return (filesystem_has_extension(path, ".txt"));
}

static int32_t file_test_create_directory_tree(const char *root_path)
{
    ft_string *nested_path;
    int32_t error_code;

    error_code = file_create_directories(root_path, 0700);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    nested_path = file_path_join(root_path, "nested");
    if (nested_path == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (nested_path->get_error() != FT_ERR_SUCCESS)
    {
        error_code = nested_path->get_error();
        (void)nested_path->destroy();
        delete nested_path;
        return (error_code);
    }
    error_code = file_create_directories(nested_path->c_str(), 0700);
    (void)nested_path->destroy();
    delete nested_path;
    return (error_code);
}

FT_TEST(test_file_move_directory_filtered_removes_empty_source_directories)
{
    const char *source = "test_file_directory_filters_move_all_source";
    const char *destination = "test_file_directory_filters_move_all_destination";
    (void)file_delete_recursive(source);
    (void)file_delete_recursive(destination);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_test_create_directory_tree(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
        "test_file_directory_filters_move_all_source/nested/keep.txt", "kept", 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_move_directory_filtered(
        source, destination, file_test_copy_filter, ft_nullptr));
    FT_ASSERT_EQ(FILE_TYPE_MISSING, file_get_type(source));
    FT_ASSERT_EQ(FILE_TYPE_REGULAR, file_get_type(
        "test_file_directory_filters_move_all_destination/nested/keep.txt"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete_recursive(destination));
    return (1);
}

static int32_t file_test_assert_file_contents(const char *path,
    const char *expected_contents)
{
    ft_string contents;
    int32_t error_code;

    error_code = contents.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = file_read_all(path, contents);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)contents.destroy();
        return (error_code);
    }
    if (contents != expected_contents)
    {
        (void)contents.destroy();
        return (FT_ERR_INTERNAL);
    }
    (void)contents.destroy();
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_file_copy_directory_filtered_copies_matching_entries_only)
{
    const char *source_path;
    const char *destination_path;
    ft_string *destination_keep_path;
    ft_string *destination_nested_keep_path;
    ft_string *destination_skip_path;

    source_path = "test_file_directory_filters_source";
    destination_path = "test_file_directory_filters_copy_destination";
    (void)file_delete_recursive(source_path);
    (void)file_delete_recursive(destination_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_test_create_directory_tree(source_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
            "test_file_directory_filters_source/keep.txt", "keep", 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
            "test_file_directory_filters_source/skip.log", "skip", 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
            "test_file_directory_filters_source/nested/nested_keep.txt",
            "nested", 6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
            "test_file_directory_filters_source/nested/nested_skip.log",
            "nested_skip", 11));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_copy_directory_filtered(source_path,
            destination_path, file_test_copy_filter, ft_nullptr));
    destination_keep_path = file_path_join(destination_path, "keep.txt");
    FT_ASSERT(destination_keep_path != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_keep_path->get_error());
    destination_nested_keep_path = file_path_join(destination_path,
            "nested/nested_keep.txt");
    FT_ASSERT(destination_nested_keep_path != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_nested_keep_path->get_error());
    destination_skip_path = file_path_join(destination_path, "skip.log");
    FT_ASSERT(destination_skip_path != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_skip_path->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_test_assert_file_contents(
            destination_keep_path->c_str(), "keep"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_test_assert_file_contents(
            destination_nested_keep_path->c_str(), "nested"));
    FT_ASSERT_EQ(FILE_TYPE_REGULAR,
            file_get_type("test_file_directory_filters_source/keep.txt"));
    FT_ASSERT_EQ(FILE_TYPE_REGULAR,
            file_get_type("test_file_directory_filters_source/skip.log"));
    FT_ASSERT_EQ(FILE_TYPE_MISSING, file_get_type(destination_skip_path->c_str()));
    (void)file_delete_recursive(source_path);
    (void)file_delete_recursive(destination_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_skip_path->destroy());
    delete destination_skip_path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_nested_keep_path->destroy());
    delete destination_nested_keep_path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_keep_path->destroy());
    delete destination_keep_path;
    return (1);
}

FT_TEST(test_file_move_directory_filtered_moves_matching_entries_and_keeps_skipped_entries)
{
    const char *source_path;
    const char *destination_path;
    ft_string *destination_keep_path;
    ft_string *destination_nested_keep_path;

    source_path = "test_file_directory_filters_move_source";
    destination_path = "test_file_directory_filters_move_destination";
    (void)file_delete_recursive(source_path);
    (void)file_delete_recursive(destination_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_test_create_directory_tree(source_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
            "test_file_directory_filters_move_source/keep.txt", "keep", 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
            "test_file_directory_filters_move_source/skip.log", "skip", 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
            "test_file_directory_filters_move_source/nested/nested_keep.txt",
            "nested", 6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
            "test_file_directory_filters_move_source/nested/nested_skip.log",
            "nested_skip", 11));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_move_directory_filtered(source_path,
            destination_path, file_test_copy_filter, ft_nullptr));
    destination_keep_path = file_path_join(destination_path, "keep.txt");
    FT_ASSERT(destination_keep_path != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_keep_path->get_error());
    destination_nested_keep_path = file_path_join(destination_path,
            "nested/nested_keep.txt");
    FT_ASSERT(destination_nested_keep_path != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_nested_keep_path->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_test_assert_file_contents(
            destination_keep_path->c_str(), "keep"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_test_assert_file_contents(
            destination_nested_keep_path->c_str(), "nested"));
    FT_ASSERT_EQ(FILE_TYPE_MISSING,
            file_get_type("test_file_directory_filters_move_source/keep.txt"));
    FT_ASSERT_EQ(FILE_TYPE_REGULAR,
            file_get_type("test_file_directory_filters_move_source/skip.log"));
    FT_ASSERT_EQ(FILE_TYPE_MISSING,
            file_get_type("test_file_directory_filters_move_source/nested/nested_keep.txt"));
    FT_ASSERT_EQ(FILE_TYPE_REGULAR,
            file_get_type("test_file_directory_filters_move_source/nested/nested_skip.log"));
    (void)file_delete_recursive(source_path);
    (void)file_delete_recursive(destination_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_nested_keep_path->destroy());
    delete destination_nested_keep_path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_keep_path->destroy());
    delete destination_keep_path;
    return (1);
}
