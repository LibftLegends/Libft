#ifndef ANALYTICS_HPP
# define ANALYTICS_HPP

# include <atomic>
# include <cstdint>
# include <condition_variable>
# include <cstdio>
# include <mutex>
# include <thread>

# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"
# include "../CPP_class/class_string.hpp"

static const uint32_t FT_ANALYTICS_MAX_REGIONS = 1024U;
static const uint32_t FT_ANALYTICS_MAX_SCOPE_DEPTH = 64U;
static const uint32_t FT_ANALYTICS_MAX_SAMPLES = 32U;
static const uint32_t FT_ANALYTICS_MAX_FRAME_SAMPLES = 120U;
static const uint32_t FT_ANALYTICS_MAX_THREAD_EVENTS = 128U;
static const uint32_t FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS = 1024U;
static const uint32_t FT_ANALYTICS_MAX_FRAME_BREAKDOWN = 64U;
static const uint32_t FT_ANALYTICS_MAX_QUEUED_FRAME_EXPORTS = 128U;
static const uint32_t FT_ANALYTICS_REGION_NAME_CAPACITY = 128U;
static const uint32_t FT_ANALYTICS_REGION_CATEGORY_CAPACITY = 64U;
static const uint32_t FT_ANALYTICS_EXPORT_BUFFER_COUNT = 3U;
static const uint32_t FT_ANALYTICS_MAX_EXPORT_BUFFER_COUNT = 8U;
static const uint32_t FT_ANALYTICS_INVALID_BUFFER_INDEX = 0xffffffffU;

enum class analytics_output_format : uint32_t
{
    NONE = 0U,
    JSONL = 1U,
    CSV = 2U
};

enum class analytics_overflow_policy : uint32_t
{
    DROP_NEW_WITH_COUNTER = 0U,
    DROP_OLDEST_COMPLETED_WITH_COUNTER = 1U,
    FAIL_SESSION = 2U
};

typedef uint64_t (*analytics_clock_callback)(void *user_data) noexcept;

struct analytics_session_config
{
    const char *output_path;
    const char *world_output_path;
    analytics_output_format output_format;
    uint32_t reserved_frame_exports;
    uint32_t reserved_trace_events;
    uint32_t trace_frame_interval;
    uint32_t frame_export_interval;
    uint32_t buffer_count;
    analytics_overflow_policy overflow_policy;
    analytics_clock_callback clock_callback;
    void *clock_user_data;
    ft_bool start_exporter;

    analytics_session_config() noexcept;
};

int32_t analytics_default_session_config(
    analytics_session_config *configuration) noexcept;

struct analytics_frame_region_statistics
{
    uint32_t region_id;
    uint64_t invocation_count;
    uint64_t inclusive_nanoseconds;
    uint64_t exclusive_nanoseconds;
};

struct analytics_region_statistics
{
    uint64_t invocation_count;
    uint64_t inclusive_nanoseconds;
    uint64_t exclusive_nanoseconds;
    uint64_t minimum_nanoseconds;
    uint64_t maximum_nanoseconds;
    uint64_t percentile_50_nanoseconds;
    uint64_t percentile_95_nanoseconds;
    uint64_t percentile_99_nanoseconds;
};

struct analytics_frame_statistics
{
    uint64_t frame_number;
    uint64_t duration_nanoseconds;
    uint64_t mean_duration_nanoseconds;
    uint64_t percentile_95_nanoseconds;
    uint64_t percentile_99_nanoseconds;
    uint64_t uninstrumented_nanoseconds;
    uint64_t completed_scope_count;
    uint64_t dropped_scope_count;
    uint32_t breakdown_count;
    uint64_t dropped_breakdown_count;
    analytics_frame_region_statistics breakdown[FT_ANALYTICS_MAX_FRAME_BREAKDOWN];
};

struct analytics_trace_event
{
    uint64_t frame_number;
    uint64_t flow_id;
    uint32_t region_id;
    uint64_t start_nanoseconds;
    uint64_t duration_nanoseconds;
    uint64_t exclusive_nanoseconds;
    uint32_t thread_id;
};

typedef void (*analytics_export_callback)(
    const analytics_frame_statistics &frame,
    void *user_data);
