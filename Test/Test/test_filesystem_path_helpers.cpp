#include "../test_internal.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_filesystem_split_path_returns_directory_and_basename)
{
    ft_string directory;
    ft_string basename;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, directory.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, basename.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_split_path(
        "/tmp/example.txt", &directory, &basename));
    FT_ASSERT_EQ(0, ft_strcmp(directory.c_str(), "/tmp"));
    FT_ASSERT_EQ(0, ft_strcmp(basename.c_str(), "example.txt"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, directory.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, basename.destroy());
    return (1);
}

FT_TEST(test_filesystem_split_path_preserves_drive_root_when_input_aliases_output)
{
    ft_string directory;
    ft_string basename;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, directory.initialize("C:/save.bin"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, basename.initialize("previous"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_split_path(directory.c_str(), &directory, &basename));
    FT_ASSERT_EQ(FT_TRUE, directory == "C:/");
    FT_ASSERT_EQ(FT_TRUE, basename == "save.bin");
    return (1);
}

FT_TEST(test_filesystem_split_path_handles_roots_and_normalized_leaf_paths)
{
    ft_string directory;
    ft_string basename;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_split_path("/", &directory, &basename));
    FT_ASSERT_EQ(FT_TRUE, directory == "/");
    FT_ASSERT_EQ(FT_TRUE, basename == "/");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_split_path("world/region/..", &directory, &basename));
    FT_ASSERT_EQ(FT_TRUE, directory == ".");
    FT_ASSERT_EQ(FT_TRUE, basename == "world");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_split_path("", &directory, &basename));
    FT_ASSERT_EQ(FT_TRUE, directory.empty());
    FT_ASSERT_EQ(FT_TRUE, basename.empty());
    return (1);
}

FT_TEST(test_filesystem_hidden_and_rooted_checks)
{
    FT_ASSERT_EQ(FT_TRUE, filesystem_is_hidden(".env"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_is_hidden("visible.txt"));
    FT_ASSERT_EQ(FT_TRUE, filesystem_is_rooted("/tmp"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_is_rooted("relative/path"));
    return (1);
}

FT_TEST(test_filesystem_reserved_name_checks_windows_device_names)
{
    FT_ASSERT_EQ(FT_TRUE, filesystem_is_reserved_name("CON"));
    FT_ASSERT_EQ(FT_TRUE, filesystem_is_reserved_name("com1.txt"));
    FT_ASSERT_EQ(FT_TRUE, filesystem_is_reserved_name("LPT9"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_is_reserved_name("notes.txt"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_is_reserved_name(ft_nullptr));
    return (1);
}

