#include "../test_internal.hpp"
#include "../../Modules/Compression/compression.hpp"
#include "../../Modules/Compression/compression_stream_test_hooks.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <unistd.h>
#include <cstdint>
#include <zlib.h>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int compression_stream_fail_deflate_init(z_stream *stream, int compression_level)
{
    (void)stream;
    (void)compression_level;
    return (Z_MEM_ERROR);
}

static int compression_stream_fail_deflate(z_stream *stream, int flush_mode)
{
    (void)stream;
    (void)flush_mode;
    return (Z_STREAM_ERROR);
}

static int compression_stream_buf_error_deflate(z_stream *stream, int flush_mode)
{
    (void)stream;
    (void)flush_mode;
    return (Z_BUF_ERROR);
}

static int compression_stream_fail_inflate_init(z_stream *stream)
{
    (void)stream;
    return (Z_MEM_ERROR);
}

static int compression_stream_fail_inflate(z_stream *stream, int flush_mode)
{
    (void)stream;
    (void)flush_mode;
    return (Z_DATA_ERROR);
}

static int compression_stream_stream_error_inflate(z_stream *stream, int flush_mode)
{
    (void)stream;
    (void)flush_mode;
    return (Z_STREAM_ERROR);
}

static int compression_stream_buf_error_inflate(z_stream *stream, int flush_mode)
{
    (void)stream;
    (void)flush_mode;
    return (Z_BUF_ERROR);
}

static int g_inflate_stream_end_extra_calls = 0;

static int g_compress_flush_first_non_finish = -1;
static int g_compress_flush_finish_count = 0;

static int compression_stream_capture_flush_deflate(z_stream *stream, int flush_mode)
{
    if (flush_mode != Z_FINISH && g_compress_flush_first_non_finish == -1)
        g_compress_flush_first_non_finish = flush_mode;
    if (flush_mode == Z_FINISH)
        g_compress_flush_finish_count += 1;
    return (deflate(stream, flush_mode));
}

static int g_decompress_last_flush_mode = -1;

static int compression_stream_capture_flush_inflate(z_stream *stream, int flush_mode)
{
    g_decompress_last_flush_mode = flush_mode;
    return (inflate(stream, flush_mode));
}

static int g_compress_first_hook_calls = 0;
static int g_compress_second_hook_calls = 0;

static int compression_stream_counting_deflate(z_stream *stream, int flush_mode)
{
    g_compress_first_hook_calls += 1;
    return (deflate(stream, flush_mode));
}

static int compression_stream_failing_replacement_deflate(z_stream *stream, int flush_mode)
{
    (void)stream;
    (void)flush_mode;
    g_compress_second_hook_calls += 1;
    return (Z_STREAM_ERROR);
}

static int compression_stream_trailing_bytes_inflate(z_stream *stream, int flush_mode)
{
    (void)flush_mode;
    if (g_inflate_stream_end_extra_calls == 0)
    {
        g_inflate_stream_end_extra_calls = 1;
        if (stream->avail_out > 0)
        {
            stream->next_out[0] = 'X';
            stream->avail_out -= 1;
        }
        if (stream->avail_in > 0)
            stream->avail_in = 1;
        return (Z_STREAM_END);
    }
    return (Z_STREAM_END);
}

