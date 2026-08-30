#include "analytics.hpp"
#include <chrono>
#include <thread>

#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"

static uint64_t analytics_percentile_value(const uint64_t *samples,
    uint32_t sample_count, uint32_t percentile) noexcept
{
    uint64_t sorted_samples[FT_ANALYTICS_MAX_SAMPLES];
    uint32_t index;
    uint32_t target_index;
    uint64_t value;

    index = 0U;
    while (index < sample_count)
    {
        sorted_samples[index] = samples[index];
        index += 1U;
    }
    index = 1U;
    while (index < sample_count)
    {
        value = sorted_samples[index];
        target_index = index;
        while (target_index > 0U
            && sorted_samples[target_index - 1U] > value)
        {
            sorted_samples[target_index] = sorted_samples[target_index - 1U];
            target_index -= 1U;
        }
        sorted_samples[target_index] = value;
        index += 1U;
    }
    target_index = (sample_count - 1U) * percentile / 100U;
    return (sorted_samples[target_index]);
}

static uint64_t analytics_frame_percentile_value(const uint64_t *samples,
    uint32_t sample_count, uint32_t percentile) noexcept
{
    uint64_t sorted_samples[FT_ANALYTICS_MAX_FRAME_SAMPLES];
    uint32_t index;
    uint32_t target_index;
    uint64_t value;

    index = 0U;
    while (index < sample_count)
    {
        sorted_samples[index] = samples[index];
        index += 1U;
    }
    index = 1U;
    while (index < sample_count)
    {
        value = sorted_samples[index];
        target_index = index;
        while (target_index > 0U
            && sorted_samples[target_index - 1U] > value)
        {
            sorted_samples[target_index] = sorted_samples[target_index - 1U];
            target_index -= 1U;
        }
        sorted_samples[target_index] = value;
        index += 1U;
    }
    target_index = (sample_count - 1U) * percentile / 100U;
    return (sorted_samples[target_index]);
}

struct analytics_scope_frame
{
    analytics_session *session;
    uint32_t region_id;
    uint64_t start_nanoseconds;
    uint64_t child_nanoseconds;
};

struct analytics_pending_event
{
    uint32_t region_id;
    uint64_t start_nanoseconds;
    uint64_t inclusive_nanoseconds;
    uint64_t exclusive_nanoseconds;
};

struct analytics_thread_state
{
    analytics_session *session;
    uint64_t frame_number;
    uint64_t frame_start_nanoseconds;
    uint64_t instrumented_top_level_nanoseconds;
    uint64_t completed_scope_count;
    uint32_t scope_depth;
    analytics_scope_frame scopes[FT_ANALYTICS_MAX_SCOPE_DEPTH];
    uint32_t pending_event_count;
    analytics_pending_event pending_events[FT_ANALYTICS_MAX_THREAD_EVENTS];
};

static thread_local analytics_thread_state g_analytics_thread_state =
{
    ft_nullptr, 0U, 0U, 0U, 0U, 0U, {}, 0U, {}
};

static void analytics_init_frame_statistics(analytics_frame_statistics *frame,
    const analytics_thread_state *thread_state, uint64_t end_nanoseconds)
{
    uint32_t index;

    frame->frame_number = thread_state->frame_number;
    frame->duration_nanoseconds = end_nanoseconds
        - thread_state->frame_start_nanoseconds;
    if (thread_state->instrumented_top_level_nanoseconds
        >= frame->duration_nanoseconds)
        frame->uninstrumented_nanoseconds = 0U;
    else
        frame->uninstrumented_nanoseconds = frame->duration_nanoseconds
            - thread_state->instrumented_top_level_nanoseconds;
    frame->completed_scope_count = thread_state->completed_scope_count;
    frame->dropped_scope_count = 0U;
    frame->breakdown_count = 0U;
    frame->dropped_breakdown_count = 0U;
    index = 0U;
    while (index < FT_ANALYTICS_MAX_FRAME_BREAKDOWN)
    {
        frame->breakdown[index].region_id = 0U;
        frame->breakdown[index].invocation_count = 0U;
        frame->breakdown[index].inclusive_nanoseconds = 0U;
        frame->breakdown[index].exclusive_nanoseconds = 0U;
        index += 1U;
    }
    return ;
}

