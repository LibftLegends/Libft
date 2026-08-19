Config_TARGET := config.a
Config_DEBUG_TARGET := config_debug.a

Config_SOURCES :=         config_parse.cpp \
                config_write.cpp \
                config_merge.cpp \
                config_runtime.cpp \
                config_entry_thread_safety.cpp

Config_HEADERS := config.hpp
