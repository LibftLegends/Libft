#include "time.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <cstdio>
#include <charconv>
#include <utility>
#include <chrono>
#include <string>
#include <vector>
#include <new>
#include <cstring>
#include <cstddef>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

typedef struct s_time_trace_frame
{
    std::string                                 name;
    std::string                                 category;
    std::chrono::steady_clock::time_point       start_time;
}   t_time_trace_frame;

static std::FILE                                *g_trace_file = ft_nullptr;
static ft_bool                                     g_trace_session_active = FT_FALSE;
static ft_bool                                     g_trace_first_event = FT_TRUE;
static std::chrono::steady_clock::time_point    g_trace_session_start;
static std::vector<t_time_trace_frame>          g_trace_stack;
static std::string g_trace_line;

static ft_bool time_trace_write_raw(const char *buffer, ft_size_t length, ft_bool flush)
{
    ft_size_t  written;

    if (buffer == ft_nullptr && length != 0)
    {
        (void)(FT_ERR_INVALID_ARGUMENT);
        return (FT_FALSE);
    }
    if (g_trace_file == ft_nullptr)
    {
        (void)(FT_ERR_INVALID_STATE);
        return (FT_FALSE);
    }
    written = std::fwrite(buffer, 1, length, g_trace_file);
    if (written != length)
    {
        (void)(FT_ERR_IO);
        return (FT_FALSE);
    }
    if (flush)
    {
        if (std::fflush(g_trace_file) != 0)
        {
            (void)(FT_ERR_IO);
            return (FT_FALSE);
        }
    }
    (void)(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

static void time_trace_append_json(std::string &line, const char *input)
{
    if (input == ft_nullptr)
        return ;
    constexpr char HEX_DIGITS[] = "0123456789abcdef";
    while (*input != '\0')
    {
        const uint8_t character = static_cast<uint8_t>(*input++);
        if (character == '"' || character == '\\')
        {
            line.push_back('\\');
            line.push_back(static_cast<char>(character));
        }
        else if (character == '\n')
            line.append("\\n");
        else if (character == '\r')
            line.append("\\r");
        else if (character == '\t')
            line.append("\\t");
        else if (character < 0x20U)
        {
            line.append("\\u00");
            line.push_back(HEX_DIGITS[character >> 4U]);
            line.push_back(HEX_DIGITS[character & 15U]);
        }
        else
            line.push_back(static_cast<char>(character));
    }
    return ;
}

static int64_t time_trace_elapsed_microseconds(
    std::chrono::steady_clock::time_point moment)
{
    return (std::chrono::duration_cast<std::chrono::microseconds>(
            moment - g_trace_session_start).count());
}

static void time_trace_append_integer(std::string &line, int64_t value)
{
    char digits[20];
    const auto result = std::to_chars(digits, digits + sizeof(digits), value);
    line.append(digits, result.ptr);
    return ;
}

static ft_bool time_trace_write_event(const char *name, const char *category,
    int64_t timestamp_us, const int64_t *duration_us)
{
    std::string &line = g_trace_line;
    line.clear();
    try
    {
        if (!g_trace_first_event)
            line.append(",\n");
        line.append("{\"name\":\"");
        time_trace_append_json(line, name);
        line.append("\",\"cat\":\"");
        time_trace_append_json(line, category);
        if (duration_us != ft_nullptr)
            line.append("\",\"ph\":\"X\",\"ts\":");
        else
            line.append("\",\"ph\":\"i\",\"ts\":");
        time_trace_append_integer(line, timestamp_us);
        if (duration_us != ft_nullptr)
        {
            line.append(",\"dur\":");
            time_trace_append_integer(line, *duration_us);
        }
        line.append(",\"pid\":0,\"tid\":0");
        if (duration_us == ft_nullptr)
            line.append(",\"s\":\"t\"");
        line.append("}\n");
    }
    catch (const std::bad_alloc &)
    {
        return (FT_FALSE);
    }
    if (!time_trace_write_raw(line.data(), line.size(), FT_TRUE))
        return (FT_FALSE);
    g_trace_first_event = FT_FALSE;
    return (FT_TRUE);
}

ft_bool    time_trace_begin_session(const char *file_path)
{
    const char  *header;
    ft_size_t      header_length;

    if (g_trace_session_active)
    {
        (void)(FT_ERR_ALREADY_INITIALISED);
        return (FT_FALSE);
    }
    if (file_path == ft_nullptr)
    {
        (void)(FT_ERR_INVALID_ARGUMENT);
        return (FT_FALSE);
    }
    g_trace_file = std::fopen(file_path, "w");
    if (g_trace_file == ft_nullptr)
    {
        (void)(FT_ERR_IO);
        return (FT_FALSE);
    }
    header = "{\"traceEvents\":[\n";
    header_length = std::strlen(header);
    if (std::fwrite(header, 1, header_length, g_trace_file) != header_length)
    {
        std::fclose(g_trace_file);
        g_trace_file = ft_nullptr;
        (void)(FT_ERR_IO);
        return (FT_FALSE);
    }
    if (std::fflush(g_trace_file) != 0)
    {
        std::fclose(g_trace_file);
        g_trace_file = ft_nullptr;
        (void)(FT_ERR_IO);
        return (FT_FALSE);
    }
    g_trace_session_start = std::chrono::steady_clock::now();
    g_trace_session_active = FT_TRUE;
    g_trace_first_event = FT_TRUE;
    std::vector<t_time_trace_frame>().swap(g_trace_stack);
    std::string().swap(g_trace_line);
    (void)(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

ft_bool    time_trace_end_session(void)
{
    const char  *footer;
    ft_size_t      footer_length;

    if (!g_trace_session_active)
    {
        (void)(FT_ERR_INVALID_STATE);
        return (FT_FALSE);
    }
    if (!g_trace_stack.empty())
    {
        (void)(FT_ERR_INVALID_STATE);
        return (FT_FALSE);
    }
    footer = "\n]}\n";
    footer_length = std::strlen(footer);
    if (std::fwrite(footer, 1, footer_length, g_trace_file) != footer_length)
    {
        std::fclose(g_trace_file);
        g_trace_file = ft_nullptr;
        g_trace_session_active = FT_FALSE;
        (void)(FT_ERR_IO);
        return (FT_FALSE);
    }
    if (std::fclose(g_trace_file) != 0)
    {
        g_trace_file = ft_nullptr;
        g_trace_session_active = FT_FALSE;
        (void)(FT_ERR_IO);
        return (FT_FALSE);
    }
    g_trace_file = ft_nullptr;
    g_trace_session_active = FT_FALSE;
    g_trace_first_event = FT_TRUE;
    std::vector<t_time_trace_frame>().swap(g_trace_stack);
    std::string().swap(g_trace_line);
    (void)(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

ft_bool time_trace_begin_event(const char *name, const char *category)
{
    if (!g_trace_session_active || g_trace_file == ft_nullptr)
        return (FT_FALSE);
    try
    {
        t_time_trace_frame frame;
        if (name != ft_nullptr)
            frame.name = name;
        if (category != ft_nullptr)
            frame.category = category;
        frame.start_time = std::chrono::steady_clock::now();
        g_trace_stack.push_back(std::move(frame));
    }
    catch (const std::bad_alloc &)
    {
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

ft_bool time_trace_end_event(void)
{
    if (!g_trace_session_active || g_trace_file == ft_nullptr || g_trace_stack.empty())
        return (FT_FALSE);
    const t_time_trace_frame frame = std::move(g_trace_stack.back());
    g_trace_stack.pop_back();
    const auto end = std::chrono::steady_clock::now();
    const int64_t start = time_trace_elapsed_microseconds(frame.start_time);
    int64_t duration = time_trace_elapsed_microseconds(end) - start;
    if (duration < 0)
        duration = 0;
    return (time_trace_write_event(frame.name.c_str(), frame.category.c_str(),
            start, &duration));
}

ft_bool time_trace_instant_event(const char *name, const char *category)
{
    if (!g_trace_session_active || g_trace_file == ft_nullptr)
        return (FT_FALSE);
    const auto now = std::chrono::steady_clock::now();
    return (time_trace_write_event(name, category,
            time_trace_elapsed_microseconds(now), ft_nullptr));
}
