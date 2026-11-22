#include "analytics.hpp"
#include <charconv>
#include <chrono>
#include <limits>
#include <type_traits>
#include <thread>

#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "../Template/algorithm.hpp"

template <typename Part>
static constexpr std::size_t analytics_record_part_capacity() noexcept
{
    if constexpr (std::is_array<Part>::value)
        return (std::extent<Part>::value - 1U);
    else
        return (std::numeric_limits<Part>::digits10 + 1U);
}

template <std::size_t Size>
static char *analytics_write_record_part(char *destination, const char (&text)[Size]) noexcept
{
    ft_memcpy_fixed<Size - 1U>(destination, text);
    return (destination + Size - 1U);
}

template <typename Number>
static char *analytics_write_record_part(char *destination, Number value) noexcept
{
    static_assert(std::is_unsigned<Number>::value);
    return (std::to_chars(destination,
        destination + analytics_record_part_capacity<Number>(), value).ptr);
}

template <typename Writer, typename... Parts>
static int32_t analytics_write_record(Writer write, const Parts &...parts) noexcept
{
    char record[(analytics_record_part_capacity<Parts>() + ...)];
    char *cursor = record;
    ((cursor = analytics_write_record_part(cursor, parts)), ...);
    return (write(record, static_cast<ft_size_t>(cursor - record)));
}

template <typename... Parts>
static int32_t analytics_append_record(ft_string &output, const Parts &...parts) noexcept
{
    return (analytics_write_record([&output](const char *record, ft_size_t length) noexcept
    {
        return (output.append(record, length));
    }, parts...));
}

template <typename Writer, std::size_t PrefixSize, std::size_t SuffixSize>
static int32_t analytics_write_trace_record(Writer write, const analytics_trace_event &event,
    const char (&prefix)[PrefixSize], const char (&suffix)[SuffixSize]) noexcept
{
    return (analytics_write_record(write, prefix, "{\"name\":\"region-", event.region_id,
        "\",\"cat\":\"analytics\",\"ph\":\"X\",\"ts\":", event.start_nanoseconds / 1000U,
        ",\"dur\":", event.duration_nanoseconds / 1000U, ",\"pid\":1,\"tid\":",
        event.thread_id, "}", suffix));
}

