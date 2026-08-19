GPGR_TARGET := GPGR.a
GPGR_DEBUG_TARGET := GPGR_debug.a

GPGR_SOURCES := gpgr_gl_funcs.cpp \
    gpgr_window.cpp \
    gpgr_shader.cpp

GPGR_MM_SOURCES :=

GPGR_HEADERS := gpgr_gl_funcs.hpp \
    ft_gpu_window.hpp \
    ft_gpu_shader.hpp

ifeq ($(OS),Windows_NT)
    GPGR_SOURCES += gpgr_window_windows.cpp
    GPGR_HEADERS += gpgr_window_windows.hpp
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    GPGR_MM_SOURCES += gpgr_window_macos.mm
    GPGR_MM_FLAGS += -x objective-c++
    GPGR_HEADERS += gpgr_window_macos.hpp
else
    GPGR_SOURCES += gpgr_window_linux.cpp
    GPGR_HEADERS += gpgr_window_linux.hpp
endif
endif
