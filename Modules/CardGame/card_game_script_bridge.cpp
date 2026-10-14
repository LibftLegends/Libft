#include "card_game_script_bridge.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include <new>

static void card_game_script_hash_byte(uint64_t *hash,
    uint8_t byte_value) noexcept
{
    *hash ^= byte_value;
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

static void card_game_script_append_u32(std::vector<uint8_t> &bytes,
    uint32_t value)
{
    for (uint32_t shift = 0U; shift < 32U; shift += 8U)
        bytes.push_back(static_cast<uint8_t>(value >> shift));
}

static void card_game_script_append_u64(std::vector<uint8_t> &bytes,
    uint64_t value)
{
    for (uint32_t shift = 0U; shift < 64U; shift += 8U)
        bytes.push_back(static_cast<uint8_t>(value >> shift));
}

static uint32_t card_game_script_read_u32(const uint8_t *bytes,
    uint32_t offset) noexcept
{
    return (static_cast<uint32_t>(bytes[offset])
        | (static_cast<uint32_t>(bytes[offset + 1U]) << 8U)
        | (static_cast<uint32_t>(bytes[offset + 2U]) << 16U)
        | (static_cast<uint32_t>(bytes[offset + 3U]) << 24U));
}

static uint64_t card_game_script_read_u64(const uint8_t *bytes,
    uint32_t offset) noexcept
{
    uint64_t value = 0U;

    for (uint32_t shift = 0U; shift < 64U; shift += 8U)
        value |= static_cast<uint64_t>(bytes[offset + shift / 8U]) << shift;
    return (value);
}

static uint64_t card_game_script_context_checksum(const uint8_t *bytes,
    uint32_t byte_count) noexcept
{
    uint64_t hash = 14695981039346656037ULL;

    for (uint32_t index = 0U; index < byte_count; ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return (hash);
}

card_game_script_bridge::card_game_script_bridge() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _engine(ft_nullptr),
      _scripting(), _bindings(), _binding_count(0U),
      _context_values(), _context_key_count(0U),
      _active_operations(ft_nullptr), _active_context(ft_nullptr),
      _operation_limit(512U), _policy_manifest_hash(0U)
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
    this->_context_key_count = 0U;
    this->_active_operations = ft_nullptr;
    this->_active_context = ft_nullptr;
    this->_policy_manifest_hash = 0U;
    result = this->_scripting.set_operation_limit(this->_operation_limit);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.register_native("card_actor_id",
            &card_game_script_bridge::card_actor_id, this, &native_id);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.register_native("card_instance_id",
            &card_game_script_bridge::card_instance_id, this, &native_id);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.register_native("session_phase",
            &card_game_script_bridge::session_phase, this, &native_id);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.register_native("emit_host_intent",
            &card_game_script_bridge::emit_host_intent, this, &native_id);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.register_native("random_bounded",
            &card_game_script_bridge::random_bounded, this, &native_id);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.register_native("choice_selected_id",
            &card_game_script_bridge::choice_selected_id, this, &native_id);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.register_native("context_value",
            &card_game_script_bridge::context_value, this, &native_id);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.register_native("draw_cards",
            &card_game_script_bridge::draw_cards, this, &native_id);
    if (result == FT_ERR_SUCCESS)
        result = this->_scripting.register_native("require_discard",
            &card_game_script_bridge::require_discard, this, &native_id);
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
    if (this->_active_operations != ft_nullptr
        || this->_active_context != ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    index = 0U;
    while (index < this->_binding_count)
    {
        if (this->_bindings[index].registered == FT_TRUE)
        {
            result = this->_engine->unregister_effect_callback(
                this->_bindings[index].effect_id,
                &card_game_script_bridge::execute_effect,
                &this->_bindings[index]);
            if (result != FT_ERR_SUCCESS)
                return (result);
        }
        index += 1U;
    }
    first_error = FT_ERR_SUCCESS;
    this->_active_operations = ft_nullptr;
    this->_active_context = ft_nullptr;
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
    ft_bzero(this->_context_values, sizeof(this->_context_values));
    this->_context_key_count = 0U;
    result = this->_scripting.destroy();
    if (result != FT_ERR_SUCCESS)
        first_error = result;
    this->_engine = ft_nullptr;
    this->_policy_manifest_hash = 0U;
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

int32_t card_game_script_bridge::serialize_script_catalog(
    std::vector<uint8_t> &bytes_out) const noexcept
{
    std::vector<uint8_t> staged;
    uint8_t *serialized_program;
    uint32_t serialized_size;
    uint32_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_active_context != ft_nullptr
        || this->_active_operations != ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    serialized_program = ft_nullptr;
    if (this->_binding_count > 0U)
        serialized_program = static_cast<uint8_t *>(cma_malloc(
            FT_SCRIPTING_MAX_SERIALIZED_PROGRAM_BYTES));
    if (this->_binding_count > 0U && serialized_program == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    result = FT_ERR_SUCCESS;
    try
    {
        staged.reserve(12U + static_cast<std::size_t>(this->_binding_count)
            * (8U + FT_SCRIPTING_MAX_SERIALIZED_PROGRAM_BYTES) + 8U);
        staged.push_back('C');
        staged.push_back('G');
        staged.push_back('S');
        staged.push_back('C');
        card_game_script_append_u32(staged, 1U);
        card_game_script_append_u32(staged, this->_binding_count);
        index = 0U;
        while (index < this->_binding_count && result == FT_ERR_SUCCESS)
        {
            if (this->_bindings[index].registered != FT_TRUE
                || this->_bindings[index].program == ft_nullptr
                || this->_bindings[index].event_type == 0U)
            {
                result = FT_ERR_INVALID_STATE;
                break ;
            }
            result = this->_scripting.serialize_program(
                *this->_bindings[index].program, serialized_program,
                FT_SCRIPTING_MAX_SERIALIZED_PROGRAM_BYTES,
                &serialized_size);
            if (result == FT_ERR_SUCCESS)
            {
                card_game_script_append_u32(staged,
                    this->_bindings[index].event_type);
                card_game_script_append_u32(staged, serialized_size);
                staged.insert(staged.end(), serialized_program,
                    serialized_program + serialized_size);
            }
            index += 1U;
        }
        if (result == FT_ERR_SUCCESS)
            card_game_script_append_u64(staged,
                card_game_script_context_checksum(staged.data(),
                    static_cast<uint32_t>(staged.size())));
    }
    catch (const std::bad_alloc &)
    {
        result = FT_ERR_NO_MEMORY;
    }
    cma_free(serialized_program);
    if (result == FT_ERR_SUCCESS)
        bytes_out.swap(staged);
    return (result);
}

int32_t card_game_script_bridge::load_script_catalog(const uint8_t *bytes,
    uint32_t byte_count, uint64_t trusted_manifest_hash) noexcept
{
    static const uint32_t CATALOG_HEADER_SIZE = 12U;
    static const uint32_t CATALOG_CHECKSUM_SIZE = 8U;
    static const uint64_t CATALOG_MAXIMUM_BYTES = 20ULL
        + static_cast<uint64_t>(FT_CARD_GAME_SCRIPT_MAX_EFFECTS)
            * (8ULL + FT_SCRIPTING_MAX_SERIALIZED_PROGRAM_BYTES);
    scripting_program *staged_programs[FT_CARD_GAME_SCRIPT_MAX_EFFECTS];
    uint32_t staged_event_types[FT_CARD_GAME_SCRIPT_MAX_EFFECTS];
    uint32_t record_count;
    uint32_t checksum_offset;
    uint32_t offset;
    uint32_t index;
    uint32_t registered_count;
    uint32_t effect_id;
    uint64_t actual_manifest_hash;
    int32_t result;
    int32_t rollback_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (bytes == ft_nullptr || byte_count < CATALOG_HEADER_SIZE
            + CATALOG_CHECKSUM_SIZE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (static_cast<uint64_t>(byte_count) > CATALOG_MAXIMUM_BYTES)
        return (FT_ERR_OUT_OF_RANGE);
    if (this->_binding_count != 0U || this->_active_context != ft_nullptr
        || this->_active_operations != ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    if (bytes[0] != 'C' || bytes[1] != 'G' || bytes[2] != 'S'
        || bytes[3] != 'C' || card_game_script_read_u32(bytes, 4U) != 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    record_count = card_game_script_read_u32(bytes, 8U);
    if (record_count > FT_CARD_GAME_SCRIPT_MAX_EFFECTS)
        return (FT_ERR_OUT_OF_RANGE);
    ft_bzero(staged_programs, sizeof(staged_programs));
    if (card_game_script_read_u64(bytes, byte_count - CATALOG_CHECKSUM_SIZE)
        != card_game_script_context_checksum(bytes,
            byte_count - CATALOG_CHECKSUM_SIZE))
        return (FT_ERR_INVALID_ARGUMENT);
    checksum_offset = byte_count - CATALOG_CHECKSUM_SIZE;
    offset = CATALOG_HEADER_SIZE;
    index = 0U;
    result = FT_ERR_SUCCESS;
    while (index < record_count)
    {
        uint32_t event_type;
        uint32_t program_size;

        if (offset > checksum_offset
            || checksum_offset - offset < 8U)
        {
            result = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        event_type = card_game_script_read_u32(bytes, offset);
        program_size = card_game_script_read_u32(bytes, offset + 4U);
        offset += 8U;
        if (event_type == 0U || program_size == 0U
            || program_size > FT_SCRIPTING_MAX_SERIALIZED_PROGRAM_BYTES
            || program_size > checksum_offset - offset)
        {
            result = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        staged_programs[index] = static_cast<scripting_program *>(
            cma_malloc(sizeof(scripting_program)));
        if (staged_programs[index] == ft_nullptr)
        {
            result = FT_ERR_NO_MEMORY;
            break ;
        }
        ft_bzero(staged_programs[index], sizeof(scripting_program));
        result = this->_scripting.deserialize_program(bytes + offset,
            program_size, staged_programs[index]);
        if (result == FT_ERR_SUCCESS)
            result = this->_scripting.verify_program(
                *staged_programs[index]);
        if (result != FT_ERR_SUCCESS)
            break ;
        staged_event_types[index] = event_type;
        offset += program_size;
        index += 1U;
    }
    if (index == record_count && offset != checksum_offset)
        result = FT_ERR_INVALID_ARGUMENT;
    if (index != record_count || result != FT_ERR_SUCCESS)
    {
        index = 0U;
        while (index < record_count)
        {
            if (staged_programs[index] != ft_nullptr)
                cma_free(staged_programs[index]);
            index += 1U;
        }
        return (result);
    }
    registered_count = 0U;
    result = FT_ERR_SUCCESS;
    while (registered_count < record_count && result == FT_ERR_SUCCESS)
    {
        this->_bindings[registered_count].bridge = this;
        this->_bindings[registered_count].program =
            staged_programs[registered_count];
        this->_bindings[registered_count].effect_id = 0U;
        this->_bindings[registered_count].event_type =
            staged_event_types[registered_count];
        this->_bindings[registered_count].registered = FT_FALSE;
        result = this->_engine->register_effect_callback(
            &card_game_script_bridge::execute_effect,
            &this->_bindings[registered_count],
            staged_event_types[registered_count], &effect_id);
        if (result == FT_ERR_SUCCESS)
        {
            this->_bindings[registered_count].effect_id = effect_id;
            this->_bindings[registered_count].registered = FT_TRUE;
            staged_programs[registered_count] = ft_nullptr;
            registered_count += 1U;
            this->_binding_count = registered_count;
        }
    }
    if (result == FT_ERR_SUCCESS)
    {
        result = this->get_manifest_hash(&actual_manifest_hash);
        if (result == FT_ERR_SUCCESS
            && actual_manifest_hash != trusted_manifest_hash)
            result = FT_ERR_CONFIGURATION;
    }
    if (result != FT_ERR_SUCCESS && registered_count < record_count)
    {
        this->_bindings[registered_count].program = ft_nullptr;
        this->_bindings[registered_count].bridge = ft_nullptr;
        this->_bindings[registered_count].event_type = 0U;
        this->_bindings[registered_count].effect_id = 0U;
        this->_bindings[registered_count].registered = FT_FALSE;
    }
    if (result != FT_ERR_SUCCESS && registered_count > 0U)
    {
        rollback_error = FT_ERR_SUCCESS;
        while (registered_count > 0U && rollback_error == FT_ERR_SUCCESS)
        {
            const uint32_t rollback_index = registered_count - 1U;
            rollback_error = this->_engine->unregister_effect_callback(
                this->_bindings[rollback_index].effect_id,
                &card_game_script_bridge::execute_effect,
                &this->_bindings[rollback_index]);
            if (rollback_error == FT_ERR_SUCCESS)
            {
                cma_free(this->_bindings[rollback_index].program);
                this->_bindings[rollback_index].program = ft_nullptr;
                this->_bindings[rollback_index].bridge = ft_nullptr;
                this->_bindings[rollback_index].registered = FT_FALSE;
                registered_count -= 1U;
                this->_binding_count = registered_count;
            }
        }
        if (rollback_error != FT_ERR_SUCCESS)
            result = rollback_error;
    }
    index = registered_count;
    while (index < record_count)
    {
        if (staged_programs[index] != ft_nullptr)
            cma_free(staged_programs[index]);
        staged_programs[index] = ft_nullptr;
        index += 1U;
    }
    return (result);
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

int32_t card_game_script_bridge::set_policy_manifest_hash(
    uint64_t manifest_hash) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_binding_count != 0U || this->_active_context != ft_nullptr
        || this->_active_operations != ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    this->_policy_manifest_hash = manifest_hash;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_script_bridge::register_context_key(uint32_t key_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (key_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_binding_count != 0U)
        return (FT_ERR_INVALID_OPERATION);
    if (this->_context_key_count >= FT_CARD_GAME_SCRIPT_MAX_CONTEXT_KEYS)
        return (FT_ERR_FULL);
    if (this->_context_key_count > 0U
        && key_id <= this->_context_values[
            this->_context_key_count - 1U].key_id)
        return (FT_ERR_INVALID_ARGUMENT);
    context_value_entry &entry =
        this->_context_values[this->_context_key_count];
    entry.key_id = key_id;
    entry.value = 0;
    entry.is_set = FT_FALSE;
    this->_context_key_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t card_game_script_bridge::set_context_value(uint32_t key_id,
    int64_t value) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (key_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_active_context != ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    for (uint32_t index = 0U; index < this->_context_key_count; ++index)
    {
        if (this->_context_values[index].key_id == key_id)
        {
            this->_context_values[index].value = value;
            this->_context_values[index].is_set = FT_TRUE;
            return (FT_ERR_SUCCESS);
        }
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_script_bridge::clear_context_value(uint32_t key_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (key_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_active_context != ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    for (uint32_t index = 0U; index < this->_context_key_count; ++index)
    {
        if (this->_context_values[index].key_id == key_id)
        {
            this->_context_values[index].value = 0;
            this->_context_values[index].is_set = FT_FALSE;
            return (FT_ERR_SUCCESS);
        }
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_script_bridge::serialize_context_values(
    std::vector<uint8_t> &bytes_out) const noexcept
{
    std::vector<uint8_t> staged;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_active_context != ft_nullptr
        || this->_active_operations != ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    try
    {
        staged.reserve(20U + this->_context_key_count * 16U);
        staged.push_back('C');
        staged.push_back('C');
        staged.push_back('V');
        staged.push_back('1');
        card_game_script_append_u32(staged, 1U);
        card_game_script_append_u32(staged, this->_context_key_count);
        for (uint32_t index = 0U; index < this->_context_key_count; ++index)
        {
            const context_value_entry &entry = this->_context_values[index];

            card_game_script_append_u32(staged, entry.key_id);
            card_game_script_append_u32(staged,
                entry.is_set == FT_TRUE ? 1U : 0U);
            card_game_script_append_u64(staged,
                static_cast<uint64_t>(entry.value));
        }
        card_game_script_append_u64(staged,
            card_game_script_context_checksum(staged.data(),
                static_cast<uint32_t>(staged.size())));
    }
    catch (...)
    {
        return (FT_ERR_NO_MEMORY);
    }
    bytes_out.swap(staged);
    return (FT_ERR_SUCCESS);
}

int32_t card_game_script_bridge::restore_context_values(
    const uint8_t *bytes, uint32_t byte_count) noexcept
{
    context_value_entry staged[FT_CARD_GAME_SCRIPT_MAX_CONTEXT_KEYS];
    uint32_t expected_size;
    uint32_t index;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (bytes == ft_nullptr || this->_active_context != ft_nullptr
        || this->_active_operations != ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (byte_count < 20U || bytes[0] != 'C' || bytes[1] != 'C'
        || bytes[2] != 'V' || bytes[3] != '1'
        || card_game_script_read_u32(bytes, 4U) != 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    const uint32_t count = card_game_script_read_u32(bytes, 8U);
    if (count > FT_CARD_GAME_SCRIPT_MAX_CONTEXT_KEYS
        || count != this->_context_key_count
        || count > (UINT32_MAX - 20U) / 16U)
        return (FT_ERR_INVALID_ARGUMENT);
    expected_size = 20U + count * 16U;
    if (byte_count != expected_size
        || card_game_script_read_u64(bytes, byte_count - 8U)
            != card_game_script_context_checksum(bytes, byte_count - 8U))
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < count)
    {
        const uint32_t offset = 12U + index * 16U;
        const uint32_t key_id = card_game_script_read_u32(bytes, offset);
        const uint32_t is_set = card_game_script_read_u32(bytes, offset + 4U);
        const uint64_t raw_value = card_game_script_read_u64(bytes,
            offset + 8U);

        if (key_id != this->_context_values[index].key_id || is_set > 1U
            || (is_set == 0U && raw_value != 0U))
            return (FT_ERR_INVALID_ARGUMENT);
        staged[index].key_id = key_id;
        staged[index].is_set = is_set == 1U ? FT_TRUE : FT_FALSE;
        staged[index].value = static_cast<int64_t>(raw_value);
        index += 1U;
    }
    index = 0U;
    while (index < count)
    {
        this->_context_values[index] = staged[index];
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t card_game_script_bridge::get_registered_context_keys(
    std::vector<uint32_t> &keys_out) const noexcept
{
    std::vector<uint32_t> staged;
    uint32_t index = 0U;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    try
    {
        staged.reserve(this->_context_key_count);
        while (index < this->_context_key_count)
        {
            staged.push_back(this->_context_values[index].key_id);
            index += 1U;
        }
    }
    catch (...)
    {
        return (FT_ERR_NO_MEMORY);
    }
    keys_out.swap(staged);
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
    uint64_t script_environment_hash;
    uint64_t result_hash;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (hash == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = this->_scripting.get_native_manifest_hash(&native_hash);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_engine->get_script_environment_hash(
        &script_environment_hash);
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
    card_game_script_hash_u32(&result_hash, 3U);
    card_game_script_hash_u64(&result_hash, native_hash);
    card_game_script_hash_u64(&result_hash, script_environment_hash);
    card_game_script_hash_u64(&result_hash, this->_policy_manifest_hash);
    card_game_script_hash_u32(&result_hash,
        this->_scripting.get_operation_limit());
    card_game_script_hash_u32(&result_hash, this->_context_key_count);
    for (index = 0U; index < this->_context_key_count; ++index)
        card_game_script_hash_u32(&result_hash,
            this->_context_values[index].key_id);
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
    bridge->_active_context = &context;
    result = bridge->_scripting.execute_program(*binding->program,
        &script_result);
    bridge->_active_context = ft_nullptr;
    bridge->_active_operations = ft_nullptr;
    return (result);
}

int32_t card_game_script_bridge::card_actor_id(
    const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    card_game_script_bridge *bridge;

    (void)arguments;
    bridge = static_cast<card_game_script_bridge *>(user_data);
    if (context == ft_nullptr || result == ft_nullptr || bridge == ft_nullptr
        || context->engine != &bridge->_scripting
        || bridge->_active_context == ft_nullptr
        || bridge->initialize_runtime() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (argument_count != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    return (scripting_value_set_integer(result,
        static_cast<int64_t>(bridge->_active_context->active_player)));
}

int32_t card_game_script_bridge::card_instance_id(
    const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    card_game_script_bridge *bridge;

    (void)arguments;
    bridge = static_cast<card_game_script_bridge *>(user_data);
    if (context == ft_nullptr || result == ft_nullptr || bridge == ft_nullptr
        || context->engine != &bridge->_scripting
        || bridge->_active_context == ft_nullptr
        || bridge->initialize_runtime() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (argument_count != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    return (scripting_value_set_integer(result,
        static_cast<int64_t>(bridge->_active_context->source_instance)));
}

int32_t card_game_script_bridge::session_phase(
    const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    card_game_script_bridge *bridge;
    uint32_t phase_id;
    int32_t error_code;

    (void)arguments;
    bridge = static_cast<card_game_script_bridge *>(user_data);
    if (context == ft_nullptr || result == ft_nullptr || bridge == ft_nullptr
        || context->engine != &bridge->_scripting
        || bridge->_active_context == ft_nullptr
        || bridge->initialize_runtime() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (argument_count != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = bridge->_engine->get_current_phase(&phase_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (scripting_value_set_integer(result,
        static_cast<int64_t>(phase_id)));
}

int32_t card_game_script_bridge::random_bounded(
    const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    card_game_script_bridge *bridge;
    uint32_t value;
    int32_t error_code;

    bridge = static_cast<card_game_script_bridge *>(user_data);
    if (context == ft_nullptr || arguments == ft_nullptr
        || result == ft_nullptr || bridge == ft_nullptr
        || context->engine != &bridge->_scripting
        || bridge->_active_context == ft_nullptr
        || bridge->initialize_runtime() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (argument_count != 1U
        || arguments[0].type != SCRIPTING_VALUE_INTEGER
        || arguments[0].integer_value <= 0
        || static_cast<uint64_t>(arguments[0].integer_value) > UINT32_MAX)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = bridge->_engine->random_bounded(
        static_cast<uint32_t>(arguments[0].integer_value), &value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (scripting_value_set_integer(result,
        static_cast<int64_t>(value)));
}

int32_t card_game_script_bridge::choice_selected_id(
    const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    card_game_script_bridge *bridge;
    card_game_choice choice;
    int32_t error_code;

    bridge = static_cast<card_game_script_bridge *>(user_data);
    if (context == ft_nullptr || arguments == ft_nullptr
        || result == ft_nullptr || bridge == ft_nullptr
        || context->engine != &bridge->_scripting
        || bridge->_active_context == ft_nullptr
        || bridge->initialize_runtime() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (argument_count != 1U
        || arguments[0].type != SCRIPTING_VALUE_INTEGER
        || arguments[0].integer_value <= 0
        || static_cast<uint64_t>(arguments[0].integer_value) > UINT32_MAX)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = bridge->_engine->get_choice(
        static_cast<uint32_t>(arguments[0].integer_value), &choice);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (choice.player_id != bridge->_active_context->active_player)
        return (FT_ERR_PERMISSION_DENIED);
    if (choice.resolved != FT_TRUE || choice.selected_option_id == 0U)
        return (FT_ERR_INVALID_STATE);
    return (scripting_value_set_integer(result,
        static_cast<int64_t>(choice.selected_option_id)));
}

int32_t card_game_script_bridge::context_value(
    const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    card_game_script_bridge *bridge =
        static_cast<card_game_script_bridge *>(user_data);

    if (context == ft_nullptr || arguments == ft_nullptr
        || result == ft_nullptr || bridge == ft_nullptr
        || context->engine != &bridge->_scripting
        || bridge->_active_context == ft_nullptr
        || bridge->initialize_runtime() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (argument_count != 1U
        || arguments[0].type != SCRIPTING_VALUE_INTEGER
        || arguments[0].integer_value <= 0
        || static_cast<uint64_t>(arguments[0].integer_value) > UINT32_MAX)
        return (FT_ERR_INVALID_ARGUMENT);
    const uint32_t key_id =
        static_cast<uint32_t>(arguments[0].integer_value);
    for (uint32_t index = 0U; index < bridge->_context_key_count; ++index)
    {
        const context_value_entry &entry = bridge->_context_values[index];

        if (entry.key_id == key_id)
        {
            if (entry.is_set != FT_TRUE)
                return (FT_ERR_INVALID_STATE);
            return (scripting_value_set_integer(result, entry.value));
        }
    }
    return (FT_ERR_NOT_FOUND);
}

int32_t card_game_script_bridge::append_count_operation(
    const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result,
    card_game_operation_type operation_type) noexcept
{
    card_game_operation operation;
    int32_t append_error;

    if (context == ft_nullptr || arguments == ft_nullptr
        || result == ft_nullptr || this->_active_operations == ft_nullptr
        || this->_active_context == ft_nullptr
        || context->engine != &this->_scripting
        || this->initialize_runtime() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (argument_count != 1U
        || arguments[0].type != SCRIPTING_VALUE_INTEGER
        || arguments[0].integer_value <= 0
        || arguments[0].integer_value > static_cast<int64_t>(
            operation_type == CARD_GAME_OPERATION_DRAW_CARDS
                ? FT_CARD_GAME_MAX_DRAW_COUNT
                : FT_CARD_GAME_MAX_REQUIRED_DISCARD_COUNT))
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bzero(&operation, sizeof(operation));
    operation.type = operation_type;
    operation.player_id = this->_active_context->active_player;
    operation.amount = static_cast<int32_t>(arguments[0].integer_value);
    append_error = this->_active_operations->append(operation);
    if (append_error != FT_ERR_SUCCESS)
        return (append_error);
    return (scripting_value_set_null(result));
}

int32_t card_game_script_bridge::draw_cards(
    const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    card_game_script_bridge *bridge =
        static_cast<card_game_script_bridge *>(user_data);

    if (bridge == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    return (bridge->append_count_operation(context, arguments,
        argument_count, result, CARD_GAME_OPERATION_DRAW_CARDS));
}

int32_t card_game_script_bridge::require_discard(
    const scripting_call_context *context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    card_game_script_bridge *bridge =
        static_cast<card_game_script_bridge *>(user_data);

    if (bridge == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    return (bridge->append_count_operation(context, arguments,
        argument_count, result, CARD_GAME_OPERATION_REQUIRE_DISCARD));
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
