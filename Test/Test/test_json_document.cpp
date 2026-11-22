#include "../test_internal.hpp"
#include "../../Modules/JSon/document.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/CPP_class/class_big_number.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_cma_failure_injection.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#include <string>

FT_TEST(test_json_document_find_item_by_pointer_success)
{
    json_document document;
    if (document.initialize() != FT_ERR_SUCCESS)
        return (0);
    json_group *group = document.create_group("config");
    FT_ASSERT(group != ft_nullptr);
    json_item *item = document.create_item("name", "value");
    FT_ASSERT(item != ft_nullptr);
    document.append_group(group);
    document.add_item(group, item);
    json_item *found_item = document.find_item_by_pointer("/config/name");
    FT_ASSERT(found_item == item);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_error());
    const char *value = document.get_value_by_pointer("/config/name");
    FT_ASSERT(value != ft_nullptr);
    FT_ASSERT_EQ(std::string("value"), std::string(value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_error());
    return (1);
}

FT_TEST(test_json_document_find_item_by_pointer_missing_path_sets_error)
{
    json_document document;
    if (document.initialize() != FT_ERR_SUCCESS)
        return (0);
    json_group *group = document.create_group("config");
    FT_ASSERT(group != ft_nullptr);
    document.append_group(group);
    json_item *missing = document.find_item_by_pointer("/missing/name");
    FT_ASSERT(missing == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, document.get_error());
    return (1);
}

FT_TEST(test_json_document_pointer_unescapes_tokens)
{
    json_document document;
    if (document.initialize() != FT_ERR_SUCCESS)
        return (0);
    json_group *group = document.create_group("config/advanced");
    FT_ASSERT(group != ft_nullptr);
    json_item *item = document.create_item("name~value", "42");
    FT_ASSERT(item != ft_nullptr);
    document.append_group(group);
    document.add_item(group, item);
    json_item *found_item = document.find_item_by_pointer("/config~1advanced/name~0value");
    FT_ASSERT(found_item == item);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_error());
    const char *value = document.get_value_by_pointer("/config~1advanced/name~0value");
    FT_ASSERT(value != ft_nullptr);
    FT_ASSERT_EQ(std::string("42"), std::string(value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_error());
    return (1);
}

FT_TEST(test_json_document_pointer_requires_leading_slash)
{
    json_document document;
    if (document.initialize() != FT_ERR_SUCCESS)
        return (0);
    json_group *group = document.create_group("config");
    FT_ASSERT(group != ft_nullptr);
    json_item *item = document.create_item("name", "value");
    FT_ASSERT(item != ft_nullptr);
    document.append_group(group);
    document.add_item(group, item);
    json_item *found_item = document.find_item_by_pointer("config/name");
    FT_ASSERT(found_item == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, document.get_error());
    return (1);
}

FT_TEST(test_json_document_pointer_rejects_empty_segment)
{
    json_document document;
    if (document.initialize() != FT_ERR_SUCCESS)
        return (0);
    json_group *group = document.create_group("config");
    FT_ASSERT(group != ft_nullptr);
    json_item *item = document.create_item("name", "value");
    FT_ASSERT(item != ft_nullptr);
    document.append_group(group);
    document.add_item(group, item);
    json_item *found_item = document.find_item_by_pointer("/config//name");
    FT_ASSERT(found_item == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, document.get_error());
    return (1);
}

FT_TEST(test_json_document_pointer_invalid_escape_sets_error)
{
    json_document document;
    if (document.initialize() != FT_ERR_SUCCESS)
        return (0);
    json_group *group = document.create_group("config");
    FT_ASSERT(group != ft_nullptr);
    json_item *item = document.create_item("name", "value");
    FT_ASSERT(item != ft_nullptr);
    document.append_group(group);
    document.add_item(group, item);
    json_item *found_item = document.find_item_by_pointer("/config/~2name");
    FT_ASSERT(found_item == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, document.get_error());
    return (1);
}

FT_TEST(test_json_document_pointer_uses_spans_and_decodes_long_escaped_keys)
{
    json_document document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    json_group *group = document.create_group("cfg~");
    const std::string base(160U, 'x');
    const std::string escaped_key = base + "/tail";
    json_item *plain = document.create_item(base.c_str(), "plain");
    json_item *escaped = document.create_item(escaped_key.c_str(), "escaped");
    FT_ASSERT(group != ft_nullptr && plain != ft_nullptr && escaped != ft_nullptr);
    document.append_group(group);
    document.add_item(group, escaped);
    document.add_item(group, plain);
    const std::string plain_pointer = "/cfg~0/" + base;
    const std::string escaped_pointer = plain_pointer + "~1tail";
    cma_set_alloc_limit(1U);
    const json_item *plain_match = document.find_item_by_pointer(plain_pointer.c_str());
    const int32_t plain_error = document.get_error();
    const json_item *escaped_failure = document.find_item_by_pointer(escaped_pointer.c_str());
    const int32_t escaped_error = document.get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(plain, plain_match);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, plain_error);
    FT_ASSERT(escaped_failure == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, escaped_error);
    FT_ASSERT_EQ(escaped, document.find_item_by_pointer(escaped_pointer.c_str()));
    FT_ASSERT(document.find_item_by_pointer(("/cfg~0/" + base.substr(0U, 159U)).c_str()) == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, document.get_error());
    FT_ASSERT(document.find_item_by_pointer((plain_pointer + "/extra").c_str()) == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, document.get_error());
    return (1);
}

