#ifndef SCRIPTING_HPP
# define SCRIPTING_HPP

# include <cstdint>
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_SCRIPTING_MAX_NATIVES = 128U;
static const uint32_t FT_SCRIPTING_MAX_ARGUMENTS = 8U;
static const uint32_t FT_SCRIPTING_MAX_LOCALS = 32U;
static const uint32_t FT_SCRIPTING_MAX_SOURCE_BYTES = 65536U;
static const uint32_t FT_SCRIPTING_MAX_TOKENS = 4096U;
static const uint32_t FT_SCRIPTING_MAX_OPERATIONS = 4096U;
static const uint32_t FT_SCRIPTING_BYTECODE_VERSION = 1U;
static const uint32_t FT_SCRIPTING_MAX_INSTRUCTIONS = 4096U;
static const uint32_t FT_SCRIPTING_MAX_STRING_BYTES = 16384U;

enum scripting_value_type : uint8_t
{
    SCRIPTING_VALUE_NULL = 0U,
    SCRIPTING_VALUE_INTEGER = 1U,
    SCRIPTING_VALUE_STRING = 2U
};

struct scripting_value
{
    scripting_value_type type;
    int64_t integer_value;
    const char *string_value;
    uint32_t string_length;
};

class scripting_engine;

struct scripting_call_context
{
    const scripting_engine *engine;
    uint32_t native_id;
    uint32_t operation_count;
};

typedef int32_t (*scripting_native_callback)(
    const scripting_call_context *context,
    const scripting_value *arguments,
    uint32_t argument_count,
    scripting_value *result,
    void *user_data) noexcept;

struct scripting_diagnostic
{
    int32_t error_code;
    uint32_t source_offset;
    uint32_t source_length;
};

enum scripting_opcode : uint8_t
{
    SCRIPTING_OP_PUSH_NULL = 0U,
    SCRIPTING_OP_PUSH_INTEGER = 1U,
    SCRIPTING_OP_LOAD_LOCAL = 2U,
    SCRIPTING_OP_STORE_LOCAL = 3U,
    SCRIPTING_OP_NEGATE = 4U,
    SCRIPTING_OP_ADD = 5U,
    SCRIPTING_OP_SUBTRACT = 6U,
    SCRIPTING_OP_MULTIPLY = 7U,
    SCRIPTING_OP_DIVIDE = 8U,
    SCRIPTING_OP_CALL_NATIVE = 9U,
    SCRIPTING_OP_RETURN = 10U,
    SCRIPTING_OP_PUSH_STRING = 11U,
    SCRIPTING_OP_POP = 12U
};

struct scripting_instruction
{
    scripting_opcode opcode;
    int64_t operand;
    uint32_t auxiliary;
};

struct scripting_program
{
    uint32_t format_version;
    uint32_t instruction_count;
    uint32_t string_data_size;
    char string_data[FT_SCRIPTING_MAX_STRING_BYTES];
    scripting_instruction instructions[FT_SCRIPTING_MAX_INSTRUCTIONS];
};

class scripting_engine
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        struct scripting_native_entry
        {
            const char *name;
            scripting_native_callback callback;
            void *user_data;
            ft_bool registered;
        };

        uint8_t _initialised_state;
        scripting_native_entry _natives[FT_SCRIPTING_MAX_NATIVES];
        uint32_t _native_count;
        uint32_t _operation_limit;
        scripting_diagnostic _last_diagnostic;

        scripting_engine(const scripting_engine &other) noexcept = delete;
        scripting_engine(scripting_engine &&other) noexcept = delete;
        scripting_engine &operator=(const scripting_engine &other) noexcept = delete;
        scripting_engine &operator=(scripting_engine &&other) noexcept = delete;

    public:
        scripting_engine() noexcept;
        ~scripting_engine() noexcept;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(scripting_engine &other) noexcept;
        int32_t register_native(const char *name,
            scripting_native_callback callback, void *user_data,
            uint32_t *native_id) noexcept;
        int32_t set_operation_limit(uint32_t operation_limit) noexcept;
        int32_t execute(const char *source, scripting_value *result) noexcept;
        int32_t compile(const char *source, scripting_program *program) noexcept;
        int32_t verify_program(const scripting_program &program) const noexcept;
        int32_t execute_program(const scripting_program &program,
            scripting_value *result) noexcept;
        int32_t get_last_diagnostic(scripting_diagnostic *diagnostic) const noexcept;
        int32_t find_native(const char *name, uint32_t name_length,
            uint32_t *native_id) const noexcept;
        int32_t get_native_name(uint32_t native_id,
            const char **name) const noexcept;
        uint32_t get_operation_limit() const noexcept;
        int32_t invoke_native_id(uint32_t native_id,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, uint32_t operation_count) noexcept;
};

int32_t scripting_value_set_null(scripting_value *value) noexcept;
int32_t scripting_value_set_integer(scripting_value *value,
    int64_t integer_value) noexcept;
int32_t scripting_value_set_string(scripting_value *value,
    const char *string, uint32_t length) noexcept;

#endif
