#include "../test_internal.hpp"
#include "../../Modules/Game/game_event_scheduler.hpp"
#include "../../Modules/Game/game_event.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/function.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/JSon/json.hpp"
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

int test_event_scheduler_cancel_missing_event(void)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_event> first(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->initialize());
    ft_vector<ft_sharedptr<game_event> > events;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());

    first->set_id(1);
    first->set_duration(3);
    scheduler.schedule_event(first);
    scheduler.cancel_event(2);
    scheduler.dump_events(events);
    if (scheduler.get_error() != FT_ERR_GAME_GENERAL_ERROR)
        return (0);
    if (events.size() != 1)
        return (0);
    if (events[0]->get_id() != 1)
        return (0);
    return (1);
}

int test_event_scheduler_reschedule_duration(void)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_event> timed(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, timed->initialize());
    ft_vector<ft_sharedptr<game_event> > events;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());

    timed->set_id(4);
    timed->set_duration(4);
    scheduler.schedule_event(timed);
    scheduler.reschedule_event(4, 7);
    scheduler.dump_events(events);
    if (scheduler.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (events.size() != 1)
        return (0);
    if (events[0]->get_duration() != 7)
        return (0);
    return (1);
}

int test_event_scheduler_clear_queue(void)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_event> pending(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pending->initialize());
    ft_vector<ft_sharedptr<game_event> > events;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());

    pending->set_id(9);
    pending->set_duration(2);
    scheduler.schedule_event(pending);
    scheduler.clear();
    scheduler.dump_events(events);
    if (scheduler.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (events.size() != 0)
        return (0);
    return (1);
}

int test_event_scheduler_prioritizes_shorter_events(void)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_event> long_event(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, long_event->initialize());
    ft_sharedptr<game_event> short_event(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, short_event->initialize());
    ft_vector<ft_sharedptr<game_event> > events;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());

    long_event->set_id(11);
    long_event->set_duration(5);
    short_event->set_id(12);
    short_event->set_duration(1);
    scheduler.schedule_event(long_event);
    scheduler.schedule_event(short_event);
    scheduler.dump_events(events);
    if (scheduler.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (events.size() != 2)
        return (0);
    if (events[0]->get_id() != 12 || events[0]->get_duration() != 1)
        return (0);
    if (events[1]->get_id() != 11 || events[1]->get_duration() != 5)
        return (0);
    return (1);
}

int test_event_scheduler_rejects_null_schedule(void)
{
    game_event_scheduler scheduler;
    ft_vector<ft_sharedptr<game_event> > events;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());

    scheduler.schedule_event(ft_sharedptr<game_event>());
    scheduler.dump_events(events);
    if (scheduler.get_error() != FT_ERR_GAME_GENERAL_ERROR)
        return (0);
    if (events.size() != 0)
        return (0);
    return (1);
}

FT_TEST(test_game_event_scheduler_dump_allocation_failure_keeps_pending_events)
{
    game_event_scheduler scheduler;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    int32_t id = 20;
    while (id > 0)
    {
        ft_sharedptr<game_event> event(new game_event());
        FT_ASSERT(event.get() != ft_nullptr);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
        event->set_id(id);
        event->set_duration(id);
        scheduler.schedule_event(event);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
        --id;
    }
    ft_vector<ft_sharedptr<game_event> > snapshot;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(64U));
    scheduler.dump_events(snapshot);
    const int32_t snapshot_error = scheduler.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, snapshot_error);
    FT_ASSERT_EQ(0U, snapshot.size());
    FT_ASSERT_EQ(20U, scheduler.size());
    scheduler.dump_events(snapshot);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(20U, snapshot.size());
    ft_size_t index = 0U;
    while (index < snapshot.size())
    {
        FT_ASSERT_EQ(static_cast<int32_t>(index + 1U), snapshot[index]->get_id());
        ++index;
    }
    FT_ASSERT_EQ(20U, scheduler.size());
    return (1);
}

