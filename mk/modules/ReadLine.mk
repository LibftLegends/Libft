ReadLine_TARGET := ReadLine.a
ReadLine_DEBUG_TARGET := ReadLine_debug.a

ReadLine_SOURCES := readline.cpp \
    readline_clear_history.cpp \
    readline_handle_keypress.cpp \
    readline_suggestions.cpp \
    readline_utilities.cpp \
    readline_customization.cpp \
    readline_tab_completion.cpp \
    readline_printeble_char.cpp \
    readline_raw_mode.cpp \
    readline_initialize.cpp \
    readline_state.cpp \
    readline_thread_safety.cpp \
    readline_terminal_dimensions.cpp \
    readline_get_terminal_width.cpp
ReadLine_SOURCES += readline_utf8.cpp

ReadLine_HEADERS := readline.hpp \
           readline_internal.hpp
