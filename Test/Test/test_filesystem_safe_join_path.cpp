#include "../test_internal.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_filesystem_safe_join_path_keeps_path_inside_root)
{
    ft_string *joined_path;
    ft_string *escaped_path;

    joined_path = filesystem_safe_join_path("root", "assets/../image.png");
    FT_ASSERT(joined_path != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, joined_path->get_error());
    FT_ASSERT_EQ(FT_TRUE, filesystem_is_inside_root("root",
        joined_path->c_str()));
    escaped_path = filesystem_safe_join_path("root", "../secret.txt");
    FT_ASSERT(escaped_path == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, joined_path->destroy());
    delete joined_path;
    return (1);
}

FT_TEST(test_filesystem_safe_join_path_checks_roots_and_normalized_relative_paths)
{
    ft_string *joined = filesystem_safe_join_path("C:/world", "region/./sub/../chunk.bin");
    FT_ASSERT(joined != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, filesystem_is_inside_root("C:/world", joined->c_str()));
    delete joined;
    FT_ASSERT(filesystem_safe_join_path("C:/world", "region/../../chunk.bin") == ft_nullptr);
    FT_ASSERT(filesystem_safe_join_path("C:/world", "region/../D:/elsewhere") == ft_nullptr);
    FT_ASSERT(filesystem_safe_join_path(".", "region/chunk.bin") == ft_nullptr);
    return (1);
}