FT_TEST(test_game_event_scheduler_load_accepts_reordered_fields_and_reports_missing_data)
{
    ft_sharedptr<game_event_scheduler> scheduler(new game_event_scheduler());
    FT_ASSERT(scheduler.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler->initialize());
    json_group *group = json_read_from_string(
        "{\"world\":{\"event_count\":2,\"event_0_id\":7,\"event_0_duration\":2,"
        "\"event_1_duration\":5,\"event_1_id\":8}}");
    FT_ASSERT(group != ft_nullptr);
    json_item *duplicate = json_create_item("event_1_id", 99);
    FT_ASSERT(duplicate != ft_nullptr);
    json_add_item_to_group(group, duplicate);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deserialize_event_scheduler(scheduler, group));
    ft_vector<ft_sharedptr<game_event> > events;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());
    scheduler->dump_events(events);
    FT_ASSERT_EQ(2U, events.size());
    FT_ASSERT_EQ(7, events[0]->get_id());
    FT_ASSERT_EQ(8, events[1]->get_id());

    scheduler->clear();
    json_remove_item(group, "event_1_duration");
    const int32_t result = deserialize_event_scheduler(scheduler, group);
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, result);
    FT_ASSERT_EQ(1U, scheduler->size());
    return (1);
}

FT_TEST(test_game_event_scheduler_load_reports_queue_growth_failure)
{
    constexpr ft_size_t ALLOCATION_LIMIT = 512U;
    constexpr ft_size_t QUEUE_CAPACITY = ALLOCATION_LIMIT / sizeof(ft_sharedptr<game_event> *);
    constexpr int32_t EVENT_COUNT = static_cast<int32_t>(2U * QUEUE_CAPACITY);
    ft_sharedptr<game_event_scheduler> source(new game_event_scheduler());
    ft_sharedptr<game_event_scheduler> destination(new game_event_scheduler());
    FT_ASSERT(source.get() != ft_nullptr && destination.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination->initialize());
    int32_t id = 1;
    while (id <= EVENT_COUNT)
    {
        ft_sharedptr<game_event> event(new game_event());
        FT_ASSERT(event.get() != ft_nullptr);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
        event->set_id(id);
        event->set_duration(id);
        source->schedule_event(event);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source->get_error());
        ++id;
    }
    json_group *group = serialize_event_scheduler(source);
    FT_ASSERT(group != ft_nullptr);
    // Event objects fit, but the queue cannot double beyond QUEUE_CAPACITY.
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(ALLOCATION_LIMIT));
    const int32_t result = deserialize_event_scheduler(destination, group);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(QUEUE_CAPACITY, destination->size());
    return (1);
}

FT_TEST(test_game_event_scheduler_processes_ready_events)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT(world.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    ft_sharedptr<game_event> ready_event(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ready_event->initialize());
    ft_vector<ft_sharedptr<game_event> > remaining;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, remaining.initialize());
    int callback_runs;

    callback_runs = 0;
    ready_event->set_id(21);
    ready_event->set_duration(1);
    ready_event->set_callback(ft_function<void(game_world&, game_event&)>([&callback_runs](game_world &world_ref, game_event &event_ref)
    {
        (void)world_ref;
        (void)event_ref;
        callback_runs += 1;
        return ;
    }));
    scheduler.schedule_event(ready_event);
    scheduler.update_events(world, 1);
    scheduler.dump_events(remaining);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(static_cast<size_t>(0), remaining.size());
    FT_ASSERT_EQ(1, callback_runs);
    return (1);
}

