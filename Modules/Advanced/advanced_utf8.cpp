#include "advanced.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/utf8.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/limits.hpp"

static int32_t ft_utf8_code_point_in_range(uint32_t code_point, uint32_t start_value,
        uint32_t end_value)
{
    if (code_point >= start_value && code_point <= end_value)
        return (1);
    return (0);
}

uint32_t ft_utf8_case_ascii_lower(uint32_t code_point)
{
    if (code_point >= 'A' && code_point <= 'Z')
        return (code_point + 32);
    return (code_point);
}

uint32_t ft_utf8_case_ascii_upper(uint32_t code_point)
{
    if (code_point >= 'a' && code_point <= 'z')
        return (code_point - 32);
    return (code_point);
}

int32_t ft_utf8_is_combining_code_point(uint32_t code_point)
{
    if (ft_utf8_code_point_in_range(code_point, 0x0300, 0x036F))
        return (1);
    if (ft_utf8_code_point_in_range(code_point, 0x1AB0, 0x1AFF))
        return (1);
    if (ft_utf8_code_point_in_range(code_point, 0x1DC0, 0x1DFF))
        return (1);
    if (ft_utf8_code_point_in_range(code_point, 0x20D0, 0x20FF))
        return (1);
    if (ft_utf8_code_point_in_range(code_point, 0xFE20, 0xFE2F))
        return (1);
    return (0);
}

int32_t ft_utf8_transform(const char *input, ft_size_t input_length,
        char *output_buffer, ft_size_t output_buffer_size,
        ft_utf8_case_hook case_hook)
{
    int32_t error_code;

    if (!input || !output_buffer || !case_hook)
        return (FT_ERR_INVALID_POINTER);
    ft_size_t effective_length = input_length;
    if (effective_length == 0)
        effective_length = ft_strlen_size_t(input);
    ft_size_t input_index = 0;
    ft_size_t output_index = 0;
    while (input_index < effective_length)
    {
        uint32_t decoded_code_point = static_cast<uint8_t>(input[input_index]);
        if (decoded_code_point < 0x80U)
            ++input_index;
        else
        {
            error_code = ft_utf8_next(input, effective_length, &input_index,
                    &decoded_code_point, ft_nullptr);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        const uint32_t mapped_code_point = case_hook(decoded_code_point);
        if (mapped_code_point < 0x80U)
        {
            if (output_index >= output_buffer_size
                || output_buffer_size - output_index <= 1U)
                return (FT_ERR_OUT_OF_RANGE);
            output_buffer[output_index++] = static_cast<char>(mapped_code_point);
            continue ;
        }
        char encoded_buffer[5];
        ft_size_t encoded_length = 0;
        error_code = ft_utf8_encode(mapped_code_point, encoded_buffer,
                sizeof(encoded_buffer), &encoded_length);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (output_index >= output_buffer_size
            || encoded_length >= output_buffer_size - output_index)
            return (FT_ERR_OUT_OF_RANGE);
        ft_size_t copy_index = 0;
        while (copy_index < encoded_length)
        {
            output_buffer[output_index] = encoded_buffer[copy_index];
            ++output_index;
            ++copy_index;
        }
    }
    if (output_index >= output_buffer_size)
        return (FT_ERR_OUT_OF_RANGE);
    output_buffer[output_index] = '\0';
    return (FT_ERR_SUCCESS);
}

int32_t ft_utf8_transform_alloc(const char *input, char **output_pointer,
        ft_utf8_case_hook case_hook)
{
    int32_t error_code;

    if (!input || !output_pointer || !case_hook)
        return (FT_ERR_INVALID_POINTER);
    ft_size_t code_point_count = 0;
    error_code = ft_utf8_count(input, &code_point_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (code_point_count > (FT_SYSTEM_SIZE_MAX - 1U) / 4U)
        return (FT_ERR_OUT_OF_RANGE);
    ft_size_t allocation_size = code_point_count * 4 + 1;
    char *allocated_buffer = static_cast<char *>(cma_malloc(allocation_size));
    if (!allocated_buffer)
        return (FT_ERR_NO_MEMORY);
    error_code = ft_utf8_transform(input, 0, allocated_buffer, allocation_size, case_hook);
    if (error_code != FT_ERR_SUCCESS)
    {
        cma_free(allocated_buffer);
        return (error_code);
    }
    *output_pointer = allocated_buffer;
    return (FT_ERR_SUCCESS);
}

int32_t ft_utf8_next_grapheme(const char *string, ft_size_t string_length,
        ft_size_t *index_pointer, ft_size_t *grapheme_length_pointer)
{
    int32_t error_code;

    if (!string || !index_pointer || !grapheme_length_pointer)
        return (FT_ERR_INVALID_POINTER);
    if (*index_pointer >= string_length)
        return (FT_ERR_OUT_OF_RANGE);
    ft_size_t local_index = *index_pointer;
    uint32_t code_point = 0;
    ft_size_t sequence_length = 0;
    error_code = ft_utf8_next(string, string_length, &local_index, &code_point, &sequence_length);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    ft_size_t grapheme_end = local_index;
    while (1)
    {
        if (grapheme_end >= string_length)
            break ;
        uint32_t lookahead_code_point = 0;
        ft_size_t lookahead_length = 0;
        ft_size_t lookahead_index = grapheme_end;
        error_code = ft_utf8_next(string, string_length, &lookahead_index,
                &lookahead_code_point, &lookahead_length);
        if (error_code != FT_ERR_SUCCESS)
            break ;
        if (!ft_utf8_is_combining_code_point(lookahead_code_point))
            break ;
        grapheme_end = lookahead_index;
    }
    *grapheme_length_pointer = grapheme_end - *index_pointer;
    *index_pointer = grapheme_end;
    return (FT_ERR_SUCCESS);
}

int32_t ft_utf8_duplicate_grapheme(const char *string, ft_size_t string_length,
        ft_size_t *index_pointer, char **grapheme_pointer)
{
    int32_t error_code;

    if (!string || !index_pointer || !grapheme_pointer)
        return (FT_ERR_INVALID_POINTER);
    *grapheme_pointer = ft_nullptr;
    ft_size_t grapheme_length = 0;
    ft_size_t start_index = *index_pointer;
    error_code = ft_utf8_next_grapheme(string, string_length, index_pointer, &grapheme_length);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    char *allocated_grapheme = ft_span_dup(string + start_index, grapheme_length);
    if (!allocated_grapheme)
        return (FT_ERR_NO_MEMORY);
    *grapheme_pointer = allocated_grapheme;
    return (FT_ERR_SUCCESS);
}