static std::size_t  compression_stream_collect_compressed_size(const char *payload,
        std::size_t payload_size,
        t_compress_stream_options *options)
{
    int             input_pipe[2];
    int             output_pipe[2];
    ssize_t         written_bytes;
    int             result;
    std::size_t     total_size;
    unsigned char   buffer[1024];
    ssize_t         read_bytes;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    written_bytes = su_write(input_pipe[1], payload, payload_size);
    FT_ASSERT_EQ(static_cast<ssize_t>(payload_size), written_bytes);
    close(input_pipe[1]);
    result = ft_compress_stream_with_options(input_pipe[0], output_pipe[1], options);
    close(input_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(0, result);
    total_size = 0;
    while (1)
    {
        read_bytes = su_read(output_pipe[0], buffer, sizeof(buffer));
        FT_ASSERT(read_bytes >= 0);
        if (read_bytes == 0)
            break ;
        total_size += static_cast<std::size_t>(read_bytes);
    }
    close(output_pipe[0]);
    return (total_size);
}

FT_TEST(test_ft_compress_stream_rejects_invalid_descriptors)
{
    int result;

    result = ft_compress_stream(-1, -1);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_compress_stream_with_options_rejects_zero_buffer)
{
    int                         input_pipe[2];
    int                         output_pipe[2];
    t_compress_stream_options   options;
    int                         result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_input_buffer_size(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_output_buffer_size(16));
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    close(input_pipe[1]);
    result = ft_compress_stream_with_options(input_pipe[0], output_pipe[1], &options);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_compress_stream_with_options_supports_custom_buffers)
{
    int                         input_pipe[2];
    int                         output_pipe[2];
    t_compress_stream_options   options;
    const char                  *payload;
    ssize_t                     written_bytes;
    int                         result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_input_buffer_size(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_output_buffer_size(12));
    payload = "custom buffer payload";
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    written_bytes = su_write(input_pipe[1], payload, ft_strlen_size_t(payload));
    FT_ASSERT_EQ(static_cast<ssize_t>(ft_strlen_size_t(payload)), written_bytes);
    close(input_pipe[1]);
    result = ft_compress_stream_with_options(input_pipe[0], output_pipe[1], &options);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(0, result);
    return (1);
}

FT_TEST(test_ft_compress_stream_speed_preset_sets_expected_values)
{
    t_compress_stream_options   options;
    Pair<int, std::size_t>      input_buffer_size;
    Pair<int, std::size_t>      output_buffer_size;
    Pair<int, int>              compression_level;
    Pair<int, int>              window_bits;
    Pair<int, int>              memory_level;
    Pair<int, int>              strategy;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.initialize());
    ft_compress_stream_apply_speed_preset(&options);
    input_buffer_size = options.get_input_buffer_size();
    output_buffer_size = options.get_output_buffer_size();
    compression_level = options.get_compression_level();
    window_bits = options.get_window_bits();
    memory_level = options.get_memory_level();
    strategy = options.get_strategy();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input_buffer_size.key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output_buffer_size.key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, compression_level.key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, window_bits.key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, memory_level.key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, strategy.key);
    FT_ASSERT_EQ(static_cast<std::size_t>(16384), input_buffer_size.value);
    FT_ASSERT_EQ(static_cast<std::size_t>(16384), output_buffer_size.value);
    FT_ASSERT_EQ(Z_BEST_SPEED, compression_level.value);
    FT_ASSERT(window_bits.value >= 8);
    FT_ASSERT(window_bits.value <= MAX_WBITS);
    FT_ASSERT(memory_level.value >= 1);
    FT_ASSERT(memory_level.value <= 9);
    FT_ASSERT_EQ(Z_DEFAULT_STRATEGY, strategy.value);
    return (1);
}

FT_TEST(test_ft_compress_stream_ratio_preset_sets_expected_values)
{
    t_compress_stream_options   speed_options;
    t_compress_stream_options   ratio_options;
    Pair<int, std::size_t>      ratio_input_buffer_size;
    Pair<int, std::size_t>      ratio_output_buffer_size;
    Pair<int, int>              ratio_compression_level;
    Pair<int, int>              ratio_window_bits;
    Pair<int, int>              speed_window_bits;
    Pair<int, int>              ratio_memory_level;
    Pair<int, int>              speed_memory_level;
    Pair<int, int>              ratio_strategy;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, speed_options.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ratio_options.initialize());
    ft_compress_stream_apply_speed_preset(&speed_options);
    ft_compress_stream_apply_ratio_preset(&ratio_options);
    ratio_input_buffer_size = ratio_options.get_input_buffer_size();
    ratio_output_buffer_size = ratio_options.get_output_buffer_size();
    ratio_compression_level = ratio_options.get_compression_level();
    ratio_window_bits = ratio_options.get_window_bits();
    speed_window_bits = speed_options.get_window_bits();
    ratio_memory_level = ratio_options.get_memory_level();
    speed_memory_level = speed_options.get_memory_level();
    ratio_strategy = ratio_options.get_strategy();
    FT_ASSERT_EQ(static_cast<std::size_t>(32768), ratio_input_buffer_size.value);
    FT_ASSERT_EQ(static_cast<std::size_t>(32768), ratio_output_buffer_size.value);
    FT_ASSERT_EQ(Z_BEST_COMPRESSION, ratio_compression_level.value);
    FT_ASSERT_EQ(MAX_WBITS, ratio_window_bits.value);
    FT_ASSERT(ratio_window_bits.value >= speed_window_bits.value);
    FT_ASSERT(ratio_memory_level.value >= speed_memory_level.value);
    FT_ASSERT(ratio_memory_level.value <= 9);
    FT_ASSERT_EQ(Z_DEFAULT_STRATEGY, ratio_strategy.value);
    return (1);
}

