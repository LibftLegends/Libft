Errno_TARGET := errno.a
Errno_DEBUG_TARGET := errno_debug.a

Errno_SOURCES :=        errno_strerror.cpp \
        errno_perror.cpp \
        errno_exit.cpp \
        errno_internal.cpp

Errno_HEADERS := errno.hpp errno_internal.hpp


GENERATED_FILES += strMTL29