typedef void (*analytics_trace_callback)(const analytics_trace_event &event,
    void *user_data);

class analytics_session;

struct analytics_flow_token
{
    analytics_session *session;
    uint64_t flow_id;
    uint32_t region_id;
    uint64_t start_nanoseconds;
    ft_bool world_active;
};

class analytics_session
{
    private:
        struct analytics_region
        {
            char name[FT_ANALYTICS_REGION_NAME_CAPACITY];
            char category[FT_ANALYTICS_REGION_CATEGORY_CAPACITY];
            analytics_region_statistics statistics;
            ft_bool registered;
            uint64_t samples[FT_ANALYTICS_MAX_SAMPLES];
            uint32_t sample_count;
            uint32_t sample_cursor;
        };

        struct analytics_export_buffer
        {
            struct buffered_frame
            {
                analytics_frame_statistics value;
                ft_bool world_active;
            };

            struct buffered_trace
            {
                analytics_trace_event value;
                ft_bool world_active;
            };

            buffered_frame frames[FT_ANALYTICS_MAX_QUEUED_FRAME_EXPORTS];
            uint32_t frame_count;
            buffered_trace traces[FT_ANALYTICS_MAX_QUEUED_TRACE_EVENTS];
            uint32_t trace_count;
            uint64_t queued_at_nanoseconds;
        };

        uint8_t _initialised_state;
        std::atomic<ft_bool> _enabled;
        mutable std::mutex _mutex;
        analytics_region _regions[FT_ANALYTICS_MAX_REGIONS];
        uint32_t _region_count;
        analytics_export_callback _export_callback;
        void *_export_user_data;
        analytics_trace_callback _trace_callback;
        void *_trace_user_data;
        analytics_export_buffer *_export_buffers;
        uint32_t _active_buffer_index;
        uint32_t _buffer_count;
        uint32_t _frame_capacity;
        uint32_t _trace_capacity;
        uint32_t _trace_frame_interval;
        uint32_t _frame_export_interval;
        analytics_overflow_policy _overflow_policy;
        uint32_t _completed_buffer_indices[
            FT_ANALYTICS_MAX_EXPORT_BUFFER_COUNT];
        uint32_t _completed_buffer_count;
        uint32_t _exporter_buffer_index;
        uint64_t _dropped_trace_count;
        uint64_t _dropped_frame_export_count;
        uint64_t _dropped_scope_count;
        uint64_t _frame_samples[FT_ANALYTICS_MAX_FRAME_SAMPLES];
        uint32_t _frame_sample_count;
        uint32_t _frame_sample_cursor;
        uint64_t _frame_duration_total;
        analytics_frame_statistics _latest_frame;
        ft_bool _has_latest_frame;

        analytics_session(const analytics_session &other) = delete;
        analytics_session(analytics_session &&other) = delete;
        analytics_session &operator=(const analytics_session &other) = delete;
        analytics_session &operator=(analytics_session &&other) = delete;

        std::FILE *_output_file;
        std::FILE *_world_output_file;
        analytics_output_format _output_format;
        analytics_output_format _world_output_format;
        std::condition_variable _export_condition;
        std::thread _export_thread;
        std::mutex _export_write_mutex;
        ft_bool _exporter_started;
        ft_bool _export_stop;
        std::atomic<int32_t> _export_error;
        std::atomic<ft_bool> _world_active;
        analytics_clock_callback _clock_callback;
        void *_clock_user_data;

        int32_t start_exporter_internal() noexcept;
        int32_t stop_exporter_internal() noexcept;
        void export_worker_main() noexcept;
        int32_t write_frame_to_file(const analytics_frame_statistics &frame)
            noexcept;
        int32_t write_trace_to_file(const analytics_trace_event &event)
            noexcept;
        int32_t write_frame_to_output(const analytics_frame_statistics &frame,
            std::FILE *output_file, analytics_output_format output_format)
            noexcept;
        int32_t write_trace_to_output(const analytics_trace_event &event,
            std::FILE *output_file)
            noexcept;
        int32_t rotate_active_buffer_locked() noexcept;
        int32_t find_free_buffer_locked(uint32_t *buffer_index) const noexcept;
        int32_t obtain_free_buffer_locked(uint32_t *buffer_index) noexcept;
        uint64_t clock_now() const noexcept;

