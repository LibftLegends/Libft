#include "json_stream_writer.hpp"

#include "json_stream_events.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"

static thread_local int32_t g_json_stream_writer_last_error = FT_ERR_SUCCESS;

static void json_stream_writer_set_error(int32_t error_code)
{
    g_json_stream_writer_last_error = error_code;
    return ;
}

#define JSON_STREAM_WRITER_ERROR_RETURN(code, value) \
    do { json_stream_writer_set_error(code); return (value); } while (0)

#define JSON_STREAM_WRITER_SUCCESS_RETURN(value) \
    do { json_stream_writer_set_error(FT_ERR_SUCCESS); return (value); } while (0)

static int32_t json_stream_writer_last_error(void)
{
    return (g_json_stream_writer_last_error);
}

static int32_t json_stream_writer_write_chunk(json_stream_writer *writer,
    const char *buffer,
    ft_size_t size)
{
    ft_size_t offset;
    ft_size_t written;

    if (!writer || !buffer)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    }
    offset = 0;
    while (offset < size)
    {
        written = writer->write_callback(writer->user_data, buffer + offset, size - offset);
        if (written == 0U || written > size - offset)
        {
            JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_IO, -1);
        }
        offset += written;
    }
    JSON_STREAM_WRITER_SUCCESS_RETURN(0);
}

template <ft_size_t Length>
static int32_t json_stream_writer_write_literal(json_stream_writer *writer,
    const char (&literal)[Length])
{
    return (json_stream_writer_write_chunk(writer, literal, Length - 1U));
}

