#include "analytics.hpp"
#include <chrono>
#include <thread>

#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../Printf/printf.hpp"

static int32_t analytics_append_text(ft_string &output,
    const char *text) noexcept
{
    if (text == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (output.append(text));
}

static int32_t analytics_append_u64(ft_string &output, uint64_t value) noexcept
{
    char buffer[64];
    int32_t length;

    length = pf_snprintf(buffer, sizeof(buffer), FT_UINT64_DECIMAL_FORMAT,
        value);
    if (length < 0 || static_cast<ft_size_t>(length) >= sizeof(buffer))
        return (FT_ERR_INVALID_ARGUMENT);
    return (output.append(buffer, static_cast<ft_size_t>(length)));
}

static int32_t analytics_append_u32(ft_string &output, uint32_t value) noexcept
{
    char buffer[32];
    int32_t length;

    length = pf_snprintf(buffer, sizeof(buffer), "%u", value);
    if (length < 0 || static_cast<ft_size_t>(length) >= sizeof(buffer))
        return (FT_ERR_INVALID_ARGUMENT);
    return (output.append(buffer, static_cast<ft_size_t>(length)));
}

static int32_t analytics_commit_output(ft_string *output,
    ft_string &temporary) noexcept
{
    if (output == ft_nullptr || output->is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    return (output->move(temporary));
}

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

static const uint32_t FT_ANALYTICS_MAX_THREAD_SESSIONS = 4U;
static thread_local analytics_thread_state g_analytics_thread_states[
    FT_ANALYTICS_MAX_THREAD_SESSIONS] = {};

static analytics_thread_state *analytics_thread_state_for(
    analytics_session *session, ft_bool allocate) noexcept
{
    uint32_t index;

    index = 0U;
    while (index < FT_ANALYTICS_MAX_THREAD_SESSIONS)
    {
        if (g_analytics_thread_states[index].session == session)
            return (&g_analytics_thread_states[index]);
        index += 1U;
    }
    if (allocate == FT_FALSE)
        return (ft_nullptr);
    index = 0U;
    while (index < FT_ANALYTICS_MAX_THREAD_SESSIONS)
    {
        if (g_analytics_thread_states[index].session == ft_nullptr)
            return (&g_analytics_thread_states[index]);
        index += 1U;
    }
    return (ft_nullptr);
}

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
      _dropped_trace_count(0U), _frame_exports(), _frame_export_count(0U),
      _dropped_frame_export_count(0U), _dropped_scope_count(0U), _frame_samples(),
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
    this->_frame_export_count = 0U;
    this->_dropped_frame_export_count = 0U;
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
    this->_frame_export_count = 0U;
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

int32_t analytics_session::get_region_name(uint32_t region_id,
    const char **name) const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U || name == ft_nullptr
        || region_id >= this->_region_count
        || this->_regions[region_id].registered == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    *name = this->_regions[region_id].name;
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
    uint64_t frame_total;
    uint32_t index;
    int32_t result;

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
        if (this->_frame_export_count >= FT_ANALYTICS_MAX_QUEUED_FRAME_EXPORTS)
        {
            this->_dropped_frame_export_count += 1U;
            result = FT_ERR_FULL;
        }
        else
        {
            this->_frame_exports[this->_frame_export_count] = enriched_frame;
            this->_frame_export_count += 1U;
            result = FT_ERR_SUCCESS;
        }
    }
    return (result);
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
    analytics_frame_statistics frame;
    analytics_trace_event event;
    analytics_export_callback export_callback;
    analytics_trace_callback trace_callback;
    void *export_user_data;
    void *trace_user_data;

    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (this->_initialised_state != 2U)
                return (FT_ERR_NOT_INITIALISED);
            if (this->_frame_export_count == 0U)
                break;
            frame = this->_frame_exports[0];
            uint32_t index = 1U;
            while (index < this->_frame_export_count)
            {
                this->_frame_exports[index - 1U] = this->_frame_exports[index];
                index += 1U;
            }
            this->_frame_export_count -= 1U;
            export_callback = this->_export_callback;
            export_user_data = this->_export_user_data;
        }
        if (export_callback != ft_nullptr)
            export_callback(frame, export_user_data);
    }
    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (this->_initialised_state != 2U)
                return (FT_ERR_NOT_INITIALISED);
            if (this->_trace_event_count == 0U)
                break;
            event = this->_trace_events[0];
            uint32_t index = 1U;
            while (index < this->_trace_event_count)
            {
                this->_trace_events[index - 1U] = this->_trace_events[index];
                index += 1U;
            }
            this->_trace_event_count -= 1U;
            trace_callback = this->_trace_callback;
            trace_user_data = this->_trace_user_data;
        }
        if (trace_callback != ft_nullptr)
            trace_callback(event, trace_user_data);
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

