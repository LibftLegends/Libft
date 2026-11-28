#include <fcntl.h>
#include <initializer_list>
#include <string_view>
#include "json_internal.hpp"
#include "document.hpp"
#include "../Basic/basic.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_big_number.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static thread_local int32_t g_json_writer_last_error = FT_ERR_SUCCESS;

static void json_writer_set_error(int32_t error_code)
{
    g_json_writer_last_error = error_code;
    return ;
}

#define JSON_WRITER_ERROR_RETURN(code, value) \
    do { json_writer_set_error(code); return (value); } while (0)

#define JSON_WRITER_SUCCESS_RETURN(value) \
    do { json_writer_set_error(FT_ERR_SUCCESS); return (value); } while (0)

using json_writer_parts = std::initializer_list<std::string_view>;

static const char *json_writer_text(const char *value, const char *null_text)
{
    if (value == ft_nullptr)
        return (null_text);
    return (value);
}

template <typename Append>
static int32_t json_writer_emit_groups(const json_group *group, const char *null_text,
    Append append)
{
    int32_t error = append({"{\n"});
    while (error == FT_ERR_SUCCESS && group != ft_nullptr)
    {
        error = append({"  \"", json_writer_text(group->name, null_text), "\": {\n"});
        const json_item *item = group->items;
        while (error == FT_ERR_SUCCESS && item != ft_nullptr)
        {
            const char *value_text = item->value;
            std::string_view quote = "\"";
            if (item->is_big_number == FT_TRUE && item->big_number != ft_nullptr)
            {
                value_text = item->big_number->c_str();
                quote = "";
            }
            std::string_view line_end = "\n";
            if (item->next != ft_nullptr)
                line_end = ",\n";
            error = append({"    \"", json_writer_text(item->key, null_text), "\": ",
                quote, json_writer_text(value_text, null_text), quote, line_end});
            item = item->next;
        }
        if (error == FT_ERR_SUCCESS)
        {
            if (group->next == ft_nullptr)
                error = append({"  }\n"});
            else
                error = append({"  },\n"});
        }
        group = group->next;
    }
    if (error == FT_ERR_SUCCESS)
        error = append({"}\n"});
    return (error);
}

static int32_t json_writer_append_string(char *output, ft_size_t capacity,
    ft_size_t &size, json_writer_parts parts)
{
    const std::string_view *part = parts.begin();
    while (part != parts.end())
    {
        const ft_size_t length = part->size();
        if (length > capacity - size)
            return (FT_ERR_OUT_OF_RANGE);
        if (output != ft_nullptr && length != 0U)
            ft_memcpy(output + size, part->data(), length);
        size += length;
        ++part;
    }
    return (FT_ERR_SUCCESS);
}