static int32_t json_stream_writer_write_escaped_string(json_stream_writer *writer,
    const char *value,
    ft_size_t length)
{
    ft_size_t index;
    unsigned char character;
    char buffer[6];
    constexpr char HEX_DIGITS[] = "0123456789ABCDEF";

    if (json_stream_writer_write_chunk(writer, "\"", 1) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    index = 0;
    while (index < length)
    {
        const ft_size_t span_start = index;
        while (index < length)
        {
            character = static_cast<unsigned char>(value[index]);
            if (character < 0x20U || character == '\\' || character == '"')
                break ;
            ++index;
        }
        if (index != span_start
            && json_stream_writer_write_chunk(writer, value + span_start, index - span_start) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        if (index == length)
            break ;
        character = static_cast<unsigned char>(value[index]);
        if (character == '\\' || character == '"')
        {
            buffer[0] = '\\';
            buffer[1] = static_cast<char>(character);
            if (json_stream_writer_write_chunk(writer, buffer, 2) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            index += 1;
            continue ;
        }
        if (character == '\b')
        {
            if (json_stream_writer_write_literal(writer, "\\b") != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            index += 1;
            continue ;
        }
        if (character == '\f')
        {
            if (json_stream_writer_write_literal(writer, "\\f") != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            index += 1;
            continue ;
        }
        if (character == '\n')
        {
            if (json_stream_writer_write_literal(writer, "\\n") != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            index += 1;
            continue ;
        }
        if (character == '\r')
        {
            if (json_stream_writer_write_literal(writer, "\\r") != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            index += 1;
            continue ;
        }
        if (character == '\t')
        {
            if (json_stream_writer_write_literal(writer, "\\t") != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            index += 1;
            continue ;
        }
        buffer[0] = '\\';
        buffer[1] = 'u';
        buffer[2] = '0';
        buffer[3] = '0';
        buffer[4] = HEX_DIGITS[(character >> 4) & 0x0F];
        buffer[5] = HEX_DIGITS[character & 0x0F];
        if (json_stream_writer_write_chunk(writer, buffer, 6) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        index += 1;
    }
    if (json_stream_writer_write_chunk(writer, "\"", 1) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    JSON_STREAM_WRITER_SUCCESS_RETURN(0);
}

static int32_t json_stream_writer_write_string(json_stream_writer *writer,
    const json_stream_scalar *scalar)
{
    if (!scalar)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    }
    if (scalar->data == ft_nullptr && scalar->length != FT_ERR_SUCCESS)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    }
    if (scalar->data == ft_nullptr)
        return (json_stream_writer_write_chunk(writer, "\"\"", 2));
    return (json_stream_writer_write_escaped_string(writer, scalar->data, scalar->length));
}

static int32_t json_stream_writer_write_number(json_stream_writer *writer,
    const json_stream_scalar *scalar)
{
    if (!scalar || scalar->data == ft_nullptr || scalar->length == FT_ERR_SUCCESS)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    }
    return (json_stream_writer_write_chunk(writer, scalar->data, scalar->length));
}

static json_stream_writer_context *json_stream_writer_current_context(json_stream_writer *writer)
{
    if (!writer)
        return (ft_nullptr);
    if (writer->context_size == FT_ERR_SUCCESS)
        return (ft_nullptr);
    return (&writer->context_stack[writer->context_size - 1]);
}

static int32_t json_stream_writer_ensure_capacity(json_stream_writer *writer,
    ft_size_t required)
{
    ft_size_t capacity;
    ft_size_t new_capacity;
    json_stream_writer_context *resized;

    if (!writer)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    }
    if (writer->context_capacity >= required)
        return (FT_ERR_SUCCESS);
    capacity = writer->context_capacity;
    if (capacity == FT_ERR_SUCCESS)
        capacity = 4;
    new_capacity = capacity;
        while (new_capacity < required)
        {
            ft_size_t next_capacity;

            next_capacity = new_capacity * 2;
            if (next_capacity <= new_capacity)
            {
                JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_OUT_OF_RANGE, -1);
            }
            new_capacity = next_capacity;
        }
    if (writer->context_stack == ft_nullptr)
        resized = static_cast<json_stream_writer_context *>(cma_malloc(new_capacity * sizeof(*resized)));
    else
        resized = static_cast<json_stream_writer_context *>(cma_realloc(writer->context_stack, new_capacity * sizeof(*resized)));
    if (!resized)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_NO_MEMORY, -1);
    }
    writer->context_stack = resized;
    writer->context_capacity = new_capacity;
    return (FT_ERR_SUCCESS);
}

static int32_t json_stream_writer_push_context(json_stream_writer *writer,
    char type)
{
    json_stream_writer_context *context;

    if (type != 'o' && type != 'a')
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    }
    if (json_stream_writer_ensure_capacity(writer, writer->context_size + 1) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    context = &writer->context_stack[writer->context_size];
    context->type = type;
    context->need_comma = FT_FALSE;
    if (type == 'o')
        context->expecting_key = FT_TRUE;
    else
        context->expecting_key = FT_FALSE;
    writer->context_size += 1;
    JSON_STREAM_WRITER_SUCCESS_RETURN(0);
}

static int32_t json_stream_writer_pop_context(json_stream_writer *writer,
    char expected_type)
{
    json_stream_writer_context *context;

    if (!writer || writer->context_size == FT_ERR_SUCCESS)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
    }
    context = &writer->context_stack[writer->context_size - 1];
    if (context->type != expected_type)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
    }
    if (expected_type == 'o' && context->expecting_key == FT_FALSE)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
    }
    writer->context_size -= 1;
    JSON_STREAM_WRITER_SUCCESS_RETURN(0);
}

static int32_t json_stream_writer_prepare_value(json_stream_writer *writer)
{
    json_stream_writer_context *current;

    current = json_stream_writer_current_context(writer);
    if (current == ft_nullptr)
    {
        if (writer->root_value_emitted != FT_FALSE)
            JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
        writer->root_value_emitted = FT_TRUE;
        return (FT_ERR_SUCCESS);
    }
    if (current->type == 'o')
    {
        if (current->expecting_key != FT_FALSE)
            JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
        return (FT_ERR_SUCCESS);
    }
    if (current->type == 'a')
    {
        if (current->need_comma != FT_FALSE)
        {
            if (json_stream_writer_write_literal(writer, ",") != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
        }
        return (FT_ERR_SUCCESS);
    }
    JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
}

static void json_stream_writer_mark_parent_consumed(json_stream_writer *writer)
{
    json_stream_writer_context *current;

    current = json_stream_writer_current_context(writer);
    if (current == ft_nullptr)
        return ;
    if (current->type == 'o')
    {
        current->expecting_key = FT_TRUE;
        current->need_comma = FT_TRUE;
        return ;
    }
    if (current->type == 'a')
    {
        current->need_comma = FT_TRUE;
        return ;
    }
    return ;
}

int32_t json_stream_writer_init(json_stream_writer *writer,
    json_stream_write_callback callback,
    void *user_data)
{
    if (!writer || !callback)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    }
    ft_bzero(writer, sizeof(*writer));
    writer->write_callback = callback;
    writer->user_data = user_data;
    writer->context_stack = ft_nullptr;
    writer->context_size = 0;
    writer->context_capacity = 0;
    writer->began_document = FT_FALSE;
    writer->finished_document = FT_FALSE;
    writer->root_value_emitted = FT_FALSE;
    writer->error_code = FT_ERR_SUCCESS;
    JSON_STREAM_WRITER_SUCCESS_RETURN(0);
}