static int32_t analytics_write_file(std::FILE *output_file,
    const char *data, ft_size_t data_size) noexcept
{
    ft_size_t written_size;
    ft_size_t remaining_size;
    const char *write_data;

    if (output_file == ft_nullptr
        || (data == ft_nullptr && data_size != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    written_size = 0U;
    write_data = data;
    while (written_size < data_size)
    {
        remaining_size = data_size - written_size;
        remaining_size = std::fwrite(write_data + written_size, 1U,
            remaining_size, output_file);
        if (remaining_size == 0U)
            return (FT_ERR_IO);
        written_size += remaining_size;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t analytics_commit_output(ft_string *output,
    ft_string &temporary) noexcept
{
    if (output == ft_nullptr || output->is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    return (output->move(temporary));
}

static void analytics_copy_samples(uint64_t *destination, const uint64_t *samples,
    uint32_t sample_count) noexcept
{
    ft_memcpy(destination, samples, static_cast<ft_size_t>(sample_count) * sizeof(uint64_t));
    return ;
}

static uint64_t analytics_percentile_value(const uint64_t *sorted_samples,
    uint32_t sample_count, uint32_t percentile) noexcept
{
    return (sorted_samples[(sample_count - 1U) * percentile / 100U]);
}

struct analytics_scope_frame
{
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
    uint32_t runtime_scope_count;
    analytics_pending_event pending_events[FT_ANALYTICS_MAX_THREAD_EVENTS];
};

static const uint32_t FT_ANALYTICS_MAX_THREAD_SESSIONS = 4U;
static const uint32_t FT_ANALYTICS_MAX_RUNTIME_SCOPES_PER_FRAME =
    FT_ANALYTICS_MAX_THREAD_EVENTS / 2U;
static thread_local analytics_thread_state g_analytics_thread_states[
    FT_ANALYTICS_MAX_THREAD_SESSIONS] = {};
static thread_local analytics_thread_state *g_analytics_current_thread_state
    = ft_nullptr;

static std::atomic<analytics_session *> g_analytics_runtime_session(
    ft_nullptr);
static uint32_t g_analytics_runtime_region_ids[
    static_cast<uint32_t>(analytics_runtime_region::COUNT)] = {};
static std::atomic<uint32_t> g_analytics_runtime_sample_rate(1U);
static std::mutex g_analytics_runtime_lifecycle_mutex;
static std::condition_variable g_analytics_runtime_lifecycle_condition;
static const uint64_t FT_ANALYTICS_RUNTIME_GATE_CLOSED =
    static_cast<uint64_t>(1U) << 63U;
static const uint64_t FT_ANALYTICS_RUNTIME_GATE_ACTIVE_MASK =
    ~FT_ANALYTICS_RUNTIME_GATE_CLOSED;
static std::atomic<uint64_t> g_analytics_runtime_gate(0U);
static thread_local uint32_t g_analytics_runtime_sample_counter = 0U;

static void analytics_release_runtime_scope() noexcept
{
    uint64_t previous_gate;

    previous_gate = g_analytics_runtime_gate.fetch_sub(1U,
        std::memory_order_acq_rel);
    if (previous_gate == (FT_ANALYTICS_RUNTIME_GATE_CLOSED | 1U))
    {
        std::lock_guard<std::mutex> lock(g_analytics_runtime_lifecycle_mutex);
        g_analytics_runtime_lifecycle_condition.notify_all();
    }
    return ;
}

static const char *analytics_runtime_region_name(
    analytics_runtime_region region) noexcept
{
    static const char *names[
        static_cast<uint32_t>(analytics_runtime_region::COUNT)] = {
        "cma_malloc", "cma_free", "cma_realloc", "cma_aligned_alloc",
        "cma_lock", "cma_unlock", "pt_mutex_lock", "pt_mutex_unlock",
        "pt_mutex_try_lock", "pt_recursive_mutex_lock",
        "pt_recursive_mutex_unlock", "pt_rwlock_read", "pt_rwlock_write",
        "pt_rwlock_unlock"};
    uint32_t region_index;

    region_index = static_cast<uint32_t>(region);
    if (region_index >= static_cast<uint32_t>(
            analytics_runtime_region::COUNT))
        return ("unknown");
    return (names[region_index]);
}

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
    *frame = {};
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
    return ;
}

static void analytics_add_frame_breakdown(analytics_frame_statistics &frame,
    const analytics_pending_event &event, uint8_t *region_slots) noexcept
{
    static_assert(FT_ANALYTICS_MAX_FRAME_BREAKDOWN <= UINT8_MAX);
    const ft_bool indexed = region_slots != ft_nullptr && event.region_id < FT_ANALYTICS_MAX_REGIONS;
    uint32_t index = frame.breakdown_count;
    if (indexed)
    {
        const uint8_t slot = region_slots[event.region_id];
        if (slot != 0U)
            index = slot - 1U;
    }
    else
    {
        index = 0U;
        while (index < frame.breakdown_count && frame.breakdown[index].region_id != event.region_id)
            ++index;
    }

    if (index < frame.breakdown_count)
    {
        ++frame.breakdown[index].invocation_count;
        frame.breakdown[index].inclusive_nanoseconds += event.inclusive_nanoseconds;
        frame.breakdown[index].exclusive_nanoseconds += event.exclusive_nanoseconds;
        return ;
    }
    if (frame.breakdown_count >= FT_ANALYTICS_MAX_FRAME_BREAKDOWN)
    {
        ++frame.dropped_breakdown_count;
        return ;
    }
    frame.breakdown[index] = {event.region_id, 1U, event.inclusive_nanoseconds, event.exclusive_nanoseconds};
    ++frame.breakdown_count;
    if (indexed)
        region_slots[event.region_id] = static_cast<uint8_t>(index + 1U);
    return ;
}

static void analytics_build_frame_breakdown(analytics_frame_statistics &frame,
    const analytics_thread_state &thread_state) noexcept
{
    constexpr uint32_t INDEX_THRESHOLD = 12U;
    uint8_t slots[FT_ANALYTICS_MAX_REGIONS];
    uint8_t *region_slots = ft_nullptr;
    if (thread_state.pending_event_count >= INDEX_THRESHOLD)
    {
        ft_bzero(slots, sizeof(slots));
        region_slots = slots;
    }
    uint32_t index = 0U;
    while (index < thread_state.pending_event_count)
    {
        analytics_add_frame_breakdown(frame, thread_state.pending_events[index], region_slots);
        ++index;
    }
    return ;
}

static uint32_t analytics_thread_id(void) noexcept
{
    static thread_local const uint32_t thread_id =
        static_cast<uint32_t>(std::hash<std::thread::id>()(std::this_thread::get_id()));
    return (thread_id);
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
      _frame_export_interval(1U), _slow_frame_export_threshold_nanoseconds(0U),
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
      _export_thread(), _export_drain_mutex(),
      _exporter_started(FT_FALSE),
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
      slow_frame_export_threshold_nanoseconds(0U),
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

int32_t analytics_runtime_register_regions(analytics_session *session) noexcept
{
    uint32_t region_index;
    int32_t error_code;
    uint64_t gate_state;
    std::lock_guard<std::mutex> lock(g_analytics_runtime_lifecycle_mutex);

    if (session == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    gate_state = g_analytics_runtime_gate.load(
        std::memory_order_acquire);
    if (g_analytics_runtime_session.load(std::memory_order_acquire)
        != ft_nullptr
        || (gate_state & FT_ANALYTICS_RUNTIME_GATE_ACTIVE_MASK) != 0U)
        return (FT_ERR_ALREADY_INITIALISED);
    region_index = 0U;
    while (region_index < static_cast<uint32_t>(
            analytics_runtime_region::COUNT))
    {
        error_code = session->register_region(
            analytics_runtime_region_name(
                static_cast<analytics_runtime_region>(region_index)),
            "libft_runtime", &g_analytics_runtime_region_ids[region_index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        region_index += 1U;
    }
    g_analytics_runtime_session.store(session, std::memory_order_release);
    g_analytics_runtime_gate.store(0U, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t analytics_runtime_shutdown() noexcept
{
    std::unique_lock<std::mutex> lock(g_analytics_runtime_lifecycle_mutex);

    g_analytics_runtime_gate.fetch_or(FT_ANALYTICS_RUNTIME_GATE_CLOSED,
        std::memory_order_acq_rel);
    g_analytics_runtime_lifecycle_condition.wait(lock, []() noexcept
    {
        return ((g_analytics_runtime_gate.load(std::memory_order_acquire)
            & FT_ANALYTICS_RUNTIME_GATE_ACTIVE_MASK) == 0U);
    });
    g_analytics_runtime_session.store(ft_nullptr, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t analytics_runtime_set_sample_rate(uint32_t sample_rate) noexcept
{
    if (sample_rate == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    g_analytics_runtime_sample_rate.store(sample_rate,
        std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t analytics_runtime_scope_begin(analytics_runtime_region region,
    analytics_runtime_scope_token *token) noexcept
{
    analytics_session *session;
    analytics_thread_state *thread_state;
    uint64_t gate_state;
    uint32_t sample_rate;
    uint32_t region_index;
    int32_t error_code;

    if (token == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    token->session = ft_nullptr;
    token->region_id = 0U;
    token->start_nanoseconds = 0U;
    token->active = FT_FALSE;
    token->runtime_scope_registered = FT_FALSE;
    region_index = static_cast<uint32_t>(region);
    if (region_index >= static_cast<uint32_t>(
            analytics_runtime_region::COUNT))
        return (FT_ERR_INVALID_ARGUMENT);
    session = g_analytics_runtime_session.load(std::memory_order_acquire);
    if (session == ft_nullptr)
        return (FT_ERR_SUCCESS);
    thread_state = analytics_thread_state_for(session, FT_FALSE);
    /* Allocation and lock events can arrive in bursts. Keep room for the
     * explicit scopes of the application, which usually finish last. */
    if (thread_state == ft_nullptr || thread_state->runtime_scope_count
            >= FT_ANALYTICS_MAX_RUNTIME_SCOPES_PER_FRAME
        || thread_state->pending_event_count >= FT_ANALYTICS_MAX_THREAD_EVENTS
        || thread_state->scope_depth >= FT_ANALYTICS_MAX_SCOPE_DEPTH)
        return (FT_ERR_SUCCESS);
    sample_rate = g_analytics_runtime_sample_rate.load(
        std::memory_order_relaxed);
    g_analytics_runtime_sample_counter += 1U;
    if (sample_rate > 1U
        && (g_analytics_runtime_sample_counter % sample_rate) != 0U)
        return (FT_ERR_SUCCESS);
    while (true)
    {
        gate_state = g_analytics_runtime_gate.load(
            std::memory_order_acquire);
        if ((gate_state & FT_ANALYTICS_RUNTIME_GATE_CLOSED) != 0U)
            return (FT_ERR_SUCCESS);
        if ((gate_state & FT_ANALYTICS_RUNTIME_GATE_ACTIVE_MASK)
            == FT_ANALYTICS_RUNTIME_GATE_ACTIVE_MASK)
            return (FT_ERR_FULL);
        if (g_analytics_runtime_gate.compare_exchange_weak(gate_state,
            gate_state + 1U, std::memory_order_acq_rel,
            std::memory_order_acquire))
            break ;
    }
    if (session != g_analytics_runtime_session.load(std::memory_order_acquire))
    {
        analytics_release_runtime_scope();
        return (FT_ERR_SUCCESS);
    }
    token->session = session;
    token->region_id = g_analytics_runtime_region_ids[region_index];
    token->start_nanoseconds = session->now_nanoseconds();
    error_code = analytics_begin_scope_at(session, token->region_id,
        token->start_nanoseconds);
    if (error_code != FT_ERR_SUCCESS)
    {
        token->session = ft_nullptr;
        analytics_release_runtime_scope();
        return (FT_ERR_SUCCESS);
    }
    thread_state->runtime_scope_count += 1U;
    token->active = FT_TRUE;
    token->runtime_scope_registered = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_runtime_scope_end(
    analytics_runtime_scope_token *token) noexcept
{
    int32_t error_code;

    if (token == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (token->active == FT_FALSE)
        return (FT_ERR_SUCCESS);
    error_code = analytics_end_scope_at(token->session,
        token->session->now_nanoseconds());
    token->session = ft_nullptr;
    token->active = FT_FALSE;
    if (token->runtime_scope_registered != FT_FALSE)
    {
        token->runtime_scope_registered = FT_FALSE;
        analytics_release_runtime_scope();
    }
    return (error_code);
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
    this->_slow_frame_export_threshold_nanoseconds =
        configuration.slow_frame_export_threshold_nanoseconds;
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
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
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

    this->_export_drain_mutex.lock();
    if (this->_initialised_state != 2U)
    {
        this->_export_drain_mutex.unlock();
        return (FT_ERR_SUCCESS);
    }
    if (this->_exporter_buffer_index != FT_ANALYTICS_INVALID_BUFFER_INDEX)
    {
        this->_export_drain_mutex.unlock();
        return (FT_ERR_INVALID_STATE);
    }
    this->_enabled.store(FT_FALSE, std::memory_order_release);
    this->_export_drain_mutex.unlock();
    if (this->_exporter_started != FT_FALSE)
        stop_error = this->stop_exporter_internal();
    else
        stop_error = this->flush_exports();
    this->_export_drain_mutex.lock();
    this->_mutex.lock();
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
    this->_mutex.unlock();
    this->_export_drain_mutex.unlock();
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
    if (active == FT_FALSE)
        this->_world_active = FT_FALSE;
    else
        this->_world_active = FT_TRUE;
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
    if (this->_output_file == ft_nullptr
        && this->_world_output_file == ft_nullptr)
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
    this->_export_condition.notify_all();
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
        this->_export_condition.wait(lock, [this]() noexcept
        {
            return (this->_export_stop != FT_FALSE
                || this->_completed_buffer_count != 0U);
        });
        if (this->_export_stop != FT_FALSE)
            break ;
        lock.unlock();
        if (this->flush_exports() != FT_ERR_SUCCESS)
            this->_export_error.store(FT_ERR_IO, std::memory_order_release);
        lock.lock();
    }
    lock.unlock();
    if (this->flush_exports() != FT_ERR_SUCCESS)
        this->_export_error.store(FT_ERR_IO, std::memory_order_release);
    return ;
}

int32_t analytics_session::write_frame_to_output(
    const analytics_frame_statistics &frame, std::FILE *output_file,
    analytics_output_format output_format) noexcept
{
    ft_string output;
    int32_t error_code;
    ft_size_t output_size;

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
        error_code = analytics_write_file(output_file, output.c_str(),
            output_size);
    }
    if (output.destroy() != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    return (error_code);
}

int32_t analytics_session::write_trace_to_output(
    const analytics_trace_event &event, std::FILE *output_file) noexcept
{
    const char *region_name = ft_nullptr;
    const int32_t result = this->get_region_name(event.region_id, &region_name);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (analytics_write_trace_record([output_file](const char *record, ft_size_t length) noexcept
    {
        return (analytics_write_file(output_file, record, length));
    }, event, "[", "]\n"));
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
    if (enabled == FT_FALSE)
        this->_enabled.store(FT_FALSE, std::memory_order_release);
    else
        this->_enabled.store(FT_TRUE, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

ft_bool analytics_session::is_enabled() const noexcept
{
    return (this->_enabled.load(std::memory_order_acquire));
}

void analytics_session::record_region_sample_locked(uint32_t region_id,
    uint64_t inclusive_nanoseconds, uint64_t exclusive_nanoseconds) noexcept
{
    analytics_region &region = this->_regions[region_id];
    analytics_region_statistics &statistics = region.statistics;
    ++statistics.invocation_count;
    statistics.inclusive_nanoseconds += inclusive_nanoseconds;
    statistics.exclusive_nanoseconds += exclusive_nanoseconds;
    if (statistics.invocation_count == 1U || inclusive_nanoseconds < statistics.minimum_nanoseconds)
        statistics.minimum_nanoseconds = inclusive_nanoseconds;
    if (inclusive_nanoseconds > statistics.maximum_nanoseconds)
        statistics.maximum_nanoseconds = inclusive_nanoseconds;
    region.samples[region.sample_cursor] = inclusive_nanoseconds;
    region.sample_cursor = (region.sample_cursor + 1U) % FT_ANALYTICS_MAX_SAMPLES;
    if (region.sample_count < FT_ANALYTICS_MAX_SAMPLES)
        ++region.sample_count;
    return ;
}

int32_t analytics_session::record_scope(uint32_t region_id,
    uint64_t inclusive_nanoseconds, uint64_t exclusive_nanoseconds) noexcept
{
    this->_mutex.lock();
    if (this->_initialised_state != 2U
        || this->_enabled.load(std::memory_order_acquire) == FT_FALSE
        || region_id >= this->_region_count
        || this->_regions[region_id].registered == FT_FALSE)
    {
        this->_mutex.unlock();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->record_region_sample_locked(region_id, inclusive_nanoseconds, exclusive_nanoseconds);
    this->_mutex.unlock();
    return (FT_ERR_SUCCESS);
}

template <typename ReadSample>
int32_t analytics_session::record_scope_samples(uint32_t count, ReadSample read_sample) noexcept
{
    this->_mutex.lock();
    if (this->_initialised_state != 2U
        || this->_enabled.load(std::memory_order_acquire) == FT_FALSE)
    {
        this->_mutex.unlock();
        return (FT_ERR_NOT_INITIALISED);
    }
    uint32_t index = 0U;
    while (index < count)
    {
        const uint32_t region_id = read_sample(index).region_id;
        if (region_id >= this->_region_count || this->_regions[region_id].registered == FT_FALSE)
        {
            this->_mutex.unlock();
            return (FT_ERR_INVALID_ARGUMENT);
        }
        ++index;
    }
    index = 0U;
    while (index < count)
    {
        const auto &sample = read_sample(index);
        this->record_region_sample_locked(sample.region_id,
            sample.inclusive_nanoseconds, sample.exclusive_nanoseconds);
        ++index;
    }
    this->_mutex.unlock();
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::record_scope_batch(const uint32_t *region_ids,
    const uint64_t *inclusive_nanoseconds,
    const uint64_t *exclusive_nanoseconds, uint32_t count) noexcept
{
    if (count > FT_ANALYTICS_MAX_THREAD_EVENTS
        || ((region_ids == ft_nullptr || inclusive_nanoseconds == ft_nullptr
            || exclusive_nanoseconds == ft_nullptr) && count != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    struct scope_sample
    {
        uint32_t region_id;
        uint64_t inclusive_nanoseconds;
        uint64_t exclusive_nanoseconds;
    };
    return (this->record_scope_samples(count, [=](uint32_t index) noexcept
    {
        return (scope_sample{region_ids[index], inclusive_nanoseconds[index], exclusive_nanoseconds[index]});
    }));
}

int32_t analytics_session::record_thread_scopes(const analytics_thread_state &thread_state) noexcept
{
    return (this->record_scope_samples(thread_state.pending_event_count,
        [&thread_state](uint32_t index) noexcept -> const analytics_pending_event &
        {
            return (thread_state.pending_events[index]);
        }));
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
    uint64_t samples[FT_ANALYTICS_MAX_SAMPLES];
    uint32_t sample_count;
    analytics_region_statistics result;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_initialised_state != 2U || statistics == ft_nullptr
            || region_id >= this->_region_count
            || this->_regions[region_id].registered == FT_FALSE)
            return (FT_ERR_INVALID_ARGUMENT);
        const analytics_region &region = this->_regions[region_id];
        result = region.statistics;
        sample_count = region.sample_count;
        analytics_copy_samples(samples, region.samples, sample_count);
    }
    if (sample_count != 0U)
    {
        ft_sort(samples, samples + sample_count);
        result.percentile_50_nanoseconds = analytics_percentile_value(samples, sample_count, 50U);
        result.percentile_95_nanoseconds = analytics_percentile_value(samples, sample_count, 95U);
        result.percentile_99_nanoseconds = analytics_percentile_value(samples, sample_count, 99U);
    }
    *statistics = result;
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
    uint64_t samples[FT_ANALYTICS_MAX_SAMPLES];
    uint32_t sample_count;
    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_initialised_state != 2U || nanoseconds == ft_nullptr
            || (percentile != 50U && percentile != 95U && percentile != 99U)
            || region_id >= this->_region_count
            || this->_regions[region_id].registered == FT_FALSE)
            return (FT_ERR_INVALID_ARGUMENT);
        const analytics_region &region = this->_regions[region_id];
        sample_count = region.sample_count;
        if (sample_count == 0U)
            return (FT_ERR_EMPTY);
        analytics_copy_samples(samples, region.samples, sample_count);
    }
    ft_sort(samples, samples + sample_count);
    *nanoseconds = analytics_percentile_value(samples, sample_count, percentile);
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::find_free_buffer_locked(
    uint32_t *buffer_index) const noexcept
{
    static_assert(FT_ANALYTICS_MAX_EXPORT_BUFFER_COUNT <= 32U);
    if (buffer_index == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    uint32_t occupied = 1U << this->_active_buffer_index;
    if (this->_exporter_buffer_index != FT_ANALYTICS_INVALID_BUFFER_INDEX)
        occupied |= 1U << this->_exporter_buffer_index;
    uint32_t completed_index = 0U;
    while (completed_index < this->_completed_buffer_count)
    {
        occupied |= 1U << this->_completed_buffer_indices[completed_index];
        ++completed_index;
    }
    uint32_t candidate_index = 0U;
    while (candidate_index < this->_buffer_count)
    {
        if ((occupied & (1U << candidate_index)) == 0U)
        {
            *buffer_index = candidate_index;
            return (FT_ERR_SUCCESS);
        }
        ++candidate_index;
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
    if (this->obtain_free_buffer_locked(&free_buffer_index) != FT_ERR_SUCCESS)
        return (FT_ERR_FULL);
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
        if (frame.frame_number
            % static_cast<uint64_t>(this->_frame_export_interval) == 0U
            || (this->_slow_frame_export_threshold_nanoseconds != 0U
                && frame.duration_nanoseconds
                    >= this->_slow_frame_export_threshold_nanoseconds))
            export_frame = FT_TRUE;
        else
            export_frame = FT_FALSE;
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
        // Only exported frames need percentiles.
        if (export_frame != FT_FALSE)
        {
            uint64_t samples[FT_ANALYTICS_MAX_FRAME_SAMPLES];
            analytics_copy_samples(samples, this->_frame_samples, this->_frame_sample_count);
            ft_sort(samples, samples + this->_frame_sample_count);
            enriched_frame.percentile_95_nanoseconds =
                analytics_percentile_value(samples, this->_frame_sample_count, 95U);
            enriched_frame.percentile_99_nanoseconds =
                analytics_percentile_value(samples, this->_frame_sample_count, 99U);
        }
        this->_latest_frame = enriched_frame;
        this->_has_latest_frame = FT_TRUE;
        if (export_frame != FT_FALSE)
        {
            index = this->_active_buffer_index;
            this->_export_buffers[index].frames[
                this->_export_buffers[index].frame_count].value = enriched_frame;
            if (world_active == FT_FALSE)
                this->_export_buffers[index].frames[
                    this->_export_buffers[index].frame_count].world_active =
                    FT_FALSE;
            else
                this->_export_buffers[index].frames[
                    this->_export_buffers[index].frame_count].world_active =
                    FT_TRUE;
            this->_export_buffers[index].frame_count += 1U;

            /* A sampled frame must be handed to the exporter promptly.  The
             * old implementation only rotated a buffer when it reached its
             * full capacity, which made a 120-frame sampling interval wait
             * for 128 sampled records (15,360 rendered frames) before any
             * file output became visible.  Rotate after publishing the
             * sampled record; the exporter still performs all formatting and
             * I/O on its own thread.  If all buffers are busy, keep the
             * record in the active buffer and let the next sampled frame
             * retry the handoff under the configured overflow policy.
             */
            if (this->_exporter_started != FT_FALSE
                && this->rotate_active_buffer_locked() == FT_ERR_SUCCESS)
                rotated = FT_TRUE;
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

int32_t analytics_session::enqueue_trace_locked(const analytics_trace_event &event,
    ft_bool world_active, ft_bool &rotated) noexcept
{
    analytics_export_buffer *active_buffer = &this->_export_buffers[this->_active_buffer_index];
    if (active_buffer->trace_count >= this->_trace_capacity)
    {
        if (this->rotate_active_buffer_locked() != FT_ERR_SUCCESS)
        {
            ++this->_dropped_trace_count;
            return (FT_ERR_FULL);
        }
        rotated = FT_TRUE;
        active_buffer = &this->_export_buffers[this->_active_buffer_index];
    }
    active_buffer->traces[active_buffer->trace_count].value = event;
    active_buffer->traces[active_buffer->trace_count].world_active = world_active != FT_FALSE;
    ++active_buffer->trace_count;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_session::publish_trace(
    const analytics_trace_event &event, ft_bool world_active) noexcept
{
    ft_bool rotated = FT_FALSE;
    this->_mutex.lock();
    if (this->_initialised_state != 2U
        || this->_enabled.load(std::memory_order_acquire) == FT_FALSE)
    {
        this->_mutex.unlock();
        return (FT_ERR_NOT_INITIALISED);
    }
    const int32_t result = this->enqueue_trace_locked(event, world_active, rotated);
    this->_mutex.unlock();
    if (rotated)
        this->_export_condition.notify_one();
    return (result);
}

int32_t analytics_session::publish_thread_traces(const analytics_thread_state &thread_state,
    ft_bool count_remaining_drops) noexcept
{
    const uint32_t count = thread_state.pending_event_count;
    if (count == 0U || this->should_capture_trace(thread_state.frame_number) == FT_FALSE)
        return (FT_ERR_SUCCESS);
    analytics_trace_event event{};
    event.frame_number = thread_state.frame_number;
    event.thread_id = analytics_thread_id();
    ft_bool rotated = FT_FALSE;
    int32_t result = FT_ERR_SUCCESS;
    this->_mutex.lock();
    if (this->_initialised_state != 2U
        || this->_enabled.load(std::memory_order_acquire) == FT_FALSE)
    {
        this->_mutex.unlock();
        return (FT_ERR_NOT_INITIALISED);
    }
    uint32_t index = 0;
    while (index < count)
    {
        const analytics_pending_event &pending = thread_state.pending_events[index];
        event.region_id = pending.region_id;
        event.start_nanoseconds = pending.start_nanoseconds;
        event.duration_nanoseconds = pending.inclusive_nanoseconds;
        event.exclusive_nanoseconds = pending.exclusive_nanoseconds;
        result = this->enqueue_trace_locked(event, pending.world_active, rotated);
        ++index;
        if (result != FT_ERR_SUCCESS)
        {
            // No buffer can become free while this producer holds the lock.
            if (count_remaining_drops && this->_enabled.load(std::memory_order_relaxed) != FT_FALSE)
                this->_dropped_trace_count += count - index;
            break ;
        }
    }
    this->_mutex.unlock();
    if (rotated)
        this->_export_condition.notify_one();
    return (result);
}

ft_bool analytics_session::should_capture_trace(uint64_t frame_number) const
    noexcept
{
    uint32_t interval;

    interval = this->_trace_frame_interval;
    if (interval == 0U)
        interval = 1U;
    if (frame_number % static_cast<uint64_t>(interval) == 0U)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t analytics_session::flush_exports() noexcept
{
    this->_export_drain_mutex.lock();
    const int32_t result = this->drain_export_buffers();
    this->_export_drain_mutex.unlock();
    return (result);
}

int32_t analytics_session::drain_export_buffers() noexcept
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
            // A callback can reenter flush_exports on the draining thread.
            if (this->_exporter_buffer_index != FT_ANALYTICS_INVALID_BUFFER_INDEX)
                return (FT_ERR_SUCCESS);
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
    if (now >= queued_at)
        return (now - queued_at);
    return (0U);
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
    if (thread_state->session != ft_nullptr)
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
    thread_state->runtime_scope_count = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_end_frame(analytics_session *session) noexcept
{
    analytics_thread_state *thread_state;
    analytics_frame_statistics frame;
    uint64_t end_nanoseconds;
    int32_t first_error;
    int32_t operation_error;

    thread_state = analytics_thread_state_for(session, FT_FALSE);
    if (session == ft_nullptr || thread_state == ft_nullptr
        || thread_state->scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    end_nanoseconds = session->now_nanoseconds();
    if (end_nanoseconds < thread_state->frame_start_nanoseconds)
        return (FT_ERR_INVALID_ARGUMENT);
    analytics_init_frame_statistics(&frame, thread_state,
        end_nanoseconds);
    first_error = FT_ERR_SUCCESS;
    analytics_build_frame_breakdown(frame, *thread_state);
    operation_error = session->record_thread_scopes(*thread_state);
    if (operation_error != FT_ERR_SUCCESS
        && first_error == FT_ERR_SUCCESS)
        first_error = operation_error;
    operation_error = session->publish_thread_traces(*thread_state, FT_TRUE);
    if (operation_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = operation_error;
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
    int32_t record_error;
    uint64_t end_nanoseconds;

    thread_state = analytics_thread_state_for(session, FT_FALSE);
    if (session == ft_nullptr || thread_state == ft_nullptr
        || thread_state->scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    end_nanoseconds = session->now_nanoseconds();
    if (end_nanoseconds < thread_state->frame_start_nanoseconds)
        return (FT_ERR_INVALID_ARGUMENT);
    analytics_init_frame_statistics(&frame, thread_state,
        end_nanoseconds);
    analytics_build_frame_breakdown(frame, *thread_state);
    record_error = session->record_thread_scopes(*thread_state);
    if (record_error != FT_ERR_SUCCESS)
    {
        thread_state->session = ft_nullptr;
        thread_state->pending_event_count = 0U;
        thread_state->completed_scope_count = 0U;
        return (record_error);
    }
    const int32_t trace_error = session->publish_thread_traces(*thread_state, FT_FALSE);
    thread_state->session = ft_nullptr;
    thread_state->pending_event_count = 0U;
    thread_state->completed_scope_count = 0U;
    if (trace_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    frame.dropped_scope_count = session->get_dropped_scope_count();
    return (session->publish_frame(frame, thread_state->world_active));
}

int32_t analytics_begin_scope(analytics_session *session,
    uint32_t region_id) noexcept
{
    uint64_t start_nanoseconds;

    if (session == ft_nullptr)
        start_nanoseconds = analytics_clock_now();
    else
        start_nanoseconds = session->now_nanoseconds();
    return (analytics_begin_scope_at(session, region_id, start_nanoseconds));
}

int32_t analytics_begin_scope_at(analytics_session *session,
    uint32_t region_id, uint64_t start_nanoseconds) noexcept
{
    analytics_thread_state *thread_state;

    if (session == ft_nullptr || session->is_enabled() == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    thread_state = analytics_thread_state_for(session, FT_FALSE);
    if (thread_state == ft_nullptr
        || thread_state->scope_depth >= FT_ANALYTICS_MAX_SCOPE_DEPTH)
        return (FT_ERR_INVALID_STATE);
    if (thread_state->scope_depth != 0U
        && start_nanoseconds
            < thread_state->scopes[thread_state->scope_depth - 1U]
                .start_nanoseconds)
        return (FT_ERR_INVALID_ARGUMENT);
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
    uint64_t end_nanoseconds;

    if (session == ft_nullptr)
        end_nanoseconds = analytics_clock_now();
    else
        end_nanoseconds = session->now_nanoseconds();
    return (analytics_end_scope_at(session, end_nanoseconds));
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
    scope = thread_state->scopes[thread_state->scope_depth - 1U];
    if (end_nanoseconds < scope.start_nanoseconds)
        return (FT_ERR_INVALID_ARGUMENT);
    inclusive_nanoseconds = end_nanoseconds - scope.start_nanoseconds;
    if (scope.child_nanoseconds > inclusive_nanoseconds)
        return (FT_ERR_INVALID_ARGUMENT);
    exclusive_nanoseconds = inclusive_nanoseconds - scope.child_nanoseconds;
    thread_state->scope_depth -= 1U;
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
    const char *region_name;

    if (session == ft_nullptr || token == ft_nullptr || flow_id == 0U
        || session->is_enabled() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (session->get_region_name(region_id, &region_name) != FT_ERR_SUCCESS)
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
    int32_t error_code;

    if (token.session == ft_nullptr || token.flow_id == 0U
        || token.session->is_enabled() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    end_nanoseconds = token.session->now_nanoseconds();
    if (end_nanoseconds < token.start_nanoseconds)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = token.session->record_scope(token.region_id,
        end_nanoseconds - token.start_nanoseconds,
        end_nanoseconds - token.start_nanoseconds);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
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
    if (output == ft_nullptr || frame.breakdown_count > FT_ANALYTICS_MAX_FRAME_BREAKDOWN)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string temporary;
    int32_t result = temporary.initialize();
    if (result == FT_ERR_SUCCESS)
        result = analytics_append_record(temporary, "{\"frame\":", frame.frame_number,
            ",\"duration_ns\":", frame.duration_nanoseconds,
            ",\"mean_duration_ns\":", frame.mean_duration_nanoseconds,
            ",\"p95_duration_ns\":", frame.percentile_95_nanoseconds,
            ",\"p99_duration_ns\":", frame.percentile_99_nanoseconds,
            ",\"uninstrumented_ns\":", frame.uninstrumented_nanoseconds,
            ",\"dropped_scopes_total\":", frame.dropped_scope_count,
            ",\"dropped_breakdown_count\":", frame.dropped_breakdown_count, ",\"breakdown\":[");
    uint32_t index = 0U;
    while (result == FT_ERR_SUCCESS && index < frame.breakdown_count)
    {
        const analytics_frame_region_statistics &entry = frame.breakdown[index];
        const char *region_name = ft_nullptr;
        result = session.get_region_name(entry.region_id, &region_name);
        if (result == FT_ERR_SUCCESS && index != 0U)
            result = temporary.append(',');
        if (result == FT_ERR_SUCCESS)
            result = analytics_append_record(temporary, "{\"region_id\":", entry.region_id,
                ",\"invocations\":", entry.invocation_count, ",\"inclusive_ns\":",
                entry.inclusive_nanoseconds, ",\"exclusive_ns\":", entry.exclusive_nanoseconds, "}");
        ++index;
    }
    if (result == FT_ERR_SUCCESS)
        result = temporary.append("]}", 2U);
    if (result == FT_ERR_SUCCESS)
        result = analytics_commit_output(output, temporary);
    return (result);
}

int32_t analytics_export_trace_json(const analytics_session &session,
    const analytics_trace_event *events, uint32_t event_count,
    ft_string *output) noexcept
{
    if (output == ft_nullptr || (events == ft_nullptr && event_count != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string temporary;
    int32_t result = temporary.initialize();
    if (result == FT_ERR_SUCCESS)
        result = temporary.append('[');
    uint32_t index = 0U;
    while (result == FT_ERR_SUCCESS && index < event_count)
    {
        const analytics_trace_event &event = events[index];
        const char *region_name = ft_nullptr;
        result = session.get_region_name(event.region_id, &region_name);
        if (result == FT_ERR_SUCCESS && index != 0U)
            result = temporary.append(',');
        if (result == FT_ERR_SUCCESS)
            result = analytics_write_trace_record([&temporary](const char *record, ft_size_t length) noexcept
            {
                return (temporary.append(record, length));
            }, event, "", "");
        ++index;
    }
    if (result == FT_ERR_SUCCESS)
        result = temporary.append(']');
    if (result == FT_ERR_SUCCESS)
        result = analytics_commit_output(output, temporary);
    return (result);
}

int32_t analytics_export_frame_csv(const analytics_session &session,
    const analytics_frame_statistics &frame, ft_string *output) noexcept
{
    if (output == ft_nullptr || frame.breakdown_count > FT_ANALYTICS_MAX_FRAME_BREAKDOWN)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string temporary;
    int32_t result = temporary.initialize();
    if (result == FT_ERR_SUCCESS)
        result = analytics_append_record(temporary, "frame,region_id,invocations,inclusive_ns,exclusive_ns\n");
    uint32_t index = 0U;
    while (result == FT_ERR_SUCCESS && index < frame.breakdown_count)
    {
        const analytics_frame_region_statistics &entry = frame.breakdown[index];
        const char *region_name = ft_nullptr;
        result = session.get_region_name(entry.region_id, &region_name);
        if (result == FT_ERR_SUCCESS)
            result = analytics_append_record(temporary, frame.frame_number, ",", entry.region_id,
                ",", entry.invocation_count, ",", entry.inclusive_nanoseconds, ",",
                entry.exclusive_nanoseconds, "\n");
        ++index;
    }
    if (result == FT_ERR_SUCCESS)
        result = analytics_commit_output(output, temporary);
    return (result);
}
