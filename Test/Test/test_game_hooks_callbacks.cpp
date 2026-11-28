#include "../test_internal.hpp"
#include "../../Modules/Game/game_hooks.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/Game/game_event.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include <cstdint>
#include <cstdlib>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/Game/game_buff.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_currency_rate.hpp"
#include "../../Modules/Game/game_debuff.hpp"
#include "../../Modules/Game/game_dialogue_line.hpp"
#include "../../Modules/Game/game_dialogue_script.hpp"
#include "../../Modules/Game/game_dialogue_table.hpp"
#include "../../Modules/Game/game_economy_table.hpp"
#include "../../Modules/Game/game_pathfinding.hpp"
#include "../../Modules/Game/game_price_definition.hpp"
#include "../../Modules/Game/game_quest.hpp"
#include "../../Modules/Game/game_rarity_band.hpp"
#include "../../Modules/Game/game_region_definition.hpp"
#include "../../Modules/Game/game_skill.hpp"
#include "../../Modules/Game/game_upgrade.hpp"
#include "../../Modules/Game/game_vendor_profile.hpp"
#include "../../Modules/Game/game_world_region.hpp"
#include "../../Modules/Game/game_world_registry.hpp"
#include "../../Modules/Game/game_world_replay.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int32_t initialize_hook_metadata(ft_game_hook_metadata &metadata,
    const char *hook_identifier, const char *listener_name,
    const char *description, const char *argument_contract)
{
    int32_t error_code;

    error_code = metadata.hook_identifier.initialize(hook_identifier);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = metadata.listener_name.initialize(listener_name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = metadata.description.initialize(description);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = metadata.argument_contract.initialize(argument_contract);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_game_hooks_builtin_dispatch_does_not_allocate_lookup_keys)
{
    game_hooks hooks;
    game_character character;
    game_item item;
    game_world world;
    game_event event;
    uint32_t crafted = 0U;
    int32_t damage_total = 0;
    uint32_t triggered = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    hooks.set_on_item_crafted([&crafted](game_character &, game_item &) { ++crafted; });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    hooks.set_on_character_damaged([&damage_total](game_character &, int32_t damage, uint8_t type)
    {
        damage_total += damage + type;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    hooks.set_on_event_triggered([&triggered](game_world &, game_event &) { ++triggered; });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    hooks.invoke_on_item_crafted(character, item);
    hooks.invoke_on_character_damaged(character, 7, 2);
    hooks.invoke_on_event_triggered(world, event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(1U, crafted);
    FT_ASSERT_EQ(9, damage_total);
    FT_ASSERT_EQ(1U, triggered);
    return (1);
}

FT_TEST(test_game_hooks_bulk_copy_preserves_independent_registries)
{
    game_hooks source;
    game_hooks copy;
    int32_t last_invocation_count = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    char hook_identifier[] = "hook-a";
    while (hook_identifier[5] <= 'j')
    {
        ft_game_hook_metadata metadata;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(metadata,
            hook_identifier, "listener", "Description", "No arguments"));
        source.register_listener(metadata, 10,
            [&last_invocation_count, count = 0](ft_game_hook_context &) mutable
            {
                last_invocation_count = ++count;
            });
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
        ++hook_identifier[5];
    }
    ft_string first_key;
    ft_string listener_name;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_key.initialize("hook-a"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, listener_name.initialize("listener"));
    ft_game_hook_context context = {};
    source.invoke_hook(first_key, context);
    FT_ASSERT_EQ(1, last_invocation_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    source.invoke_hook(first_key, context);
    FT_ASSERT_EQ(2, last_invocation_count);
    copy.invoke_hook(first_key, context);
    FT_ASSERT_EQ(2, last_invocation_count);
    source.unregister_listener(first_key, listener_name);
    copy.invoke_hook(first_key, context);
    FT_ASSERT_EQ(3, last_invocation_count);
    ft_vector<ft_game_hook_metadata> *snapshot = copy.get_catalog_metadata();
    FT_ASSERT(snapshot != ft_nullptr);
    const ft_size_t metadata_count = snapshot->size();
    const ft_bool first_key_matches = (*snapshot)[0].hook_identifier == first_key;
    delete snapshot;
    FT_ASSERT_EQ(10U, metadata_count);
    FT_ASSERT_EQ(FT_TRUE, first_key_matches);
    return (1);
}

FT_TEST(test_game_hooks_keeps_active_callbacks_alive_when_mutated)
{
    game_hooks hooks;
    ft_game_hook_metadata first;
    ft_game_hook_metadata second;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(first,
        "mutable.hook", "first", "First listener", "No arguments"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(second,
        "mutable.hook", "second", "Second listener", "No arguments"));
    hooks.register_listener(first, 10,
        [&hooks, &first, &second, payload = 1](ft_game_hook_context &context)
        {
            hooks.unregister_listener(first.hook_identifier, first.listener_name);
            hooks.unregister_listener(second.hook_identifier, second.listener_name);
            context.integer_payload = payload;
        });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    hooks.register_listener(second, 0, [](ft_game_hook_context &context)
    {
        context.integer_payload = context.integer_payload * 10 + 2;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    ft_game_hook_context context = {};
    hooks.invoke_hook(first.hook_identifier, context);
    FT_ASSERT_EQ(12, context.integer_payload);
    context.integer_payload = 0;
    hooks.invoke_hook(first.hook_identifier, context);
    FT_ASSERT_EQ(0, context.integer_payload);

    hooks.register_listener(first, 0, [&hooks, payload = 42](ft_game_hook_context &context)
    {
        hooks.reset();
        context.integer_payload = payload;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    hooks.invoke_hook(first.hook_identifier, context);
    FT_ASSERT_EQ(42, context.integer_payload);
    context.integer_payload = 0;
    hooks.invoke_hook(first.hook_identifier, context);
    FT_ASSERT_EQ(0, context.integer_payload);
    return (1);
}

FT_TEST(test_game_hooks_snapshot_failure_does_not_partially_dispatch)
{
    game_hooks hooks;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.enable_thread_safety());
    char listener_name[] = "listener-a";
    while (listener_name[9] <= 'j')
    {
        ft_game_hook_metadata metadata;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(metadata,
            "large.hook", listener_name, "Listener", "No arguments"));
        hooks.register_listener(metadata, 0, [](ft_game_hook_context &context)
        {
            ++context.integer_payload;
        });
        FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
        ++listener_name[9];
    }
    ft_string identifier;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, identifier.initialize("large.hook"));
    ft_game_hook_context context = {};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    hooks.invoke_hook(identifier, context);
    const int32_t result = hooks.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(0, context.integer_payload);
    hooks.invoke_hook(identifier, context);
    FT_ASSERT_EQ(10, context.integer_payload);
    return (1);
}

FT_TEST(test_game_hooks_move_transfers_inline_and_heap_catalogs_without_allocating)
{
    const ft_size_t listener_counts[] = {3U, 12U};
    ft_size_t scenario = 0U;
    while (scenario < 2U)
    {
        game_hooks source;
        game_hooks destination;
        game_character character;
        uint32_t damage_callbacks = 0U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
        source.set_on_character_damaged(
            [&damage_callbacks](game_character &, int32_t, uint8_t) { ++damage_callbacks; });
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
        char listener_name[] = "listener-a";
        ft_size_t index = 0U;
        while (index < listener_counts[scenario])
        {
            ft_game_hook_metadata metadata;
            FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(metadata,
                "move.hook", listener_name, "Description", "No arguments"));
            source.register_listener(metadata, 0, [](ft_game_hook_context &context)
            {
                ++context.integer_payload;
            });
            FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
            ++listener_name[9];
            ++index;
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
        const int32_t result = destination.move(source);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
        FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());

        ft_string identifier;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, identifier.initialize("move.hook"));
        ft_game_hook_context context = {};
        destination.invoke_hook(identifier, context);
        FT_ASSERT_EQ(listener_counts[scenario], static_cast<ft_size_t>(context.integer_payload));
        destination.invoke_on_character_damaged(character, 1, 0);
        FT_ASSERT_EQ(1U, damage_callbacks);
        auto *catalog = destination.get_catalog_metadata();
        FT_ASSERT(catalog != ft_nullptr);
        const ft_size_t metadata_count = catalog->size();
        delete catalog;
        FT_ASSERT_EQ(listener_counts[scenario] + 1U, metadata_count);
        ++scenario;
    }
    return (1);
}

struct game_hooks_test_backend_state
{
    void    *allocations[64];
    ft_size_t    sizes[64];
    ft_size_t    allocation_slots;
    ft_size_t    allocation_attempt_count;
    ft_size_t    fail_on_attempt_count;
};

static void game_hooks_test_backend_state_initialize(
    game_hooks_test_backend_state *state)
{
    ft_size_t index;

    index = 0;
    while (index < static_cast<ft_size_t>(64))
    {
        state->allocations[index] = ft_nullptr;
        state->sizes[index] = 0;
        index += 1;
    }
    state->allocation_slots = 0;
    state->allocation_attempt_count = 0;
    state->fail_on_attempt_count = 0;
    return ;
}

static int32_t game_hooks_test_backend_find_slot(
    const game_hooks_test_backend_state *state, const void *memory_pointer)
{
    ft_size_t index;

    if (memory_pointer == ft_nullptr)
        return (-1);
    index = 0;
    while (index < state->allocation_slots)
    {
        if (state->allocations[index] == memory_pointer)
            return (static_cast<int32_t>(index));
        index += 1;
    }
    return (-1);
}

static void *game_hooks_test_backend_allocate(ft_size_t size, void *user_data)
{
    game_hooks_test_backend_state *state;
    void *memory_pointer;

    state = static_cast<game_hooks_test_backend_state *>(user_data);
    state->allocation_attempt_count += 1;
    if (state->allocation_attempt_count == state->fail_on_attempt_count)
        return (ft_nullptr);
    memory_pointer = std::malloc(size);
    if (memory_pointer == ft_nullptr)
        return (ft_nullptr);
    if (state->allocation_slots >= static_cast<ft_size_t>(64))
    {
        std::free(memory_pointer);
        return (ft_nullptr);
    }
    state->allocations[state->allocation_slots] = memory_pointer;
    state->sizes[state->allocation_slots] = size;
    state->allocation_slots += 1;
    return (memory_pointer);
}

static void *game_hooks_test_backend_reallocate(void *memory_pointer,
    ft_size_t size, void *user_data)
{
    game_hooks_test_backend_state *state;
    int32_t slot_index;
    void *new_memory_pointer;

    state = static_cast<game_hooks_test_backend_state *>(user_data);
    if (memory_pointer == ft_nullptr)
        return (game_hooks_test_backend_allocate(size, user_data));
    if (size == 0)
    {
        slot_index = game_hooks_test_backend_find_slot(state, memory_pointer);
        if (slot_index >= 0)
        {
            state->allocations[slot_index]
                = state->allocations[state->allocation_slots - 1];
            state->sizes[slot_index] = state->sizes[state->allocation_slots - 1];
            state->allocations[state->allocation_slots - 1] = ft_nullptr;
            state->sizes[state->allocation_slots - 1] = 0;
            state->allocation_slots -= 1;
        }
        std::free(memory_pointer);
        return (ft_nullptr);
    }
    slot_index = game_hooks_test_backend_find_slot(state, memory_pointer);
    new_memory_pointer = std::realloc(memory_pointer, size);
    if (new_memory_pointer == ft_nullptr)
        return (ft_nullptr);
    if (slot_index >= 0)
    {
        state->allocations[slot_index] = new_memory_pointer;
        state->sizes[slot_index] = size;
    }
    return (new_memory_pointer);
}

static void game_hooks_test_backend_deallocate(void *memory_pointer,
    void *user_data)
{
    game_hooks_test_backend_state *state;
    int32_t slot_index;

    state = static_cast<game_hooks_test_backend_state *>(user_data);
    if (memory_pointer == ft_nullptr)
        return ;
    slot_index = game_hooks_test_backend_find_slot(state, memory_pointer);
    if (slot_index >= 0)
    {
        state->allocations[slot_index]
            = state->allocations[state->allocation_slots - 1];
        state->sizes[slot_index] = state->sizes[state->allocation_slots - 1];
        state->allocations[state->allocation_slots - 1] = ft_nullptr;
        state->sizes[state->allocation_slots - 1] = 0;
        state->allocation_slots -= 1;
    }
    std::free(memory_pointer);
    return ;
}

static ft_size_t game_hooks_test_backend_get_allocation_size(
    const void *memory_pointer, void *user_data)
{
    game_hooks_test_backend_state *state;
    int32_t slot_index;

    state = static_cast<game_hooks_test_backend_state *>(user_data);
    slot_index = game_hooks_test_backend_find_slot(state, memory_pointer);
    if (slot_index < 0)
        return (0);
    return (state->sizes[slot_index]);
}

static ft_bool game_hooks_test_backend_owns_allocation(
    const void *memory_pointer, void *user_data)
{
    game_hooks_test_backend_state *state;

    state = static_cast<game_hooks_test_backend_state *>(user_data);
    if (game_hooks_test_backend_find_slot(state, memory_pointer) < 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

FT_TEST(test_game_hooks_invoke_callbacks)
{
    game_hooks hooks;
    game_character character;
    game_item item;
    game_world world;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    game_event event;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.initialize());
    int crafted_invocations;
    int damaged_invocations;
    int event_invocations;
    int invocation_counter;
    int high_priority_step;
    int legacy_step;
    int low_priority_step;

    crafted_invocations = 0;
    damaged_invocations = 0;
    event_invocations = 0;
    invocation_counter = 0;
    high_priority_step = 0;
    legacy_step = 0;
    low_priority_step = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.initialize());
    hooks.set_on_item_crafted(ft_function<void(game_character&, game_item&)>([&crafted_invocations, &invocation_counter, &legacy_step](game_character &character_ref, game_item &item_ref)
    {
        (void)character_ref;
        (void)item_ref;
        crafted_invocations = crafted_invocations + 1;
        invocation_counter = invocation_counter + 1;
        legacy_step = invocation_counter;
        return ;
    }));
    hooks.set_on_character_damaged(ft_function<void(game_character&, int, uint8_t)>([&damaged_invocations](game_character &character_ref, int damage, uint8_t type)
    {
        (void)character_ref;
        (void)damage;
        (void)type;
        damaged_invocations = damaged_invocations + 1;
        return ;
    }));
    hooks.set_on_event_triggered(ft_function<void(game_world&, game_event&)>([&event_invocations](game_world &world_ref, game_event &event_ref)
    {
        (void)world_ref;
        (void)event_ref;
        event_invocations = event_invocations + 1;
        return ;
    }));
    ft_game_hook_metadata high_metadata;
    ft_game_hook_metadata low_metadata;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(high_metadata,
        ft_game_hook_item_crafted_identifier, "test.high",
        "High priority test listener", "game_character&,game_item&"));
    hooks.register_listener(high_metadata, 1500, ft_game_hook_make_character_item_adapter(ft_function<void(game_character&, game_item&)>([&invocation_counter, &high_priority_step](game_character &character_ref, game_item &item_ref)
    {
        (void)character_ref;
        (void)item_ref;
        invocation_counter = invocation_counter + 1;
        high_priority_step = invocation_counter;
        return ;
    })));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(low_metadata,
        ft_game_hook_item_crafted_identifier, "test.low",
        "Low priority test listener", "game_character&,game_item&"));
    hooks.register_listener(low_metadata, 500, ft_game_hook_make_character_item_adapter(ft_function<void(game_character&, game_item&)>([&invocation_counter, &low_priority_step](game_character &character_ref, game_item &item_ref)
    {
        (void)character_ref;
        (void)item_ref;
        invocation_counter = invocation_counter + 1;
        low_priority_step = invocation_counter;
        return ;
    })));
    hooks.invoke_on_item_crafted(character, item);
    hooks.invoke_on_character_damaged(character, 5, static_cast<uint8_t>(2));
    hooks.invoke_on_event_triggered(world, event);
    FT_ASSERT_EQ(1, crafted_invocations);
    FT_ASSERT_EQ(1, damaged_invocations);
    FT_ASSERT_EQ(1, event_invocations);
    FT_ASSERT_EQ(1, high_priority_step);
    FT_ASSERT_EQ(2, legacy_step);
    FT_ASSERT_EQ(3, low_priority_step);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    return (1);
}

FT_TEST(test_game_hooks_legacy_callback_setter_failure_preserves_previous_callback)
{
    game_hooks hooks;
    game_character character;
    game_item item;
    int old_invocations;
    int new_invocations;
    ft_function<void(game_character&, game_item&)> callback_after_failure;
    game_hooks_test_backend_state backend_state;
    cma_backend_hooks backend_hooks;
    int32_t setter_error;

    old_invocations = 0;
    new_invocations = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.initialize());
    hooks.set_on_item_crafted(ft_function<void(game_character&, game_item&)>(
        [&old_invocations](game_character &character_ref, game_item &item_ref)
        {
            (void)character_ref;
            (void)item_ref;
            old_invocations = old_invocations + 1;
            return ;
        }));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    callback_after_failure = hooks.get_on_item_crafted();
    callback_after_failure(character, item);
    FT_ASSERT_EQ(1, old_invocations);
    FT_ASSERT_EQ(0, new_invocations);
    game_hooks_test_backend_state_initialize(&backend_state);
    backend_state.fail_on_attempt_count = static_cast<ft_size_t>(2);
    backend_hooks.allocate = &game_hooks_test_backend_allocate;
    backend_hooks.reallocate = &game_hooks_test_backend_reallocate;
    backend_hooks.deallocate = &game_hooks_test_backend_deallocate;
    backend_hooks.aligned_allocate = ft_nullptr;
    backend_hooks.get_allocation_size = &game_hooks_test_backend_get_allocation_size;
    backend_hooks.owns_allocation = &game_hooks_test_backend_owns_allocation;
    backend_hooks.user_data = &backend_state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_backend(&backend_hooks));
    hooks.set_on_item_crafted(ft_function<void(game_character&, game_item&)>(
        [&new_invocations](game_character &character_ref, game_item &item_ref)
        {
            (void)character_ref;
            (void)item_ref;
            new_invocations = new_invocations + 1;
            return ;
        }));
    setter_error = hooks.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_clear_backend());
    FT_ASSERT_NE(FT_ERR_SUCCESS, setter_error);
    callback_after_failure = hooks.get_on_item_crafted();
    callback_after_failure(character, item);
    FT_ASSERT_EQ(2, old_invocations);
    FT_ASSERT_EQ(0, new_invocations);
    hooks.invoke_on_item_crafted(character, item);
    FT_ASSERT_EQ(3, old_invocations);
    FT_ASSERT_EQ(0, new_invocations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    hooks.set_on_item_crafted([&new_invocations](game_character &, game_item &)
    {
        ++new_invocations;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    hooks.invoke_on_item_crafted(character, item);
    callback_after_failure = hooks.get_on_item_crafted();
    callback_after_failure(character, item);
    FT_ASSERT_EQ(3, old_invocations);
    FT_ASSERT_EQ(2, new_invocations);
    return (1);
}

FT_TEST(test_game_hooks_register_listener_allocation_failure_rolls_back)
{
    game_hooks hooks;
    ft_game_hook_metadata metadata;
    game_character character;
    game_item item;
    int crafted_invocations;
    int32_t register_result;
    int32_t destroy_result;
    ft_vector<ft_game_hook_metadata> *metadata_copy;
    game_hooks_test_backend_state backend_state;
    cma_backend_hooks backend_hooks;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(metadata,
        ft_game_hook_item_crafted_identifier, "test.failure",
        "Allocation failure rollback test", "game_character&,game_item&"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0));
    destroy_result = hooks.destroy();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destroy_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    crafted_invocations = 0;
    game_hooks_test_backend_state_initialize(&backend_state);
    backend_state.fail_on_attempt_count = static_cast<ft_size_t>(4);
    backend_hooks.allocate = &game_hooks_test_backend_allocate;
    backend_hooks.reallocate = &game_hooks_test_backend_reallocate;
    backend_hooks.deallocate = &game_hooks_test_backend_deallocate;
    backend_hooks.aligned_allocate = ft_nullptr;
    backend_hooks.get_allocation_size = &game_hooks_test_backend_get_allocation_size;
    backend_hooks.owns_allocation = &game_hooks_test_backend_owns_allocation;
    backend_hooks.user_data = &backend_state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_backend(&backend_hooks));
    hooks.register_listener(metadata, 1000,
        ft_game_hook_make_character_item_adapter(ft_function<void(game_character&, game_item&)>(
            [&crafted_invocations](game_character &character_ref, game_item &item_ref)
            {
                (void)character_ref;
                (void)item_ref;
                crafted_invocations = crafted_invocations + 1;
                return ;
            })));
    register_result = hooks.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_clear_backend());
    FT_ASSERT_NE(FT_ERR_SUCCESS, register_result);
    metadata_copy = hooks.get_catalog_metadata();
    FT_ASSERT(metadata_copy != ft_nullptr);
    if (metadata_copy != ft_nullptr)
    {
        FT_ASSERT_EQ(static_cast<ft_size_t>(0), metadata_copy->size());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, metadata_copy->destroy());
        delete metadata_copy;
    }
    hooks.invoke_on_item_crafted(character, item);
    FT_ASSERT_EQ(0, crafted_invocations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    return (1);
}

