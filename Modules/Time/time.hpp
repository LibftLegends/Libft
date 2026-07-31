#ifndef TIME_HPP
# define TIME_HPP

#include <ctime>
#include <chrono>
#include "../CPP_class/class_string.hpp"

class pt_mutex;

typedef int64_t t_time;

typedef struct s_monotonic_time_point
{
    pt_mutex    *mutex;
    ft_bool        thread_safe_enabled;
    int64_t   milliseconds;
}   t_monotonic_time_point;

typedef struct s_duration_milliseconds
{
    pt_mutex    *mutex;
    ft_bool        thread_safe_enabled;
    int64_t   milliseconds;
}   t_duration_milliseconds;

typedef struct s_high_resolution_time_point
{
    int64_t nanoseconds;
}   t_high_resolution_time_point;

typedef struct s_active_clock
{
    uint64_t accumulated_microseconds;
    uint64_t started_microseconds;
    ft_bool running;
}   t_active_clock;

typedef struct s_time_info
{
    pt_mutex    *mutex;
    ft_bool        thread_safe_enabled;
    int32_t         seconds;
    int32_t         minutes;
    int32_t         hours;
    int32_t         month_day;
    int32_t         month;
    int32_t         year;
    int32_t         week_day;
    int32_t         year_day;
    int32_t         is_daylight_saving;
}   t_time_info;

struct event_loop;

typedef struct s_time_async_sleep
{
    t_monotonic_time_point deadline;
    ft_bool completed;
}   t_time_async_sleep;

typedef struct s_time_benchmark
{
    ft_size_t  sample_count;
    double  rolling_mean_ms;
    double  rolling_m2_ms;
    double  minimum_ms;
    double  maximum_ms;
    int32_t     error_code;
}   t_time_benchmark;

typedef struct s_time_benchmark_snapshot
{
    ft_size_t  sample_count;
    double  average_ms;
    double  jitter_ms;
    double  minimum_ms;
    double  maximum_ms;
}   t_time_benchmark_snapshot;

t_time  time_now(void);
int64_t    ft_time_ms(void);
char    *ft_time_format(char *buffer, ft_size_t buffer_size);
int64_t    time_now_ms(void);
int64_t   time_monotonic(void);
t_monotonic_time_point   time_monotonic_point_now(void);
t_monotonic_time_point   time_monotonic_point_add_ms(t_monotonic_time_point time_point, int64_t milliseconds);
int64_t   time_monotonic_point_diff_ms(t_monotonic_time_point start_point, t_monotonic_time_point end_point);
int32_t     time_monotonic_point_compare(t_monotonic_time_point first_point, t_monotonic_time_point second_point);
t_monotonic_time_point   time_monotonic_point_create(int64_t milliseconds);
int32_t     time_monotonic_point_enable_thread_safety(t_monotonic_time_point *time_point);
void    time_monotonic_point_disable_thread_safety(t_monotonic_time_point *time_point);
int32_t     time_monotonic_point_lock(const t_monotonic_time_point *time_point, ft_bool *lock_acquired);
void    time_monotonic_point_unlock(const t_monotonic_time_point *time_point, ft_bool lock_acquired);
ft_bool    time_monotonic_point_is_thread_safe(const t_monotonic_time_point *time_point);
t_duration_milliseconds  time_duration_ms_create(int64_t milliseconds);
int32_t     time_duration_ms_enable_thread_safety(t_duration_milliseconds *duration);
void        time_duration_ms_disable_thread_safety(t_duration_milliseconds *duration);
int32_t     time_duration_ms_lock(const t_duration_milliseconds *duration, ft_bool *lock_acquired);
void        time_duration_ms_unlock(const t_duration_milliseconds *duration, ft_bool lock_acquired);
ft_bool     time_duration_ms_is_thread_safe(const t_duration_milliseconds *duration);
int32_t     time_info_enable_thread_safety(t_time_info *time_info);
void        time_info_disable_thread_safety(t_time_info *time_info);
int32_t     time_info_lock(const t_time_info *time_info, ft_bool *lock_acquired);
void        time_info_unlock(const t_time_info *time_info, ft_bool lock_acquired);
ft_bool     time_info_is_thread_safe(const t_time_info *time_info);
void        time_local(t_time time_value, t_time_info *out);
void        time_sleep(uint32_t seconds);
void        time_sleep_ms(uint32_t milliseconds);
ft_size_t   time_strftime(char *buffer, ft_size_t size, const char *format, const t_time_info *time_info);
ft_string   *time_format_iso8601(t_time time_value);
ft_string   *time_format_iso8601_with_offset(t_time time_value, int32_t offset_minutes);
ft_size_t   time_format_timezone_offset(char *buffer, ft_size_t size, int32_t offset_minutes);
ft_string   *time_format_rfc3339(t_time time_value);
ft_string   *time_format_rfc3339_with_offset(t_time time_value, int32_t offset_minutes);
ft_string   *time_format_duration(t_duration_milliseconds duration_value);
ft_string   *time_format_relative(t_duration_milliseconds duration_value);
ft_string   *time_format_interval(t_time start_time, t_time end_time);
t_time      time_add_seconds(t_time time_value, int64_t seconds);
t_time      time_add_minutes(t_time time_value, int64_t minutes);
t_time      time_add_hours(t_time time_value, int64_t hours);
t_time      time_add_weeks(t_time time_value, int64_t weeks);
t_time      time_add_days(t_time time_value, int64_t days);
t_time      time_add_months(t_time time_value, int64_t months);
t_time      time_add_quarters(t_time time_value, int64_t quarters);
t_time      time_add_years(t_time time_value, int64_t years);
t_time      time_floor_to_second(t_time time_value);
t_time      time_floor_to_minute(t_time time_value);
t_time      time_floor_to_hour(t_time time_value);
t_time      time_floor_to_day(t_time time_value);
t_time      time_floor_to_week(t_time time_value);
t_time      time_floor_to_month(t_time time_value);
t_time      time_floor_to_quarter(t_time time_value);
t_time      time_ceiling_to_second(t_time time_value);
t_time      time_ceiling_to_minute(t_time time_value);
t_time      time_ceiling_to_hour(t_time time_value);
t_time      time_ceiling_to_day(t_time time_value);
t_time      time_ceiling_to_week(t_time time_value);
t_time      time_ceiling_to_month(t_time time_value);
t_time      time_ceiling_to_quarter(t_time time_value);
ft_bool     time_parse_iso8601(const char *string_input, std::tm *time_output, t_time *timestamp_output);
ft_bool     time_parse_rfc3339(const char *string_input, std::tm *time_output, t_time *timestamp_output);
ft_bool     time_parse_custom(const char *string_input, const char *format, std::tm *time_output, t_time *timestamp_output, ft_bool interpret_as_utc);
ft_bool     time_parse_custom(const char *string_input, const char *format, std::tm *time_output, t_time *timestamp_output);
ft_bool     time_parse_timezone_offset(const char *string_input, int32_t *offset_minutes);
ft_bool     time_parse_duration(const char *string_input, t_duration_milliseconds *duration_output);
ft_bool     time_parse_interval(const char *string_input, t_time *start_time, t_time *end_time, t_duration_milliseconds *duration_output = ft_nullptr);
ft_bool     time_high_resolution_now(t_high_resolution_time_point *time_point);
int64_t     time_high_resolution_diff_ns(t_high_resolution_time_point start_point, t_high_resolution_time_point end_point);
double      time_high_resolution_diff_seconds(t_high_resolution_time_point start_point, t_high_resolution_time_point end_point);
void        time_active_clock_init(t_active_clock *clock);
ft_bool     time_active_clock_start(t_active_clock *clock);
ft_bool     time_active_clock_stop(t_active_clock *clock);
ft_bool     time_active_clock_resume(t_active_clock *clock);
ft_bool     time_active_clock_restart(t_active_clock *clock);
uint64_t    time_active_clock_report(const t_active_clock *clock);
ft_bool    time_get_local_offset(t_time time_value, int32_t *offset_minutes, ft_bool *is_daylight_saving);
ft_bool    time_convert_timezone(t_time time_value, int32_t source_offset_minutes, int32_t target_offset_minutes, t_time *converted_time);
ft_bool    time_get_monotonic_wall_anchor(t_monotonic_time_point &anchor_monotonic,
                int64_t &anchor_wall_ms);
