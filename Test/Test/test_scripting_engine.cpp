#include "../test_internal.hpp"
#include "../../Modules/Scripting/scripting.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t scripting_test_add_native(const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    int64_t total;
    uint32_t index;

    (void)context;
    (void)user_data;
    if (arguments == ft_nullptr || result == ft_nullptr || argument_count != 2U)
        return (FT_ERR_INVALID_ARGUMENT);
    total = 0;
    index = 0U;
    while (index < argument_count)
    {
        if (arguments[index].type != SCRIPTING_VALUE_INTEGER)
            return (FT_ERR_INVALID_ARGUMENT);
        total += arguments[index].integer_value;
        index += 1U;
    }
    return (scripting_value_set_integer(result, total));
}

static int32_t scripting_test_string_length_native(
    const scripting_call_context *context, const scripting_value *arguments,
    uint32_t argument_count, scripting_value *result, void *user_data) noexcept
{
    (void)context;
    (void)user_data;
    if (arguments == ft_nullptr || result == ft_nullptr || argument_count != 1U
        || arguments[0].type != SCRIPTING_VALUE_STRING)
        return (FT_ERR_INVALID_ARGUMENT);
    return (scripting_value_set_integer(result,
        static_cast<int64_t>(arguments[0].string_length)));
}

FT_TEST(test_scripting_engine_evaluates_deterministic_expression)
{
    scripting_engine engine;
    scripting_value result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute("2 + 3 * 4", &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_INTEGER, result.type);
    FT_ASSERT_EQ(static_cast<int64_t>(14), result.integer_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute(
        "let base = 9; return base * 2;", &result));
    FT_ASSERT_EQ(static_cast<int64_t>(18), result.integer_value);
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, engine.execute(
        "let base = 1; let base = 2; return base;", &result));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_scripting_engine_invokes_typed_native_callback)
{
    scripting_engine engine;
    scripting_value result;
    uint32_t native_id;

    native_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_native("add",
        scripting_test_add_native, ft_nullptr, &native_id));
    FT_ASSERT_EQ(0U, native_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute("add(7, 5)", &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_INTEGER, result.type);
    FT_ASSERT_EQ(static_cast<int64_t>(12), result.integer_value);
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, engine.register_native("add",
        scripting_test_add_native, ft_nullptr, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_scripting_engine_reports_bounded_and_malformed_execution)
{
    scripting_engine engine;
    scripting_value result;
    scripting_diagnostic diagnostic;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.set_operation_limit(1U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.execute("missing(1)", &result));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_last_diagnostic(&diagnostic));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, diagnostic.error_code);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.execute("(1 +", &result));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.execute("returnx", &result));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, engine.execute("9223372036854775807 + 1",
        &result));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.execute("true & false",
        &result));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.execute("true &&", &result));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_scripting_engine_compiles_verifies_and_executes_bytecode)
{
    scripting_engine engine;
    scripting_program program;
    scripting_value result;
    uint32_t native_id;

    native_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_native("add",
        scripting_test_add_native, ft_nullptr, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.compile(
        "let base = 9; return add(base, 3) * 2;", &program));
    FT_ASSERT_EQ(FT_SCRIPTING_BYTECODE_VERSION, program.format_version);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.verify_program(program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute_program(program, &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_INTEGER, result.type);
    FT_ASSERT_EQ(static_cast<int64_t>(24), result.integer_value);
    program.instructions[0].opcode = static_cast<scripting_opcode>(255U);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.verify_program(program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_scripting_engine_preserves_string_literals_in_bytecode)
{
    scripting_engine engine;
    scripting_program program;
    scripting_value result;
    uint32_t native_id;

    native_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_native("length",
        scripting_test_string_length_native, ft_nullptr, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.compile(
        "return length(\"terrain\");", &program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.verify_program(program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute_program(program, &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_INTEGER, result.type);
    FT_ASSERT_EQ(static_cast<int64_t>(7), result.integer_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.compile(
        "length(\"a\"); length(\"bc\");", &program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute_program(program, &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_NULL, result.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.compile("return true;", &program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute_program(program, &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_BOOLEAN, result.type);
    FT_ASSERT_EQ(FT_TRUE, result.boolean_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.compile(
        "return if (5 > 2) 11 else 22;", &program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.verify_program(program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute_program(program, &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_INTEGER, result.type);
    FT_ASSERT_EQ(static_cast<int64_t>(11), result.integer_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.compile(
        "return if (1 == 0) 11 else if (2 == 2) 22 else 33;", &program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute_program(program, &result));
    FT_ASSERT_EQ(static_cast<int64_t>(22), result.integer_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_scripting_engine_comparisons_match_direct_and_bytecode_execution)
{
    scripting_engine engine;
    scripting_program program;
    scripting_value result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute("2 + 3 == 5", &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_BOOLEAN, result.type);
    FT_ASSERT_EQ(FT_TRUE, result.boolean_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute("7 >= 8", &result));
    FT_ASSERT_EQ(FT_FALSE, result.boolean_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute(
        "return if (3 == 3) 17 else 19;", &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_INTEGER, result.type);
    FT_ASSERT_EQ(static_cast<int64_t>(17), result.integer_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute("\"abc\" < \"abd\"",
        &result));
    FT_ASSERT_EQ(FT_TRUE, result.boolean_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute("!(2 > 3) && (1 == 1)",
        &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_BOOLEAN, result.type);
    FT_ASSERT_EQ(FT_TRUE, result.boolean_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute("false || true", &result));
    FT_ASSERT_EQ(FT_TRUE, result.boolean_value);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, engine.execute("null < null",
        &result));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.compile(
        "let value = 9; return value != 4;", &program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.verify_program(program));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.execute_program(program, &result));
    FT_ASSERT_EQ(SCRIPTING_VALUE_BOOLEAN, result.type);
    FT_ASSERT_EQ(FT_TRUE, result.boolean_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}
