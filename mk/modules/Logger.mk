Logger_TARGET := Logger.a
Logger_DEBUG_TARGET := Logger_debug.a

Logger_SOURCES := \
    logger_log_state.cpp \
    logger_log_rotate.cpp \
    logger_log_level_to_str.cpp \
    logger_log_vwrite.cpp \
    logger_log_set_level.cpp \
    logger_log_set_file.cpp \
    logger_log_set_rotation.cpp \
    logger_log_add_sink.cpp \
    logger_log_remove_sink.cpp \
    logger_log_set_alloc_logging.cpp \
    logger_log_get_alloc_logging.cpp \
    logger_log_set_api_logging.cpp \
    logger_log_get_api_logging.cpp \
    logger_log_set_color.cpp \
    logger_log_get_color.cpp \
    logger_log_redaction.cpp \
    logger_log_field_thread_safety.cpp \
    logger_log_sink_thread_safety.cpp \
    logger_file_sink_thread_safety.cpp \
    logger_network_sink_thread_safety.cpp \
    logger_log_close.cpp \
    logger_log_health.cpp \
    logger_lock_contention.cpp \
    logger_log_async_metrics_thread_safety.cpp \
    logger_log_debug.cpp \
    logger_log_info.cpp \
    logger_log_warn.cpp \
    logger_log_error.cpp \
    logger_log_context.cpp \
    logger_log_context_guard.cpp \
    logger_log_structured.cpp \
    logger_log_async.cpp \
    logger.cpp \
    logger_syslog.cpp \
    logger_network.cpp

Logger_HEADERS := logger.hpp logger_internal.hpp

Logger_CPP_FLAGS := -pthread