int32_t json_stream_writer_reset(json_stream_writer *writer)
{
    if (!writer)
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    if (!writer->write_callback)
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
    writer->context_size = 0;
    writer->began_document = FT_FALSE;
    writer->finished_document = FT_FALSE;
    writer->root_value_emitted = FT_FALSE;
    writer->error_code = FT_ERR_SUCCESS;
    JSON_STREAM_WRITER_SUCCESS_RETURN(FT_ERR_SUCCESS);
}

void json_stream_writer_destroy(json_stream_writer *writer)
{
    if (!writer)
        return ;
    if (writer->context_stack != ft_nullptr)
        cma_free(writer->context_stack);
    writer->context_stack = ft_nullptr;
    writer->context_size = 0;
    writer->context_capacity = 0;
    writer->write_callback = ft_nullptr;
    writer->user_data = ft_nullptr;
    writer->began_document = FT_FALSE;
    writer->finished_document = FT_FALSE;
    writer->root_value_emitted = FT_FALSE;
    writer->error_code = FT_ERR_SUCCESS;
    return ;
}

static int32_t json_stream_writer_write_boolean(json_stream_writer *writer,
    ft_bool value)
{
    if (value != FT_FALSE)
        return (json_stream_writer_write_literal(writer, "true"));
    return (json_stream_writer_write_literal(writer, "false"));
}

