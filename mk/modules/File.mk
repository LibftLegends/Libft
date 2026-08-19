File_TARGET         := file.a
File_DEBUG_TARGET   := file_debug.a

File_SOURCES := file_opendir.cpp \
        file_check_directory.cpp \
        file_mkdir.cpp \
        file_copy.cpp \
        file_move.cpp \
        file_exists.cpp \
        file_delete.cpp \
        file_path_join.cpp \
        file_path_relative.cpp \
        file_path_normalize.cpp \
        file_path_parts.cpp \
        file_watch.cpp \
        file_fopen.cpp \
        file_fclose.cpp \
        file_fgets.cpp \
        file_list_directory.cpp \
        file_read_write.cpp \
        file_recursive.cpp \
        file_hash.cpp \
        file_metadata.cpp \
        file_security.cpp \
        file_status.cpp

File_HEADERS := open_dir.hpp \
           file_utils.hpp \
           file_watch.hpp
