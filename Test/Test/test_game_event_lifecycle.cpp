#include "../test_internal.hpp"

#include "../../Modules/Game/game_event.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_event_thread_safe_lifecycle)
{
    game_event event;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.initialize());
    FT_ASSERT_EQ(0, event.get_id());
    FT_ASSERT_EQ(0, event.get_duration());
    FT_ASSERT_EQ(0, event.get_modifier1());
    FT_ASSERT_EQ(0, event.get_modifier2());
    FT_ASSERT_EQ(0, event.get_modifier3());
    FT_ASSERT_EQ(0, event.get_modifier4());
    event.set_id(44);
    event.set_duration(12);
    event.set_modifier1(1);
    event.set_modifier2(2);
    event.set_modifier3(3);
    event.set_modifier4(4);
    FT_ASSERT_EQ(44, event.get_id());
    FT_ASSERT_EQ(12, event.get_duration());
    FT_ASSERT_EQ(1, event.get_modifier1());
    FT_ASSERT_EQ(2, event.get_modifier2());
    FT_ASSERT_EQ(3, event.get_modifier3());
    FT_ASSERT_EQ(4, event.get_modifier4());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.enable_thread_safety());
    FT_ASSERT(event.is_thread_safe() == FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.destroy());
    return (1);
}

FT_TEST(test_game_event_duration_access_reports_operation_status)
{
    game_event event;
    int32_t id;
    int32_t duration;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.initialize());
    event.set_id(42);
    event.set_duration(INT32_MAX);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, event.add_duration(1));
    FT_ASSERT_EQ(42, event.get_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.enable_thread_safety());
    event.sub_duration(-1);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, event.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.get_schedule_values(id, duration));
    FT_ASSERT_EQ(42, id);
    FT_ASSERT_EQ(INT32_MAX, duration);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.get_error());
    event.sub_duration(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.get_error());
    FT_ASSERT_EQ(INT32_MAX - 1, event.get_duration());
    return (1);
}

FT_TEST(test_game_event_modifier_overflow_preserves_value)
{
    game_event event;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.initialize());
    event.set_modifier1(FT_INT32_MAX);
    event.add_modifier1(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, event.get_error());
    FT_ASSERT_EQ(FT_INT32_MAX, event.get_modifier1());
    event.set_modifier2(FT_INT32_MIN);
    event.add_modifier2(-1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, event.get_error());
    FT_ASSERT_EQ(FT_INT32_MIN, event.get_modifier2());
    event.set_modifier3(0);
    event.sub_modifier3(FT_INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, event.get_error());
    FT_ASSERT_EQ(0, event.get_modifier3());
    event.set_modifier4(FT_INT32_MIN);
    event.sub_modifier4(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, event.get_error());
    FT_ASSERT_EQ(FT_INT32_MIN, event.get_modifier4());
    event.sub_modifier4(FT_INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.get_error());
    FT_ASSERT_EQ(0, event.get_modifier4());
    return (1);
}

FT_TEST(test_game_event_move_preserves_mutex_and_diagnostics)
{
    game_event source;
    game_event destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    pt_recursive_mutex *source_mutex = source._mutex;
    source.set_id(12);
    source.set_duration(60);
    source.set_modifier1(-25);
    source.set_duration(-1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, destination.get_error());
    FT_ASSERT(source._mutex == ft_nullptr);
    FT_ASSERT(destination._mutex == source_mutex);
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(12, destination.get_id());
    FT_ASSERT_EQ(60, destination.get_duration());
    FT_ASSERT_EQ(-25, destination.get_modifier1());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    return (1);
}

FT_TEST(test_game_event_destructor_preserves_diagnostics)
{
    game_event observer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, observer.initialize());
    {
        game_event temporary;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, temporary.initialize());
        temporary.set_duration(-1);
    }
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, observer.get_error());
    return (1);
}

struct game_event_modifier_task
{
    game_event *event;
    int32_t result;
};

static void *game_event_add_modifiers(void *argument)
{
    game_event_modifier_task *task = static_cast<game_event_modifier_task *>(argument);
    int32_t index = 0;
    while (index < 10000)
    {
        task->event->add_modifier1(1);
        task->result = task->event->get_error();
        if (task->result != FT_ERR_SUCCESS)
            return (ft_nullptr);
        ++index;
    }
    return (ft_nullptr);
}

FT_TEST(test_game_event_thread_safe_modifier_updates)
{
    game_event event;
    pthread_t worker;
    game_event_modifier_task worker_task = {&event, FT_ERR_SUCCESS};
    game_event_modifier_task local_task = {&event, FT_ERR_SUCCESS};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.enable_thread_safety());
    FT_ASSERT_EQ(0, pt_thread_create(&worker, ft_nullptr, game_event_add_modifiers, &worker_task));
    (void)game_event_add_modifiers(&local_task);
    FT_ASSERT_EQ(0, pt_thread_join(worker, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, worker_task.result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, local_task.result);
    FT_ASSERT_EQ(20000, event.get_modifier1());
    return (1);
}
