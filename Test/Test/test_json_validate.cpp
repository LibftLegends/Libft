#include "../test_internal.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/JSon/document.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <array>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_json_validate_schema_success)
{
    json_document document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_group_list_enable_thread_safety());
    if (document.initialize() != FT_ERR_SUCCESS)
        return (0);
    json_group *group = document.create_group("user");
    json_item *item_name = document.create_item("name", "Alice");
    json_item *item_age = document.create_item("age", 30);
    document.add_item(group, item_name);
    document.add_item(group, item_age);
    document.append_group(group);
    json_schema_field name_field;
    name_field.key = "name";
    name_field.type = JSON_STRING;
    name_field.required = true;
    name_field.next = ft_nullptr;
    name_field._mutex = ft_nullptr;
    name_field._error_code = FT_ERR_SUCCESS;
    json_schema_field age_field;
    age_field.key = "age";
    age_field.type = JSON_NUMBER;
    age_field.required = true;
    age_field.next = ft_nullptr;
    age_field._mutex = ft_nullptr;
    age_field._error_code = FT_ERR_SUCCESS;
    name_field.next = &age_field;
    json_schema schema;
    schema.fields = &name_field;
    schema._mutex = ft_nullptr;
    schema._error_code = FT_ERR_SUCCESS;
    bool result = json_validate_schema(group, schema);
    FT_ASSERT_EQ(true, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_schema_field_disable_thread_safety(&age_field));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_schema_field_disable_thread_safety(&name_field));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_schema_disable_thread_safety(&schema));
    (void)json_group_list_disable_thread_safety();
    return (1);
}

FT_TEST(test_json_validate_schema_missing_field)
{
    json_document document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_group_list_enable_thread_safety());
    if (document.initialize() != FT_ERR_SUCCESS)
        return (0);
    json_group *group = document.create_group("user");
    json_item *item_name = document.create_item("name", "Alice");
    document.add_item(group, item_name);
    document.append_group(group);
    json_schema_field name_field;
    name_field.key = "name";
    name_field.type = JSON_STRING;
    name_field.required = true;
    name_field.next = ft_nullptr;
    name_field._mutex = ft_nullptr;
    name_field._error_code = FT_ERR_SUCCESS;
    json_schema_field age_field;
    age_field.key = "age";
    age_field.type = JSON_NUMBER;
    age_field.required = true;
    age_field.next = ft_nullptr;
    age_field._mutex = ft_nullptr;
    age_field._error_code = FT_ERR_SUCCESS;
    name_field.next = &age_field;
    json_schema schema;
    schema.fields = &name_field;
    schema._mutex = ft_nullptr;
    schema._error_code = FT_ERR_SUCCESS;
    bool result = json_validate_schema(group, schema);
    FT_ASSERT_EQ(false, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_schema_field_disable_thread_safety(&age_field));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_schema_field_disable_thread_safety(&name_field));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_schema_disable_thread_safety(&schema));
    (void)json_group_list_disable_thread_safety();
    return (1);
}

FT_TEST(test_json_validate_large_schema_preserves_first_duplicate_and_falls_back)
{
    constexpr ft_size_t ITEM_COUNT = 80U;
    std::array<json_item, ITEM_COUNT> items{};
    std::array<json_schema_field, ITEM_COUNT> fields{};
    char count_key[] = "count";
    char valid_number[] = "  +9223372036854775809";
    char invalid_number[] = "4.2";
    char flag_key[] = "flag";
    char flag_value[] = "true";
    json_group group{};
    group.items = items.data();
    json_schema schema{};
    schema.fields = fields.data();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_group_enable_thread_safety(&group));
    ft_size_t index = 0U;
    while (index < ITEM_COUNT)
    {
        items[index].key = flag_key;
        items[index].value = flag_value;
        fields[index].key = flag_key;
        fields[index].type = JSON_BOOL;
        fields[index].required = FT_TRUE;
        if (index + 1U < ITEM_COUNT)
        {
            items[index].next = &items[index + 1U];
            fields[index].next = &fields[index + 1U];
        }
        ++index;
    }
    items[0].key = count_key;
    items[0].value = valid_number;
    items[1].key = count_key;
    items[1].value = invalid_number;
    items[2].key = ft_nullptr;
    fields[0].key = count_key;
    fields[0].type = JSON_NUMBER;
    fields.back().key = "optional";
    fields.back().required = FT_FALSE;
    const ft_bool indexed_result = json_validate_schema(&group, schema);
    const int32_t optional_error = json_schema_field_get_error(&fields.back());
    const int32_t lookup_error = json_group_get_error(&group);
    cma_set_alloc_limit(1U);
    const ft_bool fallback_result = json_validate_schema(&group, schema);
    cma_set_alloc_limit(0U);
    items[0].value = invalid_number;
    const ft_bool invalid_result = json_validate_schema(&group, schema);
    const int32_t invalid_field_error = json_schema_field_get_error(&fields.front());
    const int32_t invalid_schema_error = json_schema_get_error(&schema);
    index = 0U;
    while (index < ITEM_COUNT)
    {
        (void)json_schema_field_disable_thread_safety(&fields[index]);
        ++index;
    }
    (void)json_schema_disable_thread_safety(&schema);
    (void)json_group_disable_thread_safety(&group);
    FT_ASSERT_EQ(FT_TRUE, indexed_result);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, optional_error);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, lookup_error);
    FT_ASSERT_EQ(FT_TRUE, fallback_result);
    FT_ASSERT_EQ(FT_FALSE, invalid_result);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, invalid_field_error);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, invalid_schema_error);
    return (1);
}
