#include "logger_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"

static void logger_append_literal(ft_string &buffer, const char *literal,
    int32_t &error_code_value)
{
    if (error_code_value != FT_ERR_SUCCESS)
        return ;
    if (!literal)
    {
        error_code_value = FT_ERR_INVALID_ARGUMENT;
        return ;
    }
    error_code_value = buffer.append(literal);
    return ;
}

static void logger_append_json_string(ft_string &buffer, const char *value,
    const s_log_redaction_snapshot *snapshot, int32_t &error_code_value)
{
    if (error_code_value != FT_ERR_SUCCESS)
        return ;
    if (snapshot == ft_nullptr)
    {
        error_code_value = logger_append_quoted_value(buffer, value, FT_TRUE);
        return ;
    }
    ft_string storage;
    error_code_value = logger_redact_log_value(snapshot, value, storage);
    if (error_code_value == FT_ERR_SUCCESS)
        error_code_value = logger_append_quoted_value(buffer, value, FT_TRUE);
    return ;
}

int32_t logger_append_json_field(ft_string &output, const char *key,
    const char *value, ft_bool &first_field, const s_log_redaction_snapshot *snapshot)
{
    if (key == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (first_field == FT_FALSE && output.append(',') != FT_ERR_SUCCESS)
        return (output.get_error());
    first_field = FT_FALSE;
    int32_t error = FT_ERR_SUCCESS;
    logger_append_json_string(output, key, snapshot, error);
    if (error == FT_ERR_SUCCESS)
        error = output.append(':');
    if (error != FT_ERR_SUCCESS)
        return (error);
    if (value == ft_nullptr)
        return (output.append("null", 4U));
    logger_append_json_string(output, value, snapshot, error);
    return (error);
}

static void logger_append_field(ft_string &output, const s_log_field &field,
    ft_bool &first_field, const s_log_redaction_snapshot *snapshot, int32_t &error)
{
    if (error != FT_ERR_SUCCESS)
        return ;
    ft_bool acquired = FT_FALSE;
    error = log_field_lock(&field, &acquired);
    if (error != FT_ERR_SUCCESS)
        return ;
    error = logger_append_json_field(output, field.key, field.value, first_field, snapshot);
    log_field_unlock(&field, acquired);
    return ;
}

int32_t logger_emit_structured(t_log_level level, const char *message,
    const s_log_field *fields, ft_size_t field_count)
{
    ft_string payload;
    int32_t error_code_value;
    ft_size_t entry_index;
    ft_bool first_field;

    if (level != LOG_LEVEL_DEBUG && level != LOG_LEVEL_INFO && level != LOG_LEVEL_WARN)
        level = LOG_LEVEL_ERROR;
    if (message == ft_nullptr || (field_count != 0U && fields == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (level < g_level.load(std::memory_order_relaxed))
        return (FT_ERR_SUCCESS);
    s_log_redaction_snapshot *snapshot;
    error_code_value = logger_acquire_redaction_snapshot(snapshot);
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    error_code_value = payload.initialize();
    logger_append_literal(payload, "{\"message\":", error_code_value);
    logger_append_json_string(payload, message, snapshot, error_code_value);
    logger_append_literal(payload, ",\"fields\":{", error_code_value);
    first_field = FT_TRUE;
    entry_index = 0;
    while (entry_index < field_count && error_code_value == FT_ERR_SUCCESS)
    {
        logger_append_field(payload, fields[entry_index], first_field, snapshot, error_code_value);
        entry_index += 1;
    }
    if (error_code_value == FT_ERR_SUCCESS)
        error_code_value = logger_context_append_json(payload, first_field, snapshot);
    logger_append_literal(payload, "}}", error_code_value);
    ft_string context;
    if (error_code_value == FT_ERR_SUCCESS)
        error_code_value = context.initialize();
    if (error_code_value == FT_ERR_SUCCESS)
        error_code_value = logger_context_format_flat(context, snapshot);
    logger_release_redaction_snapshot(snapshot);
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    ft_string formatted_message;
    error_code_value = formatted_message.initialize();
    if (error_code_value == FT_ERR_SUCCESS)
        error_code_value = logger_build_standard_message(level, payload, context, formatted_message);
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    if (g_async_running.load(std::memory_order_relaxed) != FT_FALSE)
        return (logger_enqueue_prepared(level, formatted_message, FT_TRUE, FT_TRUE));
    return (logger_write_prepared(level, formatted_message, FT_TRUE));
}

void ft_log_structured(t_log_level level, const char *message,
    const s_log_field *fields, ft_size_t field_count)
{
    (void)logger_emit_structured(level, message, fields, field_count);
    return ;
}

void ft_log_debug_structured(const char *message, const s_log_field *fields,
    ft_size_t field_count)
{
    ft_log_structured(LOG_LEVEL_DEBUG, message, fields, field_count);
    return ;
}

void ft_log_info_structured(const char *message, const s_log_field *fields,
    ft_size_t field_count)
{
    ft_log_structured(LOG_LEVEL_INFO, message, fields, field_count);
    return ;
}

void ft_log_warn_structured(const char *message, const s_log_field *fields,
    ft_size_t field_count)
{
    ft_log_structured(LOG_LEVEL_WARN, message, fields, field_count);
    return ;
}

void ft_log_error_structured(const char *message, const s_log_field *fields,
    ft_size_t field_count)
{
    ft_log_structured(LOG_LEVEL_ERROR, message, fields, field_count);
    return ;
}