FT_TEST(test_game_event_scheduler_reschedules_remaining_events)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT(world.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    ft_sharedptr<game_event> delayed_event(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, delayed_event->initialize());
    ft_vector<ft_sharedptr<game_event> > pending;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pending.initialize());

    delayed_event->set_id(22);
    delayed_event->set_duration(3);
    scheduler.schedule_event(delayed_event);
    scheduler.update_events(world, 1);
    scheduler.dump_events(pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(static_cast<size_t>(1), pending.size());
    FT_ASSERT_EQ(22, pending[0]->get_id());
    FT_ASSERT_EQ(2, pending[0]->get_duration());
    return (1);
}

FT_TEST(test_game_event_scheduler_tick_preserves_duration_boundaries)
{
    game_event_scheduler scheduler;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    ft_sharedptr<game_event> event(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event->enable_thread_safety());
    event->set_duration(2);
    event->sub_duration(-1);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, event->get_error());
    event->sub_duration(3);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, event->get_error());
    FT_ASSERT_EQ(2, event->get_duration());
    scheduler.schedule_event(event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());

    scheduler.update_events(world, -1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(2, event->get_duration());
    scheduler.update_events(world, 3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(2, event->get_duration());
    FT_ASSERT_EQ(1U, scheduler.size());
    scheduler.update_events(world, 2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(0, event->get_duration());
    FT_ASSERT_EQ(0U, scheduler.size());
    return (1);
}

FT_TEST(test_game_event_scheduler_tick_keeps_pending_events_on_allocation_failure)
{
    game_event_scheduler scheduler;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());

    int32_t duration = 3;
    while (duration <= 5)
    {
        ft_sharedptr<game_event> event(new game_event());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
        event->set_id(duration);
        event->set_duration(duration);
        scheduler.schedule_event(event);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
        ++duration;
    }
    const ft_size_t capacity = scheduler._events.capacity();
    FT_ASSERT(capacity > scheduler.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    scheduler.update_events(world, 1);
    const int32_t update_error = scheduler.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_PRIORITY_QUEUE_NO_MEMORY, update_error);

    ft_vector<ft_sharedptr<game_event> > pending;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pending.initialize());
    scheduler.dump_events(pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(3U, pending.size());
    FT_ASSERT_EQ(3, pending[0]->get_duration());
    FT_ASSERT_EQ(4, pending[1]->get_duration());
    FT_ASSERT_EQ(5, pending[2]->get_duration());

    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(capacity, scheduler._events.capacity());
    scheduler.dump_events(pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(3U, pending.size());
    FT_ASSERT_EQ(2, pending[0]->get_duration());
    FT_ASSERT_EQ(3, pending[1]->get_duration());
    FT_ASSERT_EQ(4, pending[2]->get_duration());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t initialize_error = scheduler.initialize();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_error);
    FT_ASSERT_EQ(0U, scheduler._events.capacity());
    return (1);
}

FT_TEST(test_game_event_scheduler_pending_ticks_reuse_queue_storage)
{
    game_event_scheduler scheduler;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    int32_t duration = 32;
    while (duration < 36)
    {
        ft_sharedptr<game_event> event(new game_event());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
        event->set_duration(duration);
        scheduler.schedule_event(event);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
        ++duration;
    }
    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    scheduler.update_events(world, 1);
    const int32_t result = scheduler.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);

    ft_vector<ft_sharedptr<game_event> > pending;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pending.initialize());
    scheduler.dump_events(pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(4U, pending.size());
    FT_ASSERT_EQ(29, pending[0U]->get_duration());
    FT_ASSERT_EQ(32, pending[3U]->get_duration());
    return (1);
}

