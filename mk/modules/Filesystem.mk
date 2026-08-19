Filesystem_TARGET         := filesystem.a
Filesystem_DEBUG_TARGET   := filesystem_debug.a

Filesystem_SOURCES := filesystem_path.cpp \
        filesystem_temp.cpp \
        filesystem_atomic_write.cpp \
        filesystem_walk.cpp \
        filesystem_canonical_path.cpp \
        filesystem_is_inside_root.cpp \
        filesystem_is_safe_relative_path.cpp \
        filesystem_safe_join_path.cpp \
        filesystem_glob.cpp \
        filesystem_validate_inside_root.cpp

Filesystem_HEADERS := filesystem.hpp
