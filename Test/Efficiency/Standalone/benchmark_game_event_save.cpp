#include "../../../Modules/Game/game_event_scheduler.hpp"
#include "../../../Modules/Game/game_event.hpp"
#include "../../../Modules/Game/game_world.hpp"
#include "../../../Modules/JSon/json.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <new>

static void print_result(const char *mode, double time, const char *encoded)
{
    uint64_t checksum = 0U;
    ft_size_t index = 0U;
    while (encoded[index] != '\0')
    {
        checksum = checksum * 31U + static_cast<uint8_t>(encoded[index]);
        ++index;
    }
    std::printf("event %s %.3f us/%s bytes=%" PRIu64 " checksum=%" PRIu64 "\n",
        mode, time, mode, ft_platform_cast<uint64_t>(index), checksum);
    return ;
}

static int32_t measure_load(const ft_sharedptr<game_event_scheduler> &source,
    uint32_t iterations)
{
    ft_sharedptr<game_event_scheduler> scheduler(new (std::nothrow) game_event_scheduler());
    if (!scheduler || scheduler->initialize() != FT_ERR_SUCCESS)
        return (1);
    json_group *group = serialize_event_scheduler(source);
    if (group == ft_nullptr)
        return (1);
    std::chrono::steady_clock::duration elapsed{};
    uint32_t iteration = 0U;
    int32_t result = FT_ERR_SUCCESS;
    while (iteration < iterations && result == FT_ERR_SUCCESS)
    {
        scheduler->clear();
        const auto start = std::chrono::steady_clock::now();
        result = deserialize_event_scheduler(scheduler, group);
        elapsed += std::chrono::steady_clock::now() - start;
        ++iteration;
    }
    char *expected = json_write_to_string(group);
    json_free_groups(group);
    group = serialize_event_scheduler(scheduler);
    char *actual = ft_nullptr;
    if (group != ft_nullptr)
        actual = json_write_to_string(group);
    json_free_groups(group);
    const ft_bool matches = result == FT_ERR_SUCCESS && expected != ft_nullptr
        && actual != ft_nullptr && ft_strcmp(expected, actual) == 0;
    if (matches == FT_TRUE)
    {
        const double time = std::chrono::duration<double, std::micro>(elapsed).count();
        print_result("load", time / iterations, actual);
    }
    cma_free(expected);
    cma_free(actual);
    return (matches == FT_FALSE);
}

static int32_t measure_edits(ft_sharedptr<game_event_scheduler> &scheduler,
    int32_t event_count)
{
    constexpr uint32_t ITERATIONS = 96U;
    const int32_t target_id = event_count / 2;
    ft_sharedptr<game_event> replacement(new (std::nothrow) game_event());
    if (!replacement || replacement->initialize() != FT_ERR_SUCCESS)
        return (1);
    replacement->set_id(target_id);
    replacement->set_duration(target_id);
    double reschedule_time = 0.0;
    double cancel_time = 0.0;
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        const auto reschedule_start = std::chrono::steady_clock::now();
        scheduler->reschedule_event(target_id, 0);
        const auto reschedule_end = std::chrono::steady_clock::now();
        if (scheduler->get_error() != FT_ERR_SUCCESS)
            return (1);
        const auto cancel_start = std::chrono::steady_clock::now();
        scheduler->cancel_event(target_id);
        const auto cancel_end = std::chrono::steady_clock::now();
        if (scheduler->get_error() != FT_ERR_SUCCESS
            || scheduler->size() != static_cast<ft_size_t>(event_count - 1))
            return (1);
        scheduler->schedule_event(replacement);
        if (scheduler->get_error() != FT_ERR_SUCCESS)
            return (1);
        // The scheduler holds shared handles, so the next replacement must
        // have a duration independent of the event being rescheduled.
        ft_sharedptr<game_event> next(new (std::nothrow) game_event());
        if (!next || next->initialize() != FT_ERR_SUCCESS)
            return (1);
        next->set_id(target_id);
        next->set_duration(target_id);
        replacement = next;
        reschedule_time += std::chrono::duration<double, std::micro>(
            reschedule_end - reschedule_start).count();
        cancel_time += std::chrono::duration<double, std::micro>(
            cancel_end - cancel_start).count();
        ++iteration;
    }
    json_group *group = serialize_event_scheduler(scheduler);
    char *encoded = json_write_to_string(group);
    json_free_groups(group);
    if (encoded == ft_nullptr)
        return (1);
    print_result("reschedule", reschedule_time / ITERATIONS, encoded);
    print_result("cancel", cancel_time / ITERATIONS, encoded);
    cma_free(encoded);
    return (0);
}

