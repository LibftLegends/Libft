Printf_TARGET := Printf.a
Printf_DEBUG_TARGET := Printf_debug.a

Printf_SOURCES := printf_printf.cpp \
                printf_format.cpp \
                printf_print_args.cpp \
                printf_ft_fprintf.cpp \
                printf_custom_specifier.cpp \
                printf_snprintf.cpp \
                printf_vsnprintf.cpp \
                printf_engine.cpp

Printf_HEADERS := printf.hpp \
           printf_internal.hpp
