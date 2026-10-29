#ifndef CARD_GAME_SCRIPT_BRIDGE_HPP
# define CARD_GAME_SCRIPT_BRIDGE_HPP

# include <cstdint>
# include <vector>
# include "card_game.hpp"
# include "../Scripting/scripting.hpp"

static const uint32_t FT_CARD_GAME_SCRIPT_MAX_EFFECTS = 64U;
static const uint32_t FT_CARD_GAME_SCRIPT_MAX_CONTEXT_KEYS = 64U;

class card_game_script_bridge
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        struct script_effect_binding
        {
            card_game_script_bridge *bridge;
            scripting_program *program;
            uint32_t effect_id;
            uint32_t event_type;
            ft_bool registered;
        };

        struct context_value_entry
        {
            uint32_t key_id;
            int64_t value;
            ft_bool is_set;
        };

        uint8_t _initialised_state;
        card_game_engine *_engine;
        scripting_engine _scripting;
        script_effect_binding _bindings[FT_CARD_GAME_SCRIPT_MAX_EFFECTS];
        uint32_t _binding_count;
        context_value_entry _context_values[
            FT_CARD_GAME_SCRIPT_MAX_CONTEXT_KEYS];
        uint32_t _context_key_count;
        card_game_operation_buffer *_active_operations;
        const card_game_effect_context *_active_context;
        uint32_t _operation_limit;
        uint64_t _policy_manifest_hash;

        card_game_script_bridge(const card_game_script_bridge &other) = delete;
        card_game_script_bridge(card_game_script_bridge &&other) = delete;
        card_game_script_bridge &operator=(
            const card_game_script_bridge &other) = delete;
        card_game_script_bridge &operator=(
            card_game_script_bridge &&other) = delete;

        static int32_t execute_effect(const card_game_engine &engine,
            const card_game_effect_context &context,
            card_game_operation_buffer &operations, void *user_data) noexcept;
        static int32_t emit_host_intent(
            const scripting_call_context *context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, void *user_data) noexcept;
        static int32_t card_actor_id(
            const scripting_call_context *context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, void *user_data) noexcept;
        static int32_t card_instance_id(
            const scripting_call_context *context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, void *user_data) noexcept;
        static int32_t session_phase(
            const scripting_call_context *context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, void *user_data) noexcept;
        static int32_t random_bounded(
            const scripting_call_context *context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, void *user_data) noexcept;
        static int32_t choice_selected_id(
            const scripting_call_context *context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, void *user_data) noexcept;
        static int32_t context_value(
            const scripting_call_context *context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, void *user_data) noexcept;
        static int32_t draw_cards(const scripting_call_context *context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, void *user_data) noexcept;
        static int32_t require_discard(const scripting_call_context *context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result, void *user_data) noexcept;
        int32_t append_count_operation(
            const scripting_call_context *context,
            const scripting_value *arguments, uint32_t argument_count,
            scripting_value *result,
            card_game_operation_type operation_type) noexcept;
        int32_t initialize_runtime() noexcept;

    public:
        card_game_script_bridge() noexcept;
        ~card_game_script_bridge() noexcept;

        int32_t initialize(card_game_engine &engine) noexcept;
        int32_t destroy() noexcept;
        int32_t register_script_effect(const char *source,
            uint32_t event_type, uint32_t *effect_id) noexcept;
        int32_t set_policy_manifest_hash(uint64_t manifest_hash) noexcept;
        int32_t serialize_script_catalog(
            std::vector<uint8_t> &bytes_out) const noexcept;
        int32_t load_script_catalog(const uint8_t *bytes,
            uint32_t byte_count, uint64_t trusted_manifest_hash) noexcept;
        int32_t set_operation_limit(uint32_t operation_limit) noexcept;
        /* Register stable, increasing context key IDs before script effects. */
        int32_t register_context_key(uint32_t key_id) noexcept;
        /* Values are host-owned invocation inputs and are excluded from the
         * content manifest. Set them only between engine actions. */
        int32_t set_context_value(uint32_t key_id, int64_t value) noexcept;
        int32_t clear_context_value(uint32_t key_id) noexcept;
        int32_t serialize_context_values(
            std::vector<uint8_t> &bytes_out) const noexcept;
        int32_t restore_context_values(const uint8_t *bytes,
            uint32_t byte_count) noexcept;
        int32_t get_registered_context_keys(
            std::vector<uint32_t> &keys_out) const noexcept;
        int32_t get_registered_effect_count(uint32_t *count) const noexcept;
        int32_t get_manifest_hash(uint64_t *hash) const noexcept;
};

#endif
