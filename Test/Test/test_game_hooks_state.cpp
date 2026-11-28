#include "../test_internal.hpp"
#include "../../Modules/Game/game_state.hpp"
#include "../../Modules/Game/game_hooks.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/Game/game_event.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <cstdint>
#include <atomic>
#include <chrono>
#include <new>
#include <thread>

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
#ifndef LIBFT_TEST_BUILD
#endif

static void destroy_hooks_handle(ft_sharedptr<game_hooks> *hooks)
{
    if (hooks == ft_nullptr)
        return ;
    hooks->destroy();
    delete hooks;
    return ;
}

FT_TEST(test_game_state_dispatches_registered_hooks)
{
    game_state state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    ft_sharedptr<game_hooks> hooks(new (std::nothrow) game_hooks());
    ft_sharedptr<game_hooks> *stored_hooks;
    game_character character;
    game_item item;
    game_world world;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    game_event event;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.initialize());
    int crafted_invocations;
    int damaged_invocations;
    int event_invocations;

    FT_ASSERT(static_cast<bool>(hooks));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.initialize());
    crafted_invocations = 0;
    damaged_invocations = 0;
    event_invocations = 0;
    hooks->set_on_item_crafted(ft_function<void(game_character&, game_item&)>([&crafted_invocations](game_character &character_ref, game_item &item_ref)
    {
        (void)character_ref;
        (void)item_ref;
        crafted_invocations = crafted_invocations + 1;
        return ;
    }));
    hooks->set_on_character_damaged(ft_function<void(game_character&, int, uint8_t)>([&damaged_invocations](game_character &character_ref, int damage, uint8_t type)
    {
        (void)character_ref;
        (void)damage;
        (void)type;
        damaged_invocations = damaged_invocations + 1;
        return ;
    }));
    hooks->set_on_event_triggered(ft_function<void(game_world&, game_event&)>([&event_invocations](game_world &world_ref, game_event &event_ref)
    {
        (void)world_ref;
        (void)event_ref;
        event_invocations = event_invocations + 1;
        return ;
    }));
    state.set_hooks(hooks);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    stored_hooks = state.get_hooks();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT(stored_hooks != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored_hooks->get_error());
    FT_ASSERT(static_cast<bool>(*stored_hooks));
    state.dispatch_item_crafted(character, item);
    state.dispatch_character_damaged(character, 9, static_cast<uint8_t>(1));
    state.dispatch_event_triggered(world, event);
    FT_ASSERT_EQ(1, crafted_invocations);
    FT_ASSERT_EQ(1, damaged_invocations);
    FT_ASSERT_EQ(1, event_invocations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.get_error());
    destroy_hooks_handle(stored_hooks);
    return (1);
}

FT_TEST(test_game_state_reset_hooks_clears_callbacks)
{
    game_state state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    ft_sharedptr<game_hooks> hooks(new (std::nothrow) game_hooks());
    ft_sharedptr<game_hooks> *stored_hooks;
    game_character character;
    game_item item;
    game_world world;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    game_event event;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.initialize());
    int crafted_invocations;
    int damaged_invocations;
    int event_invocations;

    FT_ASSERT(static_cast<bool>(hooks));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.initialize());
    crafted_invocations = 0;
    damaged_invocations = 0;
    event_invocations = 0;
    hooks->set_on_item_crafted(ft_function<void(game_character&, game_item&)>([&crafted_invocations](game_character &character_ref, game_item &item_ref)
    {
        (void)character_ref;
        (void)item_ref;
        crafted_invocations = crafted_invocations + 1;
        return ;
    }));
    hooks->set_on_character_damaged(ft_function<void(game_character&, int, uint8_t)>([&damaged_invocations](game_character &character_ref, int damage, uint8_t type)
    {
        (void)character_ref;
        (void)damage;
        (void)type;
        damaged_invocations = damaged_invocations + 1;
        return ;
    }));
    hooks->set_on_event_triggered(ft_function<void(game_world&, game_event&)>([&event_invocations](game_world &world_ref, game_event &event_ref)
    {
        (void)world_ref;
        (void)event_ref;
        event_invocations = event_invocations + 1;
        return ;
    }));
    state.set_hooks(hooks);
    stored_hooks = state.get_hooks();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT(stored_hooks != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored_hooks->get_error());
    FT_ASSERT(static_cast<bool>(*stored_hooks));
    state.dispatch_item_crafted(character, item);
    state.dispatch_character_damaged(character, 12, static_cast<uint8_t>(4));
    state.dispatch_event_triggered(world, event);
    FT_ASSERT_EQ(1, crafted_invocations);
    FT_ASSERT_EQ(1, damaged_invocations);
    FT_ASSERT_EQ(1, event_invocations);
    state.reset_hooks();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    crafted_invocations = 0;
    damaged_invocations = 0;
    event_invocations = 0;
    state.dispatch_item_crafted(character, item);
    state.dispatch_character_damaged(character, 6, static_cast<uint8_t>(3));
    state.dispatch_event_triggered(world, event);
    FT_ASSERT_EQ(0, crafted_invocations);
    FT_ASSERT_EQ(0, damaged_invocations);
    FT_ASSERT_EQ(0, event_invocations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.get_error());
    destroy_hooks_handle(stored_hooks);
    return (1);
}

FT_TEST(test_game_hooks_move_source_destroy_failure_rolls_back_destination)
{
    game_hooks source;
    game_hooks destination;
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

FT_TEST(test_game_hooks_move_preserves_locked_destination)
{
    game_hooks source;
    game_hooks destination;
    ft_bool acquired = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.lock(&acquired));
    FT_ASSERT_EQ(FT_TRUE, acquired);
    const int32_t result = destination.move(source);
    destination.unlock(acquired);
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, destination._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    return (1);
}

FT_TEST(test_game_state_dispatch_releases_lock_before_running_hooks)
{
    game_state state;
    game_character character;
    ft_sharedptr<game_hooks> hooks(new game_hooks());
    ft_sharedptr<game_hooks> empty;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks->initialize());
    std::atomic<ft_bool> entered(FT_FALSE);
    std::atomic<ft_bool> cleared(FT_FALSE);
    std::atomic<ft_bool> finished(FT_FALSE);
    ft_bool cleared_during_callback = FT_FALSE;
    uint32_t calls = 0U;
    hooks->set_on_character_damaged([&](game_character &, int32_t, uint8_t)
    {
        entered.store(FT_TRUE);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!cleared.load() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        cleared_during_callback = cleared.load();
        ++calls;
    });
    state.set_hooks(hooks);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.destroy());
    std::thread clearer([&]()
    {
        while (!entered.load() && !finished.load())
            std::this_thread::yield();
        if (entered.load())
        {
            state.set_hooks(empty);
            cleared.store(FT_TRUE);
        }
    });
    state.dispatch_character_damaged(character, 1, 0);
    finished.store(FT_TRUE);
    clearer.join();
    FT_ASSERT_EQ(FT_TRUE, cleared_during_callback);
    FT_ASSERT_EQ(1U, calls);
    state.dispatch_character_damaged(character, 1, 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.get_error());
    FT_ASSERT_EQ(1U, calls);
    return (1);
}
