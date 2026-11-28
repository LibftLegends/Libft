#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

FT_TEST(test_file_create_directories_builds_nested_path)
{
    const char *directory_path;

    directory_path = "test_file_create_directories_root/child/grandchild";
    (void)file_delete_recursive("test_file_create_directories_root");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directories(directory_path, 0700));
    FT_ASSERT_EQ(FILE_TYPE_DIRECTORY, file_get_type(directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete_recursive("test_file_create_directories_root"));
    return (1);
}

FT_TEST(test_file_create_directories_rejects_file_component)
{
    const char *file_path;

    file_path = "test_file_create_directories_file_component";
    (void)file_delete(file_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(file_path, "x", 1));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS,
        file_create_directories("test_file_create_directories_file_component/child", 0700));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, file_create_directories(file_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(file_path));
    return (1);
}

FT_TEST(test_file_create_directories_accepts_existing_path_with_trailing_separator)
{
    const char *root = "test_file_create_directories_trailing";
    const char *path = "test_file_create_directories_trailing/child/";
    (void)file_delete_recursive(root);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directories(path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directories(path, 0700));
    FT_ASSERT_EQ(FILE_TYPE_DIRECTORY, file_get_type(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete_recursive(root));
    return (1);
}