    public:
        analytics_session() noexcept;
        ~analytics_session() noexcept;

        int32_t initialize() noexcept;
        int32_t initialize(const analytics_session_config &config) noexcept;
        int32_t start_exporter() noexcept;
        int32_t set_world_active(ft_bool active) noexcept;
        ft_bool is_world_active() const noexcept;
        uint64_t now_nanoseconds() const noexcept;
        int32_t destroy() noexcept;
        int32_t register_region(const char *name, const char *category,
            uint32_t *region_id) noexcept;
        int32_t set_export_callback(analytics_export_callback callback,
            void *user_data) noexcept;
        int32_t set_trace_callback(analytics_trace_callback callback,
            void *user_data) noexcept;
        int32_t set_enabled(ft_bool enabled) noexcept;
        ft_bool is_enabled() const noexcept;
        int32_t record_scope(uint32_t region_id, uint64_t inclusive_nanoseconds,
            uint64_t exclusive_nanoseconds) noexcept;
        int32_t record_scope_batch(const uint32_t *region_ids,
            const uint64_t *inclusive_nanoseconds,
            const uint64_t *exclusive_nanoseconds, uint32_t count) noexcept;
        int32_t note_dropped_scope() noexcept;
        int32_t get_region_statistics(uint32_t region_id,
            analytics_region_statistics *statistics) const noexcept;
        int32_t get_region_name(uint32_t region_id,
            const char **name) const noexcept;
        int32_t get_region_percentile(uint32_t region_id, uint32_t percentile,
            uint64_t *nanoseconds) const noexcept;
        int32_t publish_frame(const analytics_frame_statistics &frame) noexcept;
        int32_t publish_frame(const analytics_frame_statistics &frame,
            ft_bool world_active) noexcept;
        int32_t get_latest_frame(analytics_frame_statistics *frame) const noexcept;
        int32_t publish_trace(const analytics_trace_event &event) noexcept;
        int32_t publish_trace(const analytics_trace_event &event,
            ft_bool world_active) noexcept;
        ft_bool should_capture_trace(uint64_t frame_number) const noexcept;
        int32_t flush_exports() noexcept;
        int32_t get_export_error() const noexcept;
        uint64_t get_dropped_scope_count() const noexcept;
        uint64_t get_dropped_trace_count() const noexcept;
        uint64_t get_dropped_frame_export_count() const noexcept;
        uint32_t get_export_queue_depth() const noexcept;
        uint64_t get_oldest_export_queue_age_nanoseconds() const noexcept;
        uint32_t get_active_frame_count() const noexcept;
        uint32_t get_active_trace_count() const noexcept;
};

int32_t analytics_export_frame_json(const analytics_session &session,
    const analytics_frame_statistics &frame, ft_string *output) noexcept;
int32_t analytics_export_trace_json(const analytics_session &session,
    const analytics_trace_event *events, uint32_t event_count,
    ft_string *output) noexcept;
int32_t analytics_export_frame_csv(const analytics_session &session,
    const analytics_frame_statistics &frame, ft_string *output) noexcept;

int32_t analytics_now_nanoseconds(uint64_t *timestamp) noexcept;
int32_t analytics_begin_frame(analytics_session *session,
    uint64_t frame_number) noexcept;
int32_t analytics_end_frame(analytics_session *session) noexcept;
int32_t analytics_end_thread_frame(analytics_session *session) noexcept;
int32_t analytics_begin_scope(analytics_session *session, uint32_t region_id)
    noexcept;
int32_t analytics_begin_scope_at(analytics_session *session, uint32_t region_id,
    uint64_t start_nanoseconds) noexcept;
int32_t analytics_end_scope(analytics_session *session) noexcept;
int32_t analytics_end_scope_at(analytics_session *session,
    uint64_t end_nanoseconds) noexcept;
int32_t analytics_begin_flow(analytics_session *session, uint64_t flow_id,
    uint32_t region_id, analytics_flow_token *token) noexcept;
int32_t analytics_end_flow(const analytics_flow_token &token) noexcept;

#endif