FT_TEST(test_ft_compress_stream_ratio_preset_outperforms_speed_preset)
{
    ft_string                   payload;
    t_compress_stream_options   speed_options;
    t_compress_stream_options   ratio_options;
    std::size_t                 speed_size;
    std::size_t                 ratio_size;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.initialize());
    payload.assign(2048, 'A');
    FT_ASSERT_EQ(FT_ERR_SUCCESS, payload.get_error());
    ft_compress_stream_apply_speed_preset(&speed_options);
    ft_compress_stream_apply_ratio_preset(&ratio_options);
    speed_size = compression_stream_collect_compressed_size(payload.data(), payload.size(), &speed_options);
    ratio_size = compression_stream_collect_compressed_size(payload.data(), payload.size(), &ratio_options);
    FT_ASSERT(speed_size >= ratio_size);
    return (1);
}

FT_TEST(test_ft_compress_stream_rejects_invalid_window_bits)
{
    int                         input_pipe[2];
    int                         output_pipe[2];
    t_compress_stream_options   options;
    int                         result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_input_buffer_size(16));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_output_buffer_size(16));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_window_bits(16));
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    close(input_pipe[1]);
    result = ft_compress_stream_with_options(input_pipe[0], output_pipe[1], &options);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_compress_stream_reports_deflate_init_failure)
{
    int input_pipe[2];
    int output_pipe[2];
    int result;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    ft_compress_stream_set_deflate_init_hook(compression_stream_fail_deflate_init);
    result = ft_compress_stream(input_pipe[0], output_pipe[1]);
    ft_compress_stream_set_deflate_init_hook(ft_nullptr);
    close(input_pipe[0]);
    close(input_pipe[1]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_compress_stream_reports_read_failure)
{
    int input_pipe[2];
    int output_pipe[2];
    int result;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    close(input_pipe[0]);
    result = ft_compress_stream(input_pipe[1], output_pipe[1]);
    close(input_pipe[1]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_compress_stream_reports_deflate_failure)
{
    int input_pipe[2];
    int output_pipe[2];
    const char *payload;
    ssize_t written;
    int result;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    payload = "stream failure";
    written = su_write(input_pipe[1], payload, ft_strlen_size_t(payload));
    FT_ASSERT_EQ(static_cast<ssize_t>(ft_strlen_size_t(payload)), written);
    close(input_pipe[1]);
    ft_compress_stream_set_deflate_hook(compression_stream_fail_deflate);
    result = ft_compress_stream(input_pipe[0], output_pipe[1]);
    ft_compress_stream_set_deflate_hook(ft_nullptr);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_compress_stream_reports_buffer_exhaustion)
{
    int input_pipe[2];
    int output_pipe[2];
    const char *payload;
    ssize_t written;
    int result;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    payload = "buffer exhaustion";
    written = su_write(input_pipe[1], payload, ft_strlen_size_t(payload));
    FT_ASSERT_EQ(static_cast<ssize_t>(ft_strlen_size_t(payload)), written);
    close(input_pipe[1]);
    ft_compress_stream_set_deflate_hook(compression_stream_buf_error_deflate);
    result = ft_compress_stream(input_pipe[0], output_pipe[1]);
    ft_compress_stream_set_deflate_hook(ft_nullptr);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_compress_stream_reports_write_failure)
{
    int input_pipe[2];
    int output_pipe[2];
    const char *payload;
    ssize_t written;
    int result;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    payload = "stream write";
    written = su_write(input_pipe[1], payload, ft_strlen_size_t(payload));
    FT_ASSERT_EQ(static_cast<ssize_t>(ft_strlen_size_t(payload)), written);
    close(input_pipe[1]);
    result = ft_compress_stream(input_pipe[0], output_pipe[0]);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_compress_stream_success_sets_errno_success)
{
    int input_pipe[2];
    int output_pipe[2];
    const char *payload;
    ssize_t written;
    int result;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    payload = "compress success";
    written = su_write(input_pipe[1], payload, ft_strlen_size_t(payload));
    FT_ASSERT_EQ(static_cast<ssize_t>(ft_strlen_size_t(payload)), written);
    close(input_pipe[1]);
    result = ft_compress_stream(input_pipe[0], output_pipe[1]);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(0, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_rejects_invalid_descriptors)
{
    int result;

    result = ft_decompress_stream(-1, -1);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_with_options_rejects_zero_buffer)
{
    int                         input_pipe[2];
    int                         output_pipe[2];
    t_compress_stream_options   options;
    int                         result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_input_buffer_size(4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_output_buffer_size(0));
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    close(input_pipe[1]);
    result = ft_decompress_stream_with_options(input_pipe[0], output_pipe[1], &options);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_with_options_supports_custom_buffers)
{
    int                         input_pipe[2];
    int                         output_pipe[2];
    const unsigned char         *payload;
    unsigned char               *compressed_buffer;
    std::size_t                 compressed_size;
    ssize_t                     written_bytes;
    t_compress_stream_options   options;
    int                         result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_input_buffer_size(10));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_output_buffer_size(7));
    payload = reinterpret_cast<const unsigned char *>("custom decompress payload");
    compressed_size = 0;
    compressed_buffer = ft_compress(payload, ft_strlen_size_t("custom decompress payload"), &compressed_size);
    FT_ASSERT(compressed_buffer != ft_nullptr);
    FT_ASSERT(compressed_size > sizeof(uint32_t));
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    written_bytes = su_write(input_pipe[1],
            compressed_buffer + sizeof(uint32_t),
            compressed_size - sizeof(uint32_t));
    FT_ASSERT_EQ(static_cast<ssize_t>(compressed_size - sizeof(uint32_t)), written_bytes);
    close(input_pipe[1]);
    cma_free(compressed_buffer);
    result = ft_decompress_stream_with_options(input_pipe[0], output_pipe[1], &options);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(0, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_reports_inflate_init_failure)
{
    int input_pipe[2];
    int output_pipe[2];
    int result;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    ft_decompress_stream_set_inflate_init_hook(compression_stream_fail_inflate_init);
    result = ft_decompress_stream(input_pipe[0], output_pipe[1]);
    ft_decompress_stream_set_inflate_init_hook(ft_nullptr);
    close(input_pipe[0]);
    close(input_pipe[1]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_reports_read_failure)
{
    int input_pipe[2];
    int output_pipe[2];
    int result;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    close(input_pipe[0]);
    result = ft_decompress_stream(input_pipe[1], output_pipe[1]);
    close(input_pipe[1]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_reports_inflate_failure)
{
    int input_pipe[2];
    int output_pipe[2];
    int result;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    close(input_pipe[1]);
    ft_decompress_stream_set_inflate_hook(compression_stream_fail_inflate);
    result = ft_decompress_stream(input_pipe[0], output_pipe[1]);
    ft_decompress_stream_set_inflate_hook(ft_nullptr);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_reports_buffer_exhaustion)
{
    int input_pipe[2];
    int output_pipe[2];
    int result;

    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    close(input_pipe[1]);
    ft_decompress_stream_set_inflate_hook(compression_stream_buf_error_inflate);
    result = ft_decompress_stream(input_pipe[0], output_pipe[1]);
    ft_decompress_stream_set_inflate_hook(ft_nullptr);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_reports_stream_errors)
{
    int input_pipe[2];
    int output_pipe[2];
    const unsigned char *payload;
    unsigned char *compressed_buffer;
    std::size_t compressed_size;
    ssize_t payload_written;
    int result;

    payload = reinterpret_cast<const unsigned char *>("stream error");
    compressed_size = 0;
    compressed_buffer = ft_compress(payload, ft_strlen_size_t("stream error"), &compressed_size);
    FT_ASSERT(compressed_buffer != ft_nullptr);
    FT_ASSERT(compressed_size > 0);
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    FT_ASSERT(compressed_size > sizeof(uint32_t));
    payload_written = su_write(input_pipe[1],
            compressed_buffer + sizeof(uint32_t),
            compressed_size - sizeof(uint32_t));
    FT_ASSERT_EQ(static_cast<ssize_t>(compressed_size - sizeof(uint32_t)),
        payload_written);
    close(input_pipe[1]);
    cma_free(compressed_buffer);
    ft_decompress_stream_set_inflate_hook(compression_stream_stream_error_inflate);
    result = ft_decompress_stream(input_pipe[0], output_pipe[1]);
    ft_decompress_stream_set_inflate_hook(ft_nullptr);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_reports_write_failure)
{
    int input_pipe[2];
    int output_pipe[2];
    const unsigned char *payload;
    unsigned char *compressed_buffer;
    std::size_t compressed_size;
    ssize_t payload_written;
    int result;

    payload = reinterpret_cast<const unsigned char *>("decompress write");
    compressed_size = 0;
    compressed_buffer = ft_compress(payload, ft_strlen_size_t("decompress write"), &compressed_size);
    FT_ASSERT(compressed_buffer != ft_nullptr);
    FT_ASSERT(compressed_size > 0);
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    FT_ASSERT(compressed_size > sizeof(uint32_t));
    payload_written = su_write(input_pipe[1],
            compressed_buffer + sizeof(uint32_t),
            compressed_size - sizeof(uint32_t));
    FT_ASSERT_EQ(static_cast<ssize_t>(compressed_size - sizeof(uint32_t)),
        payload_written);
    close(input_pipe[1]);
    cma_free(compressed_buffer);
    result = ft_decompress_stream(input_pipe[0], output_pipe[0]);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_success_sets_errno_success)
{
    int input_pipe[2];
    int output_pipe[2];
    const unsigned char *payload;
    unsigned char *compressed_buffer;
    std::size_t compressed_size;
    ssize_t payload_written;
    int result;

    payload = reinterpret_cast<const unsigned char *>("decompress success");
    compressed_size = 0;
    compressed_buffer = ft_compress(payload, ft_strlen_size_t("decompress success"), &compressed_size);
    FT_ASSERT(compressed_buffer != ft_nullptr);
    FT_ASSERT(compressed_size > 0);
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    FT_ASSERT(compressed_size > sizeof(uint32_t));
    payload_written = su_write(input_pipe[1],
            compressed_buffer + sizeof(uint32_t),
            compressed_size - sizeof(uint32_t));
    FT_ASSERT_EQ(static_cast<ssize_t>(compressed_size - sizeof(uint32_t)),
        payload_written);
    close(input_pipe[1]);
    cma_free(compressed_buffer);
    result = ft_decompress_stream(input_pipe[0], output_pipe[1]);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(0, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_rejects_trailing_bytes)
{
    int             input_pipe[2];
    int             output_pipe[2];
    unsigned char   payload[4];
    ssize_t         payload_written;
    int             result;

    payload[0] = 0x01;
    payload[1] = 0x02;
    payload[2] = 0x03;
    payload[3] = 0x04;
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    payload_written = su_write(input_pipe[1], payload, sizeof(payload));
    FT_ASSERT_EQ(static_cast<ssize_t>(sizeof(payload)), payload_written);
    close(input_pipe[1]);
    g_inflate_stream_end_extra_calls = 0;
    ft_decompress_stream_set_inflate_hook(compression_stream_trailing_bytes_inflate);
    result = ft_decompress_stream(input_pipe[0], output_pipe[1]);
    ft_decompress_stream_set_inflate_hook(ft_nullptr);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}

FT_TEST(test_ft_compress_stream_uses_finish_flush_after_partial_read)
{
    int                         input_pipe[2];
    int                         output_pipe[2];
    t_compress_stream_options   options;
    const char                  *payload;
    ssize_t                     written_bytes;
    int                         result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_input_buffer_size(4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, options.set_output_buffer_size(64));
    payload = "partial-flush";
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    written_bytes = su_write(input_pipe[1], payload, ft_strlen_size_t(payload));
    FT_ASSERT_EQ(static_cast<ssize_t>(ft_strlen_size_t(payload)), written_bytes);
    close(input_pipe[1]);
    g_compress_flush_first_non_finish = -1;
    g_compress_flush_finish_count = 0;
    ft_compress_stream_set_deflate_hook(compression_stream_capture_flush_deflate);
    result = ft_compress_stream_with_options(input_pipe[0], output_pipe[1], &options);
    ft_compress_stream_set_deflate_hook(ft_nullptr);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(0, result);
    FT_ASSERT_EQ(Z_NO_FLUSH, g_compress_flush_first_non_finish);
    FT_ASSERT(g_compress_flush_finish_count > 0);
    return (1);
}

static const char *g_fragmented_input;
static std::size_t g_fragmented_input_offset;
static std::size_t g_fragmented_input_size;

static int64_t compression_stream_fragmented_read(int file_descriptor,
    void *buffer, ft_size_t count)
{
    std::size_t remaining_size;
    std::size_t read_size;

    (void)file_descriptor;
    if (g_fragmented_input_offset >= g_fragmented_input_size)
        return (0);
    remaining_size = g_fragmented_input_size - g_fragmented_input_offset;
    read_size = remaining_size;
    if (read_size > count)
        read_size = count;
    if (read_size > 4U)
        read_size = 4U;
    ft_memcpy(buffer, g_fragmented_input + g_fragmented_input_offset,
        read_size);
    g_fragmented_input_offset += read_size;
    return (static_cast<int64_t>(read_size));
}

FT_TEST(test_ft_compress_stream_preserves_fragmented_producer_input)
{
    int input_pipe[2];
    int compressed_file_descriptor;
    char compressed_path[] = "/tmp/libft_compression_streamXXXXXX";
    const char *first_fragment;
    const char *second_fragment;
    unsigned char compressed_buffer[1024];
    unsigned char decompressed_buffer[128];
    std::size_t first_size;
    std::size_t second_size;
    std::size_t compressed_size;
    uLongf decompressed_size;
    ssize_t read_bytes;
    int compression_result;

    first_fragment = "fragment-one-";
    second_fragment = "fragment-two";
    first_size = ft_strlen_size_t(first_fragment);
    second_size = ft_strlen_size_t(second_fragment);
    FT_ASSERT_EQ(0, pipe(input_pipe));
    compressed_file_descriptor = test_create_temp_file_from_template(
        compressed_path, sizeof(compressed_path), compressed_path);
    FT_ASSERT(compressed_file_descriptor >= 0);
    unlink(compressed_path);
    close(input_pipe[1]);
    g_fragmented_input = "fragment-one-fragment-two";
    g_fragmented_input_offset = 0U;
    g_fragmented_input_size = first_size + second_size;
    ft_compress_stream_set_read_hook(compression_stream_fragmented_read);
    compression_result = ft_compress_stream(input_pipe[0],
        compressed_file_descriptor);
    ft_compress_stream_set_read_hook(ft_nullptr);
    close(input_pipe[0]);
    FT_ASSERT_EQ(0, compression_result);
    FT_ASSERT(lseek(compressed_file_descriptor, 0, SEEK_SET) >= 0);
    compressed_size = 0;
    while (compressed_size < sizeof(compressed_buffer))
    {
        read_bytes = su_read(compressed_file_descriptor,
            compressed_buffer + compressed_size,
            sizeof(compressed_buffer) - compressed_size);
        FT_ASSERT(read_bytes >= 0);
        if (read_bytes == 0)
            break ;
        compressed_size += static_cast<std::size_t>(read_bytes);
    }
    close(compressed_file_descriptor);
    FT_ASSERT(compressed_size > 0);
    decompressed_size = sizeof(decompressed_buffer);
    FT_ASSERT_EQ(Z_OK, uncompress(decompressed_buffer, &decompressed_size,
        compressed_buffer, static_cast<uLong>(compressed_size)));
    FT_ASSERT_EQ(static_cast<uLongf>(first_size + second_size),
        decompressed_size);
    FT_ASSERT_EQ(0, ft_memcmp(decompressed_buffer, first_fragment, first_size));
    FT_ASSERT_EQ(0, ft_memcmp(decompressed_buffer + first_size,
        second_fragment, second_size));
    return (1);
}

FT_TEST(test_ft_decompress_stream_uses_finish_flush_at_end)
{
    const char                  *payload;
    unsigned char               compressed_buffer[256];
    uLongf                      compressed_size;
    int                         zlib_status;
    int                         input_pipe[2];
    int                         output_pipe[2];
    int                         result;

    payload = "stream flush verification";
    compressed_size = sizeof(compressed_buffer);
    zlib_status = compress2(compressed_buffer, &compressed_size,
            reinterpret_cast<const Bytef *>(payload),
            FT_ZLIB_ULONG_CAST(ft_strlen_size_t(payload)), Z_BEST_COMPRESSION);
    FT_ASSERT_EQ(Z_OK, zlib_status);
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    FT_ASSERT_EQ(static_cast<ssize_t>(compressed_size),
        su_write(input_pipe[1], compressed_buffer, compressed_size));
    close(input_pipe[1]);
    g_decompress_last_flush_mode = -1;
    ft_decompress_stream_set_inflate_hook(compression_stream_capture_flush_inflate);
    result = ft_decompress_stream(input_pipe[0], output_pipe[1]);
    ft_decompress_stream_set_inflate_hook(ft_nullptr);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(0, result);
    FT_ASSERT_EQ(Z_FINISH, g_decompress_last_flush_mode);
    return (1);
}

FT_TEST(test_compress_stream_hooks_replace_incrementally)
{
    int         input_pipe[2];
    int         output_pipe[2];
    const char  *payload;
    ssize_t     written_bytes;
    int         result;

    payload = "hook rotation";
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    written_bytes = su_write(input_pipe[1], payload, ft_strlen_size_t(payload));
    FT_ASSERT_EQ(static_cast<ssize_t>(ft_strlen_size_t(payload)), written_bytes);
    close(input_pipe[1]);
    g_compress_first_hook_calls = 0;
    g_compress_second_hook_calls = 0;
    ft_compress_stream_set_deflate_hook(compression_stream_counting_deflate);
    result = ft_compress_stream(input_pipe[0], output_pipe[1]);
    FT_ASSERT_EQ(0, result);
    FT_ASSERT(g_compress_first_hook_calls > 0);
    ft_compress_stream_set_deflate_hook(compression_stream_failing_replacement_deflate);
    result = ft_compress_stream(input_pipe[0], output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    FT_ASSERT(g_compress_second_hook_calls > 0);
    ft_compress_stream_set_deflate_hook(ft_nullptr);
    result = ft_compress_stream(input_pipe[0], output_pipe[1]);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(0, result);
    return (1);
}

FT_TEST(test_ft_decompress_stream_reports_truncated_input)
{
    const char        *payload;
    unsigned char     compressed_buffer[256];
    uLongf            compressed_size;
    int               zlib_status;
    int               input_pipe[2];
    int               output_pipe[2];
    ssize_t           written_bytes;
    int               result;

    payload = "truncate";
    compressed_size = sizeof(compressed_buffer);
    zlib_status = compress2(compressed_buffer, &compressed_size,
            reinterpret_cast<const Bytef *>(payload),
            FT_ZLIB_ULONG_CAST(ft_strlen_size_t(payload)), Z_BEST_COMPRESSION);
    FT_ASSERT_EQ(Z_OK, zlib_status);
    FT_ASSERT(compressed_size > 1);
    FT_ASSERT_EQ(0, pipe(input_pipe));
    FT_ASSERT_EQ(0, pipe(output_pipe));
    written_bytes = su_write(input_pipe[1], compressed_buffer, compressed_size - 1);
    FT_ASSERT_EQ(static_cast<ssize_t>(compressed_size - 1), written_bytes);
    close(input_pipe[1]);
    result = ft_decompress_stream(input_pipe[0], output_pipe[1]);
    close(input_pipe[0]);
    close(output_pipe[0]);
    close(output_pipe[1]);
    FT_ASSERT_EQ(1, result);
    return (1);
}