static void analytics_add_frame_breakdown(
    analytics_frame_statistics *frame, const analytics_pending_event &event)
{
    uint32_t index;

    index = 0U;
    while (index < frame->breakdown_count)
    {
        if (frame->breakdown[index].region_id == event.region_id)
        {
            frame->breakdown[index].invocation_count += 1U;
            frame->breakdown[index].inclusive_nanoseconds +=
                event.inclusive_nanoseconds;
            frame->breakdown[index].exclusive_nanoseconds +=
                event.exclusive_nanoseconds;
            return ;
        }
        index += 1U;
    }
    if (frame->breakdown_count >= FT_ANALYTICS_MAX_FRAME_BREAKDOWN)
    {
        frame->dropped_breakdown_count += 1U;
        return ;
    }
    frame->breakdown[index].region_id = event.region_id;
    frame->breakdown[index].invocation_count = 1U;
    frame->breakdown[index].inclusive_nanoseconds = event.inclusive_nanoseconds;
    frame->breakdown[index].exclusive_nanoseconds = event.exclusive_nanoseconds;
    frame->breakdown_count += 1U;
    return ;
}

static uint32_t analytics_thread_id(void) noexcept
{
    std::size_t value;

    value = std::hash<std::thread::id>()(std::this_thread::get_id());
    return (static_cast<uint32_t>(value));
}

static uint64_t analytics_clock_now(void)
{
    std::chrono::steady_clock::time_point now;
    std::chrono::nanoseconds duration;

    now = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch());
    return (static_cast<uint64_t>(duration.count()));
}

analytics_session::analytics_session() noexcept
    : _initialised_state(0U), _enabled(FT_FALSE), _mutex(), _regions(),
      _region_count(0U), _export_callback(ft_nullptr),
      _export_user_data(ft_nullptr), _trace_callback(ft_nullptr),
      _trace_user_data(ft_nullptr), _trace_events(), _trace_event_count(0U),
      _dropped_trace_count(0U), _dropped_scope_count(0U), _frame_samples(),
      _frame_sample_count(0U), _frame_sample_cursor(0U), _latest_frame(),
      _has_latest_frame(FT_FALSE)
{
    return ;
}

