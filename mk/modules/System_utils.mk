System_utils_TARGET := System_utils.a
System_utils_DEBUG_TARGET := System_utils_debug.a

System_utils_SOURCES := System_utils_assert.cpp \
        System_utils_env.cpp \
        System_utils_abort.cpp \
        System_utils_sigabrt.cpp \
        System_utils_sigfpe.cpp \
        System_utils_sigill.cpp \
        System_utils_sigint.cpp \
        System_utils_sigsegv.cpp \
        System_utils_sigterm.cpp \
        System_utils_sysinfo.cpp \
        System_utils_file_open.cpp \
        System_utils_file_io.cpp \
        System_utils_file_stream.cpp \
        System_utils_file_utils.cpp \
        System_utils_health.cpp \
        System_utils_resource_tracer.cpp \
        System_utils_service.cpp \
        System_utils_home.cpp \
        System_utils_locale.cpp \
        test_system_utils_runner.cpp

System_utils_TEST_ONLY_SOURCES := test_system_utils_runner.cpp

System_utils_HEADERS := system_utils.hpp test_system_utils_runner.hpp
