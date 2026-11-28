#include "uri_internal.hpp"
#include "../Basic/limits.hpp"

static ft_bool uri_has_scheme(const char *input, ft_size_t input_size,
    ft_size_t *scheme_end) noexcept
{
    ft_size_t index;

    if (input_size == 0 || uri_is_alpha(input[0]) == FT_FALSE)
        return (FT_FALSE);
    index = 1;
    while (index < input_size)
    {
        if (input[index] == ':')
        {
            *scheme_end = index;
            return (FT_TRUE);
        }
        if (uri_is_scheme_character(input[index]) == FT_FALSE)
            return (FT_FALSE);
        index++;
    }
    return (FT_FALSE);
}

static int32_t uri_parse_authority(const char *input, ft_size_t start,
    ft_size_t input_size, ft_size_t &end, uri_components *components) noexcept
{
    ft_size_t host_start = start;
    ft_size_t at_index = input_size;
    ft_size_t colon_index = input_size;
    ft_size_t bracket_index;
    int32_t error_code;

    end = start;
    while (end < input_size)
    {
        const char character = input[end];
        if (character == '/' || character == '?' || character == '#')
            break ;
        if (character == '@')
            at_index = end;
        else if (character == ':')
            colon_index = end;
        end += 1U;
    }
    if (at_index != input_size)
    {
        error_code = uri_assign_component(&components->userinfo, input, start,
                at_index);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        host_start = at_index + 1;
    }
    if (host_start < end && input[host_start] == '[')
    {
        bracket_index = host_start + 1;
        while (bracket_index < end && input[bracket_index] != ']')
            bracket_index++;
        if (bracket_index == end)
            return (FT_ERR_INVALID_ARGUMENT);
        error_code = uri_assign_component(&components->host, input, host_start,
                bracket_index + 1);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (bracket_index + 1 < end)
        {
            if (input[bracket_index + 1] != ':')
                return (FT_ERR_INVALID_ARGUMENT);
            return (uri_assign_component(&components->port, input,
                bracket_index + 2, end));
        }
        return (FT_ERR_SUCCESS);
    }
    if (colon_index < end && colon_index >= host_start)
    {
        error_code = uri_assign_component(&components->host, input, host_start,
                colon_index);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        return (uri_assign_component(&components->port, input, colon_index + 1,
            end));
    }
    return (uri_assign_component(&components->host, input, host_start, end));
}

static ft_size_t uri_find_path_end(const char *input, ft_size_t input_size,
    ft_size_t start) noexcept
{
    ft_size_t index;

    index = start;
    while (index < input_size)
    {
        if (input[index] == '?' || input[index] == '#')
            return (index);
        index++;
    }
    return (input_size);
}

static int32_t uri_parse_tail(const char *input, ft_size_t input_size,
    ft_size_t start, uri_components *components) noexcept
{
    ft_size_t path_end;
    ft_size_t query_start;
    ft_size_t query_end;
    int32_t error_code;

    path_end = uri_find_path_end(input, input_size, start);
    error_code = uri_assign_component(&components->path, input, start,
            path_end);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (path_end < input_size && input[path_end] == '?')
    {
        query_start = path_end + 1;
        query_end = query_start;
        while (query_end < input_size && input[query_end] != '#')
            query_end++;
        error_code = uri_assign_component(&components->query, input,
                query_start, query_end);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        path_end = query_end;
    }
    if (path_end < input_size && input[path_end] == '#')
        return (uri_assign_component(&components->fragment, input,
            path_end + 1, input_size));
    return (FT_ERR_SUCCESS);
}

int32_t uri_parse(const char *input, ft_size_t input_size,
    uri_components *components) noexcept
{
    ft_size_t index;
    ft_size_t scheme_end;
    ft_size_t authority_end;
    int32_t error_code;

    if (components == ft_nullptr || (input == ft_nullptr && input_size != 0))
        return (uri_set_error(FT_ERR_INVALID_ARGUMENT));
    uri_components_reset(components);
    if (input_size >= FT_SYSTEM_SIZE_MAX)
        return (uri_set_error(FT_ERR_OUT_OF_RANGE));
    index = 0;
    if (uri_has_scheme(input, input_size, &scheme_end) == FT_TRUE)
    {
        error_code = uri_assign_component(&components->scheme, input, 0,
                scheme_end);
        if (error_code != FT_ERR_SUCCESS)
            return (uri_set_error(error_code));
        index = scheme_end + 1;
    }
    if (index + 1 < input_size && input[index] == '/'
        && input[index + 1] == '/')
    {
        error_code = uri_parse_authority(input, index + 2U, input_size,
            authority_end, components);
        if (error_code != FT_ERR_SUCCESS)
        {
            uri_delete_components(components);
            return (uri_set_error(error_code));
        }
        index = authority_end;
    }
    error_code = uri_parse_tail(input, input_size, index, components);
    if (error_code != FT_ERR_SUCCESS)
    {
        uri_delete_components(components);
        return (uri_set_error(error_code));
    }
    return (uri_set_error(FT_ERR_SUCCESS));
}
