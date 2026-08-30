#include "scripting.hpp"
#include <climits>

struct scripting_parser
{
    scripting_engine *engine;
    const char *source;
    uint32_t source_length;
    uint32_t offset;
    uint32_t operation_count;
    scripting_diagnostic diagnostic;
    struct scripting_local
    {
        const char *name;
        uint32_t name_length;
        scripting_value value;
    } locals[FT_SCRIPTING_MAX_LOCALS];
    uint32_t local_count;
};

static ft_bool scripting_is_space(char character) noexcept
{
    if (character == ' ' || character == '\t' || character == '\r'
        || character == '\n')
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool scripting_is_digit(char character) noexcept
{
    if (character >= '0' && character <= '9')
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool scripting_is_identifier_start(char character) noexcept
{
    if ((character >= 'a' && character <= 'z')
        || (character >= 'A' && character <= 'Z') || character == '_')
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool scripting_is_identifier_part(char character) noexcept
{
    if (scripting_is_identifier_start(character) != FT_FALSE
        || scripting_is_digit(character) != FT_FALSE)
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool scripting_equal_identifier(const char *source,
    uint32_t start, uint32_t length, const char *expected) noexcept
{
    uint32_t index;

    index = 0U;
    while (index < length && expected[index] != '\0')
    {
        if (source[start + index] != expected[index])
            return (FT_FALSE);
        index += 1U;
    }
    if (index != length || expected[index] != '\0')
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool scripting_keyword_at(const scripting_parser *parser,
    const char *keyword, uint32_t keyword_length) noexcept
{
    uint32_t next_offset;

    if (parser->source_length - parser->offset < keyword_length
        || scripting_equal_identifier(parser->source, parser->offset,
            keyword_length, keyword) == FT_FALSE)
        return (FT_FALSE);
    next_offset = parser->offset + keyword_length;
    if (next_offset < parser->source_length
        && scripting_is_identifier_part(parser->source[next_offset])
            != FT_FALSE)
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t scripting_fail(scripting_parser *parser, int32_t error_code,
    uint32_t source_offset, uint32_t source_length) noexcept
{
    parser->diagnostic.error_code = error_code;
    parser->diagnostic.source_offset = source_offset;
    parser->diagnostic.source_length = source_length;
    return (error_code);
}

static void scripting_skip_space(scripting_parser *parser) noexcept
{
    while (parser->offset < parser->source_length
        && scripting_is_space(parser->source[parser->offset]) != FT_FALSE)
        parser->offset += 1U;
    return ;
}

static int32_t scripting_find_local(scripting_parser *parser,
    const char *name, uint32_t name_length, scripting_value *value) noexcept
{
    uint32_t index;
    uint32_t character_index;

    index = 0U;
    while (index < parser->local_count)
    {
        if (parser->locals[index].name_length == name_length)
        {
            character_index = 0U;
            while (character_index < name_length
                && parser->locals[index].name[character_index]
                    == name[character_index])
                character_index += 1U;
            if (character_index == name_length)
            {
                *value = parser->locals[index].value;
                return (FT_ERR_SUCCESS);
            }
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

static int32_t scripting_parse_expression(scripting_parser *parser,
    scripting_value *result) noexcept;

static int32_t scripting_parse_term(scripting_parser *parser,
    scripting_value *result) noexcept;

static int32_t scripting_add_checked(int64_t left_value, int64_t right_value,
    int64_t *result) noexcept
{
    if ((right_value > 0 && left_value > INT64_MAX - right_value)
        || (right_value < 0 && left_value < INT64_MIN - right_value))
        return (FT_ERR_OUT_OF_RANGE);
    *result = left_value + right_value;
    return (FT_ERR_SUCCESS);
}

struct scripting_compile_parser
{
    scripting_engine *engine;
    const char *source;
    uint32_t source_length;
    uint32_t offset;
    scripting_diagnostic diagnostic;
    scripting_program program;
    struct scripting_compile_local
    {
        const char *name;
        uint32_t name_length;
    } locals[FT_SCRIPTING_MAX_LOCALS];
    uint32_t local_count;
};

static int32_t scripting_compile_fail(scripting_compile_parser *parser,
    int32_t error_code, uint32_t source_offset, uint32_t source_length) noexcept
{
    parser->diagnostic.error_code = error_code;
    parser->diagnostic.source_offset = source_offset;
    parser->diagnostic.source_length = source_length;
    return (error_code);
}

static void scripting_compile_skip_space(scripting_compile_parser *parser)
    noexcept
{
    while (parser->offset < parser->source_length
        && scripting_is_space(parser->source[parser->offset]) != FT_FALSE)
        parser->offset += 1U;
    return ;
}

static ft_bool scripting_compile_keyword_at(
    const scripting_compile_parser *parser, const char *keyword,
    uint32_t keyword_length) noexcept
{
    uint32_t next_offset;

    if (parser->source_length - parser->offset < keyword_length
        || scripting_equal_identifier(parser->source, parser->offset,
            keyword_length, keyword) == FT_FALSE)
        return (FT_FALSE);
    next_offset = parser->offset + keyword_length;
    if (next_offset < parser->source_length
        && scripting_is_identifier_part(parser->source[next_offset])
            != FT_FALSE)
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t scripting_compile_emit(scripting_compile_parser *parser,
    scripting_opcode opcode, int64_t operand, uint32_t auxiliary) noexcept
{
    if (parser->program.instruction_count >= FT_SCRIPTING_MAX_INSTRUCTIONS)
        return (scripting_compile_fail(parser, FT_ERR_FULL, parser->offset, 1U));
    parser->program.instructions[parser->program.instruction_count].opcode = opcode;
    parser->program.instructions[parser->program.instruction_count].operand = operand;
    parser->program.instructions[parser->program.instruction_count].auxiliary = auxiliary;
    parser->program.instruction_count += 1U;
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_compile_find_local(
    const scripting_compile_parser *parser, const char *name,
    uint32_t name_length, uint32_t *local_id) noexcept
{
    uint32_t index;
    uint32_t character_index;

    index = 0U;
    while (index < parser->local_count)
    {
        if (parser->locals[index].name_length == name_length)
        {
            character_index = 0U;
            while (character_index < name_length
                && parser->locals[index].name[character_index]
                    == name[character_index])
                character_index += 1U;
            if (character_index == name_length)
            {
                *local_id = index;
                return (FT_ERR_SUCCESS);
            }
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

static int32_t scripting_compile_expression(scripting_compile_parser *parser)
    noexcept;

static int32_t scripting_compile_term(scripting_compile_parser *parser)
    noexcept;

static int32_t scripting_compile_primary(scripting_compile_parser *parser)
    noexcept
{
    uint32_t start;
    uint64_t unsigned_value;
    uint32_t native_id;
    uint32_t name_length;
    uint32_t argument_count;
    uint32_t local_id;
    int32_t parse_error;

    scripting_compile_skip_space(parser);
    if (parser->offset >= parser->source_length)
        return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
            parser->offset, 1U));
    if (parser->source[parser->offset] == '(')
    {
        parser->offset += 1U;
        parse_error = scripting_compile_expression(parser);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
        scripting_compile_skip_space(parser);
        if (parser->offset >= parser->source_length
            || parser->source[parser->offset] != ')')
            return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
                parser->offset, 1U));
        parser->offset += 1U;
        return (FT_ERR_SUCCESS);
    }
    if (parser->source[parser->offset] == '"')
    {
        uint32_t string_start;
        uint32_t string_length;
        uint32_t string_offset;

        parser->offset += 1U;
        string_start = parser->offset;
        while (parser->offset < parser->source_length
            && parser->source[parser->offset] != '"')
        {
            if (parser->source[parser->offset] == '\\'
                || parser->source[parser->offset] < 0x20)
                return (scripting_compile_fail(parser,
                    FT_ERR_INVALID_ARGUMENT, parser->offset, 1U));
            parser->offset += 1U;
        }
        if (parser->offset >= parser->source_length)
            return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
                string_start, parser->offset - string_start));
        string_length = parser->offset - string_start;
        if (string_length >= FT_SCRIPTING_MAX_STRING_BYTES
            || parser->program.string_data_size
                > FT_SCRIPTING_MAX_STRING_BYTES - string_length - 1U)
            return (scripting_compile_fail(parser, FT_ERR_FULL,
                string_start, string_length));
        string_offset = parser->program.string_data_size;
        ft_memcpy(parser->program.string_data + string_offset,
            parser->source + string_start, string_length);
        parser->program.string_data[string_offset + string_length] = '\0';
        parser->program.string_data_size += string_length + 1U;
        parser->offset += 1U;
        return (scripting_compile_emit(parser, SCRIPTING_OP_PUSH_STRING,
            static_cast<int64_t>(string_offset), string_length));
    }
    if (scripting_is_digit(parser->source[parser->offset]) != FT_FALSE)
    {
        start = parser->offset;
        unsigned_value = 0U;
        while (parser->offset < parser->source_length
            && scripting_is_digit(parser->source[parser->offset]) != FT_FALSE)
        {
            if (unsigned_value > (static_cast<uint64_t>(INT64_MAX) - 9U) / 10U)
                return (scripting_compile_fail(parser, FT_ERR_OUT_OF_RANGE,
                    start, parser->offset - start + 1U));
            unsigned_value = unsigned_value * 10U
                + static_cast<uint64_t>(parser->source[parser->offset] - '0');
            parser->offset += 1U;
        }
        return (scripting_compile_emit(parser, SCRIPTING_OP_PUSH_INTEGER,
            static_cast<int64_t>(unsigned_value), 0U));
    }
    if (scripting_is_identifier_start(parser->source[parser->offset])
        == FT_FALSE)
        return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
            parser->offset, 1U));
    start = parser->offset;
    while (parser->offset < parser->source_length
        && scripting_is_identifier_part(parser->source[parser->offset])
            != FT_FALSE)
        parser->offset += 1U;
    name_length = parser->offset - start;
    if (scripting_equal_identifier(parser->source, start, name_length, "null")
        != FT_FALSE)
        return (scripting_compile_emit(parser, SCRIPTING_OP_PUSH_NULL, 0, 0U));
    scripting_compile_skip_space(parser);
    if (parser->offset >= parser->source_length
        || parser->source[parser->offset] != '(')
    {
        if (scripting_compile_find_local(parser, parser->source + start,
            name_length, &local_id) != FT_ERR_SUCCESS)
            return (scripting_compile_fail(parser, FT_ERR_NOT_FOUND, start,
                name_length));
        return (scripting_compile_emit(parser, SCRIPTING_OP_LOAD_LOCAL,
            static_cast<int64_t>(local_id), 0U));
    }
    if (parser->engine->find_native(parser->source + start, name_length,
        &native_id) != FT_ERR_SUCCESS)
        return (scripting_compile_fail(parser, FT_ERR_NOT_FOUND, start,
            name_length));
    parser->offset += 1U;
    argument_count = 0U;
    scripting_compile_skip_space(parser);
    if (parser->offset < parser->source_length
        && parser->source[parser->offset] != ')')
    {
        while (1)
        {
            if (argument_count >= FT_SCRIPTING_MAX_ARGUMENTS)
                return (scripting_compile_fail(parser, FT_ERR_FULL,
                    parser->offset, 1U));
            parse_error = scripting_compile_expression(parser);
            if (parse_error != FT_ERR_SUCCESS)
                return (parse_error);
            argument_count += 1U;
            scripting_compile_skip_space(parser);
            if (parser->offset >= parser->source_length
                || parser->source[parser->offset] != ',')
                break ;
            parser->offset += 1U;
        }
    }
    if (parser->offset >= parser->source_length
        || parser->source[parser->offset] != ')')
        return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
            parser->offset, 1U));
    parser->offset += 1U;
    return (scripting_compile_emit(parser, SCRIPTING_OP_CALL_NATIVE,
        static_cast<int64_t>(native_id), argument_count));
}

static int32_t scripting_compile_unary(scripting_compile_parser *parser)
    noexcept
{
    ft_bool negative;
    int32_t parse_error;

    scripting_compile_skip_space(parser);
    negative = FT_FALSE;
    if (parser->offset < parser->source_length
        && parser->source[parser->offset] == '-')
    {
        negative = FT_TRUE;
        parser->offset += 1U;
    }
    parse_error = scripting_compile_primary(parser);
    if (parse_error != FT_ERR_SUCCESS)
        return (parse_error);
    if (negative != FT_FALSE)
        return (scripting_compile_emit(parser, SCRIPTING_OP_NEGATE, 0, 0U));
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_compile_expression(scripting_compile_parser *parser)
    noexcept
{
    char operation;
    int32_t parse_error;

    parse_error = scripting_compile_term(parser);
    if (parse_error != FT_ERR_SUCCESS)
        return (parse_error);
    while (1)
    {
        scripting_compile_skip_space(parser);
        if (parser->offset >= parser->source_length)
            break ;
        operation = parser->source[parser->offset];
        if (operation != '+' && operation != '-')
            break ;
        parser->offset += 1U;
        parse_error = scripting_compile_term(parser);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
        if (operation == '+')
            parse_error = scripting_compile_emit(parser, SCRIPTING_OP_ADD, 0,
                0U);
        else
            parse_error = scripting_compile_emit(parser,
                SCRIPTING_OP_SUBTRACT, 0, 0U);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_compile_term(scripting_compile_parser *parser)
    noexcept
{
    char operation;
    int32_t parse_error;

    parse_error = scripting_compile_unary(parser);
    if (parse_error != FT_ERR_SUCCESS)
        return (parse_error);
    while (1)
    {
        scripting_compile_skip_space(parser);
        if (parser->offset >= parser->source_length)
            break ;
        operation = parser->source[parser->offset];
        if (operation != '*' && operation != '/')
            break ;
        parser->offset += 1U;
        parse_error = scripting_compile_unary(parser);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
        if (operation == '*')
            parse_error = scripting_compile_emit(parser,
                SCRIPTING_OP_MULTIPLY, 0, 0U);
        else
            parse_error = scripting_compile_emit(parser,
                SCRIPTING_OP_DIVIDE, 0, 0U);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_subtract_checked(int64_t left_value,
    int64_t right_value, int64_t *result) noexcept
{
    if ((right_value < 0 && left_value > INT64_MAX + right_value)
        || (right_value > 0 && left_value < INT64_MIN + right_value))
        return (FT_ERR_OUT_OF_RANGE);
    *result = left_value - right_value;
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_multiply_checked(int64_t left_value,
    int64_t right_value, int64_t *result) noexcept
{
    if (left_value == 0 || right_value == 0)
    {
        *result = 0;
        return (FT_ERR_SUCCESS);
    }
    if ((left_value == -1 && right_value == INT64_MIN)
        || (right_value == -1 && left_value == INT64_MIN))
        return (FT_ERR_OUT_OF_RANGE);
    if (left_value > 0 && right_value > 0
        && left_value > INT64_MAX / right_value)
        return (FT_ERR_OUT_OF_RANGE);
    if (left_value > 0 && right_value < 0
        && right_value < INT64_MIN / left_value)
        return (FT_ERR_OUT_OF_RANGE);
    if (left_value < 0 && right_value > 0
        && left_value < INT64_MIN / right_value)
        return (FT_ERR_OUT_OF_RANGE);
    if (left_value < 0 && right_value < 0
        && left_value < INT64_MAX / right_value)
        return (FT_ERR_OUT_OF_RANGE);
    *result = left_value * right_value;
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_parse_primary(scripting_parser *parser,
    scripting_value *result) noexcept
{
    uint32_t start;
    uint64_t unsigned_value;
    uint32_t native_id;
    uint32_t native_name_length;
    uint32_t argument_count;
    scripting_value arguments[FT_SCRIPTING_MAX_ARGUMENTS];

    scripting_skip_space(parser);
    if (parser->offset >= parser->source_length)
        return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT, parser->offset, 1U));
    if (parser->source[parser->offset] == '(')
    {
        parser->offset += 1U;
        if (scripting_parse_expression(parser, result) != FT_ERR_SUCCESS)
            return (parser->diagnostic.error_code);
        scripting_skip_space(parser);
        if (parser->offset >= parser->source_length
            || parser->source[parser->offset] != ')')
            return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT,
                parser->offset, 1U));
        parser->offset += 1U;
        return (FT_ERR_SUCCESS);
    }
    if (scripting_is_digit(parser->source[parser->offset]) != FT_FALSE)
    {
        start = parser->offset;
        unsigned_value = 0U;
        while (parser->offset < parser->source_length
            && scripting_is_digit(parser->source[parser->offset]) != FT_FALSE)
        {
        if (unsigned_value > (static_cast<uint64_t>(INT64_MAX) - 9U) / 10U)
                return (scripting_fail(parser, FT_ERR_OUT_OF_RANGE, start,
                    parser->offset - start + 1U));
            unsigned_value = unsigned_value * 10U
                + static_cast<uint64_t>(parser->source[parser->offset] - '0');
            parser->offset += 1U;
        }
        return (scripting_value_set_integer(result,
            static_cast<int64_t>(unsigned_value)));
    }
    if (parser->source[parser->offset] == '"')
    {
        uint32_t string_start;

        parser->offset += 1U;
        string_start = parser->offset;
        while (parser->offset < parser->source_length
            && parser->source[parser->offset] != '"')
        {
            if (parser->source[parser->offset] == '\\'
                || parser->source[parser->offset] < 0x20)
                return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT,
                    parser->offset, 1U));
            parser->offset += 1U;
        }
        if (parser->offset >= parser->source_length)
            return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT,
                string_start, parser->offset - string_start));
        if (scripting_value_set_string(result,
            parser->source + string_start, parser->offset - string_start)
                != FT_ERR_SUCCESS)
            return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT,
                string_start, parser->offset - string_start));
        parser->offset += 1U;
        return (FT_ERR_SUCCESS);
    }
    if (scripting_is_identifier_start(parser->source[parser->offset]) == FT_FALSE)
        return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT, parser->offset, 1U));
    start = parser->offset;
    while (parser->offset < parser->source_length
        && scripting_is_identifier_part(parser->source[parser->offset]) != FT_FALSE)
        parser->offset += 1U;
    native_name_length = parser->offset - start;
    if (scripting_equal_identifier(parser->source, start, native_name_length,
        "null") != FT_FALSE)
    {
        return (scripting_value_set_null(result));
    }
    scripting_skip_space(parser);
    if (parser->offset >= parser->source_length
        || parser->source[parser->offset] != '(')
    {
        if (scripting_find_local(parser, parser->source + start,
            native_name_length, result) != FT_ERR_SUCCESS)
            return (scripting_fail(parser, FT_ERR_NOT_FOUND, start,
                native_name_length));
        return (FT_ERR_SUCCESS);
    }
    if (parser->engine->find_native(parser->source + start,
        native_name_length, &native_id) != FT_ERR_SUCCESS)
        return (scripting_fail(parser, FT_ERR_NOT_FOUND, start,
            native_name_length));
    parser->offset += 1U;
    argument_count = 0U;
    scripting_skip_space(parser);
    if (parser->offset < parser->source_length
        && parser->source[parser->offset] != ')')
    {
        while (1)
        {
            if (argument_count >= FT_SCRIPTING_MAX_ARGUMENTS)
                return (scripting_fail(parser, FT_ERR_FULL, parser->offset, 1U));
            if (scripting_parse_expression(parser,
                &arguments[argument_count]) != FT_ERR_SUCCESS)
                return (parser->diagnostic.error_code);
            argument_count += 1U;
            scripting_skip_space(parser);
            if (parser->offset >= parser->source_length
                || parser->source[parser->offset] != ',')
                break ;
            parser->offset += 1U;
        }
    }
    if (parser->offset >= parser->source_length
        || parser->source[parser->offset] != ')')
        return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT, parser->offset, 1U));
    parser->offset += 1U;
    parser->operation_count += 1U;
    if (parser->operation_count > parser->engine->get_operation_limit())
        return (scripting_fail(parser, FT_ERR_FULL, start,
            parser->offset - start));
    int32_t callback_error;

    callback_error = parser->engine->invoke_native_id(native_id, arguments,
        argument_count, result,
        parser->operation_count);
    if (callback_error != FT_ERR_SUCCESS)
        return (scripting_fail(parser, callback_error, start,
            parser->offset - start));
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_parse_unary(scripting_parser *parser,
    scripting_value *result) noexcept
{
    ft_bool negative;

    scripting_skip_space(parser);
    negative = FT_FALSE;
    if (parser->offset < parser->source_length
        && parser->source[parser->offset] == '-')
    {
        negative = FT_TRUE;
        parser->offset += 1U;
    }
    if (scripting_parse_primary(parser, result) != FT_ERR_SUCCESS)
        return (parser->diagnostic.error_code);
    if (negative != FT_FALSE)
    {
        if (result->type != SCRIPTING_VALUE_INTEGER
            || result->integer_value == INT64_MIN)
            return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT,
                parser->offset, 1U));
        result->integer_value = -result->integer_value;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_parse_expression(scripting_parser *parser,
    scripting_value *result) noexcept
{
    scripting_value right;
    char operation;

    if (scripting_parse_term(parser, result) != FT_ERR_SUCCESS)
        return (parser->diagnostic.error_code);
    while (1)
    {
        scripting_skip_space(parser);
        if (parser->offset >= parser->source_length)
            break ;
        operation = parser->source[parser->offset];
        if (operation != '+' && operation != '-' && operation != '*'
            && operation != '/')
            break ;
        if (operation == '*' || operation == '/')
            break ;
        parser->offset += 1U;
        if (scripting_parse_term(parser, &right) != FT_ERR_SUCCESS)
            return (parser->diagnostic.error_code);
        if (result->type != SCRIPTING_VALUE_INTEGER
            || right.type != SCRIPTING_VALUE_INTEGER)
            return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT,
                parser->offset, 1U));
        if (operation == '+')
        {
            if (scripting_add_checked(result->integer_value,
                right.integer_value, &result->integer_value) != FT_ERR_SUCCESS)
                return (scripting_fail(parser, FT_ERR_OUT_OF_RANGE,
                    parser->offset, 1U));
        }
        else
        {
            if (scripting_subtract_checked(result->integer_value,
                right.integer_value, &result->integer_value) != FT_ERR_SUCCESS)
                return (scripting_fail(parser, FT_ERR_OUT_OF_RANGE,
                    parser->offset, 1U));
        }
    }
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_parse_term(scripting_parser *parser,
    scripting_value *result) noexcept
{
    scripting_value right;
    char operation;

    if (scripting_parse_unary(parser, result) != FT_ERR_SUCCESS)
        return (parser->diagnostic.error_code);
    while (1)
    {
        scripting_skip_space(parser);
        if (parser->offset >= parser->source_length)
            break ;
        operation = parser->source[parser->offset];
        if (operation != '*' && operation != '/')
            break ;
        parser->offset += 1U;
        if (scripting_parse_unary(parser, &right) != FT_ERR_SUCCESS)
            return (parser->diagnostic.error_code);
        if (result->type != SCRIPTING_VALUE_INTEGER
            || right.type != SCRIPTING_VALUE_INTEGER)
            return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT,
                parser->offset, 1U));
        if (operation == '*')
        {
            if (scripting_multiply_checked(result->integer_value,
                right.integer_value, &result->integer_value) != FT_ERR_SUCCESS)
                return (scripting_fail(parser, FT_ERR_OUT_OF_RANGE,
                    parser->offset, 1U));
        }
        else
        {
            if (right.integer_value == 0 || (result->integer_value == INT64_MIN
                && right.integer_value == -1))
                return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT,
                    parser->offset, 1U));
            result->integer_value /= right.integer_value;
        }
    }
    return (FT_ERR_SUCCESS);
}