static int32_t measure_ticks(ft_sharedptr<game_event_scheduler> &scheduler,
    int32_t event_count, ft_bool tied_priorities)
{
    constexpr int32_t TICKS = 96;
    ft_sharedptr<game_world> world(new (std::nothrow) game_world());
    if (!world || world->initialize() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t before;
    ft_size_t after;
    ft_size_t frees;
    if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
        return (1);
    int32_t tick = 0;
    const auto start = std::chrono::steady_clock::now();
    while (tick < TICKS)
    {
        scheduler->update_events(world, 1);
        if (scheduler->get_error() != FT_ERR_SUCCESS)
            return (1);
        ++tick;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (scheduler->size() != static_cast<ft_size_t>(event_count)
        || cma_get_stats(&after, &frees) != FT_ERR_SUCCESS)
        return (1);
    json_group *group = serialize_event_scheduler(scheduler);
    if (group == ft_nullptr)
        return (1);
    char *encoded = json_write_to_string(group);
    json_free_groups(group);
    if (encoded == ft_nullptr)
        return (1);
    if (tied_priorities)
        print_result("tick-ties", elapsed / TICKS, encoded);
    else
        print_result("tick", elapsed / TICKS, encoded);
    std::printf("tick allocations=%" PRIu64 "\n", ft_platform_cast<uint64_t>(after - before));
    cma_free(encoded);
    return (0);
}

static int32_t measure_mixed_ticks(ft_sharedptr<game_event_scheduler> &scheduler,
    int32_t event_count)
{
    constexpr int32_t ITERATIONS = 24;
    const int32_t ready_count = event_count / 2;
    ft_sharedptr<game_world> world(new (std::nothrow) game_world());
    if (!world || world->initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t callbacks = 0;
    uint64_t callback_checksum = 0U;
    double elapsed = 0.0;
    int32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        scheduler->clear();
        if (scheduler->get_error() != FT_ERR_SUCCESS)
            return (1);
        int32_t event_id = event_count;
        while (event_id > 0)
        {
            ft_sharedptr<game_event> event(new (std::nothrow) game_event());
            if (!event || event->initialize() != FT_ERR_SUCCESS)
                return (1);
            event->set_id(event_id);
            if (event_id <= ready_count)
            {
                event->set_duration(1);
                event->set_callback(ft_function<void(game_world&, game_event&)>(
                    [&callbacks, &callback_checksum](game_world &, game_event &processed)
                    {
                        ++callbacks;
                        callback_checksum = callback_checksum * 31U + processed.get_id();
                        return ;
                    }));
            }
            else
                event->set_duration(event_id);
            scheduler->schedule_event(event);
            if (scheduler->get_error() != FT_ERR_SUCCESS)
                return (1);
            --event_id;
        }
        const auto start = std::chrono::steady_clock::now();
        scheduler->update_events(world, 1);
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        if (scheduler->get_error() != FT_ERR_SUCCESS
            || scheduler->size() != static_cast<ft_size_t>(event_count - ready_count)
            || callbacks != ready_count * (iteration + 1))
            return (1);
        ++iteration;
    }
    json_group *group = serialize_event_scheduler(scheduler);
    if (group == ft_nullptr)
        return (1);
    char *encoded = json_write_to_string(group);
    json_free_groups(group);
    if (encoded == ft_nullptr)
        return (1);
    print_result("tick-mixed", elapsed / ITERATIONS, encoded);
    std::printf("callbacks=%" PRId32 " checksum=%" PRIu64 "\n", callbacks, callback_checksum);
    cma_free(encoded);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    constexpr int32_t EVENT_COUNT = 512;
    constexpr uint32_t ITERATIONS = 24U;
    ft_sharedptr<game_event_scheduler> scheduler(new (std::nothrow) game_event_scheduler());
    if (!scheduler || scheduler->initialize() != FT_ERR_SUCCESS)
        return (1);
    if (argument_count == 2 && ft_strcmp(arguments[1], "--tick-empty") == 0)
        return (measure_ticks(scheduler, 0, FT_FALSE));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--tick-mixed") == 0)
        return (measure_mixed_ticks(scheduler, EVENT_COUNT));
    const ft_bool tied_priorities = argument_count == 2
        && ft_strcmp(arguments[1], "--tick-ties") == 0;
    const ft_bool thread_safe_events = argument_count == 2
        && ft_strcmp(arguments[1], "--tick-thread-safe") == 0;
    const ft_bool tick_mode = tied_priorities || thread_safe_events || (argument_count == 2
        && ft_strcmp(arguments[1], "--tick") == 0);
    int32_t event_id = EVENT_COUNT;
    while (event_id > 0)
    {
        ft_sharedptr<game_event> event(new (std::nothrow) game_event());
        if (!event || event->initialize() != FT_ERR_SUCCESS)
            return (1);
        event->set_id(event_id);
        if (thread_safe_events && event->enable_thread_safety() != FT_ERR_SUCCESS)
            return (1);
        int32_t duration = event_id;
        if (tied_priorities)
            duration = EVENT_COUNT + event_id / 8;
        else if (tick_mode)
            duration += EVENT_COUNT;
        event->set_duration(duration);
        scheduler->schedule_event(event);
        if (scheduler->get_error() != FT_ERR_SUCCESS)
            return (1);
        --event_id;
    }
    if (tick_mode)
        return (measure_ticks(scheduler, EVENT_COUNT, tied_priorities));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--edit") == 0)
        return (measure_edits(scheduler, EVENT_COUNT));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--load") == 0)
        return (measure_load(scheduler, ITERATIONS));
    json_group *group = ft_nullptr;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        json_free_groups(group);
        group = serialize_event_scheduler(scheduler);
        if (group == ft_nullptr)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    char *encoded = json_write_to_string(group);
    json_free_groups(group);
    if (encoded == ft_nullptr)
        return (1);
    print_result("save", elapsed / ITERATIONS, encoded);
    cma_free(encoded);
    if (scheduler->size() != static_cast<ft_size_t>(EVENT_COUNT))
        return (1);
    return (0);
}