uint64_t analytics_session::get_dropped_frame_export_count() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return (this->_dropped_frame_export_count);
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
    analytics_thread_state *thread_state;

    if (session == ft_nullptr || session->is_enabled() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    thread_state = analytics_thread_state_for(session, FT_TRUE);
    if (thread_state == ft_nullptr)
        return (FT_ERR_FULL);
    if (thread_state->scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    thread_state->session = session;
    thread_state->frame_number = frame_number;
    thread_state->frame_start_nanoseconds = analytics_clock_now();
    thread_state->instrumented_top_level_nanoseconds = 0U;
    thread_state->completed_scope_count = 0U;
    thread_state->scope_depth = 0U;
    thread_state->pending_event_count = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_end_frame(analytics_session *session) noexcept
{
    analytics_thread_state *thread_state;
    analytics_frame_statistics frame;
    uint64_t end_nanoseconds;
    uint32_t event_index;
    analytics_trace_event trace_event;
    int32_t first_error;
    int32_t operation_error;

    thread_state = analytics_thread_state_for(session, FT_FALSE);
    if (session == ft_nullptr || thread_state == ft_nullptr
        || thread_state->scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    end_nanoseconds = analytics_clock_now();
    analytics_init_frame_statistics(&frame, thread_state,
        end_nanoseconds);
    first_error = FT_ERR_SUCCESS;
    event_index = 0U;
    while (event_index < thread_state->pending_event_count)
    {
        analytics_pending_event &pending_event =
            thread_state->pending_events[event_index];
        analytics_add_frame_breakdown(&frame, pending_event);
        operation_error = session->record_scope(pending_event.region_id,
            pending_event.inclusive_nanoseconds,
            pending_event.exclusive_nanoseconds);
        if (operation_error != FT_ERR_SUCCESS
            && first_error == FT_ERR_SUCCESS)
            first_error = operation_error;
        trace_event.frame_number = thread_state->frame_number;
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
    thread_state->session = ft_nullptr;
    thread_state->pending_event_count = 0U;
    operation_error = session->publish_frame(frame);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    return (first_error);
}

int32_t analytics_end_thread_frame(analytics_session *session) noexcept
{
    analytics_thread_state *thread_state;
    analytics_frame_statistics frame;
    uint32_t event_index;
    analytics_pending_event *pending_event;
    analytics_trace_event trace_event;
    int32_t record_error;
    uint64_t end_nanoseconds;

    thread_state = analytics_thread_state_for(session, FT_FALSE);
    if (session == ft_nullptr || thread_state == ft_nullptr
        || thread_state->scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    end_nanoseconds = analytics_clock_now();
    analytics_init_frame_statistics(&frame, thread_state,
        end_nanoseconds);
    event_index = 0U;
    while (event_index < thread_state->pending_event_count)
    {
        pending_event = &thread_state->pending_events[event_index];
        analytics_add_frame_breakdown(&frame, *pending_event);
        record_error = session->record_scope(pending_event->region_id,
            pending_event->inclusive_nanoseconds,
            pending_event->exclusive_nanoseconds);
        if (record_error != FT_ERR_SUCCESS)
            return (record_error);
        trace_event.frame_number = thread_state->frame_number;
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
    thread_state->session = ft_nullptr;
    thread_state->pending_event_count = 0U;
    thread_state->completed_scope_count = 0U;
    frame.dropped_scope_count = session->get_dropped_scope_count();
    return (session->publish_frame(frame));
}

int32_t analytics_begin_scope(analytics_session *session,
    uint32_t region_id) noexcept
{
    return (analytics_begin_scope_at(session, region_id,
        analytics_clock_now()));
}

int32_t analytics_begin_scope_at(analytics_session *session,
    uint32_t region_id, uint64_t start_nanoseconds) noexcept
{
    analytics_thread_state *thread_state;

    thread_state = analytics_thread_state_for(session, FT_FALSE);
    if (session == ft_nullptr || session->is_enabled() == FT_FALSE
        || thread_state == ft_nullptr
        || thread_state->scope_depth >= FT_ANALYTICS_MAX_SCOPE_DEPTH)
        return (FT_ERR_INVALID_STATE);
    thread_state->scopes[thread_state->scope_depth].session
        = session;
    thread_state->scopes[thread_state->scope_depth].region_id
        = region_id;
    thread_state->scopes[thread_state->scope_depth].start_nanoseconds
        = start_nanoseconds;
    thread_state->scopes[thread_state->scope_depth].child_nanoseconds
        = 0U;
    thread_state->scope_depth += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_end_scope(analytics_session *session) noexcept
{
    return (analytics_end_scope_at(session, analytics_clock_now()));
}

int32_t analytics_end_scope_at(analytics_session *session,
    uint64_t end_nanoseconds) noexcept
{
    analytics_thread_state *thread_state;
    analytics_scope_frame scope;
    uint64_t inclusive_nanoseconds;
    uint64_t exclusive_nanoseconds;

    thread_state = analytics_thread_state_for(session, FT_FALSE);
    if (session == ft_nullptr || thread_state == ft_nullptr
        || thread_state->scope_depth == 0U)
        return (FT_ERR_INVALID_STATE);
    thread_state->scope_depth -= 1U;
    scope = thread_state->scopes[thread_state->scope_depth];
    inclusive_nanoseconds = end_nanoseconds - scope.start_nanoseconds;
    exclusive_nanoseconds = inclusive_nanoseconds - scope.child_nanoseconds;
    if (thread_state->scope_depth != 0U)
        thread_state->scopes[thread_state->scope_depth - 1U].child_nanoseconds
            += inclusive_nanoseconds;
    else
        thread_state->instrumented_top_level_nanoseconds
            += inclusive_nanoseconds;
    if (thread_state->pending_event_count
        >= FT_ANALYTICS_MAX_THREAD_EVENTS)
        (void)session->note_dropped_scope();
    else
    {
        analytics_pending_event &pending_event =
            thread_state->pending_events[thread_state->pending_event_count];
        pending_event.region_id = scope.region_id;
        pending_event.start_nanoseconds = scope.start_nanoseconds;
        pending_event.inclusive_nanoseconds = inclusive_nanoseconds;
        pending_event.exclusive_nanoseconds = exclusive_nanoseconds;
        thread_state->pending_event_count += 1U;
    }
    thread_state->completed_scope_count += 1U;
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

int32_t analytics_export_frame_json(const analytics_session &session,
    const analytics_frame_statistics &frame, ft_string *output) noexcept
{
    ft_string temporary;
    int32_t error_code;
    uint32_t index;
    const char *region_name;

    if (output == ft_nullptr || frame.breakdown_count
        > FT_ANALYTICS_MAX_FRAME_BREAKDOWN)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = temporary.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = analytics_append_text(temporary, "{\"frame\":");
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_u64(temporary, frame.frame_number);
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_text(temporary, ",\"duration_ns\":");
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_u64(temporary,
            frame.duration_nanoseconds);
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_text(temporary,
            ",\"mean_duration_ns\":");
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_u64(temporary,
            frame.mean_duration_nanoseconds);
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_text(temporary,
            ",\"p95_duration_ns\":");
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_u64(temporary,
            frame.percentile_95_nanoseconds);
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_text(temporary,
            ",\"p99_duration_ns\":");
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_u64(temporary,
            frame.percentile_99_nanoseconds);
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_text(temporary,
            ",\"uninstrumented_ns\":");
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_u64(temporary,
            frame.uninstrumented_nanoseconds);
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_text(temporary, ",\"breakdown\":[");
    index = 0U;
    while (error_code == FT_ERR_SUCCESS && index < frame.breakdown_count)
    {
        region_name = ft_nullptr;
        error_code = session.get_region_name(
            frame.breakdown[index].region_id, &region_name);
        if (error_code == FT_ERR_SUCCESS && index != 0U)
            error_code = analytics_append_text(temporary, ",");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary,
                "{\"region_id\":");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u32(temporary,
                frame.breakdown[index].region_id);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary,
                ",\"invocations\":");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u64(temporary,
                frame.breakdown[index].invocation_count);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary,
                ",\"inclusive_ns\":");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u64(temporary,
                frame.breakdown[index].inclusive_nanoseconds);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary,
                ",\"exclusive_ns\":");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u64(temporary,
                frame.breakdown[index].exclusive_nanoseconds);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary, "}");
        index += 1U;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_text(temporary, "]}");
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)temporary.destroy();
        return (error_code);
    }
    error_code = analytics_commit_output(output, temporary);
    if (error_code != FT_ERR_SUCCESS)
        (void)temporary.destroy();
    return (error_code);
}

int32_t analytics_export_trace_json(const analytics_session &session,
    const analytics_trace_event *events, uint32_t event_count,
    ft_string *output) noexcept
{
    ft_string temporary;
    int32_t error_code;
    uint32_t index;
    const char *region_name;

    if (events == ft_nullptr && event_count != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = temporary.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = analytics_append_text(temporary, "[");
    index = 0U;
    while (error_code == FT_ERR_SUCCESS && index < event_count)
    {
        region_name = ft_nullptr;
        error_code = session.get_region_name(events[index].region_id,
            &region_name);
        if (error_code == FT_ERR_SUCCESS && index != 0U)
            error_code = analytics_append_text(temporary, ",");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary,
                "{\"name\":\"region-");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u32(temporary,
                events[index].region_id);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary,
                "\",\"cat\":\"analytics\",\"ph\":\"X\",\"ts\":");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u64(temporary,
                events[index].start_nanoseconds / 1000U);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary, ",\"dur\":");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u64(temporary,
                events[index].duration_nanoseconds / 1000U);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary,
                ",\"pid\":1,\"tid\":");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u32(temporary,
                events[index].thread_id);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary, "}");
        index += 1U;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = analytics_append_text(temporary, "]");
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)temporary.destroy();
        return (error_code);
    }
    error_code = analytics_commit_output(output, temporary);
    if (error_code != FT_ERR_SUCCESS)
        (void)temporary.destroy();
    return (error_code);
}