int32_t json_stream_writer_process(json_stream_writer *writer,
    const json_stream_event *event)
{
    json_stream_writer_context *current;

    if (!writer || !event)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    }
    if (!writer->write_callback)
    {
        writer->error_code = FT_ERR_INVALID_STATE;
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
    }
    if (writer->error_code != FT_ERR_SUCCESS)
    {
        json_stream_writer_set_error(writer->error_code);
        return (FT_ERR_INVALID_STATE);
    }
    if (event->type == JSON_STREAM_EVENT_BEGIN_DOCUMENT)
    {
        if (writer->began_document != FT_FALSE)
        {
            writer->error_code = FT_ERR_INVALID_STATE;
            JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
        }
        writer->began_document = FT_TRUE;
        writer->finished_document = FT_FALSE;
        writer->root_value_emitted = FT_FALSE;
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    if (event->type == JSON_STREAM_EVENT_END_DOCUMENT)
    {
        if (writer->began_document == FT_FALSE || writer->finished_document != FT_FALSE)
        {
            writer->error_code = FT_ERR_INVALID_STATE;
            JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
        }
        if (writer->context_size != FT_ERR_SUCCESS || writer->root_value_emitted == FT_FALSE)
        {
            writer->error_code = FT_ERR_INVALID_STATE;
            JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
        }
        writer->finished_document = FT_TRUE;
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    if (writer->began_document == FT_FALSE || writer->finished_document != FT_FALSE)
    {
        writer->error_code = FT_ERR_INVALID_STATE;
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
    }
    if (event->type == JSON_STREAM_EVENT_KEY)
    {
        if (!event->value.data && event->value.length != FT_ERR_SUCCESS)
        {
            writer->error_code = FT_ERR_INVALID_ARGUMENT;
            JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
        }
        current = json_stream_writer_current_context(writer);
        if (current == ft_nullptr || current->type != 'o' || current->expecting_key == FT_FALSE)
        {
            writer->error_code = FT_ERR_INVALID_STATE;
            JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
        }
        if (current->need_comma != FT_FALSE)
        {
            if (json_stream_writer_write_literal(writer, ",") != FT_ERR_SUCCESS)
            {
                writer->error_code = json_stream_writer_last_error();
                return (FT_ERR_INVALID_STATE);
            }
        }
        if (event->value.data == ft_nullptr)
        {
            if (json_stream_writer_write_chunk(writer, "\"\"", 2) != FT_ERR_SUCCESS)
            {
                writer->error_code = json_stream_writer_last_error();
                return (FT_ERR_INVALID_STATE);
            }
        }
        else if (json_stream_writer_write_escaped_string(writer,
                 event->value.data,
                 event->value.length) != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        if (json_stream_writer_write_literal(writer, ":") != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        current->expecting_key = FT_FALSE;
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    if (event->type == JSON_STREAM_EVENT_BEGIN_OBJECT)
    {
        if (json_stream_writer_prepare_value(writer) != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        if (json_stream_writer_write_literal(writer, "{") != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        json_stream_writer_mark_parent_consumed(writer);
        if (json_stream_writer_push_context(writer, 'o') != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    if (event->type == JSON_STREAM_EVENT_END_OBJECT)
    {
        if (json_stream_writer_pop_context(writer, 'o') != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        if (json_stream_writer_write_literal(writer, "}") != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    if (event->type == JSON_STREAM_EVENT_BEGIN_ARRAY)
    {
        if (json_stream_writer_prepare_value(writer) != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        if (json_stream_writer_write_literal(writer, "[") != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        json_stream_writer_mark_parent_consumed(writer);
        if (json_stream_writer_push_context(writer, 'a') != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    if (event->type == JSON_STREAM_EVENT_END_ARRAY)
    {
        if (json_stream_writer_pop_context(writer, 'a') != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        if (json_stream_writer_write_literal(writer, "]") != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    if (event->type == JSON_STREAM_EVENT_STRING)
    {
        if (json_stream_writer_prepare_value(writer) != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        if (json_stream_writer_write_string(writer, &event->value) != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        json_stream_writer_mark_parent_consumed(writer);
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    if (event->type == JSON_STREAM_EVENT_NUMBER)
    {
        if (json_stream_writer_prepare_value(writer) != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        if (json_stream_writer_write_number(writer, &event->value) != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        json_stream_writer_mark_parent_consumed(writer);
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    if (event->type == JSON_STREAM_EVENT_BOOLEAN)
    {
        if (json_stream_writer_prepare_value(writer) != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        if (json_stream_writer_write_boolean(writer, event->bool_value) != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        json_stream_writer_mark_parent_consumed(writer);
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    if (event->type == JSON_STREAM_EVENT_NULL)
    {
        if (json_stream_writer_prepare_value(writer) != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        if (json_stream_writer_write_literal(writer, "null") != FT_ERR_SUCCESS)
        {
            writer->error_code = json_stream_writer_last_error();
            return (FT_ERR_INVALID_STATE);
        }
        json_stream_writer_mark_parent_consumed(writer);
        JSON_STREAM_WRITER_SUCCESS_RETURN(0);
    }
    writer->error_code = FT_ERR_INVALID_ARGUMENT;
    JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
}

int32_t json_stream_writer_finish(json_stream_writer *writer)
{
    if (!writer)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, -1);
    }
    if (writer->error_code != FT_ERR_SUCCESS)
    {
        json_stream_writer_set_error(writer->error_code);
        return (FT_ERR_INVALID_STATE);
    }
    if (writer->began_document == FT_FALSE || writer->finished_document == FT_FALSE)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
    }
    if (writer->context_size != FT_ERR_SUCCESS)
    {
        JSON_STREAM_WRITER_ERROR_RETURN(FT_ERR_INVALID_STATE, -1);
    }
    JSON_STREAM_WRITER_SUCCESS_RETURN(0);
}