ft_bool    time_monotonic_to_wall_ms(t_monotonic_time_point monotonic_point,
                t_monotonic_time_point anchor_monotonic, int64_t anchor_wall_ms,
                int64_t &out_wall_ms);
ft_bool    time_wall_ms_to_monotonic(int64_t wall_time_ms,
                t_monotonic_time_point anchor_monotonic, int64_t anchor_wall_ms,
                t_monotonic_time_point &out_monotonic);

void        time_async_sleep_init(t_time_async_sleep *sleep_state, int64_t delay_milliseconds);
ft_bool     time_async_sleep_is_complete(const t_time_async_sleep *sleep_state);
int64_t     time_async_sleep_remaining_ms(t_time_async_sleep *sleep_state);
int32_t     time_async_sleep_poll(event_loop *loop, t_time_async_sleep *sleep_state);

typedef std::chrono::system_clock::time_point (*t_time_clock_now_hook)(void);

void        time_set_clock_now_hook(t_time_clock_now_hook hook);
void        time_reset_clock_now_hook(void);

void        time_benchmark_init(t_time_benchmark *benchmark);
void        time_benchmark_reset(t_time_benchmark *benchmark);
int32_t     time_benchmark_add_sample(t_time_benchmark *benchmark, double duration_ms);
int32_t     time_benchmark_add_duration(t_time_benchmark *benchmark,
                t_duration_milliseconds duration);
ft_bool     time_benchmark_snapshot(const t_time_benchmark *benchmark,
                t_time_benchmark_snapshot *out_snapshot);
ft_size_t   time_benchmark_get_sample_count(const t_time_benchmark *benchmark);
double      time_benchmark_get_average_ms(const t_time_benchmark *benchmark);
double      time_benchmark_get_jitter_ms(const t_time_benchmark *benchmark);
double      time_benchmark_get_minimum_ms(const t_time_benchmark *benchmark);
double      time_benchmark_get_maximum_ms(const t_time_benchmark *benchmark);
int32_t     time_benchmark_get_error(const t_time_benchmark *benchmark);
const char  *time_benchmark_get_error_str(const t_time_benchmark *benchmark);

ft_bool     time_trace_begin_session(const char *file_path);
ft_bool     time_trace_end_session(void);
ft_bool     time_trace_begin_event(const char *name, const char *category);
ft_bool     time_trace_end_event(void);
ft_bool     time_trace_instant_event(const char *name, const char *category);
#endif
