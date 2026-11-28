#include "../test_internal.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/CPP_class/class_big_number.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include <cstdio>
#include <string>
#include <unistd.h>

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

static uint32_t g_json_file_write_calls;

static int64_t json_short_write(int32_t descriptor, const void *buffer, ft_size_t length)
{
    ++g_json_file_write_calls;
    if (length > 4096U)
        length = 4096U;
    return (write(descriptor, buffer, static_cast<uint32_t>(length)));
}

static int64_t json_stalled_write(int32_t, const void *, ft_size_t)
{
    ++g_json_file_write_calls;
    return (0);
}

FT_TEST(test_json_write_to_file_matches_string_and_checks_write_progress)
{
    std::string long_value(24576U, 'v');
    char group_name[] = "sample";
    char small_key[] = "key";
    char small_value[] = "value";
    char long_key[] = "large";
    json_item large{};
    large.key = long_key;
    large.value = long_value.data();
    json_item small{};
    small.key = small_key;
    small.value = small_value;
    small.next = &large;
    json_group group{};
    group.name = group_name;
    group.items = &small;
    char *serialized = json_write_to_string(&group);
    FT_ASSERT(serialized != ft_nullptr);
    const std::string expected(serialized);
    cma_free(serialized);

    ft_string backend_output;
    ft_memory_document_sink backend;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, backend_output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, backend.initialize());
    backend.set_storage(&backend_output);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_write_to_backend(backend, &group));
    FT_ASSERT_EQ(expected, std::string(backend_output.c_str(), backend_output.size()));

    char path[1024];
    const int32_t descriptor = test_create_temp_file_from_template(path, sizeof(path),
        "json_writer_bufferXXXXXX");
    FT_ASSERT(descriptor >= 0);
    close(descriptor);
    g_json_file_write_calls = 0U;
    su_set_write_syscall_hook(json_short_write);
    const int32_t write_result = json_write_to_file(path, &group);
    su_reset_write_syscall_hook();
    const uint32_t successful_calls = g_json_file_write_calls;
    std::string actual(expected.size() + 1U, '\0');
    std::FILE *file = std::fopen(path, "rb");
    if (file == ft_nullptr)
        std::remove(path);
    FT_ASSERT(file != ft_nullptr);
    const ft_size_t size = std::fread(actual.data(), 1U, actual.size(), file);
    std::fclose(file);
    actual.resize(size);
    g_json_file_write_calls = 0U;
    su_set_write_syscall_hook(json_stalled_write);
    const int32_t failure = json_write_to_file(path, &group);
    su_reset_write_syscall_hook();
    std::remove(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, write_result);
    FT_ASSERT_EQ(expected, actual);
    FT_ASSERT_EQ(8U, successful_calls);
    FT_ASSERT(failure != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(1U, g_json_file_write_calls);
    return (1);
}
