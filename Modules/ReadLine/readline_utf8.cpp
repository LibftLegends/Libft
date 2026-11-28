#include "readline_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/utf8.hpp"
#include "../Advanced/advanced.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <cstddef>
#include <cstdint>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t rl_utf8_code_point_width(uint32_t code_point)
{
    if (ft_utf8_is_combining_code_point(code_point))
        return (FT_ERR_SUCCESS);
    if (code_point == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    if (code_point < 0x20)
        return (FT_ERR_SUCCESS);
    if (code_point >= 0x7F && code_point < 0xA0)
        return (FT_ERR_SUCCESS);
    if (code_point >= 0x1100 && code_point <= 0x115F)
        return (2);
    if (code_point == 0x2329 || code_point == 0x232A)
        return (2);
    if (code_point >= 0x2E80 && code_point <= 0xA4CF)
        return (2);
    if (code_point >= 0xAC00 && code_point <= 0xD7A3)
        return (2);
    if (code_point >= 0xF900 && code_point <= 0xFAFF)
        return (2);
    if (code_point >= 0xFE10 && code_point <= 0xFE19)
        return (2);
    if (code_point >= 0xFE30 && code_point <= 0xFE6F)
        return (2);
    if (code_point >= 0xFF00 && code_point <= 0xFF60)
        return (2);
    if (code_point >= 0xFFE0 && code_point <= 0xFFE6)
        return (2);
    if (code_point >= 0x1F300 && code_point <= 0x1F64F)
        return (2);
    if (code_point >= 0x1F900 && code_point <= 0x1F9FF)
        return (2);
    if (code_point >= 0x20000 && code_point <= 0x3FFFD)
        return (2);
    return (1);
}

static int32_t rl_utf8_measure_grapheme_width(const char *buffer, ft_size_t grapheme_length, int32_t *display_width)
{
    ft_size_t offset;
    int32_t total_width;

    if (display_width == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    offset = 0;
    total_width = 0;
    while (offset < grapheme_length)
    {
        ft_size_t decode_index;
        uint32_t code_point;
        ft_size_t sequence_length;

        decode_index = offset;
        code_point = 0;
        sequence_length = 0;
        if (ft_utf8_next(buffer, grapheme_length, &decode_index,
                &code_point, &sequence_length) != FT_ERR_SUCCESS)
        {
            total_width = static_cast<int32_t>(grapheme_length);
            *display_width = total_width;
            return (FT_ERR_SUCCESS);
        }
        total_width += rl_utf8_code_point_width(code_point);
        offset = decode_index;
    }
    *display_width = total_width;
    return (FT_ERR_SUCCESS);
}

static int32_t rl_utf8_ascii_width(uint8_t byte) noexcept
{
    return (byte >= 0x20 && byte != 0x7F);
}

static void rl_utf8_extract_grapheme(const char *buffer, ft_size_t buffer_length,
        ft_size_t start_index, ft_size_t &end_index, int32_t &display_width)
{
    const uint8_t byte = static_cast<uint8_t>(buffer[start_index]);
    end_index = start_index + 1;
    // The following byte must be ASCII too, or it may start a combining mark.
    if (byte < 0x80 && (end_index == buffer_length
            || static_cast<uint8_t>(buffer[end_index]) < 0x80))
    {
        display_width = rl_utf8_ascii_width(byte);
        return ;
    }

    ft_size_t decoder_index = start_index;
    ft_size_t grapheme_length = 0;
    if (ft_utf8_next_grapheme(buffer, buffer_length, &decoder_index, &grapheme_length) != FT_ERR_SUCCESS)
    {
        display_width = 1;
        return ;
    }
    end_index = decoder_index;
    (void)rl_utf8_measure_grapheme_width(buffer + start_index, grapheme_length, &display_width);
    return ;
}

static void rl_utf8_measure_columns(const char *buffer, ft_size_t buffer_length,
        ft_size_t prefix_length, int32_t &total_columns, int32_t &prefix_columns)
{
    total_columns = 0;
    prefix_columns = 0;
    ft_size_t index = 0;
    while (index < buffer_length)
    {
        ft_size_t next_index;
        int32_t display_width;
        rl_utf8_extract_grapheme(buffer, buffer_length, index, next_index, display_width);
        if (index < prefix_length)
        {
            if (next_index <= prefix_length)
                prefix_columns += display_width;
            else
                prefix_columns += static_cast<int32_t>(prefix_length - index);
        }
        total_columns += display_width;
        index = next_index;
    }
    return ;
}

int32_t rl_utf8_compute_columns(const char *string, int32_t *columns)
{
    if (string == ft_nullptr || columns == ft_nullptr)
        return (FT_ERR_INTERNAL);
    const ft_size_t string_length = ft_strlen_size_t(string);
    int32_t total_columns;
    int32_t unused_prefix_columns;
    rl_utf8_measure_columns(string, string_length, 0, total_columns, unused_prefix_columns);
    *columns = total_columns;
    return (FT_ERR_SUCCESS);
}

int32_t rl_utf8_find_previous_grapheme(const char *buffer, int32_t cursor_pos,
        int32_t *start_byte, int32_t *end_byte, int32_t *display_width)
{
    ft_size_t buffer_length;
    ft_size_t index;

    if (buffer == ft_nullptr || start_byte == ft_nullptr
        || end_byte == ft_nullptr || display_width == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    if (cursor_pos <= 0)
    {
        return (FT_ERR_NOT_FOUND);
    }
    buffer_length = ft_strlen_size_t(buffer);
    if (static_cast<ft_size_t>(cursor_pos) > buffer_length)
        cursor_pos = static_cast<int32_t>(buffer_length);
    if (cursor_pos == 0)
        return (FT_ERR_NOT_FOUND);
    const uint8_t previous_byte = static_cast<uint8_t>(buffer[cursor_pos - 1]);
    // A following non-ASCII byte may begin a combining mark.
    if (previous_byte < 0x80 && static_cast<uint8_t>(buffer[cursor_pos]) < 0x80)
    {
        *start_byte = cursor_pos - 1;
        *end_byte = cursor_pos;
        *display_width = rl_utf8_ascii_width(previous_byte);
        return (FT_ERR_SUCCESS);
    }
    index = 0;
    while (index < static_cast<ft_size_t>(cursor_pos))
    {
        ft_size_t next_index;
        int32_t width;

        rl_utf8_extract_grapheme(buffer, buffer_length, index, next_index, width);
        if (static_cast<int32_t>(next_index) >= cursor_pos)
        {
            if (static_cast<int32_t>(next_index) > cursor_pos)
            {
                *start_byte = cursor_pos - 1;
                *end_byte = cursor_pos;
                *display_width = 1;
                return (FT_ERR_SUCCESS);
            }
            *start_byte = static_cast<int32_t>(index);
            *end_byte = static_cast<int32_t>(next_index);
            *display_width = width;
            return (FT_ERR_SUCCESS);
        }
        index = next_index;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t rl_utf8_find_next_grapheme(const char *buffer, int32_t cursor_pos,
        int32_t *start_byte, int32_t *end_byte, int32_t *display_width)
{
    ft_size_t buffer_length;
    ft_size_t index;

    if (buffer == ft_nullptr || start_byte == ft_nullptr
        || end_byte == ft_nullptr || display_width == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    if (cursor_pos < 0)
        cursor_pos = 0;
    buffer_length = ft_strlen_size_t(buffer);
    if (static_cast<ft_size_t>(cursor_pos) >= buffer_length)
    {
        return (FT_ERR_NOT_FOUND);
    }
    // ASCII always starts a new grapheme, even when combining marks follow it.
    if (static_cast<uint8_t>(buffer[cursor_pos]) < 0x80)
    {
        ft_size_t next_index;
        rl_utf8_extract_grapheme(buffer, buffer_length, cursor_pos, next_index, *display_width);
        *start_byte = cursor_pos;
        *end_byte = static_cast<int32_t>(next_index);
        return (FT_ERR_SUCCESS);
    }
    index = 0;
    while (index < buffer_length)
    {
        ft_size_t next_index;
        int32_t width;

        rl_utf8_extract_grapheme(buffer, buffer_length, index, next_index, width);
        if (static_cast<int32_t>(index) >= cursor_pos)
        {
            *start_byte = static_cast<int32_t>(index);
            *end_byte = static_cast<int32_t>(next_index);
            *display_width = width;
            return (FT_ERR_SUCCESS);
        }
        index = next_index;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t rl_update_display_metrics(readline_state_t *state)
{
    ft_size_t buffer_length;
    int32_t total_columns;
    int32_t prefix_columns;

    if (state == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    if (state->buffer == ft_nullptr)
    {
        return (FT_ERR_INTERNAL);
    }
    buffer_length = ft_strlen_size_t(state->buffer);
    if (state->position < 0)
        state->position = 0;
    if (static_cast<ft_size_t>(state->position) > buffer_length)
        state->position = static_cast<int32_t>(buffer_length);
    rl_utf8_measure_columns(state->buffer, buffer_length, static_cast<ft_size_t>(state->position),
        total_columns, prefix_columns);
    state->display_pos = prefix_columns;
    state->prev_display_columns = total_columns;
    state->prev_buffer_length = static_cast<int32_t>(buffer_length);
    return (FT_ERR_SUCCESS);
}
