#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

FT_TEST(test_file_delete_recursive_removes_nested_tree)
{
    const char *root_path;
    const char *file_path;

    root_path = "test_file_delete_recursive_root";
    file_path = "test_file_delete_recursive_root/child/value.txt";
    (void)file_delete_recursive(root_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directories("test_file_delete_recursive_root/child", 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(file_path, "delete", 6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete_recursive(root_path));
    FT_ASSERT_EQ(FILE_TYPE_MISSING, file_get_type(root_path));
    return (1);
}

FT_TEST(test_file_delete_recursive_accepts_missing_path)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete_recursive("test_file_delete_recursive_missing"));
    return (1);
}

#if !defined(_WIN32) && !defined(_WIN64)
FT_TEST(test_file_delete_recursive_preserves_literal_backslashes_in_native_entries)
{
    const char *root = "test_file_delete_literal_backslash";
    (void)file_delete_recursive(root);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directories(root, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
        "test_file_delete_literal_backslash/native\\leaf", "x", 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete_recursive(root));
    FT_ASSERT_EQ(FILE_TYPE_MISSING, file_get_type(root));
    return (1);
}
#endif
