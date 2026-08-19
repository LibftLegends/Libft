CrossProcess_TARGET := CrossProcess.a
CrossProcess_DEBUG_TARGET := CrossProcess_debug.a

CrossProcess_SOURCES := CrossProcess_send_descriptor.cpp \
        CrossProcess_receive_descriptor.cpp \
        CrossProcess_receive_memory.cpp \
        CrossProcess_write_memory.cpp

CrossProcess_HEADERS := cross_process.hpp
