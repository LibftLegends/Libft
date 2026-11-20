#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <initializer_list>
#include <unistd.h>
#include <new>
#include "json.hpp"
#include "document.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Compatebility/compatebility_internal.hpp"
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

int32_t json_write_to_file(const char *file_path, json_group *groups)
{
    if (!file_path)
    {
        JSON_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    }
    int32_t file_descriptor = su_open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_descriptor < 0)
        return (FT_ERR_INVALID_STATE);
    pf_printf_fd(file_descriptor, "{\n");
    json_group *group_iterator = groups;
    while (group_iterator)
    {
        pf_printf_fd(file_descriptor, "  \"%s\": {\n", group_iterator->name);
        json_item *item_iterator = group_iterator->items;
        while (item_iterator)
        {
            const char *value_text = item_iterator->value;
            ft_bool is_big_number_value = FT_FALSE;
            if (item_iterator->is_big_number == FT_TRUE && item_iterator->big_number)
            {
                value_text = item_iterator->big_number->c_str();
                is_big_number_value = FT_TRUE;
            }
            if (item_iterator->next)
            {
                if (is_big_number_value == FT_TRUE)
                    pf_printf_fd(file_descriptor,
                        "    \"%s\": %s,\n", item_iterator->key, value_text);
                else
                    pf_printf_fd(file_descriptor,
                        "    \"%s\": \"%s\",\n", item_iterator->key, value_text);
            }
            else
            {
                if (is_big_number_value == FT_TRUE)
                    pf_printf_fd(file_descriptor,
                        "    \"%s\": %s\n", item_iterator->key, value_text);
                else
                    pf_printf_fd(file_descriptor,
                        "    \"%s\": \"%s\"\n", item_iterator->key, value_text);
            }
            item_iterator = item_iterator->next;
        }
        if (group_iterator->next)
            pf_printf_fd(file_descriptor, "  },\n");
        else
            pf_printf_fd(file_descriptor, "  }\n");
        group_iterator = group_iterator->next;
    }
    pf_printf_fd(file_descriptor, "}\n");
    if (cmp_close(file_descriptor) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    JSON_WRITER_SUCCESS_RETURN(0);
}

int32_t json_write_to_backend(ft_document_sink &sink, json_group *groups)
{
    char *serialized_content;
    ft_size_t serialized_length;
    int32_t write_result;

    serialized_content = json_write_to_string(groups);
    if (!serialized_content)
        return (FT_ERR_INVALID_STATE);
    serialized_length = ft_strlen(serialized_content);
    write_result = sink.write_all(serialized_content, serialized_length);
    cma_free(serialized_content);
    if (write_result != FT_ERR_SUCCESS)
    {
        json_writer_set_error(write_result);
        return (FT_ERR_INVALID_STATE);
    }
    JSON_WRITER_SUCCESS_RETURN(0);
}

static int32_t json_writer_append(char *output, ft_size_t capacity,
    ft_size_t &size, std::initializer_list<const char *> parts)
{
    const char *const *part;
    ft_size_t length;

    part = parts.begin();
    while (part != parts.end())
    {
        length = ft_strlen_size_t(*part);
        if (length > capacity - size)
            return (FT_ERR_OUT_OF_RANGE);
        if (output != ft_nullptr && length != 0U)
            ft_memcpy(output + size, *part, length);
        size += length;
        part += 1;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t json_writer_emit_groups(const json_group *group, char *output,
    ft_size_t capacity, ft_size_t &size)
{
    const json_item *item;
    ft_bool is_big_number;
    const char *value_text;
    const char *quote;
    const char *line_end;
    int32_t error_code;

    size = 0;
    error_code = json_writer_append(output, capacity, size, {"{\n"});
    while (error_code == FT_ERR_SUCCESS && group != ft_nullptr)
    {
        error_code = json_writer_append(output, capacity, size,
            {"  \"", group->name, "\": {\n"});
        item = group->items;
        while (error_code == FT_ERR_SUCCESS && item != ft_nullptr)
        {
            is_big_number = FT_FALSE;
            value_text = item->value;
            if (item->is_big_number == FT_TRUE && item->big_number != ft_nullptr)
            {
                is_big_number = FT_TRUE;
                value_text = item->big_number->c_str();
            }
            quote = "\"";
            if (is_big_number == FT_TRUE)
                quote = "";
            line_end = "\n";
            if (item->next != ft_nullptr)
                line_end = ",\n";
            error_code = json_writer_append(output, capacity, size,
                {"    \"", item->key, "\": ", quote, value_text, quote, line_end});
            item = item->next;
        }
        line_end = "  }\n";
        if (group->next != ft_nullptr)
            line_end = "  },\n";
        if (error_code == FT_ERR_SUCCESS)
            error_code = json_writer_append(output, capacity, size, {line_end});
        group = group->next;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = json_writer_append(output, capacity, size, {"}\n"});
    return (error_code);
}

char *json_write_to_string(json_group *groups)
{
    ft_size_t required_size;
    ft_size_t written_size;
    int32_t error_code;
    char *output;

    /* Count before allocating so each line is copied just once. */
    error_code = json_writer_emit_groups(groups, ft_nullptr,
        FT_SYSTEM_SIZE_MAX - 1U, required_size);
    if (error_code != FT_ERR_SUCCESS)
        JSON_WRITER_ERROR_RETURN(error_code, ft_nullptr);
    output = static_cast<char *>(cma_malloc(required_size + 1U));
    if (output == ft_nullptr)
        JSON_WRITER_ERROR_RETURN(FT_ERR_NO_MEMORY, ft_nullptr);
    error_code = json_writer_emit_groups(groups, output, required_size,
        written_size);
    if (error_code != FT_ERR_SUCCESS)
    {
        cma_free(output);
        JSON_WRITER_ERROR_RETURN(error_code, ft_nullptr);
    }
    output[written_size] = '\0';
    JSON_WRITER_SUCCESS_RETURN(output);
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