int32_t json_write_buffer(const json_group *groups, char *&output, ft_size_t &written_size)
{
    output = ft_nullptr;
    written_size = 0U;
    ft_size_t required_size = 0U;
    int32_t error = json_writer_emit_groups(groups, "", [&](json_writer_parts parts)
    {
        return (json_writer_append_string(ft_nullptr, FT_SYSTEM_SIZE_MAX - 1U, required_size, parts));
    });
    if (error != FT_ERR_SUCCESS)
        return (error);
    output = static_cast<char *>(cma_malloc(required_size + 1U));
    if (output == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    error = json_writer_emit_groups(groups, "", [&](json_writer_parts parts)
    {
        return (json_writer_append_string(output, required_size, written_size, parts));
    });
    if (error != FT_ERR_SUCCESS)
    {
        cma_free(output);
        output = ft_nullptr;
        written_size = 0U;
        return (error);
    }
    output[written_size] = '\0';
    return (FT_ERR_SUCCESS);
}

static int32_t json_writer_write_bytes(int32_t descriptor, const char *data, ft_size_t size)
{
    int32_t error;
    const int64_t written = su_write_with_error(descriptor, data, size, &error);
    if (written < 0 || static_cast<ft_size_t>(written) != size)
    {
        if (error == FT_ERR_SUCCESS)
            return (FT_ERR_IO);
        return (error);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t json_writer_flush(int32_t descriptor, const char *buffer, ft_size_t &size)
{
    if (size == 0U)
        return (FT_ERR_SUCCESS);
    const int32_t error = json_writer_write_bytes(descriptor, buffer, size);
    size = 0U;
    return (error);
}

int32_t json_write_to_descriptor(int32_t descriptor, const json_group *groups)
{
    if (descriptor < 0)
        return (FT_ERR_INVALID_HANDLE);
    char buffer[16384U];
    ft_size_t used = 0U;
    int32_t error = json_writer_emit_groups(groups, "(null)", [&](json_writer_parts parts) -> int32_t
    {
        const std::string_view *part = parts.begin();
        while (part != parts.end())
        {
            const ft_size_t size = part->size();
            if (size > sizeof(buffer) - used)
            {
                const int32_t flush_error = json_writer_flush(descriptor, buffer, used);
                if (flush_error != FT_ERR_SUCCESS)
                    return (flush_error);
            }
            if (size > sizeof(buffer))
            {
                const int32_t write_error = json_writer_write_bytes(descriptor, part->data(), size);
                if (write_error != FT_ERR_SUCCESS)
                    return (write_error);
            }
            else if (size != 0U)
            {
                ft_memcpy(buffer + used, part->data(), size);
                used += size;
            }
            ++part;
        }
        return (FT_ERR_SUCCESS);
    });
    if (error == FT_ERR_SUCCESS)
        error = json_writer_flush(descriptor, buffer, used);
    return (error);
}

int32_t json_write_to_file(const char *file_path, json_group *groups)
{
    if (file_path == ft_nullptr)
        JSON_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    const int32_t descriptor = su_open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (descriptor < 0)
        JSON_WRITER_ERROR_RETURN(FT_ERR_INVALID_HANDLE, FT_ERR_INVALID_STATE);

    int32_t error = json_write_to_descriptor(descriptor, groups);
    const int32_t close_error = su_close(descriptor);
    if (error == FT_ERR_SUCCESS)
        error = close_error;
    if (error != FT_ERR_SUCCESS)
        JSON_WRITER_ERROR_RETURN(error, FT_ERR_INVALID_STATE);
    JSON_WRITER_SUCCESS_RETURN(FT_ERR_SUCCESS);
}

int32_t json_write_to_backend(ft_document_sink &sink, json_group *groups)
{
    ft_size_t length;
    char *serialized;
    const int32_t build_error = json_write_buffer(groups, serialized, length);
    if (build_error != FT_ERR_SUCCESS)
        JSON_WRITER_ERROR_RETURN(build_error, FT_ERR_INVALID_STATE);
    const int32_t error = sink.write_all(serialized, length);
    cma_free(serialized);
    if (error != FT_ERR_SUCCESS)
        JSON_WRITER_ERROR_RETURN(error, FT_ERR_INVALID_STATE);
    JSON_WRITER_SUCCESS_RETURN(FT_ERR_SUCCESS);
}

char *json_write_to_string_with_length(const json_group *groups, ft_size_t &length)
{
    char *output;
    const int32_t error = json_write_buffer(groups, output, length);
    if (error != FT_ERR_SUCCESS)
        JSON_WRITER_ERROR_RETURN(error, ft_nullptr);
    JSON_WRITER_SUCCESS_RETURN(output);
}

char *json_write_to_string(json_group *groups)
{
    ft_size_t length;

    return (json_write_to_string_with_length(groups, length));
}

int32_t json_document_write_to_file(const char *file_path, const json_document &document)
{
    return (document.write_to_file(file_path));
}

char *json_document_write_to_string(const json_document &document)
{
    return (document.write_to_string());
}

int32_t json_document_write_to_backend(ft_document_sink &sink, const json_document &document)
{
    return (document.write_to_backend(sink));
}
