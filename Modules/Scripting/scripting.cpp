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

static int32_t scripting_parse_condition(scripting_parser *parser,
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

static int32_t scripting_compile_condition(
    scripting_compile_parser *parser)
    noexcept;

static int32_t scripting_compile_comparison(
    scripting_compile_parser *parser) noexcept;

static int32_t scripting_compile_logical_and(
    scripting_compile_parser *parser) noexcept;

static int32_t scripting_compile_block(scripting_compile_parser *parser)
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
    if (parser->source[parser->offset] == '{')
        return (scripting_compile_block(parser));
    if (scripting_compile_keyword_at(parser, "while", 5U) != FT_FALSE)
    {
        uint32_t loop_start_index;
        uint32_t jump_end_index;
        int32_t loop_error;

        parser->offset += 5U;
        scripting_compile_skip_space(parser);
        if (parser->offset >= parser->source_length
            || parser->source[parser->offset] != '(')
            return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
                parser->offset, 1U));
        parser->offset += 1U;
        loop_start_index = parser->program.instruction_count;
        loop_error = scripting_compile_condition(parser);
        if (loop_error != FT_ERR_SUCCESS)
            return (loop_error);
        scripting_compile_skip_space(parser);
        if (parser->offset >= parser->source_length
            || parser->source[parser->offset] != ')')
            return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
                parser->offset, 1U));
        parser->offset += 1U;
        jump_end_index = parser->program.instruction_count;
        loop_error = scripting_compile_emit(parser,
            SCRIPTING_OP_JUMP_IF_FALSE, 0, 0U);
        if (loop_error != FT_ERR_SUCCESS)
            return (loop_error);
        loop_error = scripting_compile_condition(parser);
        if (loop_error != FT_ERR_SUCCESS)
            return (loop_error);
        loop_error = scripting_compile_emit(parser, SCRIPTING_OP_POP, 0, 0U);
        if (loop_error != FT_ERR_SUCCESS)
            return (loop_error);
        loop_error = scripting_compile_emit(parser, SCRIPTING_OP_JUMP,
            static_cast<int64_t>(loop_start_index), 0U);
        if (loop_error != FT_ERR_SUCCESS)
            return (loop_error);
        parser->program.instructions[jump_end_index].operand =
            static_cast<int64_t>(parser->program.instruction_count);
        return (scripting_compile_emit(parser, SCRIPTING_OP_PUSH_NULL, 0,
            0U));
    }
    if (scripting_compile_keyword_at(parser, "if", 2U) != FT_FALSE)
    {
        uint32_t jump_false_index;
        uint32_t jump_end_index;
        int32_t branch_error;

        parser->offset += 2U;
        scripting_compile_skip_space(parser);
        if (parser->offset >= parser->source_length
            || parser->source[parser->offset] != '(')
            return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
                parser->offset, 1U));
        parser->offset += 1U;
        branch_error = scripting_compile_condition(parser);
        if (branch_error != FT_ERR_SUCCESS)
            return (branch_error);
        scripting_compile_skip_space(parser);
        if (parser->offset >= parser->source_length
            || parser->source[parser->offset] != ')')
            return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
                parser->offset, 1U));
        parser->offset += 1U;
        jump_false_index = parser->program.instruction_count;
        branch_error = scripting_compile_emit(parser,
            SCRIPTING_OP_JUMP_IF_FALSE, 0, 0U);
        if (branch_error != FT_ERR_SUCCESS)
            return (branch_error);
        branch_error = scripting_compile_condition(parser);
        if (branch_error != FT_ERR_SUCCESS)
            return (branch_error);
        jump_end_index = parser->program.instruction_count;
        branch_error = scripting_compile_emit(parser, SCRIPTING_OP_JUMP,
            0, 0U);
        if (branch_error != FT_ERR_SUCCESS)
            return (branch_error);
        scripting_compile_skip_space(parser);
        if (scripting_compile_keyword_at(parser, "else", 4U) == FT_FALSE)
            return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
                parser->offset, 4U));
        parser->program.instructions[jump_false_index].operand =
            static_cast<int64_t>(parser->program.instruction_count);
        parser->offset += 4U;
        branch_error = scripting_compile_condition(parser);
        if (branch_error != FT_ERR_SUCCESS)
            return (branch_error);
        parser->program.instructions[jump_end_index].operand =
            static_cast<int64_t>(parser->program.instruction_count);
        return (FT_ERR_SUCCESS);
    }
    if (parser->source[parser->offset] == '(')
    {
        parser->offset += 1U;
        parse_error = scripting_compile_condition(parser);
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
    if (scripting_equal_identifier(parser->source, start, name_length, "true")
        != FT_FALSE)
        return (scripting_compile_emit(parser,
            SCRIPTING_OP_PUSH_BOOLEAN, 1, 0U));
    if (scripting_equal_identifier(parser->source, start, name_length, "false")
        != FT_FALSE)
        return (scripting_compile_emit(parser,
            SCRIPTING_OP_PUSH_BOOLEAN, 0, 0U));
    scripting_compile_skip_space(parser);
    if (parser->offset >= parser->source_length
        || parser->source[parser->offset] != '(')
    {
        if (scripting_compile_find_local(parser, parser->source + start,
            name_length, &local_id) != FT_ERR_SUCCESS)
            return (scripting_compile_fail(parser, FT_ERR_NOT_FOUND, start,
                name_length));
        if (parser->offset < parser->source_length
            && parser->source[parser->offset] == '=')
        {
            parser->offset += 1U;
            parse_error = scripting_compile_condition(parser);
            if (parse_error != FT_ERR_SUCCESS)
                return (parse_error);
            parse_error = scripting_compile_emit(parser, SCRIPTING_OP_DUP,
                0, 0U);
            if (parse_error != FT_ERR_SUCCESS)
                return (parse_error);
            return (scripting_compile_emit(parser, SCRIPTING_OP_STORE_LOCAL,
                static_cast<int64_t>(local_id), 0U));
        }
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
            parse_error = scripting_compile_condition(parser);
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
    ft_bool logical_not;
    int32_t parse_error;

    scripting_compile_skip_space(parser);
    negative = FT_FALSE;
    logical_not = FT_FALSE;
    if (parser->offset < parser->source_length
        && parser->source[parser->offset] == '!'
        && (parser->offset + 1U >= parser->source_length
            || parser->source[parser->offset + 1U] != '='))
    {
        logical_not = FT_TRUE;
        parser->offset += 1U;
    }
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
    {
        parse_error = scripting_compile_emit(parser, SCRIPTING_OP_NEGATE,
            0, 0U);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
    }
    if (logical_not != FT_FALSE)
        return (scripting_compile_emit(parser, SCRIPTING_OP_LOGICAL_NOT,
            0, 0U));
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

static int32_t scripting_compile_comparison(
    scripting_compile_parser *parser)
    noexcept
{
    scripting_opcode comparison_opcode;
    int32_t parse_error;

    parse_error = scripting_compile_expression(parser);
    if (parse_error != FT_ERR_SUCCESS)
        return (parse_error);
    scripting_compile_skip_space(parser);
    comparison_opcode = SCRIPTING_OP_EQUAL;
    if (parser->source_length - parser->offset >= 2U
        && parser->source[parser->offset] == '='
        && parser->source[parser->offset + 1U] == '=')
    {
        comparison_opcode = SCRIPTING_OP_EQUAL;
        parser->offset += 2U;
    }
    else if (parser->source_length - parser->offset >= 2U
        && parser->source[parser->offset] == '!'
        && parser->source[parser->offset + 1U] == '=')
    {
        comparison_opcode = SCRIPTING_OP_NOT_EQUAL;
        parser->offset += 2U;
    }
    else if (parser->source_length - parser->offset >= 2U
        && parser->source[parser->offset] == '<'
        && parser->source[parser->offset + 1U] == '=')
    {
        comparison_opcode = SCRIPTING_OP_LESS_EQUAL;
        parser->offset += 2U;
    }
    else if (parser->source_length - parser->offset >= 2U
        && parser->source[parser->offset] == '>'
        && parser->source[parser->offset + 1U] == '=')
    {
        comparison_opcode = SCRIPTING_OP_GREATER_EQUAL;
        parser->offset += 2U;
    }
    else if (parser->offset < parser->source_length
        && parser->source[parser->offset] == '<')
    {
        comparison_opcode = SCRIPTING_OP_LESS_THAN;
        parser->offset += 1U;
    }
    else if (parser->offset < parser->source_length
        && parser->source[parser->offset] == '>')
    {
        comparison_opcode = SCRIPTING_OP_GREATER_THAN;
        parser->offset += 1U;
    }
    else
        return (FT_ERR_SUCCESS);
    parse_error = scripting_compile_expression(parser);
    if (parse_error != FT_ERR_SUCCESS)
        return (parse_error);
    return (scripting_compile_emit(parser, comparison_opcode, 0, 0U));
}

static int32_t scripting_compile_logical_and(
    scripting_compile_parser *parser) noexcept
{
    int32_t parse_error;

    parse_error = scripting_compile_comparison(parser);
    if (parse_error != FT_ERR_SUCCESS)
        return (parse_error);
    while (1)
    {
        scripting_compile_skip_space(parser);
        if (parser->source_length - parser->offset < 2U
            || parser->source[parser->offset] != '&'
            || parser->source[parser->offset + 1U] != '&')
            break ;
        parser->offset += 2U;
        parse_error = scripting_compile_comparison(parser);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
        parse_error = scripting_compile_emit(parser,
            SCRIPTING_OP_LOGICAL_AND, 0, 0U);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_compile_condition(scripting_compile_parser *parser)
    noexcept
{
    int32_t parse_error;

    parse_error = scripting_compile_logical_and(parser);
    if (parse_error != FT_ERR_SUCCESS)
        return (parse_error);
    while (1)
    {
        scripting_compile_skip_space(parser);
        if (parser->source_length - parser->offset < 2U)
            break ;
        if (parser->source[parser->offset] != '|'
            || parser->source[parser->offset + 1U] != '|')
            break ;
        parser->offset += 2U;
        parse_error = scripting_compile_logical_and(parser);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
        parse_error = scripting_compile_emit(parser, SCRIPTING_OP_LOGICAL_OR,
            0, 0U);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t scripting_compile_block(scripting_compile_parser *parser)
    noexcept
{
    int32_t parse_error;

    parser->offset += 1U;
    scripting_compile_skip_space(parser);
    if (parser->offset < parser->source_length
        && parser->source[parser->offset] == '}')
    {
        parser->offset += 1U;
        return (scripting_compile_emit(parser, SCRIPTING_OP_PUSH_NULL,
            0, 0U));
    }
    parse_error = scripting_compile_condition(parser);
    if (parse_error != FT_ERR_SUCCESS)
        return (parse_error);
    scripting_compile_skip_space(parser);
    while (parser->offset < parser->source_length
        && parser->source[parser->offset] == ';')
    {
        parser->offset += 1U;
        scripting_compile_skip_space(parser);
        if (parser->offset < parser->source_length
            && parser->source[parser->offset] == '}')
            break ;
        parse_error = scripting_compile_emit(parser, SCRIPTING_OP_POP, 0,
            0U);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
        parse_error = scripting_compile_condition(parser);
        if (parse_error != FT_ERR_SUCCESS)
            return (parse_error);
        scripting_compile_skip_space(parser);
    }
    if (parser->offset >= parser->source_length
        || parser->source[parser->offset] != '}')
        return (scripting_compile_fail(parser, FT_ERR_INVALID_ARGUMENT,
            parser->offset, 1U));
    parser->offset += 1U;
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

static int32_t scripting_compare_values(const scripting_value &left_value,
    const scripting_value &right_value, scripting_opcode opcode,
    ft_bool *result) noexcept
{
    int32_t comparison;
    uint32_t index;

    if (result == ft_nullptr || left_value.type != right_value.type)
        return (FT_ERR_INVALID_ARGUMENT);
    comparison = 0;
    if (left_value.type == SCRIPTING_VALUE_INTEGER)
    {
        if (left_value.integer_value < right_value.integer_value)
            comparison = -1;
        else if (left_value.integer_value > right_value.integer_value)
            comparison = 1;
    }
    else if (left_value.type == SCRIPTING_VALUE_BOOLEAN)
    {
        if (left_value.boolean_value < right_value.boolean_value)
            comparison = -1;
        else if (left_value.boolean_value > right_value.boolean_value)
            comparison = 1;
    }
    else if (left_value.type == SCRIPTING_VALUE_STRING)
    {
        index = 0U;
        while (index < left_value.string_length
            && index < right_value.string_length)
        {
            if (left_value.string_value[index]
                < right_value.string_value[index])
            {
                comparison = -1;
                break ;
            }
            if (left_value.string_value[index]
                > right_value.string_value[index])
            {
                comparison = 1;
                break ;
            }
            index += 1U;
        }
        if (comparison == 0 && left_value.string_length
            < right_value.string_length)
            comparison = -1;
        else if (comparison == 0 && left_value.string_length
            > right_value.string_length)
            comparison = 1;
    }
    else if (left_value.type == SCRIPTING_VALUE_NULL)
    {
        if (opcode != SCRIPTING_OP_EQUAL
            && opcode != SCRIPTING_OP_NOT_EQUAL)
            return (FT_ERR_INVALID_ARGUMENT);
    }
    else
        return (FT_ERR_INVALID_ARGUMENT);
    if (opcode == SCRIPTING_OP_EQUAL)
    {
        if (comparison == 0)
            *result = FT_TRUE;
        else
            *result = FT_FALSE;
    }
    else if (opcode == SCRIPTING_OP_NOT_EQUAL)
    {
        if (comparison != 0)
            *result = FT_TRUE;
        else
            *result = FT_FALSE;
    }
    else if (opcode == SCRIPTING_OP_LESS_THAN)
    {
        if (comparison < 0)
            *result = FT_TRUE;
        else
            *result = FT_FALSE;
    }
    else if (opcode == SCRIPTING_OP_LESS_EQUAL)
    {
        if (comparison <= 0)
            *result = FT_TRUE;
        else
            *result = FT_FALSE;
    }
    else if (opcode == SCRIPTING_OP_GREATER_THAN)
    {
        if (comparison > 0)
            *result = FT_TRUE;
        else
            *result = FT_FALSE;
    }
    else if (opcode == SCRIPTING_OP_GREATER_EQUAL)
    {
        if (comparison >= 0)
            *result = FT_TRUE;
        else
            *result = FT_FALSE;
    }
    else
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

static ft_bool scripting_value_is_true(const scripting_value &value) noexcept
{
    if (value.type == SCRIPTING_VALUE_BOOLEAN)
        return (value.boolean_value);
    if (value.type == SCRIPTING_VALUE_INTEGER)
    {
        if (value.integer_value != 0)
            return (FT_TRUE);
        return (FT_FALSE);
    }
    if (value.type == SCRIPTING_VALUE_STRING)
    {
        if (value.string_length != 0U)
            return (FT_TRUE);
    }
    return (FT_FALSE);
}

static int32_t scripting_verify_enqueue(uint32_t *stack_depths,
    uint32_t *work_queue, uint32_t *queue_tail, uint32_t instruction_index,
    uint32_t stack_depth) noexcept
{
    if (stack_depths[instruction_index] == UINT32_MAX)
    {
        stack_depths[instruction_index] = stack_depth;
        work_queue[*queue_tail] = instruction_index;
        *queue_tail += 1U;
        return (FT_ERR_SUCCESS);
    }
    if (stack_depths[instruction_index] != stack_depth)
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

static void scripting_write_u32(uint8_t *output, uint32_t value) noexcept
{
    uint32_t index;

    index = 0U;
    while (index < 4U)
    {
        output[index] = static_cast<uint8_t>(value & 0xffU);
        value >>= 8U;
        index += 1U;
    }
    return ;
}

static void scripting_write_u64(uint8_t *output, uint64_t value) noexcept
{
    uint32_t index;

    index = 0U;
    while (index < 8U)
    {
        output[index] = static_cast<uint8_t>(value & 0xffU);
        value >>= 8U;
        index += 1U;
    }
    return ;
}

static uint32_t scripting_read_u32(const uint8_t *input) noexcept
{
    uint32_t value;
    uint32_t index;

    value = 0U;
    index = 0U;
    while (index < 4U)
    {
        value |= static_cast<uint32_t>(input[index]) << (index * 8U);
        index += 1U;
    }
    return (value);
}

static uint64_t scripting_read_u64(const uint8_t *input) noexcept
{
    uint64_t value;
    uint32_t index;

    value = 0U;
    index = 0U;
    while (index < 8U)
    {
        value |= static_cast<uint64_t>(input[index]) << (index * 8U);
        index += 1U;
    }
    return (value);
}

static uint64_t scripting_hash_bytes(const uint8_t *input,
    uint32_t input_size) noexcept
{
    uint64_t hash;
    uint32_t index;

    hash = 1469598103934665603ULL;
    index = 0U;
    while (index < input_size)
    {
        hash ^= static_cast<uint64_t>(input[index]);
        hash *= 1099511628211ULL;
        index += 1U;
    }
    return (hash);
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
        if (scripting_parse_condition(parser, result) != FT_ERR_SUCCESS)
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
    if (scripting_equal_identifier(parser->source, start, native_name_length,
        "true") != FT_FALSE)
        return (scripting_value_set_boolean(result, FT_TRUE));
    if (scripting_equal_identifier(parser->source, start, native_name_length,
        "false") != FT_FALSE)
        return (scripting_value_set_boolean(result, FT_FALSE));
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
            if (scripting_parse_condition(parser,
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

static int32_t scripting_parse_condition(scripting_parser *parser,
    scripting_value *result) noexcept
{
    scripting_value right;
    scripting_opcode comparison_opcode;
    ft_bool comparison_result;

    if (scripting_parse_expression(parser, result) != FT_ERR_SUCCESS)
        return (parser->diagnostic.error_code);
    scripting_skip_space(parser);
    comparison_opcode = SCRIPTING_OP_EQUAL;
    if (parser->source_length - parser->offset >= 2U
        && parser->source[parser->offset] == '='
        && parser->source[parser->offset + 1U] == '=')
    {
        comparison_opcode = SCRIPTING_OP_EQUAL;
        parser->offset += 2U;
    }
    else if (parser->source_length - parser->offset >= 2U
        && parser->source[parser->offset] == '!'
        && parser->source[parser->offset + 1U] == '=')
    {
        comparison_opcode = SCRIPTING_OP_NOT_EQUAL;
        parser->offset += 2U;
    }
    else if (parser->source_length - parser->offset >= 2U
        && parser->source[parser->offset] == '<'
        && parser->source[parser->offset + 1U] == '=')
    {
        comparison_opcode = SCRIPTING_OP_LESS_EQUAL;
        parser->offset += 2U;
    }
    else if (parser->source_length - parser->offset >= 2U
        && parser->source[parser->offset] == '>'
        && parser->source[parser->offset + 1U] == '=')
    {
        comparison_opcode = SCRIPTING_OP_GREATER_EQUAL;
        parser->offset += 2U;
    }
    else if (parser->offset < parser->source_length
        && parser->source[parser->offset] == '<')
    {
        comparison_opcode = SCRIPTING_OP_LESS_THAN;
        parser->offset += 1U;
    }
    else if (parser->offset < parser->source_length
        && parser->source[parser->offset] == '>')
    {
        comparison_opcode = SCRIPTING_OP_GREATER_THAN;
        parser->offset += 1U;
    }
    else
        return (FT_ERR_SUCCESS);
    if (scripting_parse_expression(parser, &right) != FT_ERR_SUCCESS)
        return (parser->diagnostic.error_code);
    if (scripting_compare_values(*result, right, comparison_opcode,
        &comparison_result) != FT_ERR_SUCCESS)
        return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT,
            parser->offset, 1U));
    return (scripting_value_set_boolean(result, comparison_result));
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
        this->_natives[index].name[0] = '\0';
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
    if (name_length >= FT_SCRIPTING_MAX_NATIVE_NAME_BYTES)
        return (FT_ERR_OUT_OF_RANGE);
    if (this->find_native(name, name_length, &existing_id) == FT_ERR_SUCCESS)
        return (FT_ERR_ALREADY_EXISTS);
    if (this->_native_count >= FT_SCRIPTING_MAX_NATIVES)
        return (FT_ERR_FULL);
    ft_memcpy(this->_natives[this->_native_count].name, name, name_length);
    this->_natives[this->_native_count].name[name_length] = '\0';
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
    scripting_program program;
    int32_t compile_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (source == ft_nullptr || result == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    compile_error = this->compile(source, &program);
    if (compile_error != FT_ERR_SUCCESS)
        return (compile_error);
    return (this->execute_program(program, result));
}

int32_t scripting_engine::execute_direct(const char *source,
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
        if (scripting_parse_condition(&parser, &declaration_value)
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
    parse_error = scripting_parse_condition(&parser, result);
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
        parse_error = scripting_compile_condition(&parser);
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
    parse_error = scripting_compile_condition(&parser);
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
            parse_error = scripting_compile_condition(&parser);
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
    uint32_t stack_depths[FT_SCRIPTING_MAX_INSTRUCTIONS];
    uint32_t work_queue[FT_SCRIPTING_MAX_INSTRUCTIONS];
    uint32_t instruction_index;
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t stack_depth;
    uint32_t stack_after;
    uint32_t target_index;
    const scripting_instruction *instruction;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (program.format_version != FT_SCRIPTING_BYTECODE_VERSION
        || program.instruction_count == 0U
        || program.instruction_count > FT_SCRIPTING_MAX_INSTRUCTIONS
        || program.string_data_size > FT_SCRIPTING_MAX_STRING_BYTES)
        return (FT_ERR_INVALID_ARGUMENT);
    instruction_index = 0U;
    while (instruction_index < FT_SCRIPTING_MAX_INSTRUCTIONS)
    {
        stack_depths[instruction_index] = UINT32_MAX;
        instruction_index += 1U;
    }
    queue_head = 0U;
    queue_tail = 0U;
    if (scripting_verify_enqueue(stack_depths, work_queue, &queue_tail,
        0U, 0U) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    while (queue_head < queue_tail)
    {
        instruction_index = work_queue[queue_head];
        queue_head += 1U;
        stack_depth = stack_depths[instruction_index];
        stack_after = stack_depth;
        instruction = &program.instructions[instruction_index];
        if (instruction->opcode == SCRIPTING_OP_PUSH_NULL
            || instruction->opcode == SCRIPTING_OP_PUSH_INTEGER
            || instruction->opcode == SCRIPTING_OP_LOAD_LOCAL
            || instruction->opcode == SCRIPTING_OP_PUSH_BOOLEAN)
        {
            if (instruction->opcode == SCRIPTING_OP_PUSH_BOOLEAN
                && (instruction->operand != 0 && instruction->operand != 1))
                return (FT_ERR_INVALID_ARGUMENT);
            if (instruction->opcode == SCRIPTING_OP_LOAD_LOCAL
                && (instruction->operand < 0
                    || instruction->operand >= FT_SCRIPTING_MAX_LOCALS))
                return (FT_ERR_INVALID_ARGUMENT);
            stack_after += 1U;
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
            stack_after += 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_DUP)
        {
            if (stack_depth == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_after += 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_STORE_LOCAL
            || instruction->opcode == SCRIPTING_OP_NEGATE
            || instruction->opcode == SCRIPTING_OP_LOGICAL_NOT)
        {
            if (instruction->opcode == SCRIPTING_OP_STORE_LOCAL
                && (instruction->operand < 0
                    || instruction->operand >= FT_SCRIPTING_MAX_LOCALS))
                return (FT_ERR_INVALID_ARGUMENT);
            if (stack_depth == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            if (instruction->opcode == SCRIPTING_OP_STORE_LOCAL)
                stack_after -= 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_POP)
        {
            if (stack_depth == 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_after -= 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_ADD
            || instruction->opcode == SCRIPTING_OP_SUBTRACT
            || instruction->opcode == SCRIPTING_OP_MULTIPLY
            || instruction->opcode == SCRIPTING_OP_DIVIDE)
        {
            if (stack_depth < 2U)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_after -= 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_EQUAL
            || instruction->opcode == SCRIPTING_OP_NOT_EQUAL
            || instruction->opcode == SCRIPTING_OP_LESS_THAN
            || instruction->opcode == SCRIPTING_OP_LESS_EQUAL
            || instruction->opcode == SCRIPTING_OP_GREATER_THAN
            || instruction->opcode == SCRIPTING_OP_GREATER_EQUAL)
        {
            if (stack_depth < 2U)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_after -= 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_LOGICAL_AND
            || instruction->opcode == SCRIPTING_OP_LOGICAL_OR)
        {
            if (stack_depth < 2U)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_after -= 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_CALL_NATIVE)
        {
            if (instruction->operand < 0
                || static_cast<uint64_t>(instruction->operand)
                    >= this->_native_count
                || this->_natives[instruction->operand].registered == FT_FALSE
                || instruction->auxiliary > FT_SCRIPTING_MAX_ARGUMENTS
                || stack_depth < instruction->auxiliary)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_after -= instruction->auxiliary;
            stack_after += 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_JUMP_IF_FALSE)
        {
            if (stack_depth == 0U
                || instruction->operand < 0
                || static_cast<uint64_t>(instruction->operand)
                    >= program.instruction_count)
                return (FT_ERR_INVALID_ARGUMENT);
            stack_after -= 1U;
            target_index = static_cast<uint32_t>(instruction->operand);
            if (scripting_verify_enqueue(stack_depths, work_queue,
                &queue_tail, target_index, stack_after) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_ARGUMENT);
        }
        else if (instruction->opcode == SCRIPTING_OP_JUMP)
        {
            if (instruction->operand < 0
                || static_cast<uint64_t>(instruction->operand)
                    >= program.instruction_count)
                return (FT_ERR_INVALID_ARGUMENT);
            target_index = static_cast<uint32_t>(instruction->operand);
            if (scripting_verify_enqueue(stack_depths, work_queue,
                &queue_tail, target_index, stack_after) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_ARGUMENT);
        }
        else if (instruction->opcode == SCRIPTING_OP_RETURN)
        {
            if (stack_depth != 1U)
                return (FT_ERR_INVALID_ARGUMENT);
            continue ;
        }
        else
            return (FT_ERR_INVALID_ARGUMENT);
        if (stack_after > FT_SCRIPTING_MAX_OPERATIONS)
            return (FT_ERR_FULL);
        if (instruction->opcode != SCRIPTING_OP_JUMP)
        {
            if (instruction_index + 1U >= program.instruction_count)
                return (FT_ERR_INVALID_ARGUMENT);
            if (scripting_verify_enqueue(stack_depths, work_queue,
                &queue_tail, instruction_index + 1U,
                stack_after) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_ARGUMENT);
        }
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
    uint32_t jump_target;
    int64_t calculated_value;
    scripting_value duplicated_value;
    ft_bool comparison_result;
    ft_bool jumped;
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
        jumped = FT_FALSE;
        if (instruction->opcode == SCRIPTING_OP_PUSH_NULL)
            scripting_value_set_null(&stack[stack_count++]);
        else if (instruction->opcode == SCRIPTING_OP_PUSH_INTEGER)
            scripting_value_set_integer(&stack[stack_count++],
                instruction->operand);
        else if (instruction->opcode == SCRIPTING_OP_PUSH_STRING)
            scripting_value_set_string(&stack[stack_count++],
                program.string_data + instruction->operand,
                instruction->auxiliary);
        else if (instruction->opcode == SCRIPTING_OP_PUSH_BOOLEAN)
        {
            if (instruction->operand == 0)
                scripting_value_set_boolean(&stack[stack_count++], FT_FALSE);
            else
                scripting_value_set_boolean(&stack[stack_count++], FT_TRUE);
        }
        else if (instruction->opcode == SCRIPTING_OP_LOAD_LOCAL)
            stack[stack_count++] = locals[instruction->operand];
        else if (instruction->opcode == SCRIPTING_OP_DUP)
        {
            duplicated_value = stack[stack_count - 1U];
            stack[stack_count] = duplicated_value;
            stack_count += 1U;
        }
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
        else if (instruction->opcode == SCRIPTING_OP_JUMP_IF_FALSE)
        {
            if (scripting_value_is_true(stack[stack_count - 1U])
                == FT_FALSE)
            {
                jump_target = static_cast<uint32_t>(instruction->operand);
                instruction_index = jump_target;
                jumped = FT_TRUE;
            }
            stack_count -= 1U;
        }
        else if (instruction->opcode == SCRIPTING_OP_JUMP)
        {
            jump_target = static_cast<uint32_t>(instruction->operand);
            instruction_index = jump_target;
            jumped = FT_TRUE;
        }
        else if (instruction->opcode == SCRIPTING_OP_LOGICAL_NOT)
        {
            if (scripting_value_is_true(stack[stack_count - 1U])
                == FT_FALSE)
                scripting_value_set_boolean(&stack[stack_count - 1U], FT_TRUE);
            else
                scripting_value_set_boolean(&stack[stack_count - 1U], FT_FALSE);
        }
        else if (instruction->opcode == SCRIPTING_OP_RETURN)
        {
            *result = stack[stack_count - 1U];
            return (FT_ERR_SUCCESS);
        }
        else if (instruction->opcode == SCRIPTING_OP_EQUAL
            || instruction->opcode == SCRIPTING_OP_NOT_EQUAL
            || instruction->opcode == SCRIPTING_OP_LESS_THAN
            || instruction->opcode == SCRIPTING_OP_LESS_EQUAL
            || instruction->opcode == SCRIPTING_OP_GREATER_THAN
            || instruction->opcode == SCRIPTING_OP_GREATER_EQUAL)
        {
            execution_error = scripting_compare_values(
                stack[stack_count - 2U], stack[stack_count - 1U],
                instruction->opcode, &comparison_result);
            if (execution_error != FT_ERR_SUCCESS)
                return (execution_error);
            stack_count -= 2U;
            scripting_value_set_boolean(&stack[stack_count++],
                comparison_result);
        }
        else if (instruction->opcode == SCRIPTING_OP_LOGICAL_AND
            || instruction->opcode == SCRIPTING_OP_LOGICAL_OR)
        {
            if (instruction->opcode == SCRIPTING_OP_LOGICAL_AND)
            {
                if (scripting_value_is_true(stack[stack_count - 2U])
                    != FT_FALSE
                    && scripting_value_is_true(stack[stack_count - 1U])
                        != FT_FALSE)
                    comparison_result = FT_TRUE;
                else
                    comparison_result = FT_FALSE;
            }
            else
            {
                if (scripting_value_is_true(stack[stack_count - 2U])
                    != FT_FALSE
                    || scripting_value_is_true(stack[stack_count - 1U])
                        != FT_FALSE)
                    comparison_result = FT_TRUE;
                else
                    comparison_result = FT_FALSE;
            }
            stack_count -= 2U;
            scripting_value_set_boolean(&stack[stack_count++],
                comparison_result);
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
        if (jumped == FT_FALSE)
            instruction_index += 1U;
    }
    return (FT_ERR_INVALID_STATE);
}

int32_t scripting_engine::serialize_program(const scripting_program &program,
    uint8_t *output, uint32_t output_capacity,
    uint32_t *output_size) const noexcept
{
    uint32_t required_size;
    uint32_t offset;
    uint32_t instruction_index;
    uint64_t checksum;
    const scripting_instruction *instruction;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (output == ft_nullptr || output_size == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->verify_program(program) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    required_size = FT_SCRIPTING_SERIALIZED_HEADER_BYTES
        + program.string_data_size
        + program.instruction_count * FT_SCRIPTING_SERIALIZED_INSTRUCTION_BYTES;
    if (output_capacity < required_size)
        return (FT_ERR_FULL);
    scripting_write_u32(output, FT_SCRIPTING_BYTECODE_MAGIC);
    scripting_write_u32(output + 4U, program.format_version);
    scripting_write_u32(output + 8U, program.instruction_count);
    scripting_write_u32(output + 12U, program.string_data_size);
    offset = FT_SCRIPTING_SERIALIZED_HEADER_BYTES;
    if (program.string_data_size != 0U)
    {
        ft_memcpy(output + offset, program.string_data,
            program.string_data_size);
        offset += program.string_data_size;
    }
    instruction_index = 0U;
    while (instruction_index < program.instruction_count)
    {
        instruction = &program.instructions[instruction_index];
        output[offset] = static_cast<uint8_t>(instruction->opcode);
        scripting_write_u64(output + offset + 1U,
            static_cast<uint64_t>(instruction->operand));
        scripting_write_u32(output + offset + 9U, instruction->auxiliary);
        offset += FT_SCRIPTING_SERIALIZED_INSTRUCTION_BYTES;
        instruction_index += 1U;
    }
    checksum = scripting_hash_bytes(output + FT_SCRIPTING_SERIALIZED_HEADER_BYTES,
        offset - FT_SCRIPTING_SERIALIZED_HEADER_BYTES);
    scripting_write_u64(output + 16U, checksum);
    *output_size = offset;
    return (FT_ERR_SUCCESS);
}

int32_t scripting_engine::deserialize_program(const uint8_t *input,
    uint32_t input_size, scripting_program *program) const noexcept
{
    scripting_program loaded_program;
    uint32_t instruction_count;
    uint32_t string_data_size;
    uint32_t required_size;
    uint32_t offset;
    uint32_t instruction_index;
    uint64_t stored_checksum;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (input == ft_nullptr || program == ft_nullptr
        || input_size < FT_SCRIPTING_SERIALIZED_HEADER_BYTES
        || input_size > FT_SCRIPTING_MAX_SERIALIZED_PROGRAM_BYTES)
        return (FT_ERR_INVALID_ARGUMENT);
    if (scripting_read_u32(input) != FT_SCRIPTING_BYTECODE_MAGIC)
        return (FT_ERR_INVALID_ARGUMENT);
    loaded_program = {};
    loaded_program.format_version = scripting_read_u32(input + 4U);
    instruction_count = scripting_read_u32(input + 8U);
    string_data_size = scripting_read_u32(input + 12U);
    if (instruction_count == 0U
        || instruction_count > FT_SCRIPTING_MAX_INSTRUCTIONS
        || string_data_size > FT_SCRIPTING_MAX_STRING_BYTES)
        return (FT_ERR_INVALID_ARGUMENT);
    required_size = FT_SCRIPTING_SERIALIZED_HEADER_BYTES + string_data_size
        + instruction_count * FT_SCRIPTING_SERIALIZED_INSTRUCTION_BYTES;
    if (required_size != input_size)
        return (FT_ERR_INVALID_ARGUMENT);
    stored_checksum = scripting_read_u64(input + 16U);
    if (stored_checksum != scripting_hash_bytes(
        input + FT_SCRIPTING_SERIALIZED_HEADER_BYTES,
        input_size - FT_SCRIPTING_SERIALIZED_HEADER_BYTES))
        return (FT_ERR_INVALID_ARGUMENT);
    loaded_program.instruction_count = instruction_count;
    loaded_program.string_data_size = string_data_size;
    offset = FT_SCRIPTING_SERIALIZED_HEADER_BYTES;
    if (string_data_size != 0U)
    {
        ft_memcpy(loaded_program.string_data, input + offset,
            string_data_size);
        offset += string_data_size;
    }
    instruction_index = 0U;
    while (instruction_index < instruction_count)
    {
        loaded_program.instructions[instruction_index].opcode =
            static_cast<scripting_opcode>(input[offset]);
        loaded_program.instructions[instruction_index].operand =
            static_cast<int64_t>(scripting_read_u64(input + offset + 1U));
        loaded_program.instructions[instruction_index].auxiliary =
            scripting_read_u32(input + offset + 9U);
        offset += FT_SCRIPTING_SERIALIZED_INSTRUCTION_BYTES;
        instruction_index += 1U;
    }
    if (this->verify_program(loaded_program) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    *program = loaded_program;
    return (FT_ERR_SUCCESS);
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
    value->boolean_value = FT_FALSE;
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
    value->boolean_value = FT_FALSE;
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
    value->boolean_value = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t scripting_value_set_boolean(scripting_value *value,
    ft_bool boolean_value) noexcept
{
    if (value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    value->type = SCRIPTING_VALUE_BOOLEAN;
    value->integer_value = 0;
    value->string_value = ft_nullptr;
    value->string_length = 0U;
    if (boolean_value == FT_FALSE)
        value->boolean_value = FT_FALSE;
    else
        value->boolean_value = FT_TRUE;
    return (FT_ERR_SUCCESS);
}
