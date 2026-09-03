#include "analytics.hpp"
#include <chrono>
#include <thread>

#include "../CMA/CMA.hpp"
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
    ft_bool world_active;
};

struct analytics_pending_event
{
    uint32_t region_id;
    uint64_t start_nanoseconds;
    uint64_t inclusive_nanoseconds;
    uint64_t exclusive_nanoseconds;
    ft_bool world_active;
};

struct analytics_thread_state
{
    analytics_session *session;
    uint64_t frame_number;
    uint64_t frame_start_nanoseconds;
    uint64_t instrumented_top_level_nanoseconds;
    uint64_t completed_scope_count;
    ft_bool world_active;
    uint32_t scope_depth;
    analytics_scope_frame scopes[FT_ANALYTICS_MAX_SCOPE_DEPTH];
    uint32_t pending_event_count;
    analytics_pending_event pending_events[FT_ANALYTICS_MAX_THREAD_EVENTS];
};

static const uint32_t FT_ANALYTICS_MAX_THREAD_SESSIONS = 4U;
static thread_local analytics_thread_state g_analytics_thread_states[
    FT_ANALYTICS_MAX_THREAD_SESSIONS] = {};
static thread_local analytics_thread_state *g_analytics_current_thread_state
    = ft_nullptr;

static analytics_thread_state *analytics_thread_state_for(
    analytics_session *session, ft_bool allocate) noexcept
{
    uint32_t index;

    /* The normal producer path has one active session per thread.  Keep a
     * validated pointer for that path so every scope does not linearly scan
     * the small fallback table.  The pointer is never trusted after a frame
     * or when the session no longer matches. */
    if (g_analytics_current_thread_state != ft_nullptr
        && g_analytics_current_thread_state->session == session)
        return (g_analytics_current_thread_state);

    index = 0U;
    while (index < FT_ANALYTICS_MAX_THREAD_SESSIONS)
    {
        if (g_analytics_thread_states[index].session == session)
        {
            g_analytics_current_thread_state =
                &g_analytics_thread_states[index];
            return (&g_analytics_thread_states[index]);
        }
        index += 1U;
    }
    if (allocate == FT_FALSE)
        return (ft_nullptr);
    index = 0U;
    while (index < FT_ANALYTICS_MAX_THREAD_SESSIONS)
    {
        if (g_analytics_thread_states[index].session == ft_nullptr)
        {
            g_analytics_current_thread_state =
                &g_analytics_thread_states[index];
            return (&g_analytics_thread_states[index]);
        }
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
      _trace_user_data(ft_nullptr), _export_buffers(ft_nullptr),
      _active_buffer_index(0U), _buffer_count(FT_ANALYTICS_EXPORT_BUFFER_COUNT),
      _frame_capacity(
          FT_ANALYTICS_MAX_QUEUED_FRAME_EXPORTS), _trace_capacity(
          FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS), _trace_frame_interval(1U),
      _frame_export_interval(1U),
      _overflow_policy(
          analytics_overflow_policy::DROP_NEW_WITH_COUNTER),
      _completed_buffer_indices(),
      _completed_buffer_count(0U),
      _exporter_buffer_index(FT_ANALYTICS_INVALID_BUFFER_INDEX),
      _dropped_trace_count(0U),
      _dropped_frame_export_count(0U), _dropped_scope_count(0U), _frame_samples(),
      _frame_sample_count(0U), _frame_sample_cursor(0U),
      _frame_duration_total(0U), _latest_frame(),
      _has_latest_frame(FT_FALSE), _output_file(ft_nullptr),
      _world_output_file(ft_nullptr),
      _output_format(analytics_output_format::NONE),
      _world_output_format(analytics_output_format::NONE), _export_condition(),
      _export_thread(), _export_write_mutex(), _exporter_started(FT_FALSE),
      _export_stop(FT_FALSE), _export_error(FT_ERR_SUCCESS),
      _world_active(FT_FALSE), _clock_callback(ft_nullptr),
      _clock_user_data(ft_nullptr)
{
    return ;
}

analytics_session_config::analytics_session_config() noexcept
    : output_path(ft_nullptr), world_output_path(ft_nullptr),
      output_format(analytics_output_format::NONE),
      reserved_frame_exports(FT_ANALYTICS_MAX_QUEUED_FRAME_EXPORTS),
      reserved_trace_events(FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS),
      trace_frame_interval(1U),
      frame_export_interval(1U),
      buffer_count(FT_ANALYTICS_EXPORT_BUFFER_COUNT),
      overflow_policy(analytics_overflow_policy::DROP_NEW_WITH_COUNTER),
      clock_callback(ft_nullptr), clock_user_data(ft_nullptr),
      start_exporter(FT_FALSE)
{
    return ;
}

int32_t analytics_default_session_config(
    analytics_session_config *configuration) noexcept
{
    if (configuration == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *configuration = analytics_session_config();
    return (FT_ERR_SUCCESS);
}

analytics_session::~analytics_session() noexcept
{
    int32_t error_code;

    error_code = this->destroy();
    if (error_code != FT_ERR_SUCCESS)
        std::fprintf(stderr,
            "[LIBFT][Analytics] destructor shutdown failed: %d\n",
            error_code);
    return ;
}

int32_t analytics_session::initialize() noexcept
{
    analytics_session_config configuration;

    return (this->initialize(configuration));
}

int32_t analytics_session::initialize(
    const analytics_session_config &configuration) noexcept
{
    uint32_t index;

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_initialised_state == 2U)
        return (FT_ERR_INVALID_STATE);
    index = 0U;
    while (index < FT_ANALYTICS_MAX_REGIONS)
    {
        ft_memset(this->_regions[index].name, 0,
            FT_ANALYTICS_REGION_NAME_CAPACITY);
        ft_memset(this->_regions[index].category, 0,
            FT_ANALYTICS_REGION_CATEGORY_CAPACITY);
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
    index = 0U;
    while (index < FT_ANALYTICS_MAX_EXPORT_BUFFER_COUNT)
    {
        this->_completed_buffer_indices[index] =
            FT_ANALYTICS_INVALID_BUFFER_INDEX;
        index += 1U;
    }
    this->_active_buffer_index = 0U;
    this->_buffer_count = configuration.buffer_count;
    if (this->_buffer_count == 0U)
        this->_buffer_count = FT_ANALYTICS_EXPORT_BUFFER_COUNT;
    if (this->_buffer_count > FT_ANALYTICS_MAX_EXPORT_BUFFER_COUNT)
    {
        this->_initialised_state = 1U;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_frame_capacity = configuration.reserved_frame_exports;
    this->_trace_capacity = configuration.reserved_trace_events;
    this->_trace_frame_interval = configuration.trace_frame_interval;
    this->_frame_export_interval = configuration.frame_export_interval;
    this->_overflow_policy = configuration.overflow_policy;
    this->_clock_callback = configuration.clock_callback;
    this->_clock_user_data = configuration.clock_user_data;
    if (this->_frame_capacity == 0U)
        this->_frame_capacity = FT_ANALYTICS_MAX_QUEUED_FRAME_EXPORTS;
    if (this->_trace_capacity == 0U)
        this->_trace_capacity = FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS;
    if (this->_trace_frame_interval == 0U)
        this->_trace_frame_interval = 1U;
    if (this->_frame_export_interval == 0U)
        this->_frame_export_interval = 1U;
    if (this->_frame_capacity > FT_ANALYTICS_MAX_QUEUED_FRAME_EXPORTS
        || this->_trace_capacity > FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS)
    {
        this->_initialised_state = 1U;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (static_cast<uint32_t>(this->_overflow_policy)
        > static_cast<uint32_t>(analytics_overflow_policy::FAIL_SESSION))
    {
        this->_initialised_state = 1U;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_export_buffers = static_cast<analytics_export_buffer *>(cma_malloc(
        sizeof(analytics_export_buffer) * this->_buffer_count));
    if (this->_export_buffers == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    ft_memset(this->_export_buffers, 0,
        sizeof(analytics_export_buffer) * this->_buffer_count);
    this->_completed_buffer_count = 0U;
    this->_exporter_buffer_index = FT_ANALYTICS_INVALID_BUFFER_INDEX;
    this->_dropped_trace_count = 0U;
    this->_dropped_frame_export_count = 0U;
    this->_dropped_scope_count = 0U;
    this->_frame_sample_count = 0U;
    this->_frame_sample_cursor = 0U;
    this->_frame_duration_total = 0U;
    this->_latest_frame = {};
    this->_has_latest_frame = FT_FALSE;
    this->_output_file = ft_nullptr;
    this->_world_output_file = ft_nullptr;
    this->_output_format = analytics_output_format::NONE;
    this->_world_output_format = analytics_output_format::NONE;
    this->_exporter_started = FT_FALSE;
    this->_export_stop = FT_FALSE;
    this->_export_error.store(FT_ERR_SUCCESS, std::memory_order_release);
    this->_world_active = FT_FALSE;
    if (configuration.output_path != ft_nullptr
        && configuration.output_format != analytics_output_format::NONE)
    {
        this->_output_format = configuration.output_format;
        this->_output_file = std::fopen(configuration.output_path, "wb");
        if (this->_output_file == ft_nullptr)
        {
            cma_free(this->_export_buffers);
            this->_export_buffers = ft_nullptr;
            this->_initialised_state = 1U;
            return (FT_ERR_FILE_OPEN_FAILED);
        }
    }
    if (configuration.world_output_path != ft_nullptr
        && configuration.output_format != analytics_output_format::NONE)
    {
        this->_world_output_format = configuration.output_format;
        this->_world_output_file = std::fopen(configuration.world_output_path,
            "wb");
        if (this->_world_output_file == ft_nullptr)
        {
            if (this->_output_file != ft_nullptr)
                std::fclose(this->_output_file);
            cma_free(this->_export_buffers);
            this->_export_buffers = ft_nullptr;
            this->_output_file = ft_nullptr;
            this->_initialised_state = 1U;
            return (FT_ERR_FILE_OPEN_FAILED);
        }
    }
    this->_enabled.store(FT_TRUE, std::memory_order_release);
    this->_initialised_state = 2U;
    if (configuration.start_exporter != FT_FALSE)
    {
        int32_t exporter_error;

        exporter_error = this->start_exporter_internal();
        if (exporter_error != FT_ERR_SUCCESS)
        {
            if (this->_output_file != ft_nullptr)
                std::fclose(this->_output_file);
            if (this->_world_output_file != ft_nullptr)
                std::fclose(this->_world_output_file);
            this->_output_file = ft_nullptr;
            this->_world_output_file = ft_nullptr;
            cma_free(this->_export_buffers);
            this->_export_buffers = ft_nullptr;
            this->_initialised_state = 1U;
            return (exporter_error);
        }
    }
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::destroy() noexcept
{
    int32_t stop_error;

    if (this->_initialised_state != 2U)
        return (FT_ERR_SUCCESS);
    this->_enabled.store(FT_FALSE, std::memory_order_release);
    if (this->_exporter_started != FT_FALSE)
        stop_error = this->stop_exporter_internal();
    else
        stop_error = this->flush_exports();
    std::lock_guard<std::mutex> lock(this->_mutex);
    this->_has_latest_frame = FT_FALSE;
    this->_frame_duration_total = 0U;
    if (this->_output_file != ft_nullptr)
    {
        if (std::fflush(this->_output_file) != 0
            && stop_error == FT_ERR_SUCCESS)
            stop_error = FT_ERR_IO;
        if (std::fclose(this->_output_file) != 0
            && stop_error == FT_ERR_SUCCESS)
            stop_error = FT_ERR_IO;
        this->_output_file = ft_nullptr;
    }
    if (this->_world_output_file != ft_nullptr)
    {
        if (std::fflush(this->_world_output_file) != 0
            && stop_error == FT_ERR_SUCCESS)
            stop_error = FT_ERR_IO;
        if (std::fclose(this->_world_output_file) != 0
            && stop_error == FT_ERR_SUCCESS)
            stop_error = FT_ERR_IO;
        this->_world_output_file = ft_nullptr;
    }
    if (this->_export_buffers != ft_nullptr)
    {
        cma_free(this->_export_buffers);
        this->_export_buffers = ft_nullptr;
    }
    this->_initialised_state = 1U;
    if (stop_error != FT_ERR_SUCCESS)
        return (stop_error);
    return (this->_export_error.load(std::memory_order_acquire));
}

int32_t analytics_session::start_exporter() noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    return (this->start_exporter_internal());
}

int32_t analytics_session::set_world_active(ft_bool active) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U)
        return (FT_ERR_NOT_INITIALISED);
    this->_world_active = active == FT_FALSE ? FT_FALSE : FT_TRUE;
    return (FT_ERR_SUCCESS);
}

ft_bool analytics_session::is_world_active() const noexcept
{
    return (this->_world_active.load(std::memory_order_acquire));
}

uint64_t analytics_session::clock_now() const noexcept
{
    if (this->_clock_callback != ft_nullptr)
        return (this->_clock_callback(this->_clock_user_data));
    return (analytics_clock_now());
}

uint64_t analytics_session::now_nanoseconds() const noexcept
{
    return (this->clock_now());
}

int32_t analytics_session::start_exporter_internal() noexcept
{
    if (this->_output_file == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    if (this->_exporter_started != FT_FALSE)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_export_stop = FT_FALSE;
    try
    {
        this->_export_thread = std::thread(&analytics_session::export_worker_main,
            this);
    }
    catch (...)
    {
        return (FT_ERR_NO_MEMORY);
    }
    this->_exporter_started = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::stop_exporter_internal() noexcept
{
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_exporter_started == FT_FALSE)
            return (FT_ERR_SUCCESS);
        this->_export_stop = FT_TRUE;
    }
    this->_export_condition.notify_one();
    if (this->_export_thread.joinable())
        this->_export_thread.join();
    this->_exporter_started = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

void analytics_session::export_worker_main() noexcept
{
    std::unique_lock<std::mutex> lock(this->_mutex);

    while (this->_export_stop == FT_FALSE)
    {
        this->_export_condition.wait(lock);
        if (this->_export_stop == FT_FALSE)
        {
            lock.unlock();
            if (this->flush_exports() != FT_ERR_SUCCESS)
                this->_export_error.store(FT_ERR_IO, std::memory_order_release);
            lock.lock();
        }
    }
    lock.unlock();
    if (this->flush_exports() != FT_ERR_SUCCESS)
        this->_export_error.store(FT_ERR_IO, std::memory_order_release);
    return ;
}

int32_t analytics_session::write_frame_to_file(
    const analytics_frame_statistics &frame) noexcept
{
    return (this->write_frame_to_output(frame, this->_output_file,
        this->_output_format));
}

int32_t analytics_session::write_frame_to_output(
    const analytics_frame_statistics &frame, std::FILE *output_file,
    analytics_output_format output_format) noexcept
{
    ft_string output;
    int32_t error_code;
    ft_size_t output_size;

    std::lock_guard<std::mutex> write_lock(this->_export_write_mutex);

    error_code = output.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (output_format == analytics_output_format::CSV)
        error_code = analytics_export_frame_csv(*this, frame, &output);
    else
        error_code = analytics_export_frame_json(*this, frame, &output);
    if (error_code == FT_ERR_SUCCESS)
    {
        if (output_format == analytics_output_format::JSONL)
            error_code = output.append("\n");
    }
    if (error_code == FT_ERR_SUCCESS)
    {
        output_size = output.size();
        if (output_file == ft_nullptr
            || std::fwrite(output.c_str(), 1U, output_size,
                output_file) != output_size)
            error_code = FT_ERR_IO;
    }
    if (output.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    return (error_code);
}

int32_t analytics_session::write_trace_to_file(
    const analytics_trace_event &event) noexcept
{
    return (this->write_trace_to_output(event, this->_output_file));
}

int32_t analytics_session::write_trace_to_output(
    const analytics_trace_event &event, std::FILE *output_file) noexcept
{
    ft_string output;
    int32_t error_code;
    ft_size_t output_size;

    std::lock_guard<std::mutex> write_lock(this->_export_write_mutex);

    error_code = output.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = analytics_export_trace_json(*this, &event, 1U, &output);
    if (error_code == FT_ERR_SUCCESS)
    {
        error_code = output.append("\n");
    }
    if (error_code == FT_ERR_SUCCESS)
    {
        output_size = output.size();
        if (output_file == ft_nullptr
            || std::fwrite(output.c_str(), 1U, output_size,
                output_file) != output_size)
            error_code = FT_ERR_IO;
    }
    if (output.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    return (error_code);
}

int32_t analytics_session::register_region(const char *name,
    const char *category, uint32_t *region_id) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U || name == ft_nullptr
        || region_id == ft_nullptr || this->_region_count
            >= FT_ANALYTICS_MAX_REGIONS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (ft_strlcpy(this->_regions[this->_region_count].name, name,
        FT_ANALYTICS_REGION_NAME_CAPACITY)
        >= FT_ANALYTICS_REGION_NAME_CAPACITY)
        return (FT_ERR_INVALID_ARGUMENT);
    if (category != ft_nullptr
        && ft_strlcpy(this->_regions[this->_region_count].category, category,
            FT_ANALYTICS_REGION_CATEGORY_CAPACITY)
            >= FT_ANALYTICS_REGION_CATEGORY_CAPACITY)
        return (FT_ERR_INVALID_ARGUMENT);
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

    if (this->_initialised_state != 2U
        || this->_enabled.load(std::memory_order_acquire) == FT_FALSE
        || region_id >= this->_region_count
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

int32_t analytics_session::record_scope_batch(const uint32_t *region_ids,
    const uint64_t *inclusive_nanoseconds,
    const uint64_t *exclusive_nanoseconds, uint32_t count) noexcept
{
    uint32_t index;
    analytics_region_statistics *statistics;

    if (count > FT_ANALYTICS_MAX_THREAD_EVENTS
        || ((region_ids == ft_nullptr || inclusive_nanoseconds == ft_nullptr
            || exclusive_nanoseconds == ft_nullptr) && count != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_initialised_state != 2U
        || this->_enabled.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    index = 0U;
    while (index < count)
    {
        if (region_ids[index] >= this->_region_count
            || this->_regions[region_ids[index]].registered == FT_FALSE)
            return (FT_ERR_INVALID_ARGUMENT);
        index += 1U;
    }
    index = 0U;
    while (index < count)
    {
        statistics = &this->_regions[region_ids[index]].statistics;
        statistics->invocation_count += 1U;
        statistics->inclusive_nanoseconds += inclusive_nanoseconds[index];
        statistics->exclusive_nanoseconds += exclusive_nanoseconds[index];
        if (statistics->invocation_count == 1U
            || inclusive_nanoseconds[index] < statistics->minimum_nanoseconds)
            statistics->minimum_nanoseconds = inclusive_nanoseconds[index];
        if (inclusive_nanoseconds[index] > statistics->maximum_nanoseconds)
            statistics->maximum_nanoseconds = inclusive_nanoseconds[index];
        this->_regions[region_ids[index]].samples[
            this->_regions[region_ids[index]].sample_cursor]
            = inclusive_nanoseconds[index];
        this->_regions[region_ids[index]].sample_cursor =
            (this->_regions[region_ids[index]].sample_cursor + 1U)
            % FT_ANALYTICS_MAX_SAMPLES;
        if (this->_regions[region_ids[index]].sample_count
            < FT_ANALYTICS_MAX_SAMPLES)
            this->_regions[region_ids[index]].sample_count += 1U;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::note_dropped_scope() noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != 2U
        || this->_enabled.load(std::memory_order_acquire) == FT_FALSE)
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

int32_t analytics_session::find_free_buffer_locked(
    uint32_t *buffer_index) const noexcept
{
    uint32_t candidate_index;
    uint32_t completed_index;

    if (buffer_index == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    candidate_index = 0U;
    while (candidate_index < this->_buffer_count)
    {
        if (candidate_index != this->_active_buffer_index
            && candidate_index != this->_exporter_buffer_index)
        {
            completed_index = 0U;
            while (completed_index < this->_completed_buffer_count
                && this->_completed_buffer_indices[completed_index]
                    != candidate_index)
                completed_index += 1U;
            if (completed_index == this->_completed_buffer_count)
            {
                *buffer_index = candidate_index;
                return (FT_ERR_SUCCESS);
            }
        }
        candidate_index += 1U;
    }
    return (FT_ERR_FULL);
}

int32_t analytics_session::obtain_free_buffer_locked(
    uint32_t *buffer_index) noexcept
{
    uint32_t completed_index;
    uint32_t recycled_index;

    if (buffer_index == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->find_free_buffer_locked(buffer_index) == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    if (this->_overflow_policy
        == analytics_overflow_policy::DROP_OLDEST_COMPLETED_WITH_COUNTER
        && this->_completed_buffer_count != 0U)
    {
        recycled_index = this->_completed_buffer_indices[0];
        this->_dropped_frame_export_count +=
            this->_export_buffers[recycled_index].frame_count;
        this->_dropped_trace_count +=
            this->_export_buffers[recycled_index].trace_count;
        completed_index = 1U;
        while (completed_index < this->_completed_buffer_count)
        {
            this->_completed_buffer_indices[completed_index - 1U]
                = this->_completed_buffer_indices[completed_index];
            completed_index += 1U;
        }
        this->_completed_buffer_count -= 1U;
        this->_export_buffers[recycled_index].frame_count = 0U;
        this->_export_buffers[recycled_index].trace_count = 0U;
        this->_export_buffers[recycled_index].queued_at_nanoseconds = 0U;
        *buffer_index = recycled_index;
        return (FT_ERR_SUCCESS);
    }
    if (this->_overflow_policy == analytics_overflow_policy::FAIL_SESSION)
    {
        this->_enabled.store(FT_FALSE, std::memory_order_release);
        this->_export_error.store(FT_ERR_FULL, std::memory_order_release);
    }
    return (FT_ERR_FULL);
}

int32_t analytics_session::rotate_active_buffer_locked() noexcept
{
    analytics_export_buffer *active_buffer;
    uint32_t free_buffer_index;

    active_buffer = &this->_export_buffers[this->_active_buffer_index];
    if (active_buffer->frame_count == 0U && active_buffer->trace_count == 0U)
        return (FT_ERR_SUCCESS);
    if (this->find_free_buffer_locked(&free_buffer_index) != FT_ERR_SUCCESS)
    {
        if (this->obtain_free_buffer_locked(&free_buffer_index)
            != FT_ERR_SUCCESS)
            return (FT_ERR_FULL);
    }
    this->_completed_buffer_indices[this->_completed_buffer_count]
        = this->_active_buffer_index;
    this->_completed_buffer_count += 1U;
    this->_export_buffers[this->_active_buffer_index].queued_at_nanoseconds =
        this->clock_now();
    this->_active_buffer_index = free_buffer_index;
    this->_export_buffers[free_buffer_index].frame_count = 0U;
    this->_export_buffers[free_buffer_index].trace_count = 0U;
    this->_export_buffers[free_buffer_index].queued_at_nanoseconds = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::publish_frame(
    const analytics_frame_statistics &frame) noexcept
{
    return (this->publish_frame(frame, this->is_world_active()));
}

int32_t analytics_session::publish_frame(
    const analytics_frame_statistics &frame, ft_bool world_active) noexcept
{
    analytics_frame_statistics enriched_frame;
    uint32_t index;
    int32_t result;
    ft_bool rotated;
    ft_bool export_frame;

    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_initialised_state != 2U
            || this->_enabled.load(std::memory_order_acquire) == FT_FALSE)
            return (FT_ERR_NOT_INITIALISED);
        export_frame = frame.frame_number
            % static_cast<uint64_t>(this->_frame_export_interval) == 0U
            ? FT_TRUE : FT_FALSE;
        rotated = FT_FALSE;
        if (export_frame != FT_FALSE
            && this->_export_buffers[this->_active_buffer_index].frame_count
            >= this->_frame_capacity)
        {
            if (this->rotate_active_buffer_locked() != FT_ERR_SUCCESS)
            {
                this->_dropped_frame_export_count += 1U;
                return (FT_ERR_FULL);
            }
            rotated = FT_TRUE;
        }
        if (this->_frame_sample_count == FT_ANALYTICS_MAX_FRAME_SAMPLES)
            this->_frame_duration_total -=
                this->_frame_samples[this->_frame_sample_cursor];
        this->_frame_samples[this->_frame_sample_cursor] = frame.duration_nanoseconds;
        this->_frame_sample_cursor = (this->_frame_sample_cursor + 1U)
            % FT_ANALYTICS_MAX_FRAME_SAMPLES;
        if (this->_frame_sample_count < FT_ANALYTICS_MAX_FRAME_SAMPLES)
            this->_frame_sample_count += 1U;
        this->_frame_duration_total += frame.duration_nanoseconds;
        enriched_frame = frame;
        enriched_frame.mean_duration_nanoseconds = this->_frame_duration_total
            / static_cast<uint64_t>(this->_frame_sample_count);
        /* Percentiles are deliberately calculated only for exported samples.
         * Sorting the rolling window here would put O(window^2) work on the
         * instrumented thread for every frame. */
        if (export_frame != FT_FALSE)
        {
            enriched_frame.percentile_95_nanoseconds =
                analytics_frame_percentile_value(this->_frame_samples,
                    this->_frame_sample_count, 95U);
            enriched_frame.percentile_99_nanoseconds =
                analytics_frame_percentile_value(this->_frame_samples,
                    this->_frame_sample_count, 99U);
        }
        this->_latest_frame = enriched_frame;
        this->_has_latest_frame = FT_TRUE;
        if (export_frame != FT_FALSE)
        {
            index = this->_active_buffer_index;
            this->_export_buffers[index].frames[
                this->_export_buffers[index].frame_count].value = enriched_frame;
            this->_export_buffers[index].frames[
                this->_export_buffers[index].frame_count].world_active =
                world_active == FT_FALSE ? FT_FALSE : FT_TRUE;
            this->_export_buffers[index].frame_count += 1U;
        }
        result = FT_ERR_SUCCESS;
    }
    if (result == FT_ERR_SUCCESS && rotated != FT_FALSE)
        this->_export_condition.notify_one();
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
    return (this->publish_trace(event, this->is_world_active()));
}

int32_t analytics_session::publish_trace(
    const analytics_trace_event &event, ft_bool world_active) noexcept
{
    analytics_export_buffer *active_buffer;
    ft_bool rotated;

    {
        std::lock_guard<std::mutex> lock(this->_mutex);

        if (this->_initialised_state != 2U
            || this->_enabled.load(std::memory_order_acquire) == FT_FALSE)
            return (FT_ERR_NOT_INITIALISED);
        rotated = FT_FALSE;
        active_buffer = &this->_export_buffers[this->_active_buffer_index];
        if (active_buffer->trace_count >= this->_trace_capacity)
        {
            if (this->rotate_active_buffer_locked() != FT_ERR_SUCCESS)
            {
                this->_dropped_trace_count += 1U;
                return (FT_ERR_FULL);
            }
            rotated = FT_TRUE;
            active_buffer = &this->_export_buffers[this->_active_buffer_index];
        }
        if (active_buffer->trace_count >= this->_trace_capacity)
        {
            this->_dropped_trace_count += 1U;
            return (FT_ERR_FULL);
        }
        active_buffer->traces[active_buffer->trace_count].value = event;
        active_buffer->traces[active_buffer->trace_count].world_active =
            world_active == FT_FALSE ? FT_FALSE : FT_TRUE;
        active_buffer->trace_count += 1U;
    }
    if (rotated != FT_FALSE)
        this->_export_condition.notify_one();
    return (FT_ERR_SUCCESS);
}

ft_bool analytics_session::should_capture_trace(uint64_t frame_number) const
    noexcept
{
    uint32_t interval;

    interval = this->_trace_frame_interval;
    if (interval == 0U)
        interval = 1U;
    return (frame_number % static_cast<uint64_t>(interval) == 0U
        ? FT_TRUE : FT_FALSE);
}

int32_t analytics_session::flush_exports() noexcept
{
    analytics_export_buffer *export_buffer;
    uint32_t export_buffer_index;
    uint32_t event_index;
    analytics_export_callback export_callback;
    analytics_trace_callback trace_callback;
    void *export_user_data;
    void *trace_user_data;
    std::FILE *world_output_file;
    int32_t first_error;
    int32_t operation_error;

    first_error = FT_ERR_SUCCESS;

    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            if (this->_initialised_state != 2U)
                return (FT_ERR_NOT_INITIALISED);
            if (this->_completed_buffer_count == 0U
                && this->rotate_active_buffer_locked() != FT_ERR_SUCCESS)
                break;
            if (this->_completed_buffer_count == 0U)
                break;
            export_buffer_index = this->_completed_buffer_indices[0];
            event_index = 1U;
            while (event_index < this->_completed_buffer_count)
            {
                this->_completed_buffer_indices[event_index - 1U]
                    = this->_completed_buffer_indices[event_index];
                event_index += 1U;
            }
            this->_completed_buffer_count -= 1U;
            this->_exporter_buffer_index = export_buffer_index;
            export_callback = this->_export_callback;
            export_user_data = this->_export_user_data;
            trace_callback = this->_trace_callback;
            trace_user_data = this->_trace_user_data;
            world_output_file = this->_world_output_file;
        }
        export_buffer = &this->_export_buffers[export_buffer_index];
        event_index = 0U;
        while (event_index < export_buffer->frame_count)
        {
            if (export_callback != ft_nullptr)
                export_callback(export_buffer->frames[event_index].value,
                    export_user_data);
            else
            {
                if (export_buffer->frames[event_index].world_active == FT_FALSE
                    && this->_output_file != ft_nullptr)
                {
                    operation_error = this->write_frame_to_output(
                        export_buffer->frames[event_index].value,
                        this->_output_file, this->_output_format);
                    if (operation_error != FT_ERR_SUCCESS
                        && first_error == FT_ERR_SUCCESS)
                        first_error = operation_error;
                }
                if (export_buffer->frames[event_index].world_active != FT_FALSE
                    && world_output_file != ft_nullptr)
                {
                    operation_error = this->write_frame_to_output(
                        export_buffer->frames[event_index].value,
                        world_output_file, this->_world_output_format);
                    if (operation_error != FT_ERR_SUCCESS
                        && first_error == FT_ERR_SUCCESS)
                        first_error = operation_error;
                }
            }
            event_index += 1U;
        }
        event_index = 0U;
        while (event_index < export_buffer->trace_count)
        {
            if (trace_callback != ft_nullptr)
                trace_callback(export_buffer->traces[event_index].value,
                    trace_user_data);
            else
            {
                if (export_buffer->traces[event_index].world_active == FT_FALSE
                    && this->_output_file != ft_nullptr)
                {
                    operation_error = this->write_trace_to_output(
                        export_buffer->traces[event_index].value,
                        this->_output_file);
                    if (operation_error != FT_ERR_SUCCESS
                        && first_error == FT_ERR_SUCCESS)
                        first_error = operation_error;
                }
                if (export_buffer->traces[event_index].world_active != FT_FALSE
                    && world_output_file != ft_nullptr)
                {
                    operation_error = this->write_trace_to_output(
                        export_buffer->traces[event_index].value,
                        world_output_file);
                    if (operation_error != FT_ERR_SUCCESS
                        && first_error == FT_ERR_SUCCESS)
                        first_error = operation_error;
                }
            }
            event_index += 1U;
        }
        {
            std::lock_guard<std::mutex> lock(this->_mutex);
            export_buffer->frame_count = 0U;
            export_buffer->trace_count = 0U;
            export_buffer->queued_at_nanoseconds = 0U;
            this->_exporter_buffer_index = FT_ANALYTICS_INVALID_BUFFER_INDEX;
        }
        this->_export_condition.notify_one();
    }
    if (first_error != FT_ERR_SUCCESS)
        this->_export_error.store(first_error, std::memory_order_release);
    return (first_error);
}

int32_t analytics_session::get_export_error() const noexcept
{
    return (this->_export_error.load(std::memory_order_acquire));
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

uint32_t analytics_session::get_export_queue_depth() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return (this->_completed_buffer_count);
}

uint64_t analytics_session::get_oldest_export_queue_age_nanoseconds() const
    noexcept
{
    uint64_t queued_at;
    uint64_t now;

    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_initialised_state != 2U || this->_export_buffers == ft_nullptr
        || this->_completed_buffer_count == 0U)
        return (0U);
    queued_at = this->_export_buffers[this->_completed_buffer_indices[0U]]
        .queued_at_nanoseconds;
    if (queued_at == 0U)
        return (0U);
    now = this->clock_now();
    return (now >= queued_at ? now - queued_at : 0U);
}

uint32_t analytics_session::get_active_frame_count() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_initialised_state != 2U
        || this->_export_buffers == ft_nullptr)
        return (0U);
    return (this->_export_buffers[this->_active_buffer_index].frame_count);
}

uint32_t analytics_session::get_active_trace_count() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    if (this->_initialised_state != 2U
        || this->_export_buffers == ft_nullptr)
        return (0U);
    return (this->_export_buffers[this->_active_buffer_index].trace_count);
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
    g_analytics_current_thread_state = thread_state;
    thread_state->frame_number = frame_number;
    thread_state->frame_start_nanoseconds = session->now_nanoseconds();
    thread_state->instrumented_top_level_nanoseconds = 0U;
    thread_state->completed_scope_count = 0U;
    thread_state->world_active = session->is_world_active();
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
    uint32_t region_ids[FT_ANALYTICS_MAX_THREAD_EVENTS];
    uint64_t inclusive_nanoseconds[FT_ANALYTICS_MAX_THREAD_EVENTS];
    uint64_t exclusive_nanoseconds[FT_ANALYTICS_MAX_THREAD_EVENTS];
    analytics_trace_event trace_event;
    int32_t first_error;
    int32_t operation_error;

    thread_state = analytics_thread_state_for(session, FT_FALSE);
    if (session == ft_nullptr || thread_state == ft_nullptr
        || thread_state->scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    end_nanoseconds = session->now_nanoseconds();
    analytics_init_frame_statistics(&frame, thread_state,
        end_nanoseconds);
    first_error = FT_ERR_SUCCESS;
    event_index = 0U;
    while (event_index < thread_state->pending_event_count)
    {
        analytics_pending_event &pending_event =
            thread_state->pending_events[event_index];
        analytics_add_frame_breakdown(&frame, pending_event);
        region_ids[event_index] = pending_event.region_id;
        inclusive_nanoseconds[event_index] = pending_event.inclusive_nanoseconds;
        exclusive_nanoseconds[event_index] = pending_event.exclusive_nanoseconds;
        event_index += 1U;
    }
    operation_error = session->record_scope_batch(region_ids,
        inclusive_nanoseconds, exclusive_nanoseconds,
        thread_state->pending_event_count);
    if (operation_error != FT_ERR_SUCCESS
        && first_error == FT_ERR_SUCCESS)
        first_error = operation_error;
    event_index = 0U;
    while (event_index < thread_state->pending_event_count)
    {
        analytics_pending_event &pending_event =
            thread_state->pending_events[event_index];
        if (session->should_capture_trace(thread_state->frame_number)
            != FT_FALSE)
        {
            trace_event.frame_number = thread_state->frame_number;
            trace_event.flow_id = 0U;
            trace_event.region_id = pending_event.region_id;
            trace_event.start_nanoseconds = pending_event.start_nanoseconds;
            trace_event.duration_nanoseconds =
                pending_event.inclusive_nanoseconds;
            trace_event.exclusive_nanoseconds =
                pending_event.exclusive_nanoseconds;
            trace_event.thread_id = analytics_thread_id();
            operation_error = session->publish_trace(trace_event,
                pending_event.world_active);
            if (operation_error != FT_ERR_SUCCESS
                && first_error == FT_ERR_SUCCESS)
                first_error = operation_error;
        }
        event_index += 1U;
    }
    frame.dropped_scope_count = session->get_dropped_scope_count();
    thread_state->session = ft_nullptr;
    if (g_analytics_current_thread_state == thread_state)
        g_analytics_current_thread_state = ft_nullptr;
    thread_state->pending_event_count = 0U;
    operation_error = session->publish_frame(frame, thread_state->world_active);
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
    uint32_t region_ids[FT_ANALYTICS_MAX_THREAD_EVENTS];
    uint64_t inclusive_nanoseconds[FT_ANALYTICS_MAX_THREAD_EVENTS];
    uint64_t exclusive_nanoseconds[FT_ANALYTICS_MAX_THREAD_EVENTS];

    thread_state = analytics_thread_state_for(session, FT_FALSE);
    if (session == ft_nullptr || thread_state == ft_nullptr
        || thread_state->scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    end_nanoseconds = session->now_nanoseconds();
    analytics_init_frame_statistics(&frame, thread_state,
        end_nanoseconds);
    event_index = 0U;
    while (event_index < thread_state->pending_event_count)
    {
        pending_event = &thread_state->pending_events[event_index];
        analytics_add_frame_breakdown(&frame, *pending_event);
        region_ids[event_index] = pending_event->region_id;
        inclusive_nanoseconds[event_index] = pending_event->inclusive_nanoseconds;
        exclusive_nanoseconds[event_index] = pending_event->exclusive_nanoseconds;
        event_index += 1U;
    }
    record_error = session->record_scope_batch(region_ids,
        inclusive_nanoseconds, exclusive_nanoseconds,
        thread_state->pending_event_count);
    if (record_error != FT_ERR_SUCCESS)
        return (record_error);
    event_index = 0U;
    while (event_index < thread_state->pending_event_count)
    {
        pending_event = &thread_state->pending_events[event_index];
        if (session->should_capture_trace(thread_state->frame_number)
            != FT_FALSE)
        {
            trace_event.frame_number = thread_state->frame_number;
            trace_event.flow_id = 0U;
            trace_event.region_id = pending_event->region_id;
            trace_event.start_nanoseconds = pending_event->start_nanoseconds;
            trace_event.duration_nanoseconds =
                pending_event->inclusive_nanoseconds;
            trace_event.exclusive_nanoseconds =
                pending_event->exclusive_nanoseconds;
            trace_event.thread_id = analytics_thread_id();
            if (session->publish_trace(trace_event,
                    pending_event->world_active) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
        }
        event_index += 1U;
    }
    thread_state->session = ft_nullptr;
    thread_state->pending_event_count = 0U;
    thread_state->completed_scope_count = 0U;
    frame.dropped_scope_count = session->get_dropped_scope_count();
    return (session->publish_frame(frame, thread_state->world_active));
}

int32_t analytics_begin_scope(analytics_session *session,
    uint32_t region_id) noexcept
{
    return (analytics_begin_scope_at(session, region_id,
        session == ft_nullptr ? analytics_clock_now()
        : session->now_nanoseconds()));
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
    /* Classification is intentionally captured at scope start.  Although
     * Minecraft normally changes it between frames, the public API permits a
     * caller to transition it within a frame and the recorded scope must
     * retain the state it actually started in. */
    thread_state->scopes[thread_state->scope_depth].world_active =
        session->is_world_active();
    thread_state->scope_depth += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_end_scope(analytics_session *session) noexcept
{
    return (analytics_end_scope_at(session, session == ft_nullptr
        ? analytics_clock_now() : session->now_nanoseconds()));
}

int32_t analytics_end_scope_at(analytics_session *session,
    uint64_t end_nanoseconds) noexcept
{
    analytics_thread_state *thread_state;
    analytics_scope_frame scope;
    uint64_t inclusive_nanoseconds;
    uint64_t exclusive_nanoseconds;
    int32_t drop_error;

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
    {
        drop_error = session->note_dropped_scope();
        if (drop_error != FT_ERR_SUCCESS)
            return (drop_error);
    }
    else
    {
        analytics_pending_event &pending_event =
            thread_state->pending_events[thread_state->pending_event_count];
        pending_event.region_id = scope.region_id;
        pending_event.start_nanoseconds = scope.start_nanoseconds;
        pending_event.inclusive_nanoseconds = inclusive_nanoseconds;
        pending_event.exclusive_nanoseconds = exclusive_nanoseconds;
        pending_event.world_active = scope.world_active;
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
    token->start_nanoseconds = session->now_nanoseconds();
    token->world_active = session->is_world_active();
    return (FT_ERR_SUCCESS);
}

int32_t analytics_end_flow(const analytics_flow_token &token) noexcept
{
    analytics_trace_event event;
    uint64_t end_nanoseconds;

    if (token.session == ft_nullptr || token.flow_id == 0U
        || token.session->is_enabled() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    end_nanoseconds = token.session->now_nanoseconds();
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
    return (token.session->publish_trace(event, token.world_active));
}

int32_t analytics_export_frame_json(const analytics_session &session,
    const analytics_frame_statistics &frame, ft_string *output) noexcept
{
    ft_string temporary;
    int32_t error_code;
    uint32_t index;
    const char *region_name;
    int32_t destroy_error;

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
        destroy_error = temporary.destroy();
        if (error_code == FT_ERR_SUCCESS)
            error_code = destroy_error;
        return (error_code);
    }
    error_code = analytics_commit_output(output, temporary);
    if (error_code != FT_ERR_SUCCESS)
    {
        destroy_error = temporary.destroy();
        if (error_code == FT_ERR_SUCCESS)
            error_code = destroy_error;
    }
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
    int32_t destroy_error;

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
        destroy_error = temporary.destroy();
        if (error_code == FT_ERR_SUCCESS)
            error_code = destroy_error;
        return (error_code);
    }
    error_code = analytics_commit_output(output, temporary);
    if (error_code != FT_ERR_SUCCESS)
    {
        destroy_error = temporary.destroy();
        if (error_code == FT_ERR_SUCCESS)
            error_code = destroy_error;
    }
    return (error_code);
}

int32_t analytics_export_frame_csv(const analytics_session &session,
    const analytics_frame_statistics &frame, ft_string *output) noexcept
{
    ft_string temporary;
    int32_t error_code;
    uint32_t index;
    const char *region_name;
    int32_t destroy_error;

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
        destroy_error = temporary.destroy();
        if (error_code == FT_ERR_SUCCESS)
            error_code = destroy_error;
        return (error_code);
    }
    error_code = analytics_commit_output(output, temporary);
    if (error_code != FT_ERR_SUCCESS)
    {
        destroy_error = temporary.destroy();
        if (error_code == FT_ERR_SUCCESS)
            error_code = destroy_error;
    }
    return (error_code);
}