FT_TEST(test_game_hooks_failed_priority_insert_preserves_existing_listener)
{
    game_hooks hooks;
    ft_game_hook_metadata lower_priority;
    ft_game_hook_metadata higher_priority;
    ft_game_hook_context context = {};
    int32_t invocation_order = 0;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(lower_priority,
        "priority.hook", "lower", "Lower priority listener", "No arguments"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(higher_priority,
        "priority.hook", "higher", "Higher priority listener", "No arguments"));
    hooks.register_listener(lower_priority, 1,
        [&invocation_order](ft_game_hook_context &)
        {
            invocation_order = invocation_order * 10 + 1;
        });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());

    game_hooks_test_backend_state backend_state;
    game_hooks_test_backend_state_initialize(&backend_state);
    // Fail while appending the catalog metadata, after staging the listener.
    backend_state.fail_on_attempt_count = 5U;
    cma_backend_hooks backend_hooks = {};
    backend_hooks.allocate = &game_hooks_test_backend_allocate;
    backend_hooks.reallocate = &game_hooks_test_backend_reallocate;
    backend_hooks.deallocate = &game_hooks_test_backend_deallocate;
    backend_hooks.get_allocation_size = &game_hooks_test_backend_get_allocation_size;
    backend_hooks.owns_allocation = &game_hooks_test_backend_owns_allocation;
    backend_hooks.user_data = &backend_state;
    const auto higher_callback = [&invocation_order](ft_game_hook_context &)
    {
        invocation_order = invocation_order * 10 + 2;
    };
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_backend(&backend_hooks));
    hooks.register_listener(higher_priority, 2, higher_callback);
    const int32_t register_result = hooks.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_clear_backend());
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, register_result);
    hooks.invoke_hook(lower_priority.hook_identifier, context);
    FT_ASSERT_EQ(1, invocation_order);

    invocation_order = 0;
    hooks.register_listener(higher_priority, 2, higher_callback);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    hooks.invoke_hook(lower_priority.hook_identifier, context);
    FT_ASSERT_EQ(21, invocation_order);

    invocation_order = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    hooks.unregister_listener(higher_priority.hook_identifier, higher_priority.listener_name);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    hooks.invoke_hook(lower_priority.hook_identifier, context);
    FT_ASSERT_EQ(1, invocation_order);
    ft_vector<ft_game_hook_metadata> *catalog = hooks.get_catalog_metadata();
    FT_ASSERT(catalog != ft_nullptr);
    const ft_size_t catalog_size = catalog->size();
    delete catalog;
    FT_ASSERT_EQ(1U, catalog_size);
    return (1);
}