FT_TEST(test_json_document_copies_text_directly_and_preserves_destination_on_failure)
{
    json_document source;
    json_document destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.read_from_string("{\"old\":{\"key\":\"retained\"}}"));
    json_group *group = source.create_group("sample");
    FT_ASSERT(group != ft_nullptr);
    source.append_group(group);
    const char value[] = "quoted \"text\" and \\";
    json_item *item = source.create_item("key", value);
    FT_ASSERT(item != ft_nullptr);
    source.add_item(group, item);

    test_cma_failure_controller controller;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_begin(controller));
    const int32_t armed = test_cma_failure_controller_fail_after(controller,
        TEST_CMA_FAILURE_ALLOCATE, 2U);
    const int32_t failure = destination.initialize(static_cast<const json_document &>(source));
    const int32_t cleanup = test_cma_failure_controller_destroy(controller);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, armed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleanup);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failure);
    FT_ASSERT_EQ(0, ft_strcmp(destination.get_value_by_pointer("/old/key"), "retained"));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(static_cast<const json_document &>(source)));
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    const json_item *copied = destination.find_item_by_pointer("/sample/key");
    FT_ASSERT(copied != ft_nullptr && copied != item && copied->value != item->value);
    FT_ASSERT_EQ(0, ft_strcmp(copied->value, value));
    source.update_item(group, "key", "changed");
    FT_ASSERT_EQ(0, ft_strcmp(destination.get_value_by_pointer("/sample/key"), value));

    json_document empty;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(static_cast<const json_document &>(empty)));
    FT_ASSERT(destination.get_groups() == ft_nullptr);
    return (1);
}

FT_TEST(test_json_update_item_reuses_equal_values_and_preserves_failed_replacements)
{
    json_document document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    json_group *group = document.create_group("settings");
    json_item *item = document.create_item("value", "original");
    FT_ASSERT(group != ft_nullptr && item != ft_nullptr);
    document.add_item(group, item);
    document.append_group(group);
    const char *original = item->value;
    cma_set_alloc_limit(1U);
    document.update_item(group, "value", "original");
    const int32_t unchanged_error = document.get_error();
    document.update_item(group, "value", "replacement");
    const int32_t failure = document.get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, unchanged_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failure);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, json_group_get_error(group));
    FT_ASSERT_EQ(original, item->value);
    FT_ASSERT_EQ(0, ft_strcmp(item->value, "original"));

    document.update_item(group, "value", item->value + 3U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(item->value, "ginal"));
    document.update_item(group, "missing", "replacement");
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, document.get_error());
    document.update_item(group, "value", FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(item->value, "true"));
    document.update_item(group, "value", static_cast<int32_t>(-2147483647 - 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(item->value, "-2147483648"));
    return (1);
}

FT_TEST(test_json_update_item_preserves_big_number_state_until_commit)
{
    json_document document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    json_group *group = document.create_group("settings");
    json_item *item = document.create_item("value", "+9223372036854775808");
    FT_ASSERT(group != ft_nullptr && item != ft_nullptr);
    document.add_item(group, item);
    document.append_group(group);
    FT_ASSERT_EQ(FT_TRUE, item->is_big_number);
    FT_ASSERT(item->big_number != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(item->big_number->c_str(), "9223372036854775808"));
    ft_big_number *previous = item->big_number;
    cma_set_alloc_limit(1U);
    json_update_item(group, "value", "18446744073709551617");
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, json_group_get_error(group));
    FT_ASSERT_EQ(previous, item->big_number);
    FT_ASSERT_EQ(0, ft_strcmp(item->value, "+9223372036854775808"));

    test_cma_failure_controller controller;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_cma_failure_controller_begin(controller));
    const int32_t armed = test_cma_failure_controller_fail_after(controller,
        TEST_CMA_FAILURE_ALLOCATE, 1U);
    document.update_item(group, "value", "18446744073709551617");
    const int32_t number_error = document.get_error();
    const ft_size_t attempts = test_cma_failure_controller_attempt_count(controller,
        TEST_CMA_FAILURE_ALLOCATE);
    const int32_t cleanup = test_cma_failure_controller_destroy(controller);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, armed);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleanup);
    FT_ASSERT_EQ(2U, attempts);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, number_error);
    FT_ASSERT_EQ(previous, item->big_number);
    FT_ASSERT_EQ(0, ft_strcmp(item->value, "+9223372036854775808"));

    document.update_item(group, "value", *item->big_number);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(item->value, "9223372036854775808"));
    document.update_item(group, "value", "-9223372036854775808");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_error());
    FT_ASSERT_EQ(FT_FALSE, item->is_big_number);
    FT_ASSERT(item->big_number == ft_nullptr);
    return (1);
}