analytics_session::~analytics_session() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t analytics_session::initialize() noexcept
{
    uint32_t index;

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_initialised_state == 2U)
        return (FT_ERR_INVALID_STATE);
    index = 0U;
    while (index < FT_ANALYTICS_MAX_REGIONS)
    {
        this->_regions[index].name = ft_nullptr;
        this->_regions[index].category = ft_nullptr;
        this->_regions[index].registered = FT_FALSE;
        this->_regions[index].statistics = {};
        this->_regions[index].sample_count = 0U;
        this->_regions[index].sample_cursor = 0U;
        index += 1U;
    }
    this->_region_count = 0U;
    this->_export_callback = ft_nullptr;
    this->_export_user_data = ft_nullptr;
    this->_trace_callback = ft_nullptr;
    this->_trace_user_data = ft_nullptr;
    this->_trace_event_count = 0U;
    this->_dropped_trace_count = 0U;
    this->_dropped_scope_count = 0U;
    this->_frame_sample_count = 0U;
    this->_frame_sample_cursor = 0U;
    this->_latest_frame = {};
    this->_has_latest_frame = FT_FALSE;
    this->_enabled.store(FT_TRUE, std::memory_order_release);
    this->_initialised_state = 2U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::destroy() noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_initialised_state != 2U)
        return (FT_ERR_SUCCESS);
    this->_enabled.store(FT_FALSE, std::memory_order_release);
    this->_trace_event_count = 0U;
    this->_has_latest_frame = FT_FALSE;
    this->_initialised_state = 1U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::register_region(const char *name,
    const char *category, uint32_t *region_id) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U || name == ft_nullptr
        || region_id == ft_nullptr || this->_region_count
            >= FT_ANALYTICS_MAX_REGIONS)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_regions[this->_region_count].name = name;
    this->_regions[this->_region_count].category = category;
    this->_regions[this->_region_count].registered = FT_TRUE;
    this->_regions[this->_region_count].statistics = {};
    this->_regions[this->_region_count].sample_count = 0U;
    this->_regions[this->_region_count].sample_cursor = 0U;
    *region_id = this->_region_count;
    this->_region_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::set_export_callback(
    analytics_export_callback callback, void *user_data) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    this->_export_callback = callback;
    this->_export_user_data = user_data;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::set_trace_callback(analytics_trace_callback callback,
    void *user_data) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    this->_trace_callback = callback;
    this->_trace_user_data = user_data;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::set_enabled(ft_bool enabled) noexcept
{
    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    this->_enabled.store(enabled == FT_FALSE ? FT_FALSE : FT_TRUE,
        std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

ft_bool analytics_session::is_enabled() const noexcept
{
    return (this->_enabled.load(std::memory_order_acquire));
}

int32_t analytics_session::record_scope(uint32_t region_id,
    uint64_t inclusive_nanoseconds, uint64_t exclusive_nanoseconds) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    analytics_region_statistics *statistics;

    if (this->_initialised_state != 2U || region_id >= this->_region_count
        || this->_regions[region_id].registered == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    statistics = &this->_regions[region_id].statistics;
    statistics->invocation_count += 1U;
    statistics->inclusive_nanoseconds += inclusive_nanoseconds;
    statistics->exclusive_nanoseconds += exclusive_nanoseconds;
    if (statistics->invocation_count == 1U
        || inclusive_nanoseconds < statistics->minimum_nanoseconds)
        statistics->minimum_nanoseconds = inclusive_nanoseconds;
    if (inclusive_nanoseconds > statistics->maximum_nanoseconds)
        statistics->maximum_nanoseconds = inclusive_nanoseconds;
    this->_regions[region_id].samples[this->_regions[region_id].sample_cursor]
        = inclusive_nanoseconds;
    this->_regions[region_id].sample_cursor =
        (this->_regions[region_id].sample_cursor + 1U)
        % FT_ANALYTICS_MAX_SAMPLES;
    if (this->_regions[region_id].sample_count < FT_ANALYTICS_MAX_SAMPLES)
        this->_regions[region_id].sample_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::note_dropped_scope() noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    this->_dropped_scope_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::get_region_statistics(uint32_t region_id,
    analytics_region_statistics *statistics) const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U || statistics == ft_nullptr
        || region_id >= this->_region_count
        || this->_regions[region_id].registered == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    *statistics = this->_regions[region_id].statistics;
    if (this->_regions[region_id].sample_count != 0U)
    {
        statistics->percentile_50_nanoseconds = analytics_percentile_value(
            this->_regions[region_id].samples,
            this->_regions[region_id].sample_count, 50U);
        statistics->percentile_95_nanoseconds = analytics_percentile_value(
            this->_regions[region_id].samples,
            this->_regions[region_id].sample_count, 95U);
        statistics->percentile_99_nanoseconds = analytics_percentile_value(
            this->_regions[region_id].samples,
            this->_regions[region_id].sample_count, 99U);
    }
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::get_region_percentile(uint32_t region_id,
    uint32_t percentile, uint64_t *nanoseconds) const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U || nanoseconds == ft_nullptr
        || (percentile != 50U && percentile != 95U && percentile != 99U)
        || region_id >= this->_region_count
        || this->_regions[region_id].registered == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_regions[region_id].sample_count == 0U)
        return (FT_ERR_EMPTY);
    *nanoseconds = analytics_percentile_value(this->_regions[region_id].samples,
        this->_regions[region_id].sample_count, percentile);
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::publish_frame(
    const analytics_frame_statistics &frame) noexcept
{
    analytics_frame_statistics enriched_frame;
    analytics_export_callback callback;
    void *user_data;
    uint64_t frame_total;
    uint32_t index;

    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_initialised_state != 2U)
            return (FT_ERR_NOT_INITIALISED);
        this->_frame_samples[this->_frame_sample_cursor] = frame.duration_nanoseconds;
        this->_frame_sample_cursor = (this->_frame_sample_cursor + 1U)
            % FT_ANALYTICS_MAX_FRAME_SAMPLES;
        if (this->_frame_sample_count < FT_ANALYTICS_MAX_FRAME_SAMPLES)
            this->_frame_sample_count += 1U;
        frame_total = 0U;
        index = 0U;
        while (index < this->_frame_sample_count)
        {
            frame_total += this->_frame_samples[index];
            index += 1U;
        }
        enriched_frame = frame;
        enriched_frame.mean_duration_nanoseconds = frame_total
            / static_cast<uint64_t>(this->_frame_sample_count);
        enriched_frame.percentile_95_nanoseconds =
            analytics_frame_percentile_value(this->_frame_samples,
                this->_frame_sample_count, 95U);
        enriched_frame.percentile_99_nanoseconds =
            analytics_frame_percentile_value(this->_frame_samples,
                this->_frame_sample_count, 99U);
        this->_latest_frame = enriched_frame;
        this->_has_latest_frame = FT_TRUE;
        callback = this->_export_callback;
        user_data = this->_export_user_data;
    }
    if (callback != ft_nullptr)
        callback(enriched_frame, user_data);
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::get_latest_frame(
    analytics_frame_statistics *frame) const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U || frame == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_has_latest_frame == FT_FALSE)
        return (FT_ERR_EMPTY);
    *frame = this->_latest_frame;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::publish_trace(
    const analytics_trace_event &event) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_trace_event_count >= FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS)
    {
        this->_dropped_trace_count += 1U;
        return (FT_ERR_FULL);
    }
    this->_trace_events[this->_trace_event_count] = event;
    this->_trace_event_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::flush_exports() noexcept
{
    analytics_trace_event events[FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS];
    analytics_trace_callback trace_callback;
    void *trace_user_data;
    uint32_t event_count;
    uint32_t event_index;

    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_initialised_state != 2U)
            return (FT_ERR_NOT_INITIALISED);
        event_count = this->_trace_event_count;
        event_index = 0U;
        while (event_index < event_count)
        {
            events[event_index] = this->_trace_events[event_index];
            event_index += 1U;
        }
        this->_trace_event_count = 0U;
        trace_callback = this->_trace_callback;
        trace_user_data = this->_trace_user_data;
    }
    if (trace_callback == ft_nullptr)
        return (FT_ERR_SUCCESS);
    event_index = 0U;
    while (event_index < event_count)
    {
        trace_callback(events[event_index], trace_user_data);
        event_index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

uint64_t analytics_session::get_dropped_scope_count() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return (this->_dropped_scope_count);
}

