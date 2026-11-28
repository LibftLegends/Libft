#include "logger_internal.hpp"
#include <unistd.h>
#include <cerrno>
#include "../Printf/printf.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"
#include "../System_utils/system_utils.hpp"

struct s_json_sink_field
{
    ft_string key;
    ft_string value;
    ft_bool has_value;


    s_json_sink_field()
        : key(), value(), has_value(FT_FALSE)
    {
        return ;
    }

    int32_t initialize(const s_json_sink_field &other) noexcept
    {
        int32_t error_code;

        error_code = this->key.initialize(other.key);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = this->value.initialize(other.value);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        this->has_value = other.has_value;

        return (FT_ERR_SUCCESS);
    }

    int32_t destroy() noexcept
    {
        int32_t first_error;
        int32_t error_code;

        first_error = this->key.destroy();
        error_code = this->value.destroy();
        if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
            first_error = error_code;
        this->has_value = FT_FALSE;

        return (first_error);
    }
};

static int32_t logger_json_sink_append_literal(ft_string &buffer, const char *literal)
{
    if (!literal)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    return (buffer.append(literal));
}

static int32_t logger_json_sink_append_character_sequence(ft_string &buffer, const char *sequence)
{
    if (!sequence)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    return (buffer.append(sequence));
}

static int32_t logger_json_sink_hex_value(char character)
{
    if (character >= '0' && character <= '9')
        return (character - '0');
    if (character >= 'a' && character <= 'f')
        return (10 + character - 'a');
    if (character >= 'A' && character <= 'F')
        return (10 + character - 'A');
    return (FT_ERR_INVALID_ARGUMENT);
}

static int32_t logger_json_sink_decode_quoted(const char *message,
    ft_size_t &index, ft_string &destination)
{
    while (message[index] != '\0' && message[index] != '"')
    {
        const ft_size_t start = index;
        while (message[index] != '\0' && message[index] != '"'
            && message[index] != '\\')
            index += 1U;
        if (index != start)
        {
            const int32_t error = destination.append(message + start, index - start);
            if (error != FT_ERR_SUCCESS)
                return (error);
        }
        if (message[index] == '\\')
        {
            char character = message[index++];
            if (message[index] != '\0')
            {
                character = message[index++];
                if (character == 'x' && message[index] != '\0'
                    && message[index + 1U] != '\0')
                {
                    const int32_t high = logger_json_sink_hex_value(message[index]);
                    const int32_t low = logger_json_sink_hex_value(message[index + 1U]);
                    if (high >= 0 && low >= 0)
                    {
                        character = static_cast<char>((high << 4U) | low);
                        index += 2U;
                    }
                }
            }
            const int32_t error = destination.append(character);
            if (error != FT_ERR_SUCCESS)
                return (error);
        }
    }
    if (message[index] == '"')
        index += 1U;
    return (FT_ERR_SUCCESS);
}

static int32_t logger_json_sink_decode_unquoted(const char *message,
    ft_size_t &index, ft_string &destination)
{
    const ft_size_t start = index;
    while (message[index] != '\0' && message[index] != ' ' && message[index] != '\n')
        index += 1U;
    return (destination.append(message + start, index - start));
}

static std::atomic<uint64_t> g_next_sink_identity{1U};

