Observability_TARGET := Observability.a
Observability_DEBUG_TARGET := Observability_debug.a

Observability_SOURCES := observability.cpp \
        observability_task_scheduler_bridge.cpp \
        observability_networking_metrics.cpp \
        observability_game_metrics.cpp \
        observability_histogram.cpp

Observability_HEADERS := observability_task_scheduler_bridge.hpp \
           observability_networking_metrics.hpp \
           observability_game_metrics.hpp \
           observability_histogram.hpp
