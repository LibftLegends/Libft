#include "logger_internal.hpp"
#include <unistd.h>
#include <charconv>
#include "../Time/time_format_internal.hpp"
#include "../Printf/printf.hpp"
#include "../Basic/basic.hpp"
#include "../Sink/sink.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"

static ft_size_t logger_escape_control(uint8_t character, char (&escaped)[6],
    ft_bool use_json)
{
    constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
    escaped[0] = '\\';
    escaped[1] = static_cast<char>(character);
    if (character == '"' || character == '\\')
        return (2U);
    if (use_json == FT_FALSE)
    {
        escaped[1] = 'x';
        escaped[2] = HEX_DIGITS[character >> 4U];
        escaped[3] = HEX_DIGITS[character & 0x0FU];
        return (4U);
    }
    if (character == '\b')
        escaped[1] = 'b';
    else if (character == '\f')
        escaped[1] = 'f';
    else if (character == '\n')
        escaped[1] = 'n';
    else if (character == '\r')
        escaped[1] = 'r';
    else if (character == '\t')
        escaped[1] = 't';
    else
    {
        escaped[1] = 'u';
        escaped[2] = '0';
        escaped[3] = '0';
        escaped[4] = HEX_DIGITS[character >> 4U];
        escaped[5] = HEX_DIGITS[character & 0x0FU];
        return (6U);
    }
    return (2U);
}

int32_t logger_append_quoted_value(ft_string &buffer, const char *value,
    ft_bool use_json)
{
    if (value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t error = buffer.append('"');
    if (error != FT_ERR_SUCCESS)
        return (error);
    const char *span = value;
    const char *cursor = value;
    while (*cursor != '\0')
    {
        const uint8_t character = static_cast<uint8_t>(*cursor);
        if (character < 0x20U || character == '"' || character == '\\')
        {
            if (cursor != span)
            {
                error = buffer.append(span, static_cast<ft_size_t>(cursor - span));
                if (error != FT_ERR_SUCCESS)
                    return (error);
            }
            char escaped[6];
            const ft_size_t length = logger_escape_control(character, escaped, use_json);
            error = buffer.append(escaped, length);
            if (error != FT_ERR_SUCCESS)
                return (error);
            span = cursor + 1U;
        }
        cursor += 1U;
    }
    if (cursor != span)
    {
        error = buffer.append(span, static_cast<ft_size_t>(cursor - span));
        if (error != FT_ERR_SUCCESS)
            return (error);
    }
    return (buffer.append('"'));
}

int32_t logger_build_standard_message(t_log_level level, const ft_string &message_text,
    const ft_string &context_fragment, ft_string &formatted_message)
{
    char timestamp[21];
    const ft_size_t timestamp_length = time_format_rfc3339_buffer(time_now(), timestamp);
    if (timestamp_length == 0U)
        return (FT_ERR_INTERNAL);
    char severity[16];
    const auto severity_result = std::to_chars(severity, severity + sizeof(severity),
        ft_log_level_to_severity(level));
    if (severity_result.ec != std::errc{})
        return (FT_ERR_INTERNAL);
    const ft_size_t severity_length = static_cast<ft_size_t>(severity_result.ptr - severity);
    if (formatted_message.append("time=", 5U) != FT_ERR_SUCCESS
        || formatted_message.append(timestamp, timestamp_length) != FT_ERR_SUCCESS
        || formatted_message.append(" level=", 7U) != FT_ERR_SUCCESS
        || formatted_message.append(ft_level_to_str(level)) != FT_ERR_SUCCESS
        || formatted_message.append(" severity=", 10U) != FT_ERR_SUCCESS
        || formatted_message.append(severity, severity_length) != FT_ERR_SUCCESS
        || formatted_message.append(" message=", 9U) != FT_ERR_SUCCESS
        || logger_append_quoted_value(formatted_message, message_text.c_str(),
            FT_FALSE) != FT_ERR_SUCCESS)
        return (formatted_message.get_error());
    if (context_fragment.size() != 0U && (formatted_message.append(' ') != FT_ERR_SUCCESS
        || formatted_message.append(context_fragment) != FT_ERR_SUCCESS))
        return (formatted_message.get_error());
    if (formatted_message.append('\n') != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

int32_t logger_prepare_message(t_log_level level, ft_string &text,
    ft_string &formatted_message)
{
    ft_string context;
    int32_t error = logger_prepare_redacted_context(text, context);
    if (error == FT_ERR_SUCCESS)
        error = formatted_message.initialize();
    if (error == FT_ERR_SUCCESS)
        error = logger_build_standard_message(level, text, context, formatted_message);
    return (error);
}

int32_t ft_log_vwrite(t_log_level level, const char *format_string, va_list argument_list)
{
    if (format_string == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (level < g_level.load(std::memory_order_relaxed))
        return (FT_ERR_SUCCESS);
    char message_buffer[1024];
    va_list args_copy;
    va_copy(args_copy, argument_list);
    const int32_t formatted_length = pf_vsnprintf(message_buffer, sizeof(message_buffer),
        format_string, args_copy);
    va_end(args_copy);
    if (formatted_length < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string text;
    const int32_t error = text.initialize(message_buffer);
    if (error != FT_ERR_SUCCESS)
        return (error);
    return (logger_write_text(level, text));
}

int32_t logger_write_text(t_log_level level, ft_string &text)
{
    ft_string final_message;
    const int32_t error = logger_prepare_message(level, text, final_message);
    if (error != FT_ERR_SUCCESS)
        return (error);
    return (logger_write_prepared(level, final_message, FT_FALSE));
}

int32_t logger_write_prepared(t_log_level level, const ft_string &final_message, ft_bool structured)
{
    (void)sink_record_message(static_cast<int32_t>(level), final_message.c_str(), final_message.size());
    return (logger_deliver_sync(level, final_message, structured));
}

int32_t logger_deliver_sync(t_log_level level, const ft_string &final_message, ft_bool structured)
{
    ft_bool has_sinks = FT_FALSE;
    const int32_t error = logger_deliver_to_sinks(final_message, FT_FALSE, has_sinks, structured);
    if (error != FT_ERR_SUCCESS || has_sinks != FT_FALSE)
        return (error);
    const ft_bool use_color = g_use_color.load(std::memory_order_relaxed) && isatty(1);
    if (use_color != FT_FALSE)
    {
        const char *color_code = "\x1b[31m";
        if (level == LOG_LEVEL_DEBUG)
            color_code = "\x1b[36m";
        else if (level == LOG_LEVEL_INFO)
            color_code = "\x1b[32m";
        else if (level == LOG_LEVEL_WARN)
            color_code = "\x1b[33m";
        if (su_write(1, color_code, 5U) != 5)
            return (FT_ERR_IO);
    }
    const ft_size_t length = final_message.size();
    const int64_t written = su_write(1, final_message.c_str(), length);
    if (use_color != FT_FALSE && su_write(1, "\x1b[0m", 4U) != 4)
        return (FT_ERR_IO);
    if (written < 0 || static_cast<ft_size_t>(written) != length)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}