FT_TEST(test_game_event_scheduler_mixed_tick_preserves_callback_order)
{
    game_event_scheduler scheduler;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.enable_thread_safety());
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    ft_sharedptr<game_event> added(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, added->initialize());
    added->set_id(9);
    added->set_duration(9);
    ft_size_t observed_queue_size = 0U;
    int32_t callback_order = 0;

    int32_t id = 3;
    while (id >= 0)
    {
        ft_sharedptr<game_event> event(new game_event());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
        event->set_id(id + 1);
        event->set_duration(id);
        event->set_callback(ft_function<void(game_world&, game_event&)>(
            [&](game_world &, game_event &processed)
            {
                callback_order = callback_order * 10 + processed.get_id();
                observed_queue_size = scheduler.size();
                if (processed.get_id() == 1)
                    scheduler.schedule_event(added);
                return ;
            }));
        scheduler.schedule_event(event);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
        --id;
    }
    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(12, callback_order);
    FT_ASSERT_EQ(3U, observed_queue_size);
    ft_vector<ft_sharedptr<game_event> > pending;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pending.initialize());
    scheduler.dump_events(pending);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(3U, pending.size());
    FT_ASSERT_EQ(1, pending[0]->get_duration());
    FT_ASSERT_EQ(2, pending[1]->get_duration());
    FT_ASSERT_EQ(9, pending[2]->get_duration());
    return (1);
}

FT_TEST(test_game_event_scheduler_ready_cache_retries_after_growth_failure)
{
    game_event_scheduler scheduler;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());

    constexpr int32_t EVENT_COUNT = 12;
    uint32_t callback_count = 0U;
    int32_t event_id = 1;
    while (event_id <= EVENT_COUNT)
    {
        ft_sharedptr<game_event> event(new game_event());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
        event->set_id(event_id);
        event->set_duration(1);
        event->set_callback(ft_function<void(game_world&, game_event&)>(
            [&callback_count](game_world &, game_event &)
            {
                ++callback_count;
                return ;
            }));
        scheduler.schedule_event(event);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
        ++event_id;
    }
    const ft_size_t pending_bytes = scheduler._events.capacity()
        * sizeof(ft_sharedptr<game_event> *);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(pending_bytes));
    scheduler.update_events(world, 1);
    const int32_t update_error = scheduler.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, update_error);
    FT_ASSERT_EQ(8U, callback_count);
    FT_ASSERT_EQ(4U, scheduler.size());

    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(12U, callback_count);
    FT_ASSERT_EQ(0U, scheduler.size());
    return (1);
}

FT_TEST(test_game_event_scheduler_ready_cache_retains_events_until_next_tick)
{
    game_event_scheduler scheduler;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    ft_sharedptr<game_event> event(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
    event->set_duration(1);
    scheduler.schedule_event(event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(2, event.use_count());

    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(1, event.use_count());
    return (1);
}

FT_TEST(test_game_event_scheduler_tick_preserves_shared_event_entries)
{
    game_event_scheduler scheduler;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    ft_sharedptr<game_event> event(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
    event->set_duration(2);
    uint32_t calls = 0U;
    event->set_callback(ft_function<void(game_world&, game_event&)>(
        [&calls](game_world &, game_event &)
        {
            ++calls;
            return ;
        }));
    scheduler.schedule_event(event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    scheduler.schedule_event(event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());

    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(1U, scheduler.size());
    FT_ASSERT_EQ(1U, calls);
    FT_ASSERT_EQ(0, event->get_duration());
    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(0U, scheduler.size());
    FT_ASSERT_EQ(2U, calls);
    return (1);
}

FT_TEST(test_game_event_scheduler_rejects_null_world)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_event> mission(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mission->initialize());
    ft_vector<ft_sharedptr<game_event> > queued;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queued.initialize());
    ft_sharedptr<game_world> world;

    mission->set_id(23);
    mission->set_duration(2);
    scheduler.schedule_event(mission);
    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, scheduler.get_error());
    scheduler.dump_events(queued);
    FT_ASSERT_EQ(static_cast<size_t>(1), scheduler.size());
    FT_ASSERT_EQ(static_cast<size_t>(1), queued.size());
    return (1);
}

FT_TEST(test_game_event_scheduler_profiles_updates)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT(world.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    ft_sharedptr<game_event> task(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, task->initialize());
    t_event_scheduler_profile profile;

    scheduler.enable_profiling(true);
    task->set_id(24);
    task->set_duration(1);
    scheduler.schedule_event(task);
    scheduler.update_events(world, 1);
    scheduler.snapshot_profile(profile);
    FT_ASSERT_EQ(true, scheduler.profiling_enabled());
    FT_ASSERT_EQ(0, profile.update_count);
    FT_ASSERT_EQ(0, profile.events_processed);
    FT_ASSERT_EQ(0, profile.events_rescheduled);
    FT_ASSERT_EQ(static_cast<size_t>(0), profile.max_queue_depth);
    FT_ASSERT_EQ(static_cast<size_t>(0), profile.max_ready_batch);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.last_error_code);
    return (1);
}

