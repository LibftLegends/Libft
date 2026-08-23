Compatebility_TARGET := Compatebility.a
Compatebility_DEBUG_TARGET := Compatebility_debug.a

Compatebility_SOURCES := Compatebility_file_io.cpp Compatebility_file_ops.cpp Compatebility_file_watch.cpp Compatebility_file_dir.cpp Compatebility_file_path.cpp Compatebility_path_canonical.cpp Compatebility_rng.cpp Compatebility_readline.cpp Compatebility_pthread.cpp Compatebility_system.cpp Compatebility_write.cpp Compatebility_syslog.cpp Compatebility_networking.cpp Compatebility_time.cpp Compatebility_cross_process_posix.cpp Compatebility_cross_process_windows.cpp Compatebility_cma_platform.cpp Compatebility_service.cpp Compatebility_storage_memory_mapped.cpp Compatebility_stack_trace.cpp
Compatebility_MM_SOURCES :=
Compatebility_HEADERS := compatebility_internal.hpp \
        compatebility_cma_platform.hpp \
        compatebility_cross_process.hpp \
        compatebility_stack_trace.hpp

ifeq ($(OS),Windows_NT)
Compatebility_SOURCES += Compatebility_dumb_render_win32.cpp
Compatebility_SOURCES += Compatebility_dumb_controls_win32.cpp
Compatebility_SOURCES += Compatebility_dumb_sound_win32.cpp
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
Compatebility_MM_SOURCES += Compatebility_dumb_render_platform_macos.mm
Compatebility_MM_SOURCES += Compatebility_dumb_controls_platform_macos.mm
Compatebility_MM_FLAGS += -x objective-c++
Compatebility_SOURCES += Compatebility_dumb_sound_macos_coreaudio.cpp
else
Compatebility_SOURCES += Compatebility_dumb_render_linux_x11.cpp
Compatebility_SOURCES += Compatebility_dumb_controls_linux_x11.cpp
Compatebility_SOURCES += Compatebility_dumb_sound_linux_alsa.cpp
endif
endif
