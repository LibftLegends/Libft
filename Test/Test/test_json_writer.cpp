#include "../test_internal.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/CPP_class/class_big_number.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static json_group *create_sample_group(void)
{
    json_group *group = json_create_json_group("sample");
    if (!group)
        return (ft_nullptr);
    json_item *item = json_create_item("key", "value");
    if (!item)
    {
        json_free_groups(group);
        return (ft_nullptr);
    }
    json_add_item_to_group(group, item);
    return (group);
}

FT_TEST(test_json_write_to_string_initial_alloc_failure_sets_errno)
{
    cma_set_alloc_limit(1);
    char *result = json_write_to_string(ft_nullptr);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, result);
    return (1);
}

FT_TEST(test_json_write_to_string_output_alloc_failure_sets_errno)
{
    json_group *group = create_sample_group();
    FT_ASSERT(group != ft_nullptr);
    cma_set_alloc_limit(16);
    char *result = json_write_to_string(group);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, result);
    json_free_groups(group);
    return (1);
}

FT_TEST(test_json_write_to_string_success_resets_errno)
{
    json_group *group = create_sample_group();
    FT_ASSERT(group != ft_nullptr);
    char *result = json_write_to_string(group);
    FT_ASSERT(result != ft_nullptr);
    const char *expected = "{\n  \"sample\": {\n    \"key\": \"value\"\n  }\n}\n";
    FT_ASSERT_EQ(0, ft_strcmp(expected, result));
    cma_free(result);
    json_free_groups(group);
    return (1);
}

FT_TEST(test_json_write_to_string_preserves_group_and_item_format)
{
    ft_big_number number;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, number.initialize());
    number.assign("18446744073709551616");
    json_group *sample = create_sample_group();
    json_group *empty = json_create_json_group("empty");
    json_item *numeric = json_create_item("count", number);
    FT_ASSERT(sample != ft_nullptr && empty != ft_nullptr);
    FT_ASSERT(numeric != ft_nullptr);
    json_add_item_to_group(sample, numeric);
    json_append_group(&sample, empty);
    char *result = json_write_to_string(sample);
    FT_ASSERT(result != ft_nullptr);
    const char *expected = "{\n  \"sample\": {\n    \"key\": \"value\",\n"
        "    \"count\": 18446744073709551616\n  },\n  \"empty\": {\n  }\n}\n";
    FT_ASSERT_EQ(0, ft_strcmp(expected, result));
    cma_free(result);
    json_free_groups(sample);
    return (1);
}
