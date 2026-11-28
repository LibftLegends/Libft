#include "../test_internal.hpp"
#include "../../Modules/Game/game_state.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"

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
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_game_state_adds_valid_character)
{
    game_state state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    ft_sharedptr<game_character> hero(new (std::nothrow) game_character());
    ft_vector<ft_sharedptr<game_character> > *characters;
    int result;

    FT_ASSERT(static_cast<bool>(hero));
    result = state.add_character(hero);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    characters = &state.get_characters();
    FT_ASSERT_EQ(1, characters->size());
    FT_ASSERT((*characters)[0] == hero);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    return (1);
}

FT_TEST(test_game_state_rejects_null_character)
{
    game_state state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    ft_sharedptr<game_character> empty;
    int result;

    result = state.add_character(empty);
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, result);
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, state.get_error());
    FT_ASSERT_EQ(0, state.get_characters().size());
    return (1);
}

FT_TEST(test_game_state_remove_invalid_index_sets_error)
{
    game_state state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    ft_sharedptr<game_character> hero(new (std::nothrow) game_character());

    FT_ASSERT(static_cast<bool>(hero));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.add_character(hero));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    state.remove_character(5);
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, state.get_error());
    FT_ASSERT_EQ(1, state.get_characters().size());
    return (1);
}

FT_TEST(test_game_state_remove_character_shrinks_vector)
{
    game_state state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    ft_sharedptr<game_character> first(new (std::nothrow) game_character());
    ft_sharedptr<game_character> second(new (std::nothrow) game_character());

    FT_ASSERT(static_cast<bool>(first));
    FT_ASSERT(static_cast<bool>(second));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.add_character(first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.add_character(second));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    state.remove_character(0);
    FT_ASSERT_EQ(1, state.get_characters().size());
    FT_ASSERT(state.get_characters()[0] == second);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    return (1);
}

FT_TEST(test_game_state_thread_safety_toggle)
{
    game_state state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());

    FT_ASSERT_EQ(false, state.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(true, state.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.disable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(false, state.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    return (1);
}

FT_TEST(test_game_state_move_transfers_heap_collections_without_allocating)
{
    game_state source;
    game_state destination;
    ft_string key;
    ft_string value;
    ft_size_t world_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, key.initialize("mode"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize("survival"));
    source.set_variable(key, value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    world_index = 0;
    while (world_index < 8)
    {
        ft_sharedptr<game_world> world_pointer(new (std::nothrow) game_world());

        FT_ASSERT(world_pointer.get() != ft_nullptr);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
        source.get_worlds().push_back(world_pointer);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_worlds().get_error());
        world_index += 1;
    }
    FT_ASSERT_EQ(static_cast<ft_size_t>(9), source.get_worlds().size());
    const auto *world_storage = source.get_worlds().begin();
    const ft_string *stored = source.get_variable(key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1));
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(9U, destination.get_worlds().size());
    FT_ASSERT(destination.get_worlds().begin() == world_storage);
    FT_ASSERT(destination.get_variable(key) == stored);
    FT_ASSERT_STR_EQ("survival", stored->c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(1U, source.get_worlds().size());
    return (1);
}

FT_TEST(test_game_state_move_staging_failure_keeps_source_intact)
{
    game_state source;
    game_state destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_characters().enable_thread_safety());
    const auto *world = source.get_worlds()[0].get();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    FT_ASSERT_EQ(1U, source.get_worlds().size());
    FT_ASSERT(source.get_worlds()[0].get() == world);
    FT_ASSERT_EQ(FT_TRUE, source.get_characters().is_thread_safe());
    return (1);
}

FT_TEST(test_game_state_move_preserves_locked_destination_collection)
{
    game_state source;
    game_state destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    auto &worlds = destination.get_worlds();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, worlds.enable_thread_safety());
    ft_bool locked = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, worlds.lock(&locked));
    const int32_t result = destination.move(source);
    worlds.unlock(locked);
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, result);
    FT_ASSERT_EQ(1U, destination.get_worlds().size());
    FT_ASSERT_EQ(1U, source.get_worlds().size());
    return (1);
}

FT_TEST(test_game_state_character_removal_reports_collection_lock_failure)
{
    game_state state;
    ft_sharedptr<game_character> first(new game_character());
    ft_sharedptr<game_character> second(new game_character());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.add_character(first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.add_character(second));
    auto &characters = state.get_characters();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, characters.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    state.remove_character(0U);
    const int32_t result = state.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(2U, characters.size());
    FT_ASSERT(characters[0].get() == first.get());
    state.remove_character(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(1U, characters.size());
    FT_ASSERT(characters[0].get() == second.get());
    return (1);
}

FT_TEST(test_game_state_initialize_can_retry_after_allocation_failure)
{
    game_state state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = state.initialize();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, state._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(1U, state.get_worlds().size());
    FT_ASSERT_EQ(0U, state.get_characters().size());
    return (1);
}

FT_TEST(test_game_state_add_character_allocation_failure_reports_error)
{
    game_state state;
    ft_sharedptr<game_character> characters[9];
    ft_size_t character_index;
    int32_t add_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    character_index = 0;
    while (character_index < 9)
    {
        characters[character_index] = ft_sharedptr<game_character>(new (std::nothrow) game_character());
        FT_ASSERT(characters[character_index].get() != ft_nullptr);
        character_index += 1;
    }
    character_index = 0;
    while (character_index < 8)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, state.add_character(characters[character_index]));
        character_index += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1));
    add_result = state.add_character(characters[8]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0));
    FT_ASSERT_NE(FT_ERR_SUCCESS, add_result);
    FT_ASSERT_EQ(static_cast<ft_size_t>(8), state.get_characters().size());
    return (1);
}

FT_TEST(test_game_state_set_variable_insert_failure_reports_error)
{
    game_state state;
    ft_string key;
    ft_string value;
    int32_t state_error;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, key.initialize("quest_stage_long_key"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize("active_stage_long_value"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1));
    state.set_variable(key, value);
    state_error = state.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0));
    FT_ASSERT_NE(FT_ERR_SUCCESS, state_error);
    FT_ASSERT(state.get_variable(key) == ft_nullptr);
    return (1);
}

FT_TEST(test_game_state_move_source_destroy_failure_rolls_back_destination)
{
    game_state source;
    game_state destination;
    bool lock_acquired;
    int32_t move_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT(source._mutex != ft_nullptr);
    lock_acquired = false;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source._mutex->lock_state(&lock_acquired));
    FT_ASSERT_EQ(true, lock_acquired);
    move_result = destination.move(source);
    FT_ASSERT_NE(FT_ERR_SUCCESS, move_result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());
    source._mutex->unlock_state(lock_acquired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.disable_thread_safety());
    return (1);
}
