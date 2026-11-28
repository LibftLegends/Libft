#include "../test_internal.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <string>
#include <cstdio>
#include <memory>
#include <unistd.h>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_json_read_from_string_null_input_sets_errno)
{
    json_group *groups = json_read_from_string(ft_nullptr);
    FT_ASSERT(groups == ft_nullptr);
    return (1);
}

FT_TEST(test_json_read_from_string_missing_quote_sets_errno)
{
    const char *content = "{ \"config\": { \"name\": \"value } }";
    json_group *groups = json_read_from_string(content);
    FT_ASSERT(groups == ft_nullptr);
    return (1);
}

FT_TEST(test_json_read_from_string_missing_colon_sets_errno)
{
    const char *content = "{ \"config\" { \"name\": \"value\" } }";
    json_group *groups = json_read_from_string(content);
    FT_ASSERT(groups == ft_nullptr);
    return (1);
}

FT_TEST(test_json_read_from_string_missing_closing_sets_errno)
{
    const char *content = "{ \"config\": { \"name\": \"value\" }";
    json_group *groups = json_read_from_string(content);
    FT_ASSERT(groups == ft_nullptr);
    return (1);
}

FT_TEST(test_json_read_from_string_missing_fraction_digits_sets_errno)
{
    const char *content = "{ \"config\": { \"value\": 1. } }";
    json_group *groups = json_read_from_string(content);
    FT_ASSERT(groups == ft_nullptr);
    return (1);
}

FT_TEST(test_json_read_from_string_missing_exponent_digits_sets_errno)
{
    const char *content = "{ \"config\": { \"value\": 1e } }";
    json_group *groups = json_read_from_string(content);
    FT_ASSERT(groups == ft_nullptr);
    return (1);
}

FT_TEST(test_json_read_from_string_success_resets_errno)
{
    const char *content = "{ \"config\": { \"name\": \"value\" } }";
    json_group *groups = json_read_from_string(content);
    FT_ASSERT(groups != ft_nullptr);
    json_free_groups(groups);
    return (1);
}

FT_TEST(test_json_read_from_string_decodes_escaped_strings)
{
    std::string content_string = "{ \"config\": { \"value\": \"";
    content_string.append("Line\\nBreak \\\"Quote\\\" Backslash\\\\ Unicode ");
    content_string.append("\\u263A ");
    content_string.append("\\uD834\\uDD1E");
    content_string.append("\" } }");
    json_group *groups = json_read_from_string(content_string.c_str());
    FT_ASSERT(groups != ft_nullptr);
    json_group *group = json_find_group(groups, "config");
    FT_ASSERT(group != ft_nullptr);
    json_item *item = json_find_item(group, "value");
    FT_ASSERT(item != ft_nullptr);
    FT_ASSERT(item->value != ft_nullptr);
    std::string expected = "Line";
    expected.push_back('\n');
    expected.append("Break \"Quote\" Backslash\\ Unicode ");
    expected.append("\xE2\x98\xBA ");
    expected.append("\xF0\x9D\x84\x9E");
    std::string actual = item->value;
    FT_ASSERT_EQ(expected, actual);
    json_free_groups(groups);
    return (1);
}

FT_TEST(test_json_read_from_file_missing_file_sets_errno)
{
    json_group *groups = json_read_from_file("Test/nonexistent_json_reader.json");
    FT_ASSERT(groups == ft_nullptr);
    return (1);
}

FT_TEST(test_json_reader_handles_long_spans_and_matches_file_input)
{
    const std::string prefix(4096U, 'a');
    const std::string suffix(8192U, 'z');
    const std::string document = "{\"group\":{\"value\":\"" + prefix
        + "\\n" + suffix + "\\u263A\",\"count\":-12.5e+3,\"flag\":false}}";
    const std::string expected_value = prefix + "\n" + suffix + "\xE2\x98\xBA";
    char temporary_path[1024];
    const int32_t descriptor = test_create_temp_file_from_template(temporary_path,
        sizeof(temporary_path), "json_reader_spanXXXXXX");
    FT_ASSERT(descriptor >= 0);
    close(descriptor);
    std::FILE *stream = std::fopen(temporary_path, "wb");
    if (stream == ft_nullptr)
        std::remove(temporary_path);
    FT_ASSERT(stream != ft_nullptr);
    const ft_size_t written = std::fwrite(document.data(), 1U, document.size(), stream);
    const int32_t close_result = std::fclose(stream);
    using group_owner = std::unique_ptr<json_group, decltype(&json_free_groups)>;
    const group_owner file_groups(json_read_from_file(temporary_path), json_free_groups);
    std::remove(temporary_path);
    FT_ASSERT_EQ(document.size(), written);
    FT_ASSERT_EQ(0, close_result);
    const group_owner string_groups(json_read_from_string(document.c_str()), json_free_groups);
    FT_ASSERT(file_groups.get() != ft_nullptr);
    FT_ASSERT(string_groups.get() != ft_nullptr);
    const json_item *file_item = file_groups->items;
    const json_item *string_item = string_groups->items;
    FT_ASSERT(file_item != ft_nullptr);
    FT_ASSERT(string_item != ft_nullptr);
    FT_ASSERT_EQ(expected_value, std::string(file_item->value));
    FT_ASSERT_EQ(expected_value, std::string(string_item->value));
    FT_ASSERT(string_item->next != ft_nullptr && string_item->next->next != ft_nullptr);
    FT_ASSERT_EQ(std::string("-12.5e+3"), std::string(string_item->next->value));
    FT_ASSERT_EQ(std::string("false"), std::string(string_item->next->next->value));
    return (1);
}

FT_TEST(test_json_reader_rejects_raw_controls_and_recovers_after_allocation_failure)
{
    std::string invalid = "{\"group\":{\"value\":\"raw";
    invalid += static_cast<char>(0x1FU);
    invalid += "text\"}}";
    FT_ASSERT(json_read_from_string(invalid.c_str()) == ft_nullptr);

    const std::string value(4096U, 'a');
    const std::string document = "{\"group\":{\"first\":true,\"second\":\""
        + value + "\"}}";
    using group_owner = std::unique_ptr<json_group, decltype(&json_free_groups)>;
    cma_set_alloc_limit(512U);
    const group_owner failed(json_read_from_string(document.c_str()), json_free_groups);
    cma_set_alloc_limit(0U);
    FT_ASSERT(failed.get() == ft_nullptr);
    const group_owner retry(json_read_from_string(document.c_str()), json_free_groups);
    FT_ASSERT(retry.get() != ft_nullptr);
    const json_item *second = json_find_item(retry.get(), "second");
    FT_ASSERT(second != ft_nullptr);
    FT_ASSERT_EQ(value, std::string(second->value));
    return (1);
}

FT_TEST(test_json_create_item_releases_value_if_key_allocation_fails)
{
    const std::string oversized_key(4096U, 'k');
    ft_size_t allocations_before;
    ft_size_t frees_before;
    ft_size_t allocations_after;
    ft_size_t frees_after;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&allocations_before, &frees_before));
    cma_set_alloc_limit(512U);
    json_item *item = json_create_item(oversized_key.c_str(), "small");
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&allocations_after, &frees_after));
    if (item != ft_nullptr)
        json_free_items(item);
    FT_ASSERT(item == ft_nullptr);
    FT_ASSERT_EQ(allocations_after - allocations_before, frees_after - frees_before);
    return (1);
}