FT_TEST(test_game_event_scheduler_reset_profile_clears_metrics)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT(world.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    ft_sharedptr<game_event> quick_task(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quick_task->initialize());
    t_event_scheduler_profile profile;

    scheduler.enable_profiling(true);
    quick_task->set_id(25);
    quick_task->set_duration(1);
    scheduler.schedule_event(quick_task);
    scheduler.update_events(world, 1);
    scheduler.reset_profile();
    scheduler.snapshot_profile(profile);
    FT_ASSERT_EQ(true, scheduler.profiling_enabled());
    FT_ASSERT_EQ(0, profile.update_count);
    FT_ASSERT_EQ(0, profile.events_processed);
    FT_ASSERT_EQ(0, profile.events_rescheduled);
    FT_ASSERT_EQ(static_cast<size_t>(0), profile.max_queue_depth);
    FT_ASSERT_EQ(static_cast<size_t>(0), profile.max_ready_batch);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.last_error_code);
    return (1);
}

FT_TEST(test_game_event_scheduler_size_tracks_scheduled_events)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_event> first(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->initialize());
    ft_sharedptr<game_event> second(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->initialize());

    first->set_id(31);
    first->set_duration(2);
    second->set_id(32);
    second->set_duration(3);
    scheduler.schedule_event(first);
    scheduler.schedule_event(second);
    FT_ASSERT_EQ(static_cast<size_t>(2), scheduler.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    return (1);
}

FT_TEST(test_game_event_scheduler_cancel_existing_event_clears_queue)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_event> quest(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest->initialize());
    ft_vector<ft_sharedptr<game_event> > events;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());

    quest->set_id(33);
    quest->set_duration(4);
    scheduler.schedule_event(quest);
    scheduler.cancel_event(33);
    scheduler.dump_events(events);
    FT_ASSERT_EQ(static_cast<size_t>(0), events.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    return (1);
}

FT_TEST(test_game_event_scheduler_edits_all_matching_ids_without_allocating)
{
    game_event_scheduler scheduler;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    ft_sharedptr<game_event> first(new game_event());
    ft_sharedptr<game_event> second(new game_event());
    ft_sharedptr<game_event> retained(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, retained->initialize());
    first->set_id(3);
    first->set_duration(1);
    second->set_id(3);
    second->set_duration(9);
    retained->set_id(8);
    retained->set_duration(5);
    scheduler.schedule_event(first);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    scheduler.schedule_event(retained);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    scheduler.schedule_event(second);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    scheduler.schedule_event(first);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    scheduler.cancel_event(99);
    const int32_t missing_cancel = scheduler.get_error();
    scheduler.reschedule_event(99, 2);
    const int32_t missing_reschedule = scheduler.get_error();
    scheduler.reschedule_event(3, 7);
    const int32_t reschedule_error = scheduler.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, missing_cancel);
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, missing_reschedule);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reschedule_error);
    FT_ASSERT_EQ(7, first->get_duration());
    FT_ASSERT_EQ(7, second->get_duration());
    FT_ASSERT_EQ(5, retained->get_duration());

    ft_vector<ft_sharedptr<game_event> > events;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());
    scheduler.dump_events(events);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(4U, events.size());
    FT_ASSERT_EQ(8, events[0]->get_id());
    FT_ASSERT_EQ(3, events[1]->get_id());
    FT_ASSERT_EQ(3, events[2]->get_id());
    FT_ASSERT_EQ(3, events[3]->get_id());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    scheduler.cancel_event(3);
    const int32_t cancel_error = scheduler.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cancel_error);
    scheduler.dump_events(events);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(1U, events.size());
    FT_ASSERT_EQ(8, events[0]->get_id());
    return (1);
}

