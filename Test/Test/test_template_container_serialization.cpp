#include "../test_internal.hpp"
#include "../../Modules/Template/container_serialization.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <limits>

FT_TEST(test_template_container_serialization_formats_integer_limits)
{
    ft_string text;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, default_string_serializer(
        std::numeric_limits<int64_t>::min(), text));
    FT_ASSERT_EQ(0, ft_strcmp("-9223372036854775808", text.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, default_string_serializer(
        std::numeric_limits<uint64_t>::max(), text));
    FT_ASSERT_EQ(0, ft_strcmp("18446744073709551615", text.c_str()));
    return (1);
}

FT_TEST(test_template_container_serialization_roundtrips_json_and_yaml)
{
    ft_vector<int64_t> values;
    ft_vector<int64_t> parsed_json;
    ft_vector<int64_t> parsed_yaml;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, parsed_json.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, parsed_json.push_back(777));
    int64_t value = -32;
    while (value <= 32)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, values.push_back(value));
        ++value;
    }
    json_group *group = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_vector_serialize_json(values, "values", group, "size", "entry_"));
    FT_ASSERT(group != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp("entry_0", group->items->key));
    FT_ASSERT_EQ(0, ft_strcmp("-22", json_find_item(group, "entry_10")->value));
    FT_ASSERT_EQ(0, ft_strcmp("65", json_find_item(group, "size")->value));
    const int32_t json_result = ft_vector_deserialize_json(group, parsed_json, "size", "entry_");
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_result);
    yaml_value *list = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_vector_serialize_yaml(values, list));
    FT_ASSERT(list != ft_nullptr);
    const int32_t yaml_result = ft_vector_deserialize_yaml(*list, parsed_yaml);
    yaml_free(list);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, yaml_result);
    FT_ASSERT_EQ(values.size(), parsed_json.size());
    FT_ASSERT_EQ(values.size(), parsed_yaml.size());
    ft_size_t index = 0U;
    while (index < values.size())
    {
        FT_ASSERT_EQ(values[index], parsed_json[index]);
        FT_ASSERT_EQ(values[index], parsed_yaml[index]);
        ++index;
    }
    return (1);
}
