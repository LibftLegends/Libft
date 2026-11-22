#include "../test_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include <string>

FT_TEST(test_filesystem_path_has_wildcards_detects_tokens)
{
    FT_ASSERT_EQ(FT_TRUE, filesystem_path_has_wildcards("src/*.cpp"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_path_has_wildcards("src/main.cpp"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_path_has_wildcards(ft_nullptr));
    return (1);
}

FT_TEST(test_filesystem_match_glob_supports_wildcards_and_globstar)
{
    FT_ASSERT_EQ(FT_TRUE, filesystem_match_glob("src/*.cpp", "src/main.cpp"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_match_glob("src/*.cpp", "src/sub/main.cpp"));
    FT_ASSERT_EQ(FT_TRUE, filesystem_match_glob("**/main.cpp", "main.cpp"));
    FT_ASSERT_EQ(FT_TRUE, filesystem_match_glob("**/main.cpp", "src/tools/main.cpp"));
    FT_ASSERT_EQ(FT_TRUE, filesystem_match_glob("logs/?01.txt", "logs/a01.txt"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_match_glob("logs/?01.txt", "logs/ab1.txt"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_match_glob(ft_nullptr, "logs/a01.txt"));
    return (1);
}

FT_TEST(test_filesystem_match_glob_preserves_separator_rules_with_multiple_stars)
{
    FT_ASSERT_EQ(FT_TRUE, filesystem_match_glob("**/prefix*/**/part?.bin/**", "aprefix/sub/part2.bin/"));
    FT_ASSERT_EQ(FT_TRUE, filesystem_match_glob("root/*//file*.bin", "root\\alpha/\\file1.bin/"));
    FT_ASSERT_EQ(FT_FALSE, filesystem_match_glob("root/*//file*.bin", "root/alpha/beta/file1.bin"));
    FT_ASSERT_EQ(FT_TRUE, filesystem_match_glob("root/*/**/file*.bin", "root/alpha/beta/file1.bin"));
    FT_ASSERT_EQ(FT_TRUE, filesystem_match_glob("*a/**/", "aaa/"));
    return (1);
}

FT_TEST(test_filesystem_match_glob_handles_long_matches_and_near_misses)
{
    std::string path(1024, 'a');
    path.push_back('Z');
    FT_ASSERT_EQ(FT_TRUE, filesystem_match_glob("*a*a*a*a*a*Z", path.c_str()));
    path.back() = 'Y';
    FT_ASSERT_EQ(FT_FALSE, filesystem_match_glob("*a*a*a*a*a*Z", path.c_str()));
    return (1);
}
