Time_TARGET         := time.a
Time_DEBUG_TARGET   := time_debug.a

Time_SOURCES := time_active_clock.cpp \
        time_now.cpp \
        time_now_ms.cpp \
        time_monotonic.cpp \
        time_monotonic_point.cpp \
        time_monotonic_point_thread_safety.cpp \
        time_duration.cpp \
        time_duration_thread_safety.cpp \
        time_info_thread_safety.cpp \
        time_local.cpp \
        time_sleep.cpp \
        time_sleep_ms.cpp \
        time_sleep_async.cpp \
        time_fps.cpp \
        time_timer.cpp \
        time_strftime.cpp \
        time_format.cpp \
        time_relative.cpp \
        time_relative_format.cpp \
        time_interval.cpp \
        time_parse.cpp \
        time_high_resolution.cpp \
        time_monotonic_translate.cpp \
        time_timezone.cpp \
        time_benchmark.cpp \
        time_trace.cpp \
        time_basic.cpp

Time_HEADERS := time.hpp \
           time_fps.hpp \
           time_timer.hpp