scripting_engine::scripting_engine() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _natives(),
      _native_count(0U), _operation_limit(FT_SCRIPTING_MAX_OPERATIONS),
      _last_diagnostic()
{
    return ;
}

scripting_engine::~scripting_engine() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t scripting_engine::initialize() noexcept
{
    uint32_t index;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    index = 0U;
    while (index < FT_SCRIPTING_MAX_NATIVES)
    {
        this->_natives[index].name = ft_nullptr;
        this->_natives[index].callback = ft_nullptr;
        this->_natives[index].user_data = ft_nullptr;
        this->_natives[index].registered = FT_FALSE;
        index += 1U;
    }
    this->_native_count = 0U;
    this->_operation_limit = FT_SCRIPTING_MAX_OPERATIONS;
    this->_last_diagnostic.error_code = FT_ERR_SUCCESS;
    this->_last_diagnostic.source_offset = 0U;
    this->_last_diagnostic.source_length = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t scripting_engine::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->_native_count = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t scripting_engine::move(scripting_engine &other) noexcept
{
    uint32_t index;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    index = 0U;
    while (index < FT_SCRIPTING_MAX_NATIVES)
    {
        this->_natives[index] = other._natives[index];
        other._natives[index].registered = FT_FALSE;
        index += 1U;
    }
    this->_native_count = other._native_count;
    this->_operation_limit = other._operation_limit;
    this->_last_diagnostic = other._last_diagnostic;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t scripting_engine::find_native(const char *name, uint32_t name_length,
    uint32_t *native_id) const noexcept
{
    uint32_t index;
    uint32_t registered_name_length;

    if (name == ft_nullptr || native_id == ft_nullptr || name_length == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < this->_native_count)
    {
        registered_name_length = 0U;
        while (this->_natives[index].name[registered_name_length] != '\0')
            registered_name_length += 1U;
        if (registered_name_length == name_length)
        {
            uint32_t character_index = 0U;
            while (character_index < registered_name_length
                && this->_natives[index].name[character_index]
                    == name[character_index])
                character_index += 1U;
            if (character_index == registered_name_length)
            {
                *native_id = index;
                return (FT_ERR_SUCCESS);
            }
        }
        index += 1U;
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t scripting_engine::get_native_name(uint32_t native_id,
    const char **name) const noexcept
{
    if (name == ft_nullptr || native_id >= this->_native_count
        || this->_natives[native_id].registered == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    *name = this->_natives[native_id].name;
    return (FT_ERR_SUCCESS);
}

uint32_t scripting_engine::get_operation_limit() const noexcept
{
    return (this->_operation_limit);
}

int32_t scripting_engine::register_native(const char *name,
    scripting_native_callback callback, void *user_data,
    uint32_t *native_id) noexcept
{
    uint32_t existing_id;
    uint32_t name_length;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (name == ft_nullptr || name[0] == '\0' || callback == ft_nullptr
        || native_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    name_length = 0U;
    while (name[name_length] != '\0')
        name_length += 1U;
    if (this->find_native(name, name_length, &existing_id) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    if (this->_native_count >= FT_SCRIPTING_MAX_NATIVES)
        return (FT_ERR_FULL);
    this->_natives[this->_native_count].name = name;
    this->_natives[this->_native_count].callback = callback;
    this->_natives[this->_native_count].user_data = user_data;
    this->_natives[this->_native_count].registered = FT_TRUE;
    *native_id = this->_native_count;
    this->_native_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t scripting_engine::set_operation_limit(uint32_t operation_limit) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (operation_limit == 0U || operation_limit > FT_SCRIPTING_MAX_OPERATIONS)
        return (FT_ERR_OUT_OF_RANGE);
    this->_operation_limit = operation_limit;
    return (FT_ERR_SUCCESS);
}

int32_t scripting_engine::invoke_native_id(uint32_t native_id,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, uint32_t operation_count) noexcept
{
    scripting_call_context context;

    if (arguments == ft_nullptr || result == ft_nullptr
        || argument_count > FT_SCRIPTING_MAX_ARGUMENTS
        || native_id >= this->_native_count
        || this->_natives[native_id].registered == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    context.engine = this;
    context.native_id = native_id;
    context.operation_count = operation_count;
    return (this->_natives[native_id].callback(&context, arguments,
        argument_count, result, this->_natives[native_id].user_data));
}

int32_t scripting_engine::execute(const char *source,
    scripting_value *result) noexcept
{
    scripting_parser parser;
    int32_t parse_error;
    uint32_t declaration_start;
    uint32_t declaration_name_length;
    scripting_value declaration_value;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (source == ft_nullptr || result == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    parser.engine = this;
    parser.source = source;
    parser.source_length = 0U;
    while (source[parser.source_length] != '\0')
    {
        if (parser.source_length >= FT_SCRIPTING_MAX_SOURCE_BYTES)
            return (FT_ERR_OUT_OF_RANGE);
        parser.source_length += 1U;
    }
    parser.offset = 0U;
    parser.operation_count = 0U;
    parser.local_count = 0U;
    parser.diagnostic.error_code = FT_ERR_SUCCESS;
    parser.diagnostic.source_offset = 0U;
    parser.diagnostic.source_length = 0U;
    scripting_skip_space(&parser);
    while (parser.source_length - parser.offset >= 3U
        && scripting_equal_identifier(parser.source, parser.offset, 3U,
            "let") != FT_FALSE)
    {
        parser.offset += 3U;
        scripting_skip_space(&parser);
        declaration_start = parser.offset;
        while (parser.offset < parser.source_length
            && scripting_is_identifier_part(parser.source[parser.offset])
                != FT_FALSE)
            parser.offset += 1U;
        declaration_name_length = parser.offset - declaration_start;
        if (declaration_name_length == 0U
            || parser.local_count >= FT_SCRIPTING_MAX_LOCALS)
        {
            parse_error = scripting_fail(&parser, FT_ERR_FULL,
                declaration_start, 1U);
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
        if (scripting_find_local(&parser, parser.source + declaration_start,
            declaration_name_length, &declaration_value) == FT_ERR_SUCCESS)
        {
            parse_error = scripting_fail(&parser, FT_ERR_ALREADY_EXISTS,
                declaration_start, declaration_name_length);
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
        scripting_skip_space(&parser);
        if (parser.offset >= parser.source_length
            || parser.source[parser.offset] != '=')
        {
            parse_error = scripting_fail(&parser, FT_ERR_INVALID_ARGUMENT,
                parser.offset, 1U);
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
        parser.offset += 1U;
        if (scripting_parse_expression(&parser, &declaration_value)
            != FT_ERR_SUCCESS)
        {
            this->_last_diagnostic = parser.diagnostic;
            return (parser.diagnostic.error_code);
        }
        scripting_skip_space(&parser);
        if (parser.offset >= parser.source_length
            || parser.source[parser.offset] != ';')
        {
            parse_error = scripting_fail(&parser, FT_ERR_INVALID_ARGUMENT,
                parser.offset, 1U);
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
        parser.locals[parser.local_count].name = parser.source
            + declaration_start;
        parser.locals[parser.local_count].name_length = declaration_name_length;
        parser.locals[parser.local_count].value = declaration_value;
        parser.local_count += 1U;
        parser.offset += 1U;
        scripting_skip_space(&parser);
    }
    if (scripting_keyword_at(&parser, "return", 6U) != FT_FALSE)
        parser.offset += 6U;
    parse_error = scripting_parse_expression(&parser, result);
    scripting_skip_space(&parser);
    if (parse_error == FT_ERR_SUCCESS && parser.offset < parser.source_length
        && parser.source[parser.offset] == ';')
        parser.offset += 1U;
    scripting_skip_space(&parser);
    if (parse_error == FT_ERR_SUCCESS && parser.offset != parser.source_length)
        parse_error = scripting_fail(&parser, FT_ERR_INVALID_ARGUMENT,
            parser.offset, parser.source_length - parser.offset);
    this->_last_diagnostic = parser.diagnostic;
    return (parse_error);
}

int32_t scripting_engine::compile(const char *source,
    scripting_program *program) noexcept
{
    scripting_compile_parser parser;
    scripting_program compiled_program;
    uint32_t declaration_start;
    uint32_t declaration_name_length;
    uint32_t local_id;
    int32_t parse_error;
    ft_bool explicit_return;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (source == ft_nullptr || program == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    compiled_program = {};
    compiled_program.format_version = FT_SCRIPTING_BYTECODE_VERSION;
    parser.engine = this;
    parser.source = source;
    parser.source_length = 0U;
    while (source[parser.source_length] != '\0')
    {
        if (parser.source_length >= FT_SCRIPTING_MAX_SOURCE_BYTES)
            return (FT_ERR_OUT_OF_RANGE);
        parser.source_length += 1U;
    }
    parser.offset = 0U;
    parser.diagnostic.error_code = FT_ERR_SUCCESS;
    parser.diagnostic.source_offset = 0U;
    parser.diagnostic.source_length = 0U;
    parser.program = compiled_program;
    parser.local_count = 0U;
    scripting_compile_skip_space(&parser);
    while (scripting_compile_keyword_at(&parser, "let", 3U) != FT_FALSE)
    {
        parser.offset += 3U;
        scripting_compile_skip_space(&parser);
        declaration_start = parser.offset;
        while (parser.offset < parser.source_length
            && scripting_is_identifier_part(parser.source[parser.offset])
                != FT_FALSE)
            parser.offset += 1U;
        declaration_name_length = parser.offset - declaration_start;
        if (declaration_name_length == 0U
            || parser.local_count >= FT_SCRIPTING_MAX_LOCALS)
        {
            parse_error = scripting_compile_fail(&parser, FT_ERR_FULL,
                declaration_start, 1U);
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
        if (scripting_compile_find_local(&parser,
            parser.source + declaration_start, declaration_name_length,
            &local_id) == FT_ERR_SUCCESS)
        {
            parse_error = scripting_compile_fail(&parser,
                FT_ERR_ALREADY_EXISTS, declaration_start,
                declaration_name_length);
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
        scripting_compile_skip_space(&parser);
        if (parser.offset >= parser.source_length
            || parser.source[parser.offset] != '=')
        {
            parse_error = scripting_compile_fail(&parser,
                FT_ERR_INVALID_ARGUMENT, parser.offset, 1U);
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
        parser.offset += 1U;
        parse_error = scripting_compile_expression(&parser);
        if (parse_error != FT_ERR_SUCCESS)
        {
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
        parse_error = scripting_compile_emit(&parser, SCRIPTING_OP_STORE_LOCAL,
            static_cast<int64_t>(parser.local_count), 0U);
        if (parse_error != FT_ERR_SUCCESS)
        {
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
        parser.locals[parser.local_count].name = parser.source
            + declaration_start;
        parser.locals[parser.local_count].name_length = declaration_name_length;
        parser.local_count += 1U;
        scripting_compile_skip_space(&parser);
        if (parser.offset >= parser.source_length
            || parser.source[parser.offset] != ';')
        {
            parse_error = scripting_compile_fail(&parser,
                FT_ERR_INVALID_ARGUMENT, parser.offset, 1U);
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
        parser.offset += 1U;
        scripting_compile_skip_space(&parser);
    }
    explicit_return = FT_FALSE;
    if (scripting_compile_keyword_at(&parser, "return", 6U) != FT_FALSE)
    {
        parser.offset += 6U;
        explicit_return = FT_TRUE;
    }
    parse_error = scripting_compile_expression(&parser);
    if (parse_error != FT_ERR_SUCCESS)
    {
        this->_last_diagnostic = parser.diagnostic;
        return (parse_error);
    }
    scripting_compile_skip_space(&parser);
    if (explicit_return != FT_FALSE)
    {
        if (parser.offset < parser.source_length
            && parser.source[parser.offset] == ';')
            parser.offset += 1U;
        scripting_compile_skip_space(&parser);
        if (parser.offset != parser.source_length)
        {
            parse_error = scripting_compile_fail(&parser,
                FT_ERR_INVALID_ARGUMENT, parser.offset,
                parser.source_length - parser.offset);
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
    }
    else
    {
        while (parser.offset < parser.source_length
            && parser.source[parser.offset] == ';')
        {
            parse_error = scripting_compile_emit(&parser,
                SCRIPTING_OP_POP, 0, 0U);
            if (parse_error != FT_ERR_SUCCESS)
            {
                this->_last_diagnostic = parser.diagnostic;
                return (parse_error);
            }
            parser.offset += 1U;
            scripting_compile_skip_space(&parser);
            if (parser.offset >= parser.source_length)
            {
                parse_error = scripting_compile_emit(&parser,
                    SCRIPTING_OP_PUSH_NULL, 0, 0U);
                if (parse_error != FT_ERR_SUCCESS)
                {
                    this->_last_diagnostic = parser.diagnostic;
                    return (parse_error);
                }
                break ;
            }
            parse_error = scripting_compile_expression(&parser);
            if (parse_error != FT_ERR_SUCCESS)
            {
                this->_last_diagnostic = parser.diagnostic;
                return (parse_error);
            }
            scripting_compile_skip_space(&parser);
        }
        if (parser.offset != parser.source_length)
        {
            parse_error = scripting_compile_fail(&parser,
                FT_ERR_INVALID_ARGUMENT, parser.offset,
                parser.source_length - parser.offset);
            this->_last_diagnostic = parser.diagnostic;
            return (parse_error);
        }
    }
    parse_error = scripting_compile_emit(&parser, SCRIPTING_OP_RETURN, 0, 0U);
    if (parse_error != FT_ERR_SUCCESS)
    {
        this->_last_diagnostic = parser.diagnostic;
        return (parse_error);
    }
    *program = parser.program;
    this->_last_diagnostic = parser.diagnostic;
    return (FT_ERR_SUCCESS);
}

int32_t scripting_engine::verify_program(
    const scripting_program &program) const noexcept
{
    uint32_t instruction_index;
    uint32_t stack_depth;
    const scripting_instruction *instruction;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (program.format_version != FT_SCRIPTING_BYTECODE_VERSION
        || program.instruction_count == 0U
        || program.instruction_count > FT_SCRIPTING_MAX_INSTRUCTIONS
        || program.string_data_size > FT_SCRIPTING_MAX_STRING_BYTES)
        return (FT_ERR_INVALID_ARGUMENT);
    stack_depth = 0U;
    instruction_index = 0U;
    while (instruction_index < program.instruction_count)
    {
        instruction = &program.instructions[instruction_index];
        if (instruction->opcode == SCRIPTING_OP_PUSH_NULL
            || instruction->opcode == SCRIPTING_OP_PUSH_INTEGER
            || instruction->opcode == SCRIPTING_OP_LOAD_LOCAL)
        {
            if (instruction->opcode == SCRIPTING_OP_LOAD_LOCAL
                && (instruction->operand < 0
                    || instruction->operand >= FT_SCRIPTING_MAX_LOCALS))
                return (FT_ERR_INVALID_ARGUMENT);
            stack_depth += 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_PUSH_STRING)
        {
            if (instruction->operand < 0
                || static_cast<uint64_t>(instruction->operand)
                    > program.string_data_size
                || instruction->auxiliary > program.string_data_size
                || static_cast<uint64_t>(instruction->operand)
                    + instruction->auxiliary > program.string_data_size)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_depth += 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_STORE_LOCAL
            || instruction->opcode == SCRIPTING_OP_NEGATE)
        {
            if (instruction->opcode == SCRIPTING_OP_STORE_LOCAL
                && (instruction->operand < 0
                    || instruction->operand >= FT_SCRIPTING_MAX_LOCALS))
                return (FT_ERR_INVALID_ARGUMENT);
            if (stack_depth == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            if (instruction->opcode == SCRIPTING_OP_STORE_LOCAL)
                stack_depth -= 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_POP)
        {
            if (stack_depth == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_depth -= 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_ADD
            || instruction->opcode == SCRIPTING_OP_SUBTRACT
            || instruction->opcode == SCRIPTING_OP_MULTIPLY
            || instruction->opcode == SCRIPTING_OP_DIVIDE)
        {
            if (stack_depth < 2U)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_depth -= 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_CALL_NATIVE)
        {
            if (instruction->auxiliary > FT_SCRIPTING_MAX_ARGUMENTS
                || stack_depth < instruction->auxiliary)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_depth -= instruction->auxiliary;
            stack_depth += 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_RETURN)
        {
            if (stack_depth == 0U
                || instruction_index + 1U != program.instruction_count)
                return (FT_ERR_INVALID_ARGUMENT);
        }
        else
            return (FT_ERR_INVALID_ARGUMENT);
        if (stack_depth > FT_SCRIPTING_MAX_OPERATIONS)
            return (FT_ERR_FULL);
        instruction_index += 1U;
    }
    if (program.instructions[program.instruction_count - 1U].opcode
        != SCRIPTING_OP_RETURN)
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

int32_t scripting_engine::execute_program(const scripting_program &program,
    scripting_value *result) noexcept
{
    scripting_value stack[FT_SCRIPTING_MAX_OPERATIONS];
    scripting_value locals[FT_SCRIPTING_MAX_LOCALS];
    scripting_value arguments[FT_SCRIPTING_MAX_ARGUMENTS];
    const scripting_instruction *instruction;
    uint32_t stack_count;
    uint32_t instruction_index;
    uint32_t local_index;
    uint32_t argument_index;
    uint32_t operation_count;
    int64_t calculated_value;
    int32_t execution_error;

    if (result == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    execution_error = this->verify_program(program);
    if (execution_error != FT_ERR_SUCCESS)
        return (execution_error);
    stack_count = 0U;
    local_index = 0U;
    while (local_index < FT_SCRIPTING_MAX_LOCALS)
    {
        scripting_value_set_null(&locals[local_index]);
        local_index += 1U;
    }
    instruction_index = 0U;
    operation_count = 0U;
    while (instruction_index < program.instruction_count)
    {
        instruction = &program.instructions[instruction_index];
        operation_count += 1U;
        if (operation_count > this->_operation_limit)
            return (FT_ERR_FULL);
        if (instruction->opcode == SCRIPTING_OP_PUSH_NULL)
            scripting_value_set_null(&stack[stack_count++]);
        else if (instruction->opcode == SCRIPTING_OP_PUSH_INTEGER)
            scripting_value_set_integer(&stack[stack_count++],
                instruction->operand);
        else if (instruction->opcode == SCRIPTING_OP_PUSH_STRING)
            scripting_value_set_string(&stack[stack_count++],
                program.string_data + instruction->operand,
                instruction->auxiliary);
        else if (instruction->opcode == SCRIPTING_OP_LOAD_LOCAL)
            stack[stack_count++] = locals[instruction->operand];
        else if (instruction->opcode == SCRIPTING_OP_STORE_LOCAL)
        {
            local_index = static_cast<uint32_t>(instruction->operand);
            locals[local_index] = stack[stack_count - 1U];
            stack_count -= 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_POP)
            stack_count -= 1U;
        else if (instruction->opcode == SCRIPTING_OP_NEGATE)
        {
            if (stack[stack_count - 1U].type != SCRIPTING_VALUE_INTEGER
                || stack[stack_count - 1U].integer_value == INT64_MIN)
                return (FT_ERR_OUT_OF_RANGE);
            stack[stack_count - 1U].integer_value =
                -stack[stack_count - 1U].integer_value;
        }
        else if (instruction->opcode == SCRIPTING_OP_CALL_NATIVE)
        {
            argument_index = 0U;
            while (argument_index < instruction->auxiliary)
            {
                arguments[argument_index] = stack[stack_count
                    - instruction->auxiliary + argument_index];
                argument_index += 1U;
            }
            execution_error = this->invoke_native_id(
                static_cast<uint32_t>(instruction->operand), arguments,
                instruction->auxiliary, &stack[stack_count
                    - instruction->auxiliary], operation_count);
            if (execution_error != FT_ERR_SUCCESS)
                return (execution_error);
            stack_count -= instruction->auxiliary;
            stack_count += 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_RETURN)
        {
            *result = stack[stack_count - 1U];
            return (FT_ERR_SUCCESS);
        }
        else
        {
            if (stack[stack_count - 1U].type != SCRIPTING_VALUE_INTEGER
                || stack[stack_count - 2U].type != SCRIPTING_VALUE_INTEGER)
                return (FT_ERR_INVALID_ARGUMENT);
            if (instruction->opcode == SCRIPTING_OP_ADD)
                execution_error = scripting_add_checked(
                    stack[stack_count - 2U].integer_value,
                    stack[stack_count - 1U].integer_value, &calculated_value);
            else if (instruction->opcode == SCRIPTING_OP_SUBTRACT)
                execution_error = scripting_subtract_checked(
                    stack[stack_count - 2U].integer_value,
                    stack[stack_count - 1U].integer_value, &calculated_value);
            else if (instruction->opcode == SCRIPTING_OP_MULTIPLY)
                execution_error = scripting_multiply_checked(
                    stack[stack_count - 2U].integer_value,
                    stack[stack_count - 1U].integer_value, &calculated_value);
            else if (instruction->opcode == SCRIPTING_OP_DIVIDE)
            {
                if (stack[stack_count - 1U].integer_value == 0
                    || (stack[stack_count - 2U].integer_value == INT64_MIN
                        && stack[stack_count - 1U].integer_value == -1))
                    return (FT_ERR_INVALID_ARGUMENT);
                calculated_value = stack[stack_count - 2U].integer_value
                    / stack[stack_count - 1U].integer_value;
                execution_error = FT_ERR_SUCCESS;
            }
            else
                return (FT_ERR_INVALID_ARGUMENT);
            if (execution_error != FT_ERR_SUCCESS)
                return (execution_error);
            stack_count -= 2U;
            scripting_value_set_integer(&stack[stack_count++], calculated_value);
        }
        instruction_index += 1U;
    }
    return (FT_ERR_INVALID_STATE);
}

int32_t scripting_engine::get_last_diagnostic(
    scripting_diagnostic *diagnostic) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || diagnostic == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *diagnostic = this->_last_diagnostic;
    return (FT_ERR_SUCCESS);
}

int32_t scripting_value_set_null(scripting_value *value) noexcept
{
    if (value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    value->type = SCRIPTING_VALUE_NULL;
    value->integer_value = 0;
    value->string_value = ft_nullptr;
    value->string_length = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t scripting_value_set_integer(scripting_value *value,
    int64_t integer_value) noexcept
{
    if (value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    value->type = SCRIPTING_VALUE_INTEGER;
    value->integer_value = integer_value;
    value->string_value = ft_nullptr;
    value->string_length = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t scripting_value_set_string(scripting_value *value,
    const char *string, uint32_t length) noexcept
{
    if (value == ft_nullptr || length > FT_SCRIPTING_MAX_STRING_BYTES
        || (string == ft_nullptr && length != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    value->type = SCRIPTING_VALUE_STRING;
    value->integer_value = 0;
    value->string_value = string;
    value->string_length = length;
    return (FT_ERR_SUCCESS);
}