uint64_t analytics_session::get_dropped_trace_count() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return (this->_dropped_trace_count);
}

int32_t analytics_now_nanoseconds(uint64_t *timestamp) noexcept
{
    if (timestamp == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *timestamp = analytics_clock_now();
    return (FT_ERR_SUCCESS);
}

int32_t analytics_begin_frame(analytics_session *session,
    uint64_t frame_number) noexcept
{
    if (session == ft_nullptr || session->is_enabled() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (g_analytics_thread_state.session != ft_nullptr
        && g_analytics_thread_state.scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    g_analytics_thread_state.session = session;
    g_analytics_thread_state.frame_number = frame_number;
    g_analytics_thread_state.frame_start_nanoseconds = analytics_clock_now();
    g_analytics_thread_state.instrumented_top_level_nanoseconds = 0U;
    g_analytics_thread_state.completed_scope_count = 0U;
    g_analytics_thread_state.scope_depth = 0U;
    g_analytics_thread_state.pending_event_count = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_end_frame(analytics_session *session) noexcept
{
    analytics_frame_statistics frame;
    uint64_t end_nanoseconds;
    uint32_t event_index;
    analytics_trace_event trace_event;
    int32_t first_error;
    int32_t operation_error;

    if (session == ft_nullptr || g_analytics_thread_state.session != session
        || g_analytics_thread_state.scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    end_nanoseconds = analytics_clock_now();
    analytics_init_frame_statistics(&frame, &g_analytics_thread_state,
        end_nanoseconds);
    first_error = FT_ERR_SUCCESS;
    event_index = 0U;
    while (event_index < g_analytics_thread_state.pending_event_count)
    {
        analytics_pending_event &pending_event =
            g_analytics_thread_state.pending_events[event_index];
        analytics_add_frame_breakdown(&frame, pending_event);
        operation_error = session->record_scope(pending_event.region_id,
            pending_event.inclusive_nanoseconds,
            pending_event.exclusive_nanoseconds);
        if (operation_error != FT_ERR_SUCCESS
            && first_error == FT_ERR_SUCCESS)
            first_error = operation_error;
        trace_event.frame_number = g_analytics_thread_state.frame_number;
        trace_event.flow_id = 0U;
        trace_event.region_id = pending_event.region_id;
        trace_event.start_nanoseconds = pending_event.start_nanoseconds;
        trace_event.duration_nanoseconds = pending_event.inclusive_nanoseconds;
        trace_event.exclusive_nanoseconds = pending_event.exclusive_nanoseconds;
        trace_event.thread_id = analytics_thread_id();
        operation_error = session->publish_trace(trace_event);
        if (operation_error != FT_ERR_SUCCESS
            && first_error == FT_ERR_SUCCESS)
            first_error = operation_error;
        event_index += 1U;
    }
    frame.dropped_scope_count = session->get_dropped_scope_count();
    g_analytics_thread_state.session = ft_nullptr;
    g_analytics_thread_state.pending_event_count = 0U;
    operation_error = session->publish_frame(frame);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    return (first_error);
}

int32_t analytics_end_thread_frame(analytics_session *session) noexcept
{
    analytics_frame_statistics frame;
    uint32_t event_index;
    analytics_pending_event *pending_event;
    analytics_trace_event trace_event;
    int32_t record_error;
    uint64_t end_nanoseconds;

    if (session == ft_nullptr || g_analytics_thread_state.session != session
        || g_analytics_thread_state.scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    end_nanoseconds = analytics_clock_now();
    analytics_init_frame_statistics(&frame, &g_analytics_thread_state,
        end_nanoseconds);
    event_index = 0U;
    while (event_index < g_analytics_thread_state.pending_event_count)
    {
        pending_event = &g_analytics_thread_state.pending_events[event_index];
        analytics_add_frame_breakdown(&frame, *pending_event);
        record_error = session->record_scope(pending_event->region_id,
            pending_event->inclusive_nanoseconds,
            pending_event->exclusive_nanoseconds);
        if (record_error != FT_ERR_SUCCESS)
            return (record_error);
        trace_event.frame_number = g_analytics_thread_state.frame_number;
        trace_event.flow_id = 0U;
        trace_event.region_id = pending_event->region_id;
        trace_event.start_nanoseconds = pending_event->start_nanoseconds;
        trace_event.duration_nanoseconds = pending_event->inclusive_nanoseconds;
        trace_event.exclusive_nanoseconds = pending_event->exclusive_nanoseconds;
        trace_event.thread_id = analytics_thread_id();
        if (session->publish_trace(trace_event) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        event_index += 1U;
    }
    g_analytics_thread_state.session = ft_nullptr;
    g_analytics_thread_state.pending_event_count = 0U;
    g_analytics_thread_state.completed_scope_count = 0U;
    frame.dropped_scope_count = session->get_dropped_scope_count();
    return (session->publish_frame(frame));
}

int32_t analytics_begin_scope(analytics_session *session,
    uint32_t region_id) noexcept
{
    if (session == ft_nullptr || session->is_enabled() == FT_FALSE
        || g_analytics_thread_state.session != session
        || g_analytics_thread_state.scope_depth >= FT_ANALYTICS_MAX_SCOPE_DEPTH)
        return (FT_ERR_INVALID_STATE);
    g_analytics_thread_state.scopes[g_analytics_thread_state.scope_depth].session
        = session;
    g_analytics_thread_state.scopes[g_analytics_thread_state.scope_depth].region_id
        = region_id;
    g_analytics_thread_state.scopes[g_analytics_thread_state.scope_depth].start_nanoseconds
        = analytics_clock_now();
    g_analytics_thread_state.scopes[g_analytics_thread_state.scope_depth].child_nanoseconds
        = 0U;
    g_analytics_thread_state.scope_depth += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_end_scope(analytics_session *session) noexcept
{
    analytics_scope_frame scope;
    uint64_t inclusive_nanoseconds;
    uint64_t exclusive_nanoseconds;

    if (session == ft_nullptr || g_analytics_thread_state.scope_depth == 0U
        || g_analytics_thread_state.session != session)
        return (FT_ERR_INVALID_STATE);
    g_analytics_thread_state.scope_depth -= 1U;
    scope = g_analytics_thread_state.scopes[
        g_analytics_thread_state.scope_depth];
    inclusive_nanoseconds = analytics_clock_now() - scope.start_nanoseconds;
    exclusive_nanoseconds = inclusive_nanoseconds - scope.child_nanoseconds;
    if (g_analytics_thread_state.scope_depth != 0U)
        g_analytics_thread_state.scopes[
            g_analytics_thread_state.scope_depth - 1U].child_nanoseconds
            += inclusive_nanoseconds;
    else
        g_analytics_thread_state.instrumented_top_level_nanoseconds
            += inclusive_nanoseconds;
    if (g_analytics_thread_state.pending_event_count
        >= FT_ANALYTICS_MAX_THREAD_EVENTS)
        (void)session->note_dropped_scope();
    else
    {
        analytics_pending_event &pending_event =
            g_analytics_thread_state.pending_events[
                g_analytics_thread_state.pending_event_count];
        pending_event.region_id = scope.region_id;
        pending_event.start_nanoseconds = scope.start_nanoseconds;
        pending_event.inclusive_nanoseconds = inclusive_nanoseconds;
        pending_event.exclusive_nanoseconds = exclusive_nanoseconds;
        g_analytics_thread_state.pending_event_count += 1U;
    }
    g_analytics_thread_state.completed_scope_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_begin_flow(analytics_session *session, uint64_t flow_id,
    uint32_t region_id, analytics_flow_token *token) noexcept
{
    if (session == ft_nullptr || token == ft_nullptr || flow_id == 0U
        || session->is_enabled() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    token->session = session;
    token->flow_id = flow_id;
    token->region_id = region_id;
    token->start_nanoseconds = analytics_clock_now();
    return (FT_ERR_SUCCESS);
}

int32_t analytics_end_flow(const analytics_flow_token &token) noexcept
{
    analytics_trace_event event;
    uint64_t end_nanoseconds;

    if (token.session == ft_nullptr || token.flow_id == 0U
        || token.session->is_enabled() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    end_nanoseconds = analytics_clock_now();
    if (token.session->record_scope(token.region_id,
        end_nanoseconds - token.start_nanoseconds,
        end_nanoseconds - token.start_nanoseconds) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    event.frame_number = 0U;
    event.flow_id = token.flow_id;
    event.region_id = token.region_id;
    event.start_nanoseconds = token.start_nanoseconds;
    event.duration_nanoseconds = end_nanoseconds - token.start_nanoseconds;
    event.exclusive_nanoseconds = event.duration_nanoseconds;
    event.thread_id = analytics_thread_id();
    return (token.session->publish_trace(event));
}