int32_t analytics_export_frame_csv(const analytics_session &session,
    const analytics_frame_statistics &frame, ft_string *output) noexcept
{
    ft_string temporary;
    int32_t error_code;
    uint32_t index;
    const char *region_name;

    if (frame.breakdown_count > FT_ANALYTICS_MAX_FRAME_BREAKDOWN)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = temporary.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = analytics_append_text(temporary,
        "frame,region_id,invocations,inclusive_ns,exclusive_ns\n");
    index = 0U;
    while (error_code == FT_ERR_SUCCESS && index < frame.breakdown_count)
    {
        region_name = ft_nullptr;
        error_code = session.get_region_name(
            frame.breakdown[index].region_id, &region_name);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u64(temporary, frame.frame_number);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary, ",");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u32(temporary,
                frame.breakdown[index].region_id);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary, ",");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u64(temporary,
                frame.breakdown[index].invocation_count);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary, ",");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u64(temporary,
                frame.breakdown[index].inclusive_nanoseconds);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary, ",");
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_u64(temporary,
                frame.breakdown[index].exclusive_nanoseconds);
        if (error_code == FT_ERR_SUCCESS)
            error_code = analytics_append_text(temporary, "\n");
        index += 1U;
    }
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)temporary.destroy();
        return (error_code);
    }
    error_code = analytics_commit_output(output, temporary);
    if (error_code != FT_ERR_SUCCESS)
        (void)temporary.destroy();
    return (error_code);
}
