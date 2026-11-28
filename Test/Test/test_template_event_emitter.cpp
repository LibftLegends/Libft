#include "../test_internal.hpp"
#include "../../Modules/Template/event_emitter.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <limits>
#include <thread>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int g_event_listener_one_total = 0;
static int g_event_listener_two_total = 0;
static std::atomic<int> g_concurrent_listener_total(0);

static void event_listener_add_to_first(int value)
{
    g_event_listener_one_total += value;
    return ;
}

static void event_listener_add_to_second(int value)
{
    g_event_listener_two_total += value;
    return ;
}

static void event_listener_add_to_atomic(int value)
{
    g_concurrent_listener_total.fetch_add(value, std::memory_order_relaxed);
    return ;
}

static void event_listener_record_one(uint32_t &order)
{
    order = order * 10U + 1U;
    return ;
}

static void event_listener_record_two(uint32_t &order)
{
    order = order * 10U + 2U;
    return ;
}

FT_TEST(test_ft_event_emitter_removal_preserves_order_after_growth)
{
    ft_event_emitter<uint32_t, uint32_t &> emitter;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter.initialize());
    emitter.on(42U, event_listener_record_one);
    uint32_t event = 100U;
    while (event < 256U)
    {
        emitter.on(event, event_listener_record_one);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter.get_error());
        ++event;
    }
    emitter.on(42U, event_listener_record_two);
    emitter.on(42U, event_listener_record_one);
    emitter.remove_listener(42U, event_listener_record_one);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter.get_error());
    uint32_t order = 0U;
    emitter.emit(42U, order);
    FT_ASSERT_EQ(21U, order);
    return (1);
}

FT_TEST(test_ft_event_emitter_rejects_oversized_initial_capacity)
{
    ft_event_emitter<uint32_t> emitter(FT_SYSTEM_SIZE_MAX);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, emitter.initialize());
    return (1);
}

static void emit_in_thread(ft_event_emitter<int, int>* emitter_instance, int event_identifier, int emit_count, int emit_value)
{
    int emission_index;

    emission_index = 0;
    while (emission_index < emit_count)
    {
        emitter_instance->emit(event_identifier, emit_value);
        ++emission_index;
    }
    return ;
}

FT_TEST(test_ft_event_emitter_invokes_registered_listeners)
{
    ft_event_emitter<int, int> emitter_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.initialize());

    g_event_listener_one_total = 0;
    g_event_listener_two_total = 0;

    emitter_instance.on(7, event_listener_add_to_first);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());
    emitter_instance.on(7, event_listener_add_to_second);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());

    emitter_instance.emit(7, 5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());

    FT_ASSERT_EQ(5, g_event_listener_one_total);
    FT_ASSERT_EQ(5, g_event_listener_two_total);
    FT_ASSERT_EQ(2UL, emitter_instance.size());
    FT_ASSERT_EQ(false, emitter_instance.empty());
    return (1);
}

FT_TEST(test_ft_event_emitter_remove_listener_stops_callback)
{
    ft_event_emitter<int, int> emitter_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.initialize());

    g_event_listener_one_total = 0;
    g_event_listener_two_total = 0;

    emitter_instance.on(3, event_listener_add_to_first);
    emitter_instance.on(3, event_listener_add_to_second);
    FT_ASSERT_EQ(2UL, emitter_instance.size());

    emitter_instance.remove_listener(3, event_listener_add_to_first);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());
    FT_ASSERT_EQ(1UL, emitter_instance.size());

    emitter_instance.emit(3, 4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());

    FT_ASSERT_EQ(0, g_event_listener_one_total);
    FT_ASSERT_EQ(4, g_event_listener_two_total);

    emitter_instance.remove_listener(3, event_listener_add_to_first);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, emitter_instance.get_error());
    return (1);
}

FT_TEST(test_ft_event_emitter_emit_missing_sets_error)
{
    ft_event_emitter<int, int> emitter_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.initialize());

    emitter_instance.emit(1, 10);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, emitter_instance.get_error());
    FT_ASSERT_EQ(0UL, emitter_instance.size());
    FT_ASSERT(emitter_instance.empty());
    return (1);
}

FT_TEST(test_ft_event_emitter_allocation_failure_sets_error)
{
    ft_event_emitter<int, int> emitter_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.initialize());

    cma_set_alloc_limit(1);
    emitter_instance.on(9, event_listener_add_to_first);
    cma_set_alloc_limit(0);

    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, emitter_instance.get_error());
    FT_ASSERT_EQ(0UL, emitter_instance.size());
    FT_ASSERT(emitter_instance.empty());
    return (1);
}

FT_TEST(test_ft_event_emitter_growth_preserves_existing_listeners)
{
    ft_event_emitter<int, int> emitter_instance(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.initialize());

    g_event_listener_one_total = 0;
    g_event_listener_two_total = 0;

    emitter_instance.on(11, event_listener_add_to_first);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());
    emitter_instance.on(12, event_listener_add_to_second);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());
    emitter_instance.on(11, event_listener_add_to_second);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());

    emitter_instance.emit(11, 2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());

    FT_ASSERT_EQ(2, g_event_listener_one_total);
    FT_ASSERT_EQ(2, g_event_listener_two_total);
    FT_ASSERT_EQ(3UL, emitter_instance.size());
    return (1);
}

FT_TEST(test_ft_event_emitter_capacity_overflow_sets_error)
{
    ft_event_emitter<int, int> emitter_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.initialize());
    cma_set_alloc_limit(1);
    emitter_instance.on(1, event_listener_add_to_first);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, emitter_instance.get_error());
    FT_ASSERT(emitter_instance.empty());
    return (1);
}

FT_TEST(test_ft_event_emitter_supports_concurrent_emitters)
{
    ft_event_emitter<int, int> emitter_instance;
    std::thread first_thread;
    std::thread second_thread;
    std::thread third_thread;
    std::thread fourth_thread;
    int emit_iterations;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.initialize());
    g_concurrent_listener_total.store(0, std::memory_order_relaxed);
    emitter_instance.on(99, event_listener_add_to_atomic);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());

    emit_iterations = 1000;
    first_thread = std::thread(emit_in_thread, &emitter_instance, 99, emit_iterations, 1);
    second_thread = std::thread(emit_in_thread, &emitter_instance, 99, emit_iterations, 1);
    third_thread = std::thread(emit_in_thread, &emitter_instance, 99, emit_iterations, 1);
    fourth_thread = std::thread(emit_in_thread, &emitter_instance, 99, emit_iterations, 1);

    first_thread.join();
    second_thread.join();
    third_thread.join();
    fourth_thread.join();

    FT_ASSERT_EQ(4000, g_concurrent_listener_total.load(std::memory_order_relaxed));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, emitter_instance.get_error());
    return (1);
}
