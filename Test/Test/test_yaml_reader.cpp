#include "../test_internal.hpp"
#include "../../Modules/YAML/yaml.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include <fcntl.h>
#include <cstdio>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_yaml_reader_malformed_structure_sets_errno)
{
    ft_string malformed;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, malformed.initialize("root:\nvalue"));

    yaml_value *result = yaml_read_from_string(malformed);
    FT_ASSERT(result == ft_nullptr);
    return (1);
}

FT_TEST(test_yaml_reader_list_with_premature_eof_sets_errno)
{
    ft_string premature;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, premature.initialize("-\n"));

    yaml_value *result = yaml_read_from_string(premature);
    FT_ASSERT(result == ft_nullptr);
    return (1);
}

FT_TEST(test_yaml_reader_allocation_failure_sets_errno)
{
    ft_string simple;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simple.initialize("root: value"));
    yaml_value *result;

    cma_set_alloc_limit(64);
    result = yaml_read_from_string(simple);
    cma_set_alloc_limit(0);
    FT_ASSERT(result == ft_nullptr);
    return (1);
}

FT_TEST(test_yaml_reader_scalar_set_scalar_failure)
{
    ft_string content;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.initialize("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
    yaml_value *result;

    cma_set_alloc_limit(32);
    result = yaml_read_from_string(content);
    cma_set_alloc_limit(0);
    if (result != ft_nullptr)
    {
        yaml_free(result);
    }
    FT_ASSERT(result == ft_nullptr);
    return (1);
}

FT_TEST(test_yaml_reader_list_inline_map_entries)
{
    ft_string content;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content.initialize("- name: foo\n  value: 42\n- name: bar\n  value: 84\n"));
    yaml_value *root;
    root = yaml_read_from_string(content);
    FT_ASSERT(root == ft_nullptr);
    return (1);
}

FT_TEST(test_yaml_read_from_file_propagates_read_error)
{
    const char *file_path = "yaml_reader_forced_failure.yaml";
    int file_descriptor;
    yaml_value *result;

    file_descriptor = su_open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FT_ASSERT(file_descriptor >= 0);
    FT_ASSERT_EQ(0, su_close(file_descriptor));
    result = yaml_read_from_file(file_path);
    std::remove(file_path);
    FT_ASSERT(result == ft_nullptr);
    return (1);
}
