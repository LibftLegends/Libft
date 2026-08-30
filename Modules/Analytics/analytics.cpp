#include "analytics.hpp"
#include <chrono>

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

struct analytics_scope_frame
{
    analytics_session *session;
    uint32_t region_id;
    uint64_t start_nanoseconds;
    uint64_t child_nanoseconds;
};

struct analytics_thread_state
{
    analytics_session *session;
    uint64_t frame_number;
    uint64_t frame_start_nanoseconds;
    uint64_t completed_scope_count;
    uint32_t scope_depth;
    analytics_scope_frame scopes[FT_ANALYTICS_MAX_SCOPE_DEPTH];
};

static thread_local analytics_thread_state g_analytics_thread_state =
{
    ft_nullptr, 0U, 0U, 0U, 0U, {}
};

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
      _export_user_data(ft_nullptr), _dropped_scope_count(0U)
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
    this->_dropped_scope_count = 0U;
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
    analytics_export_callback callback;
    void *user_data;

    {
        std::lock_guard<std::mutex> lock(this->_mutex);
        if (this->_initialised_state != 2U)
            return (FT_ERR_NOT_INITIALISED);
        callback = this->_export_callback;
        user_data = this->_export_user_data;
    }
    if (callback != ft_nullptr)
        callback(frame, user_data);
    return (FT_ERR_SUCCESS);
}

uint64_t analytics_session::get_dropped_scope_count() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    return (this->_dropped_scope_count);
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
    g_analytics_thread_state.completed_scope_count = 0U;
    g_analytics_thread_state.scope_depth = 0U;
    return (FT_ERR_SUCCESS);
}

int32_t analytics_end_frame(analytics_session *session) noexcept
{
    analytics_frame_statistics frame;
    uint64_t end_nanoseconds;

    if (session == ft_nullptr || g_analytics_thread_state.session != session
        || g_analytics_thread_state.scope_depth != 0U)
        return (FT_ERR_INVALID_STATE);
    end_nanoseconds = analytics_clock_now();
    frame.frame_number = g_analytics_thread_state.frame_number;
    frame.duration_nanoseconds = end_nanoseconds
        - g_analytics_thread_state.frame_start_nanoseconds;
    frame.completed_scope_count = g_analytics_thread_state.completed_scope_count;
    frame.dropped_scope_count = session->get_dropped_scope_count();
    g_analytics_thread_state.session = ft_nullptr;
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
    if (session->record_scope(scope.region_id, inclusive_nanoseconds,
            exclusive_nanoseconds) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    g_analytics_thread_state.completed_scope_count += 1U;
    return (FT_ERR_SUCCESS);
}