FT_TEST(test_game_hooks_failed_replacement_keeps_callback_and_metadata)
{
    game_hooks hooks;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.initialize());
    char listener_name[] = "listener-a";
    while (listener_name[9] <= 'i')
    {
        ft_game_hook_metadata metadata;
        const char *hook_identifier = "replace.hook";
        if (listener_name[9] == 'i')
            hook_identifier = "other.hook";
        FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(metadata,
            hook_identifier, listener_name, "Original description", "No arguments"));
        const int32_t payload_digit = listener_name[9] - 'a' + 1;
        hooks.register_listener(metadata, 1, [payload_digit](ft_game_hook_context &context)
        {
            context.integer_payload = context.integer_payload * 10 + payload_digit;
        });
        FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
        ++listener_name[9];
    }
    ft_game_hook_metadata replacement;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_hook_metadata(replacement,
        "replace.hook", "listener-a", "Replacement description", "No arguments"));
    game_hooks_test_backend_state backend_state;
    game_hooks_test_backend_state_initialize(&backend_state);
    // The metadata vector has spare capacity. Fail growing the full listener
    // vector after both sets of four metadata strings have been allocated.
    backend_state.fail_on_attempt_count = 9U;
    cma_backend_hooks backend_hooks = {};
    backend_hooks.allocate = &game_hooks_test_backend_allocate;
    backend_hooks.reallocate = &game_hooks_test_backend_reallocate;
    backend_hooks.deallocate = &game_hooks_test_backend_deallocate;
    backend_hooks.get_allocation_size = &game_hooks_test_backend_get_allocation_size;
    backend_hooks.owns_allocation = &game_hooks_test_backend_owns_allocation;
    backend_hooks.user_data = &backend_state;
    const auto replacement_callback = [](ft_game_hook_context &context)
    {
        context.integer_payload = context.integer_payload * 10 + 9;
    };
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_backend(&backend_hooks));
    hooks.register_listener(replacement, 0, replacement_callback);
    const int32_t register_result = hooks.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_clear_backend());
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, register_result);
    ft_game_hook_context context = {};
    hooks.invoke_hook(replacement.hook_identifier, context);
    FT_ASSERT_EQ(12345678, context.integer_payload);
    ft_vector<ft_game_hook_metadata> *catalog = hooks.get_catalog_metadata();
    FT_ASSERT(catalog != ft_nullptr);
    const ft_size_t old_count = catalog->size();
    const ft_bool old_description = (*catalog)[0].description == "Original description";
    delete catalog;
    FT_ASSERT_EQ(9U, old_count);
    FT_ASSERT_EQ(FT_TRUE, old_description);

    hooks.register_listener(replacement, 0, replacement_callback);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.get_error());
    context.integer_payload = 0;
    hooks.invoke_hook(replacement.hook_identifier, context);
    FT_ASSERT_EQ(23456789, context.integer_payload);
    catalog = hooks.get_catalog_metadata();
    FT_ASSERT(catalog != ft_nullptr);
    const ft_size_t new_count = catalog->size();
    const ft_bool new_description = (*catalog)[8].description == "Replacement description";
    delete catalog;
    FT_ASSERT_EQ(9U, new_count);
    FT_ASSERT_EQ(FT_TRUE, new_description);
    return (1);
}
