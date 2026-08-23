CLI_TARGET := cli.a
CLI_DEBUG_TARGET := cli_debug.a

CLI_SOURCES := cli.cpp \
        cli_completion.cpp \
        cli_config.cpp \
        cli_get_bool.cpp \
        cli_get_double.cpp \
        cli_get_int64.cpp \
        cli_get_option.cpp \
        cli_get_string.cpp \
        cli_get_uint64.cpp \
        cli_validate.cpp

CLI_HEADERS := cli.hpp