int32_t logger_add_sink_locked(t_log_sink sink, void *user_data,
    s_log_sink_registration **registration)
{
    s_log_sink entry;
    entry.registration = new (std::nothrow) s_log_sink_registration();
    if (entry.registration == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    entry.registration->identity = g_next_sink_identity.fetch_add(1U,
        std::memory_order_relaxed);
    entry.function = sink;
    entry.user_data = user_data;
    g_sinks.push_back(entry);
    const int32_t error = g_sinks.get_error();
    if (error != FT_ERR_SUCCESS)
    {
        logger_release_sink(entry);
        return (error);
    }
    if (registration != ft_nullptr)
        *registration = entry.registration;
    return (FT_ERR_SUCCESS);
}

int32_t ft_log_add_sink(t_log_sink sink, void *user_data)
{
    if (sink == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const int32_t lock_error = logger_lock_sinks();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t error = logger_add_sink_locked(sink, user_data, ft_nullptr);
    const int32_t unlock_error = logger_unlock_sinks();
    if (error != FT_ERR_SUCCESS)
        return (error);
    return (unlock_error);
}
int32_t logger_write_json_sink(const char *message, void *user_data, ft_bool structured)
{
    char time_buffer[64];
    char level_buffer[16];
    char severity_buffer[16];
    char key_buffer[128];
    ft_string value_buffer;
    ft_string message_buffer;
    ft_vector<s_json_sink_field> context_fields;
    ft_bool structured_message_parsed = FT_FALSE;
    ft_size_t entry_index;
    int32_t file_descriptor;

    int32_t error_code_value;

    if (!message)
        return (FT_ERR_INVALID_ARGUMENT);
    time_buffer[0] = '\0';
    level_buffer[0] = '\0';
    severity_buffer[0] = '\0';
    if (value_buffer.initialize() != FT_ERR_SUCCESS
        || message_buffer.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);

    if (context_fields.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    file_descriptor = 1;
    if (user_data)
        file_descriptor = *static_cast<int32_t *>(user_data);
    entry_index = 0;
    while (message[entry_index] == ' ')
        entry_index += 1;
    while (message[entry_index] != '\0' && message[entry_index] != '\n')
    {
        ft_size_t key_length;
        ft_bool has_value;


        key_length = 0;
        if (message[entry_index] == '"')
        {
            entry_index += 1U;
            error_code_value = value_buffer.clear();
            if (error_code_value == FT_ERR_SUCCESS)
                error_code_value = logger_json_sink_decode_quoted(message, entry_index, value_buffer);
            if (error_code_value != FT_ERR_SUCCESS)
                return (error_code_value);
            ft_strlcpy(key_buffer, value_buffer.c_str(), sizeof(key_buffer));
            key_length = ft_strlen_size_t(key_buffer);
        }
        else
        {
            while (message[entry_index] != '\0' && message[entry_index] != '='
                && message[entry_index] != ' ' && message[entry_index] != '\n')
            {
                if (key_length + 1U < sizeof(key_buffer))
                {
                    key_buffer[key_length] = message[entry_index];
                    key_length += 1U;
                }
                entry_index += 1U;
            }
        }
        key_buffer[key_length] = '\0';
        while (message[entry_index] == ' ')
            entry_index += 1;
        has_value = FT_FALSE;

        error_code_value = value_buffer.clear();
        if (error_code_value != FT_ERR_SUCCESS)
            return (error_code_value);
        if (message[entry_index] == '=')
        {
            has_value = FT_TRUE;
            entry_index += 1;
            if (message[entry_index] == '"')
            {
                entry_index += 1;
                error_code_value = logger_json_sink_decode_quoted(message,
                    entry_index, value_buffer);
            }
            else
                error_code_value = logger_json_sink_decode_unquoted(message,
                    entry_index, value_buffer);
            if (error_code_value != FT_ERR_SUCCESS)
                return (error_code_value);
        }
        while (message[entry_index] == ' ')
            entry_index += 1;
        if (key_buffer[0] == '\0')
        {
            if (message[entry_index] == '\0' || message[entry_index] == '\n')
                break ;
            continue ;
        }
        if (structured_message_parsed != FT_FALSE && (ft_strcmp(key_buffer, "message") == 0
            || ft_strcmp(key_buffer, "time") == 0 || ft_strcmp(key_buffer, "level") == 0
            || ft_strcmp(key_buffer, "severity") == 0))
            continue ;
        if (ft_strcmp(key_buffer, "time") == 0 && has_value)
            ft_strlcpy(time_buffer, value_buffer.c_str(), sizeof(time_buffer));
        else if (ft_strcmp(key_buffer, "level") == 0 && has_value)
            ft_strlcpy(level_buffer, value_buffer.c_str(), sizeof(level_buffer));
        else if (ft_strcmp(key_buffer, "severity") == 0 && has_value)
            ft_strlcpy(severity_buffer, value_buffer.c_str(), sizeof(severity_buffer));
        else if (ft_strcmp(key_buffer, "message") == 0 && has_value)
        {
            error_code_value = message_buffer.move(value_buffer);
            if (error_code_value != FT_ERR_SUCCESS)
                return (error_code_value);
            structured_message_parsed = structured;
        }
        else
        {
            s_json_sink_field field;

            if (field.key.initialize(key_buffer) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            field.has_value = has_value;

            if (field.value.initialize(value_buffer.c_str()) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            context_fields.push_back(field);
            if (context_fields.get_error() != FT_ERR_SUCCESS)
                return (context_fields.get_error());
        }
        if (message[entry_index] == '\0' || message[entry_index] == '\n')
            break ;
    }
    ft_string payload;
    if (payload.initialize("{") != FT_ERR_SUCCESS)
    {
        return (FT_ERR_NO_MEMORY);
    }
    error_code_value = logger_json_sink_append_literal(payload, "\"time\":");
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    error_code_value = logger_append_quoted_value(payload, time_buffer, FT_TRUE);
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    error_code_value = logger_json_sink_append_literal(payload, ",\"level\":");
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    error_code_value = logger_append_quoted_value(payload, level_buffer, FT_TRUE);
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    if (severity_buffer[0] != '\0')
    {
        error_code_value = logger_json_sink_append_literal(payload, ",\"severity\":");
        if (error_code_value != FT_ERR_SUCCESS)
            return (error_code_value);
        error_code_value = logger_json_sink_append_character_sequence(payload, severity_buffer);
        if (error_code_value != FT_ERR_SUCCESS)
            return (error_code_value);
    }
    error_code_value = logger_json_sink_append_literal(payload, ",\"message\":");
    if (error_code_value != FT_ERR_SUCCESS)
        return (error_code_value);
    if (structured != FT_FALSE)
    {
        error_code_value = logger_json_sink_append_character_sequence(payload, message_buffer.c_str());
        if (error_code_value != FT_ERR_SUCCESS)
            return (error_code_value);
    }
    else
    {
        error_code_value = logger_append_quoted_value(payload, message_buffer.c_str(), FT_TRUE);
        if (error_code_value != FT_ERR_SUCCESS)
            return (error_code_value);
    }
    ft_size_t context_count;
    ft_size_t context_index;

    context_count = context_fields.size();
    if (context_fields.get_error() != FT_ERR_SUCCESS)
    {
        return (context_fields.get_error());
    }
    context_index = 0;
    while (context_index < context_count)
    {
        const s_json_sink_field &field = context_fields[context_index];

        error_code_value = payload.append(',');
        if (error_code_value == FT_ERR_SUCCESS)
            error_code_value = logger_append_quoted_value(payload, field.key.c_str(), FT_TRUE);
        if (error_code_value == FT_ERR_SUCCESS)
            error_code_value = payload.append(':');
        if (error_code_value == FT_ERR_SUCCESS && field.has_value != FT_FALSE)
            error_code_value = logger_append_quoted_value(payload, field.value.c_str(), FT_TRUE);
        else if (error_code_value == FT_ERR_SUCCESS)
            error_code_value = payload.append("true", 4U);
        if (error_code_value != FT_ERR_SUCCESS)
            return (error_code_value);
        context_index += 1;
    }
    error_code_value = logger_json_sink_append_literal(payload, "}\n");
    if (error_code_value != FT_ERR_SUCCESS)
    {
        return (error_code_value);
    }
    const int64_t write_result = su_write(file_descriptor, payload.c_str(), payload.size());
    if (write_result < 0 || static_cast<ft_size_t>(write_result) != payload.size())
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

int32_t ft_json_sink(const char *message, void *user_data)
{
    return (logger_write_json_sink(message, user_data, FT_FALSE));
}
