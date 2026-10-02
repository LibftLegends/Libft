#include "card_game_script_bridge.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"

static void card_game_script_hash_byte(uint64_t *hash, uint8_t byte) noexcept
{
    *hash ^= byte;
    *hash *= 1099511628211ULL;
}

static void card_game_script_hash_u32(uint64_t *hash, uint32_t value) noexcept
{
    uint32_t shift;

    shift = 0U;
    while (shift < 32U)
    {
        card_game_script_hash_byte(hash,
            static_cast<uint8_t>(value >> shift));
        shift += 8U;
    }
}

static void card_game_script_hash_u64(uint64_t *hash, uint64_t value) noexcept
{
    uint32_t shift;

    shift = 0U;
    while (shift < 64U)
    {
        card_game_script_hash_byte(hash,
            static_cast<uint8_t>(value >> shift));
        shift += 8U;
    }
}

card_game_script_bridge::card_game_script_bridge() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _engine(ft_nullptr),
      _scripting(), _bindings(), _binding_count(0U),
      _active_operations(ft_nullptr), _operation_limit(512U)
{
    return ;
}

card_game_script_bridge::~card_game_script_bridge() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t card_game_script_bridge::initialize(card_game_engine &engine) noexcept
{
    uint64_t rules_hash;
    uint32_t native_id;
    int32_t result;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    result = engine.get_rules_hash(&rules_hash);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_scripting.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (result);
    }
    this->_engine = &engine;
    this->_binding_count = 0U;
    this->_active_operations = ft_nullptr;
    result = this->_scripting.set_operation_limit(this->_operation_limit);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.register_native("emit_host_intent",
            &card_game_script_bridge::emit_host_intent, this, &native_id);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_scripting.destroy();
        this->_engine = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (result);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_script_bridge::initialize_runtime() noexcept
{
    uint64_t rules_hash;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_engine == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    if (this->_engine->get_rules_hash(&rules_hash) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_script_bridge::destroy() noexcept
{
    uint32_t index;
    int32_t first_error;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    first_error = FT_ERR_SUCCESS;
    this->_active_operations = ft_nullptr;
    index = 0U;
    while (index < this->_binding_count)
    {
        if (this->_bindings[index].program != ft_nullptr)
        {
            cma_free(this->_bindings[index].program);
        }
        this->_bindings[index].program = ft_nullptr;
        this->_bindings[index].bridge = ft_nullptr;
        this->_bindings[index].registered = FT_FALSE;
        index += 1U;
    }
    this->_binding_count = 0U;
    result = this->_scripting.destroy();
    if (result != FT_ERR_SUCCESS)
        first_error = result;
    this->_engine = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

int32_t card_game_script_bridge::register_script_effect(const char *source,
    uint32_t event_type, uint32_t *effect_id) noexcept
{
    scripting_program *program;
    uint32_t binding_index;
    int32_t result;

    result = this->initialize_runtime();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (source == ft_nullptr || event_type == 0U || effect_id == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_binding_count >= FT_CARD_GAME_SCRIPT_MAX_EFFECTS)
        return (FT_ERR_FULL);
    program = static_cast<scripting_program *>(cma_malloc(
        sizeof(scripting_program)));
    if (program == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    ft_bzero(program, sizeof(scripting_program));
    result = this->_scripting.compile(source, program);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.verify_program(*program);
    if (result != FT_ERR_SUCCESS)
    {
        cma_free(program);
        return (result);
    }
    binding_index = this->_binding_count;
    this->_bindings[binding_index].bridge = this;
    this->_bindings[binding_index].program = program;
    this->_bindings[binding_index].effect_id = 0U;
    this->_bindings[binding_index].registered = FT_FALSE;
    result = this->_engine->register_effect_callback(
        &card_game_script_bridge::execute_effect,
        &this->_bindings[binding_index], event_type, effect_id);
    if (result != FT_ERR_SUCCESS)
    {
        cma_free(program);
        this->_bindings[binding_index].program = ft_nullptr;
        this->_bindings[binding_index].bridge = ft_nullptr;
        return (result);
    }
    this->_bindings[binding_index].effect_id = *effect_id;
    this->_bindings[binding_index].event_type = event_type;
    this->_bindings[binding_index].registered = FT_TRUE;
    this->_binding_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_script_bridge::set_operation_limit(
    uint32_t operation_limit) noexcept
{
    if (operation_limit == 0U
        || operation_limit > FT_SCRIPTING_MAX_OPERATIONS)
        return (FT_ERR_OUT_OF_RANGE);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (this->_scripting.set_operation_limit(operation_limit));
    this->_operation_limit = operation_limit;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_script_bridge::get_registered_effect_count(
    uint32_t *count) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *count = this->_binding_count;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_script_bridge::get_manifest_hash(uint64_t *hash) const noexcept
{
    uint8_t *serialized_program;
    uint32_t serialized_size;
    uint32_t index;
    uint32_t byte_index;
    uint64_t native_hash;
    uint64_t result_hash;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (hash == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_scripting.get_native_manifest_hash(&native_hash);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result_hash = 14695981039346656037ULL;
    static const uint8_t domain[] = {
        'L', 'i', 'b', 'f', 't', '.', 'C', 'a', 'r', 'd', 'G', 'a', 'm',
        'e', '.', 'S', 'c', 'r', 'i', 'p', 't', 'M', 'a', 'n', 'i', 'f',
        'e', 's', 't', 0U
    };
    byte_index = 0U;
    while (byte_index < sizeof(domain))
    {
        card_game_script_hash_byte(&result_hash, domain[byte_index]);
        byte_index += 1U;
    }
    card_game_script_hash_u32(&result_hash, 1U);
    card_game_script_hash_u64(&result_hash, native_hash);
    card_game_script_hash_u32(&result_hash, this->_binding_count);
    serialized_program = ft_nullptr;
    if (this->_binding_count > 0U)
    {
        serialized_program = static_cast<uint8_t *>(cma_malloc(
            FT_SCRIPTING_MAX_SERIALIZED_PROGRAM_BYTES));
        if (serialized_program == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    index = 0U;
    while (index < this->_binding_count)
    {
        if (this->_bindings[index].registered != FT_TRUE
            || this->_bindings[index].program == ft_nullptr)
        {
            cma_free(serialized_program);
            return (FT_ERR_INVALID_STATE);
        }
        result = this->_scripting.serialize_program(
            *this->_bindings[index].program, serialized_program,
            FT_SCRIPTING_MAX_SERIALIZED_PROGRAM_BYTES, &serialized_size);
        if (result != FT_ERR_SUCCESS)
        {
            cma_free(serialized_program);
            return (result);
        }
        card_game_script_hash_u32(&result_hash,
            this->_bindings[index].event_type);
        card_game_script_hash_u32(&result_hash, serialized_size);
        byte_index = 0U;
        while (byte_index < serialized_size)
        {
            card_game_script_hash_byte(&result_hash,
                serialized_program[byte_index]);
            byte_index += 1U;
        }
        index += 1U;
    }
    cma_free(serialized_program);
    *hash = result_hash;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_script_bridge::execute_effect(
    const card_game_engine &engine, const card_game_effect_context &context,
    card_game_operation_buffer &operations, void *user_data) noexcept
{
    script_effect_binding *binding;
    card_game_script_bridge *bridge;
    scripting_value script_result;
    int32_t result;

    (void)engine;
    (void)context;
    binding = static_cast<script_effect_binding *>(user_data);
    if (binding == ft_nullptr || binding->registered == FT_FALSE
        || binding->bridge == ft_nullptr || binding->program == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    bridge = binding->bridge;
    result = bridge->initialize_runtime();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (bridge->_active_operations != ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    bridge->_active_operations = &operations;
    result = bridge->_scripting.execute_program(*binding->program,
        &script_result);
    bridge->_active_operations = ft_nullptr;
    return (result);
}

int32_t card_game_script_bridge::emit_host_intent(
    const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    card_game_script_bridge *bridge;
    card_game_host_intent intent;
    uint32_t index;
    uint32_t field_count;
    uint64_t raw_type;
    uint64_t raw_value;
    int32_t append_error;

    bridge = static_cast<card_game_script_bridge *>(user_data);
    if (context == ft_nullptr || arguments == ft_nullptr
        || result == ft_nullptr || bridge == ft_nullptr
        || bridge->_active_operations == ft_nullptr
        || bridge->initialize_runtime() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (argument_count < 2U || ((argument_count - 2U) % 2U) != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    field_count = (argument_count - 2U) / 2U;
    if (field_count > FT_CARD_GAME_MAX_HOST_INTENT_FIELDS
        || arguments[0].type != SCRIPTING_VALUE_INTEGER
        || arguments[1].type != SCRIPTING_VALUE_INTEGER
        || arguments[0].integer_value <= 0
        || arguments[0].integer_value > UINT32_MAX
        || arguments[1].integer_value <= 0
        || arguments[1].integer_value > UINT32_MAX)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bzero(&intent, sizeof(intent));
    intent.type_id = static_cast<uint32_t>(arguments[0].integer_value);
    intent.version = static_cast<uint32_t>(arguments[1].integer_value);
    intent.field_count = field_count;
    index = 0U;
    while (index < field_count)
    {
        const scripting_value &type_value = arguments[2U + index * 2U];
        const scripting_value &field_value = arguments[3U + index * 2U];

        if (type_value.type != SCRIPTING_VALUE_INTEGER
            || type_value.integer_value < CARD_GAME_HOST_INTENT_U32
            || type_value.integer_value > CARD_GAME_HOST_INTENT_BOOL)
            return (FT_ERR_INVALID_ARGUMENT);
        raw_type = static_cast<uint64_t>(type_value.integer_value);
        intent.fields[index].type =
            static_cast<card_game_host_intent_scalar_type>(raw_type);
        if (intent.fields[index].type == CARD_GAME_HOST_INTENT_BOOL)
        {
            if (field_value.type != SCRIPTING_VALUE_BOOLEAN)
                return (FT_ERR_INVALID_ARGUMENT);
            intent.fields[index].value = field_value.boolean_value;
        }
        else
        {
            if (field_value.type != SCRIPTING_VALUE_INTEGER)
                return (FT_ERR_INVALID_ARGUMENT);
            if (intent.fields[index].type == CARD_GAME_HOST_INTENT_U32
                && (field_value.integer_value < 0
                    || field_value.integer_value > UINT32_MAX))
                return (FT_ERR_OUT_OF_RANGE);
            if (intent.fields[index].type == CARD_GAME_HOST_INTENT_I32
                && (field_value.integer_value < INT32_MIN
                    || field_value.integer_value > INT32_MAX))
                return (FT_ERR_OUT_OF_RANGE);
            if (intent.fields[index].type == CARD_GAME_HOST_INTENT_U64
                && field_value.integer_value < 0)
                return (FT_ERR_OUT_OF_RANGE);
            raw_value = static_cast<uint64_t>(field_value.integer_value);
            intent.fields[index].value = raw_value;
        }
        index += 1U;
    }
    append_error = bridge->_active_operations->append_host_intent(intent);
    if (append_error != FT_ERR_SUCCESS)
        return (append_error);
    return (scripting_value_set_null(result));
}
