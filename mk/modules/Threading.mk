Threading_TARGET := Threading.a
Threading_DEBUG_TARGET := Threading_debug.a

Threading_SOURCES := threading_thread.cpp \
        threading_compile_cancellation.cpp \
        threading_cancellation.cpp \
        threading_compile_thread_pool.cpp \
        threading_thread_pool.cpp \
        threading_concurrency.cpp \
        threading_task_scheduler.cpp \
        threading_task_scheduler_tracing.cpp

Threading_HEADERS := thread.hpp cancellation.hpp thread_pool.hpp concurrency.hpp task_scheduler.hpp task_scheduler_tracing.hpp lock_guard.hpp unique_lock.hpp errno_guard.hpp
