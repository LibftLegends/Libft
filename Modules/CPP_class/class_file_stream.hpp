#ifndef CLASS_FILE_STREAM_HPP
# define CLASS_FILE_STREAM_HPP

#include "class_file.hpp"
#include <cstddef>

ft_size_t  ft_file_default_buffer_size(void) noexcept;
int32_t     ft_file_stream_copy(ft_file &source_file, ft_file &destination_file, ft_size_t buffer_size) noexcept;
int32_t     ft_file_stream_copy_into_buffer(ft_file &source_file, ft_file &destination_file,
                char *buffer, ft_size_t buffer_size) noexcept;

#endif
