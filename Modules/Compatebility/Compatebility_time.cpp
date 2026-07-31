#include "compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/pthread.hpp"
#include <cerrno>
#include <ctime>
#include <climits>

#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"
#if !defined(_WIN32) && !defined(_WIN64)
# include <unistd.h>
# include <sys/time.h>
#else
# include <sysinfoapi.h>
# include <profileapi.h>
#endif

#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_VERSION)
static int32_t cmp_localtime_from_shared_state(const std::time_t *time_value, std::tm *output)
{
    static pt_mutex localtime_mutex;
    std::tm *shared_result;

    if (pt_mutex_lock_if_not_null(&localtime_mutex) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    shared_result = std::localtime(time_value);
    if (shared_result == ft_nullptr)
    {
        (void)pt_mutex_unlock_if_not_null(&localtime_mutex);
        if (errno != 0)
            return (cmp_map_system_error_to_ft(errno));
        return (FT_ERR_INVALID_ARGUMENT);
    }
    *output = *shared_result;
    (void)pt_mutex_unlock_if_not_null(&localtime_mutex);
    return (FT_ERR_SUCCESS);
}
#endif

int32_t cmp_localtime(const std::time_t *time_value, std::tm *output)
{
    if (time_value == ft_nullptr || output == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
#if defined(_WIN32) || defined(_WIN64)
    errno_t error_code;

    error_code = localtime_s(output, time_value);
    if (error_code == 0)
    {
        return (FT_ERR_SUCCESS);
    }
    return (cmp_map_system_error_to_ft(static_cast<int32_t>(error_code)));
#else
# if defined(_POSIX_VERSION)
    if (localtime_r(time_value, output) != ft_nullptr)
    {
        return (FT_ERR_SUCCESS);
    }
    if (errno != 0)
        return (cmp_map_system_error_to_ft(errno));
    return (FT_ERR_INVALID_ARGUMENT);
# else
    return (cmp_localtime_from_shared_state(time_value, output));
# endif
#endif
}

int32_t cmp_gmtime(const std::time_t *time_value, std::tm *output)
{
    if (time_value == ft_nullptr || output == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
#if defined(_WIN32) || defined(_WIN64)
    errno_t error_code;

    error_code = gmtime_s(output, time_value);
    if (error_code == 0)
    {
        return (FT_ERR_SUCCESS);
    }
    return (cmp_map_system_error_to_ft(static_cast<int32_t>(error_code)));
#else
# if defined(_POSIX_VERSION)
    if (gmtime_r(time_value, output) != ft_nullptr)
    {
        return (FT_ERR_SUCCESS);
    }
    if (errno != 0)
        return (cmp_map_system_error_to_ft(errno));
    return (FT_ERR_INVALID_ARGUMENT);
# else
    static pt_mutex gmtime_mutex;
    std::tm *shared_result;

    if (pt_mutex_lock_if_not_null(&gmtime_mutex) != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    shared_result = std::gmtime(time_value);
    if (shared_result == ft_nullptr)
    {
        (void)pt_mutex_unlock_if_not_null(&gmtime_mutex);
        if (errno != 0)
            return (cmp_map_system_error_to_ft(errno));
        return (FT_ERR_INVALID_ARGUMENT);
    }
    *output = *shared_result;
    (void)pt_mutex_unlock_if_not_null(&gmtime_mutex);
    return (FT_ERR_SUCCESS);
# endif
#endif
}

int32_t cmp_time_get_time_of_day(struct timeval *time_value)
{
    if (time_value == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
#if defined(_WIN32) || defined(_WIN64)
    FILETIME file_time;
    ULARGE_INTEGER file_time_value;
    uint64_t microseconds_since_epoch;

    GetSystemTimeAsFileTime(&file_time);
    file_time_value.LowPart = file_time.dwLowDateTime;
    file_time_value.HighPart = file_time.dwHighDateTime;
    microseconds_since_epoch = (file_time_value.QuadPart - 116444736000000000ULL) / 10ULL;
    time_value->tv_sec = static_cast<decltype(time_value->tv_sec)>(microseconds_since_epoch / 1000000ULL);
    time_value->tv_usec = static_cast<decltype(time_value->tv_usec)>(microseconds_since_epoch % 1000000ULL);
    return (FT_ERR_SUCCESS);
#else
    if (gettimeofday(time_value, ft_nullptr) == 0)
    {
        return (FT_ERR_SUCCESS);
    }
    if (errno != 0)
        return (cmp_map_system_error_to_ft(errno));
    return (FT_ERR_IO);
#endif
}

#if !defined(_WIN32) && !defined(_WIN64)
static int32_t cmp_timespec_to_nanoseconds(const struct timespec *time_value, int64_t *nanoseconds_out)
{
    __int128 seconds_component;
    __int128 total_nanoseconds;

    if (time_value == ft_nullptr || nanoseconds_out == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    seconds_component = static_cast<__int128>(time_value->tv_sec);
    seconds_component *= static_cast<__int128>(1000000000LL);
    total_nanoseconds = seconds_component;
    total_nanoseconds += static_cast<__int128>(time_value->tv_nsec);
    if (total_nanoseconds > static_cast<__int128>(LLONG_MAX))
    {
        return (FT_ERR_OUT_OF_RANGE);
    }
    if (total_nanoseconds < static_cast<__int128>(LLONG_MIN))
    {
        return (FT_ERR_OUT_OF_RANGE);
    }
    *nanoseconds_out = static_cast<int64_t>(total_nanoseconds);
    return (FT_ERR_SUCCESS);
}
#endif

int32_t cmp_high_resolution_time(int64_t *nanoseconds_out)
{
    if (nanoseconds_out == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER performance_counter;
    LARGE_INTEGER performance_frequency;
    long double scaled_value;

    if (!QueryPerformanceCounter(&performance_counter))
    {
        return (cmp_map_system_error_to_ft(static_cast<int32_t>(GetLastError())));
    }
    if (!QueryPerformanceFrequency(&performance_frequency))
    {
        return (cmp_map_system_error_to_ft(static_cast<int32_t>(GetLastError())));
    }
    if (performance_frequency.QuadPart <= 0)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    scaled_value = static_cast<long double>(performance_counter.QuadPart);
    scaled_value *= 1000000000.0L;
    scaled_value /= static_cast<long double>(performance_frequency.QuadPart);
    if (scaled_value >= static_cast<long double>(LLONG_MAX))
    {
        return (FT_ERR_OUT_OF_RANGE);
    }
    if (scaled_value <= static_cast<long double>(LLONG_MIN))
    {
        return (FT_ERR_OUT_OF_RANGE);
    }
    *nanoseconds_out = static_cast<int64_t>(scaled_value);
    return (FT_ERR_SUCCESS);
#else
    struct timespec time_value;
    int32_t call_result;

# if defined(CLOCK_MONOTONIC_RAW)
    clockid_t clock_identifier;
    int32_t attempt_index;

    attempt_index = 0;
    call_result = -1;
    while (attempt_index < 2)
    {
        if (attempt_index == 0)
            clock_identifier = CLOCK_MONOTONIC_RAW;
        else
            clock_identifier = CLOCK_MONOTONIC;
        call_result = clock_gettime(clock_identifier, &time_value);
        if (call_result == 0)
            break ;
        if (errno != EINVAL)
        {
            return (cmp_map_system_error_to_ft(errno));
        }
        attempt_index += 1;
    }
    if (call_result != 0)
    {
        if (errno != 0)
            return (cmp_map_system_error_to_ft(errno));
        return (FT_ERR_INVALID_OPERATION);
    }
# else
    if (clock_gettime(CLOCK_MONOTONIC, &time_value) != 0)
    {
        if (errno != 0)
            return (cmp_map_system_error_to_ft(errno));
        return (FT_ERR_INVALID_OPERATION);
    }
# endif
    return (cmp_timespec_to_nanoseconds(&time_value, nanoseconds_out));
#endif
}

int32_t cmp_active_clock_now_microseconds(uint64_t *microseconds_out)
{
    int64_t nanoseconds = 0;

    if (microseconds_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (cmp_high_resolution_time(&nanoseconds) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    if (nanoseconds < 0)
        return (FT_ERR_OUT_OF_RANGE);
    *microseconds_out = static_cast<uint64_t>(nanoseconds) / 1000ULL;
    return (FT_ERR_SUCCESS);
}
