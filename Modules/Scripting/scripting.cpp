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
        result->type = SCRIPTING_VALUE_INTEGER;
        result->integer_value = static_cast<int64_t>(unsigned_value);
        return (FT_ERR_SUCCESS);
    }
    if (scripting_is_identifier_start(parser->source[parser->offset]) == FT_FALSE)
        return (scripting_fail(parser, FT_ERR_INVALID_ARGUMENT, parser->offset, 1U));
    start = parser->offset;
    while (parser->offset < parser->source_length
        && scripting_is_identifier_part(parser->source[parser->offset]) != FT_FALSE)
        parser->offset += 1U;
    native_name_length = parser->offset - start;
    native_name_length = parser->offset - start;
    if (scripting_equal_identifier(parser->source, start, native_name_length,
        "null") != FT_FALSE)
    {
        result->type = SCRIPTING_VALUE_NULL;
        result->integer_value = 0;
        return (FT_ERR_SUCCESS);
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
    (void)native_id;
    int32_t callback_error;

    callback_error = parser->engine->invoke_native(parser->source + start,
        native_name_length, arguments, argument_count, result,
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

int32_t scripting_engine::invoke_native(const char *name, uint32_t name_length,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, uint32_t operation_count) noexcept
{
    uint32_t native_id;
    scripting_call_context context;
    int32_t find_error;

    if (arguments == ft_nullptr || result == ft_nullptr
        || argument_count > FT_SCRIPTING_MAX_ARGUMENTS)
        return (FT_ERR_INVALID_ARGUMENT);
    find_error = this->find_native(name, name_length, &native_id);
    if (find_error != FT_ERR_SUCCESS)
        return (find_error);
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
    return (FT_ERR_SUCCESS);
}

int32_t scripting_value_set_integer(scripting_value *value,
    int64_t integer_value) noexcept
{
    if (value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    value->type = SCRIPTING_VALUE_INTEGER;
    value->integer_value = integer_value;
    return (FT_ERR_SUCCESS);
}
