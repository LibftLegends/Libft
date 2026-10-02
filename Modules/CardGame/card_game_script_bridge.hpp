#ifndef CARD_GAME_SCRIPT_BRIDGE_HPP
# define CARD_GAME_SCRIPT_BRIDGE_HPP

# include <cstdint>
# include "card_game.hpp"
# include "../Scripting/scripting.hpp"

static const uint32_t FT_CARD_GAME_SCRIPT_MAX_EFFECTS = 64U;

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

        uint8_t _initialised_state;
        card_game_engine *_engine;
        scripting_engine _scripting;
        script_effect_binding _bindings[FT_CARD_GAME_SCRIPT_MAX_EFFECTS];
        uint32_t _binding_count;
        card_game_operation_buffer *_active_operations;
        uint32_t _operation_limit;

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
        int32_t initialize_runtime() noexcept;

    public:
        card_game_script_bridge() noexcept;
        ~card_game_script_bridge() noexcept;

        int32_t initialize(card_game_engine &engine) noexcept;
        int32_t destroy() noexcept;
        int32_t register_script_effect(const char *source,
            uint32_t event_type, uint32_t *effect_id) noexcept;
        int32_t set_operation_limit(uint32_t operation_limit) noexcept;
        int32_t get_registered_effect_count(uint32_t *count) const noexcept;
        int32_t get_manifest_hash(uint64_t *hash) const noexcept;
};

#endif
