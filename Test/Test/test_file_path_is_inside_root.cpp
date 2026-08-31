#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

FT_TEST(test_file_path_is_inside_root_accepts_descendant)
{
    FT_ASSERT_EQ(FT_TRUE, file_path_is_inside_root("safe/root",
            "safe/root/child/file.txt"));
    FT_ASSERT_EQ(FT_TRUE, file_path_is_inside_root("safe/root",
            "safe/root/child/../file.txt"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_validate_path_inside_root("safe/root",
            "safe/root/file.txt"));
    return (1);
}

FT_TEST(test_file_path_is_inside_root_rejects_escape)
{
    FT_ASSERT_EQ(FT_FALSE, file_path_is_inside_root("safe/root",
            "safe/root/../../etc/passwd"));
    FT_ASSERT_EQ(FT_FALSE, file_path_is_inside_root("safe/root",
            "safe/root_evil/file.txt"));
    FT_ASSERT_EQ(FT_FALSE, file_path_is_inside_root(ft_nullptr,
            "safe/root/file.txt"));
    FT_ASSERT_EQ(FT_ERR_INVALID_PATH, file_validate_path_inside_root("safe/root",
            "safe/root/../outside.txt"));
    return (1);
}

FT_TEST(test_file_validate_regular_file_inside_root)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_validate_regular_file_inside_root(
            "Test/Lua", "Test/Lua/export_values.lua"));
    FT_ASSERT_EQ(FT_ERR_INVALID_PATH,
            file_validate_regular_file_inside_root("Test/Lua",
                "Test/Lua/../test_voxel_runtime_blocks.cpp"));
    FT_ASSERT_EQ(FT_ERR_INVALID_PATH,
            file_validate_regular_file_inside_root("Test/Lua", "Test/Lua"));
    FT_ASSERT_EQ(FT_ERR_INVALID_PATH,
            file_validate_regular_file_inside_root("Test/Lua",
                "Test/Lua/missing_asset.bin"));
    return (1);
}
