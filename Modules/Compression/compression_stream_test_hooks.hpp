#ifndef COMPRESSION_STREAM_TEST_HOOKS_HPP
# define COMPRESSION_STREAM_TEST_HOOKS_HPP

#include <zlib.h>
#include <cstdint>
#include "../Basic/basic.hpp"

typedef int (*t_compress_stream_deflate_init_hook)(z_stream *stream, int compression_level);
typedef int (*t_compress_stream_deflate_hook)(z_stream *stream, int flush_mode);
typedef int (*t_decompress_stream_inflate_init_hook)(z_stream *stream);
typedef int (*t_decompress_stream_inflate_hook)(z_stream *stream, int flush_mode);
typedef int64_t (*t_compress_stream_read_hook)(int file_descriptor,
    void *buffer, ft_size_t count);

void    ft_compress_stream_set_deflate_init_hook(t_compress_stream_deflate_init_hook hook);
void    ft_compress_stream_set_deflate_hook(t_compress_stream_deflate_hook hook);
void    ft_decompress_stream_set_inflate_init_hook(t_decompress_stream_inflate_init_hook hook);
void    ft_decompress_stream_set_inflate_hook(t_decompress_stream_inflate_hook hook);
void    ft_compress_stream_set_read_hook(t_compress_stream_read_hook hook);

#endif
