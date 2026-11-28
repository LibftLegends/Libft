#include "filesystem.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../CMA/CMA.hpp"

static ft_bool filesystem_glob_is_separator(char character) noexcept
{
    if (character == '/' || character == '\\')
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool filesystem_path_has_wildcards(const char *path) noexcept
{
    ft_size_t index;

    if (path == ft_nullptr)
        return (FT_FALSE);
    index = 0;
    while (path[index] != '\0')
    {
        if (path[index] == '*' || path[index] == '?')
            return (FT_TRUE);
        index++;
    }
    return (FT_FALSE);
}

static ft_bool filesystem_glob_consume_literals(const char *&pattern, const char *&path) noexcept
{
    while (*pattern != '\0' && *pattern != '*')
    {
        if (filesystem_glob_is_separator(*pattern) == FT_TRUE)
        {
            if (filesystem_glob_is_separator(*path) == FT_FALSE)
                return (FT_FALSE);
            while (filesystem_glob_is_separator(*pattern) == FT_TRUE)
                ++pattern;
            while (filesystem_glob_is_separator(*path) == FT_TRUE)
                ++path;
            continue ;
        }
        if (*pattern == '?')
        {
            if (*path == '\0' || filesystem_glob_is_separator(*path) == FT_TRUE)
                return (FT_FALSE);
        }
        else if (*pattern != *path)
            return (FT_FALSE);
        ++pattern;
        ++path;
    }
    return (FT_TRUE);
}

static ft_bool filesystem_glob_match_tail(const char *pattern, const char *path) noexcept
{
    if (filesystem_glob_consume_literals(pattern, path) == FT_FALSE)
        return (FT_FALSE);
    while (filesystem_glob_is_separator(*path) == FT_TRUE)
        ++path;
    return (*path == '\0');
}

static const char *filesystem_glob_skip_star_run(const char *begin, const char *end) noexcept
{
    while (end > begin && end[-1] == '*')
        --end;
    return (end);
}

static void filesystem_glob_fill_star_row(ft_bool *row, const ft_bool *suffix,
    const char *path, ft_size_t length, ft_bool crosses_separator) noexcept
{
    row[length] = suffix[length];
    while (length > 0)
    {
        --length;
        row[length] = suffix[length] || (row[length + 1]
            && (crosses_separator == FT_TRUE || filesystem_glob_is_separator(path[length]) == FT_FALSE));
    }
    return ;
}

static void filesystem_glob_fill_separator_row(ft_bool *row, const ft_bool *suffix,
    const char *path, ft_size_t length) noexcept
{
    ft_size_t after_separator = length;
    row[length] = FT_FALSE;
    while (length > 0)
    {
        --length;
        if (filesystem_glob_is_separator(path[length]) == FT_TRUE)
            row[length] = suffix[after_separator];
        else
        {
            row[length] = FT_FALSE;
            after_separator = length;
        }
    }
    return ;
}

static void filesystem_glob_fill_literal_row(ft_bool *row, const ft_bool *suffix,
    const char *path, ft_size_t length, char token) noexcept
{
    row[length] = FT_FALSE;
    while (length > 0)
    {
        --length;
        ft_bool matches = token == path[length];
        if (token == '?')
            matches = filesystem_glob_is_separator(path[length]) == FT_FALSE;
        row[length] = matches && suffix[length + 1];
    }
    return ;
}

static ft_bool filesystem_glob_match_rows(const char *pattern, const char *path,
    ft_size_t path_length, ft_bool *suffix, ft_bool *row) noexcept
{
    // Each row records which path offsets match the pattern suffix.
    suffix[path_length] = FT_TRUE;
    ft_size_t index = path_length;
    while (index > 0)
    {
        --index;
        suffix[index] = suffix[index + 1] && filesystem_glob_is_separator(path[index]);
    }

    const char *cursor = pattern + ft_strlen_size_t(pattern);
    while (cursor > pattern)
    {
        const char token = *--cursor;
        if (token == '*')
        {
            const char *start = filesystem_glob_skip_star_run(pattern, cursor);
            filesystem_glob_fill_star_row(row, suffix, path, path_length, start != cursor);
            cursor = start;
        }
        else if (filesystem_glob_is_separator(token) == FT_TRUE)
        {
            while (cursor > pattern && filesystem_glob_is_separator(cursor[-1]) == FT_TRUE)
                --cursor;
            const char *stars = filesystem_glob_skip_star_run(pattern, cursor);
            if (cursor - stars > 1)
            {
                filesystem_glob_fill_star_row(row, suffix, path, path_length, FT_TRUE);
                cursor = stars;
            }
            else
                filesystem_glob_fill_separator_row(row, suffix, path, path_length);
        }
        else
            filesystem_glob_fill_literal_row(row, suffix, path, path_length, token);
        ft_bool *previous_row = suffix;
        suffix = row;
        row = previous_row;
    }
    return (suffix[0]);
}

static ft_bool filesystem_glob_match_multiple_stars(const char *pattern, const char *path) noexcept
{
    constexpr ft_size_t INLINE_ROW_SIZE = 512;
    ft_bool inline_rows[2 * INLINE_ROW_SIZE];
    const ft_size_t path_length = ft_strlen_size_t(path);
    if (path_length < INLINE_ROW_SIZE)
        return (filesystem_glob_match_rows(pattern, path, path_length,
            inline_rows, inline_rows + INLINE_ROW_SIZE));
    if (path_length >= FT_SYSTEM_SIZE_MAX / 2)
        return (FT_FALSE);
    const ft_size_t row_size = path_length + 1;
    ft_bool *rows = static_cast<ft_bool *>(cma_malloc(2 * row_size));
    if (rows == ft_nullptr)
        return (FT_FALSE);
    const ft_bool matches = filesystem_glob_match_rows(pattern, path, path_length,
        rows, rows + row_size);
    cma_free(rows);
    return (matches);
}

static ft_bool filesystem_glob_match_segment_tail(const char *tail, ft_size_t tail_length,
    const char *path, ft_bool crosses_separator) noexcept
{
    const char *end = path;
    if (crosses_separator == FT_TRUE)
    {
        end += ft_strlen_size_t(path);
        while (end > path && filesystem_glob_is_separator(end[-1]) == FT_TRUE)
            --end;
    }
    else
    {
        while (*end != '\0' && filesystem_glob_is_separator(*end) == FT_FALSE)
            ++end;
    }
    if (static_cast<ft_size_t>(end - path) < tail_length)
        return (FT_FALSE);
    return (filesystem_glob_match_tail(tail, end - tail_length));
}

ft_bool filesystem_match_glob(const char *pattern, const char *path) noexcept
{
    if (pattern == ft_nullptr || path == ft_nullptr)
        return (FT_FALSE);
    if (filesystem_glob_consume_literals(pattern, path) == FT_FALSE)
        return (FT_FALSE);
    if (*pattern == '\0')
        return (filesystem_glob_match_tail(pattern, path));

    const char *tail = pattern + 1;
    while (*tail == '*')
        ++tail;
    const ft_bool crosses_separator = tail - pattern > 1;
    if (crosses_separator == FT_TRUE)
    {
        while (filesystem_glob_is_separator(*tail) == FT_TRUE)
            ++tail;
        if (*tail == '\0')
            return (FT_TRUE);
    }
    ft_bool tail_has_separator = FT_FALSE;
    const char *tail_end = tail;
    while (*tail_end != '\0')
    {
        if (*tail_end == '*')
            return (filesystem_glob_match_multiple_stars(pattern, path));
        if (filesystem_glob_is_separator(*tail_end) == FT_TRUE)
            tail_has_separator = FT_TRUE;
        ++tail_end;
    }
    if (tail_has_separator == FT_FALSE)
        return (filesystem_glob_match_segment_tail(tail, static_cast<ft_size_t>(tail_end - tail),
            path, crosses_separator));
    while (filesystem_glob_match_tail(tail, path) == FT_FALSE)
    {
        if (*path == '\0'
            || (crosses_separator == FT_FALSE && filesystem_glob_is_separator(*path) == FT_TRUE))
            return (FT_FALSE);
        ++path;
    }
    return (FT_TRUE);
}
