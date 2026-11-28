#include "../test_internal.hpp"
#include "../../Modules/Game/game_server.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/CMA/CMA.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
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

struct game_server_run_args
{
    game_server *server_pointer;
    int iterations;
};

static void *game_server_run_task(void *argument)
{
    game_server_run_args *arguments = static_cast<game_server_run_args *>(argument);
    if (arguments == ft_nullptr || arguments->server_pointer == ft_nullptr)
        return (ft_nullptr);
    for (int index = 0; index < arguments->iterations; ++index)
        arguments->server_pointer->run_once();
    return (ft_nullptr);
}

FT_TEST(test_game_server_thread_safety)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_server server;
    pthread_t threads[4];
    game_server_run_args arguments[4];
    int thread_index = 0;
    int create_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.enable_thread_safety());
    while (thread_index < 4)
    {
        arguments[thread_index].server_pointer = &server;
        arguments[thread_index].iterations = 100;
        create_result = pt_thread_create(&threads[thread_index], ft_nullptr,
            game_server_run_task, &arguments[thread_index]);
        FT_ASSERT_EQ(0, create_result);
        thread_index += 1;
    }

    thread_index = 0;
    while (thread_index < 4)
    {
        FT_ASSERT_EQ(0, pt_thread_join(threads[thread_index], ft_nullptr));
        thread_index += 1;
    }
    return (1);
}

FT_TEST(test_game_server_move_transfers_thread_safety_ownership)
{
    ft_sharedptr<game_world> world_pointer(new (std::nothrow) game_world());
    game_server source;
    game_server destination;

    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    return (1);
}

FT_TEST(test_game_server_handle_event_message_schedules_initialized_event)
{
    ft_sharedptr<game_world> world_pointer(new (std::nothrow) game_world());
    game_server server;
    ft_string message;
    ft_vector<ft_sharedptr<game_event> > events;

    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message.initialize("{ \"event\": { \"id\": 42, \"duration\": 9 } }"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.handle_message_locked(-1, message));
    world_pointer->get_event_scheduler()->dump_events(events);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), events.size());
    FT_ASSERT_EQ(42, events[0]->get_id());
    FT_ASSERT_EQ(9, events[0]->get_duration());
    return (1);
}

FT_TEST(test_game_server_move_transfers_resources_without_allocating)
{
    ft_sharedptr<game_world> world_pointer(new (std::nothrow) game_world());
    game_server source;
    game_server destination;
    int32_t move_result;

    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(world_pointer, "test-token"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.join_client_locked(5, -1));
    const auto *client_storage = source._clients.find(5);
    const auto *transport = source._server;
    const char *token_storage = source._auth_token.c_str();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1));
    move_result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, move_result);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT(destination._server == transport);
    FT_ASSERT(destination._world.get() == world_pointer.get());
    FT_ASSERT(destination._auth_token.c_str() == token_storage);
    FT_ASSERT(destination._clients.find(5) == client_storage);
    FT_ASSERT(source._server == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    return (1);
}

FT_TEST(test_game_server_auth_allocation_failure_allows_retry)
{
    game_server server;
    ft_sharedptr<game_world> world;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = server.initialize(world, "test-token");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, server._initialised_state);
    FT_ASSERT(server._server == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(world, "test-token"));
    FT_ASSERT_STR_EQ("test-token", server._auth_token.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_FALSE, server._auth_token.is_initialised());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, server._clients.is_initialised());
    return (1);
}

FT_TEST(test_game_server_move_preserves_locked_destination)
{
    game_server source;
    game_server destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    ft_bool acquired = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.lock_internal(&acquired));
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.unlock_internal(acquired));
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, destination._initialised_state);
    return (1);
}

FT_TEST(test_game_server_move_source_destroy_failure_rolls_back_destination)
{
    ft_sharedptr<game_world> world_pointer(new (std::nothrow) game_world());
    game_server source;
    game_server destination;
    bool lock_acquired;
    int32_t move_result;

    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(world_pointer, "move_source_destroy_failure"));
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
