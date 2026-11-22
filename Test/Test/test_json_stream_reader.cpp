#include "../test_internal.hpp"
#include "../../Modules/JSon/json_stream_reader.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/JSon/document.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#include <cstdio>
#include <string>

typedef struct json_stream_test_state
{
    const char **chunks;
    const size_t *sizes;
    size_t count;
    size_t index;
    size_t offset;
} json_stream_test_state;

static ft_size_t test_chunk_callback(void *user_data, char *buffer,
    ft_size_t max_size)
{
    if (!user_data || !buffer || max_size == 0)
        return (0);
    json_stream_test_state *state = static_cast<json_stream_test_state *>(user_data);
    ft_size_t written = 0;
    while (written < max_size && state->index < state->count)
    {
        const char *chunk = state->chunks[state->index];
        size_t chunk_size = state->sizes[state->index];
        if (state->offset >= chunk_size)
        {
            state->index += 1;
            state->offset = 0;
            continue;
        }
        size_t remaining = chunk_size - state->offset;
        ft_size_t available = max_size - written;
        ft_size_t to_copy = remaining;
        if (to_copy > available)
            to_copy = available;
        ft_memcpy(buffer + written, chunk + state->offset, to_copy);
        written += to_copy;
        state->offset += to_copy;
        if (state->offset >= chunk_size)
        {
            state->index += 1;
            state->offset = 0;
        }
    }
    return (written);
}

FT_TEST(test_json_stream_reader_file_small_buffer)
{
    std::string content = "{";
    int index = 0;
    while (index < 32)
    {
        content += "\"group";
        content += std::to_string(index);
        content += "\": { \"value\": \"";
        content += std::to_string(index);
        content += "\" }";
        index += 1;
        if (index < 32)
            content += ", ";
    }
    content += "}";
    FILE *file = tmpfile();
    FT_ASSERT(file != ft_nullptr);
    size_t written = 0;
    size_t total_size = content.size();
    while (written < total_size)
    {
        size_t chunk = fwrite(content.c_str() + written, 1, total_size - written, file);
        FT_ASSERT(chunk > 0);
        written += chunk;
    }
    fflush(file);
    rewind(file);
    json_group *groups = json_read_from_file_stream(file, 8);
    FT_ASSERT(groups != ft_nullptr);
    json_group *current_group = groups;
    int seen = 0;
    while (current_group)
    {
        FT_ASSERT(current_group->name != ft_nullptr);
        std::string group_name = current_group->name;
        json_item *item = current_group->items;
        FT_ASSERT(item != ft_nullptr);
        FT_ASSERT(item->next == ft_nullptr);
        FT_ASSERT(item->key != ft_nullptr);
        FT_ASSERT(item->value != ft_nullptr);
        std::string key = item->key;
        std::string value = item->value;
        FT_ASSERT_EQ(std::string("value"), key);
        FT_ASSERT_EQ(group_name.substr(5), value);
        current_group = current_group->next;
        seen += 1;
    }
    FT_ASSERT_EQ(32, seen);
    json_free_groups(groups);
    fclose(file);
    return (1);
}

FT_TEST(test_json_stream_reader_callback_error_propagation)
{
    const char *chunks[2];
    size_t sizes[2];
    chunks[0] = "{ \"group\": { \"value\": \"one\" }";
    sizes[0] = ft_strlen_size_t(chunks[0]);
    chunks[1] = ft_nullptr;
    sizes[1] = 0;
    json_stream_test_state state = { chunks, sizes, 1, 0, 0 };
    json_group *groups = json_read_from_stream(test_chunk_callback, &state, 4);
    FT_ASSERT(groups == ft_nullptr);
    return (1);
}

FT_TEST(test_json_stream_reader_respects_allocator_limits)
{
    std::string content = "{";
    int index = 0;
    while (index < 24)
    {
        content += "\"group";
        content += std::to_string(index);
        content += "\": { \"value\": \"";
        content += std::to_string(index);
        content += "\" }";
        index += 1;
        if (index < 24)
            content += ", ";
    }
    content += "}";
    const char *chunk_data[1];
    size_t chunk_sizes[1];
    chunk_data[0] = content.c_str();
    chunk_sizes[0] = content.size();
    json_stream_test_state state = { chunk_data, chunk_sizes, 1, 0, 0 };
    cma_set_alloc_limit(4096);
    json_group *groups = json_read_from_stream(test_chunk_callback, &state, 5);
    cma_set_alloc_limit(0);
    FT_ASSERT(groups != ft_nullptr);
    json_group *current_group = groups;
    int seen = 0;
    while (current_group)
    {
        seen += 1;
        current_group = current_group->next;
    }
    FT_ASSERT_EQ(24, seen);
    json_free_groups(groups);
    return (1);
}

FT_TEST(test_json_stream_reader_decodes_escaped_strings)
{
    std::string json_text = "{ \"config\": { \"value\": \"";
    json_text.append("Line\\nBreak \\\"Quote\\\" Backslash\\\\ Unicode ");
    json_text.append("\\u263A ");
    json_text.append("\\uD834\\uDD1E");
    json_text.append("\" } }");
    const char *chunk_data[1];
    size_t chunk_sizes[1];
    chunk_data[0] = json_text.c_str();
    chunk_sizes[0] = json_text.size();
    json_stream_test_state state = { chunk_data, chunk_sizes, 1, 0, 0 };
    json_group *groups = json_read_from_stream(test_chunk_callback, &state, 5);
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

FT_TEST(test_json_document_streaming_loads_file)
{
    const char *file_path = "Test/tmp_json_stream_reader.json";
    std::string content = "{ \"config\": { \"value\": \"42\" } }";
    FILE *file = fopen(file_path, "wb");
    FT_ASSERT(file != ft_nullptr);
    size_t written = 0;
    size_t total_size = content.size();
    while (written < total_size)
    {
        size_t chunk = fwrite(content.c_str() + written, 1, total_size - written, file);
        FT_ASSERT(chunk > 0);
        written += chunk;
    }
    fclose(file);
    json_document document;
    if (document.initialize() != FT_ERR_SUCCESS)
        return (0);
    int status = document.read_from_file_streaming(file_path, 4);
    FT_ASSERT_EQ(0, status);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_error());
    json_group *group = document.find_group("config");
    FT_ASSERT(group != ft_nullptr);
    json_item *item = document.find_item(group, "value");
    FT_ASSERT(item != ft_nullptr);
    FT_ASSERT(item->value != ft_nullptr);
    FT_ASSERT_EQ(std::string("42"), std::string(item->value));
    document.clear();
    std::remove(file_path);
    return (1);
}

FT_TEST(test_json_stream_reader_preserves_empty_groups_and_one_byte_numeric_tokens)
{
    const char *document = "{\"empty\":{}, \"config\": {\"number\": -12.340e+05}}";
    const size_t length = ft_strlen_size_t(document);
    json_stream_test_state state{&document, &length, 1U, 0U, 0U};
    json_group *groups = json_read_from_stream(test_chunk_callback, &state, 1U);
    FT_ASSERT(groups != ft_nullptr);
    const json_group *empty = json_find_group(groups, "empty");
    const json_group *config = json_find_group(groups, "config");
    FT_ASSERT(empty != ft_nullptr);
    FT_ASSERT(empty->items == ft_nullptr);
    FT_ASSERT(config != ft_nullptr && config->items != ft_nullptr);
    FT_ASSERT_EQ(std::string("-12.340e+05"), std::string(config->items->value));
    json_free_groups(groups);
    return (1);
}
