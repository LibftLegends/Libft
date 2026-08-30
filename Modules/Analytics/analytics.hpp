#ifndef ANALYTICS_HPP
# define ANALYTICS_HPP

# include <atomic>
# include <cstdint>
# include <mutex>

# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_ANALYTICS_MAX_REGIONS = 1024U;
static const uint32_t FT_ANALYTICS_MAX_SCOPE_DEPTH = 64U;
static const uint32_t FT_ANALYTICS_MAX_SAMPLES = 32U;
static const uint32_t FT_ANALYTICS_MAX_THREAD_EVENTS = 128U;

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
    uint64_t completed_scope_count;
    uint64_t dropped_scope_count;
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
};

class analytics_session
{
    private:
        struct analytics_region
        {
            const char *name;
            const char *category;
            analytics_region_statistics statistics;
            ft_bool registered;
            uint64_t samples[FT_ANALYTICS_MAX_SAMPLES];
            uint32_t sample_count;
            uint32_t sample_cursor;
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
        uint64_t _dropped_scope_count;

        analytics_session(const analytics_session &other) = delete;
        analytics_session(analytics_session &&other) = delete;
        analytics_session &operator=(const analytics_session &other) = delete;
        analytics_session &operator=(analytics_session &&other) = delete;

    public:
        analytics_session() noexcept;
        ~analytics_session() noexcept;

        int32_t initialize() noexcept;
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
        int32_t note_dropped_scope() noexcept;
        int32_t get_region_statistics(uint32_t region_id,
            analytics_region_statistics *statistics) const noexcept;
        int32_t get_region_percentile(uint32_t region_id, uint32_t percentile,
            uint64_t *nanoseconds) const noexcept;
        int32_t publish_frame(const analytics_frame_statistics &frame) noexcept;
        int32_t publish_trace(const analytics_trace_event &event) noexcept;
        uint64_t get_dropped_scope_count() const noexcept;
};

int32_t analytics_now_nanoseconds(uint64_t *timestamp) noexcept;
int32_t analytics_begin_frame(analytics_session *session,
    uint64_t frame_number) noexcept;
int32_t analytics_end_frame(analytics_session *session) noexcept;
int32_t analytics_begin_scope(analytics_session *session, uint32_t region_id)
    noexcept;
int32_t analytics_end_scope(analytics_session *session) noexcept;
int32_t analytics_begin_flow(analytics_session *session, uint64_t flow_id,
    uint32_t region_id, analytics_flow_token *token) noexcept;
int32_t analytics_end_flow(const analytics_flow_token &token) noexcept;

#endif