FT_TEST(test_game_event_scheduler_update_without_events_is_noop)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT(world.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());

    scheduler.update_events(world, 1);
    FT_ASSERT_EQ(static_cast<size_t>(0), scheduler.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    return (1);
}

FT_TEST(test_game_event_scheduler_snapshot_without_profiling_is_zeroed)
{
    game_event_scheduler scheduler;
    t_event_scheduler_profile profile;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());

    scheduler.snapshot_profile(profile);
    FT_ASSERT_EQ(false, scheduler.profiling_enabled());
    FT_ASSERT_EQ(0, profile.update_count);
    FT_ASSERT_EQ(0, profile.events_processed);
    FT_ASSERT_EQ(0, profile.events_rescheduled);
    FT_ASSERT_EQ(static_cast<size_t>(0), profile.max_queue_depth);
    FT_ASSERT_EQ(static_cast<size_t>(0), profile.max_ready_batch);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.last_error_code);
    return (1);
}

FT_TEST(test_game_event_scheduler_thread_safety_toggle)
{
    game_event_scheduler scheduler;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());

    FT_ASSERT_EQ(false, scheduler.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.enable_thread_safety());
    FT_ASSERT_EQ(true, scheduler.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.disable_thread_safety());
    FT_ASSERT_EQ(false, scheduler.is_thread_safe());
    return (1);
}

FT_TEST(test_game_event_scheduler_profile_counts_reschedules)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT(world.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    ft_sharedptr<game_event> delayed(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, delayed->initialize());
    t_event_scheduler_profile profile;

    scheduler.enable_profiling(true);
    delayed->set_id(34);
    delayed->set_duration(2);
    scheduler.schedule_event(delayed);
    scheduler.update_events(world, 1);
    scheduler.snapshot_profile(profile);
    FT_ASSERT_EQ(true, scheduler.profiling_enabled());
    FT_ASSERT_EQ(0, profile.update_count);
    FT_ASSERT_EQ(0, profile.events_processed);
    FT_ASSERT_EQ(0, profile.events_rescheduled);
    FT_ASSERT_EQ(static_cast<size_t>(0), profile.max_queue_depth);
    FT_ASSERT_EQ(static_cast<size_t>(0), profile.max_ready_batch);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.last_error_code);
    return (1);
}

FT_TEST(test_game_event_scheduler_move_into_uninitialised_destination)
{
    game_event_scheduler source;
    game_event_scheduler destination;
    ft_sharedptr<game_event> event_pointer(new (std::nothrow) game_event());
    ft_vector<ft_sharedptr<game_event> > moved_events;

    FT_ASSERT(event_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_events.initialize());
    event_pointer->set_id(77);
    event_pointer->set_duration(5);
    source.schedule_event(event_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    destination.dump_events(moved_events);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), moved_events.size());
    FT_ASSERT_EQ(77, moved_events[0]->get_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.disable_thread_safety());
    return (1);
}

FT_TEST(test_game_event_scheduler_destroy_allows_reinitialize)
{
    game_event_scheduler scheduler;
    ft_sharedptr<game_event> event_pointer(new (std::nothrow) game_event());
    ft_vector<ft_sharedptr<game_event> > events;

    FT_ASSERT(event_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, events.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    event_pointer->set_id(88);
    event_pointer->set_duration(4);
    scheduler.schedule_event(event_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.initialize());
    scheduler.dump_events(events);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.get_error());
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), events.size());
    return (1);
}
