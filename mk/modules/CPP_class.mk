CPP_class_TARGET := CPP_class.a
CPP_class_DEBUG_TARGET := CPP_class_debug.a

CPP_class_SOURCES :=  cpp_class_string_constructors.cpp \
         cpp_class_string_methods.cpp \
         cpp_class_nullptr.cpp \
         cpp_class_file.cpp \
         cpp_class_file_stream.cpp \
         cpp_class_data_buffer.cpp \
         cpp_class_stringbuf.cpp \
         cpp_class_istream.cpp \
         cpp_class_fd_istream.cpp \
         cpp_class_istringstream.cpp \
         cpp_class_ofstream.cpp \
         cpp_class_big_number.cpp \
         cpp_class_big_number_serialization.cpp \
         cpp_class_bitset.cpp

CPP_class_HEADERS := class_string.hpp \
          ../Basic/class_nullptr.hpp \
          class_file.hpp \
          class_file_stream.hpp \
          class_data_buffer.hpp \
          class_stringbuf.hpp \
          class_istream.hpp \
          class_fd_istream.hpp \
          class_istringstream.hpp \
          class_ofstream.hpp \
          class_big_number.hpp \
          bitset.hpp
