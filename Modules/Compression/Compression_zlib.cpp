#include <zlib.h>
#include <limits>
#include "compression.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#include "../Template/vector.hpp"

static const unsigned char g_compression_empty_input = 0;

static bool compression_vector_operation_failed(const ft_vector<unsigned char> &vector)
{
    return (vector.get_error() != FT_ERR_SUCCESS);
}

static int compression_string_pop_error(const ft_string &string)
{
    if (string.is_initialised() != FT_TRUE)
        return (1);
    return (0);
}

static ft_bool compression_size_fits_zlib_uLong(std::size_t size_value)
{
    if (size_value > std::numeric_limits<uLong>::max())
        return (FT_FALSE);
    return (FT_TRUE);
}

static int  compression_store_in_vector(ft_vector<unsigned char> &destination, const unsigned char *buffer, std::size_t size)
{
    unsigned char   *destination_data;

    if (size == 0)
    {
        destination.clear();
        if (compression_vector_operation_failed(destination))
            return (1);
        return (0);
    }
    if (buffer == ft_nullptr)
    {
        return (1);
    }
    destination.resize(size);
    if (compression_vector_operation_failed(destination))
        return (1);
    destination_data = destination.begin();
    if (compression_vector_operation_failed(destination))
        return (1);
    ft_memcpy(destination_data, buffer, size);
    return (0);
}

static int  compression_assign_string(ft_string &destination, const unsigned char *buffer, std::size_t size)
{
    int32_t assign_error;

    if (destination.is_initialised() != FT_TRUE)
        return (1);
    if (size == 0)
    {
        assign_error = destination.clear();
        if (assign_error != FT_ERR_SUCCESS)
            return (1);
        return (0);
    }
    if (buffer == ft_nullptr)
    {
        return (1);
    }
    assign_error = destination.assign(reinterpret_cast<const char *>(buffer), size);
    if (assign_error != FT_ERR_SUCCESS)
        return (1);
    return (0);
}

uint8_t *compress_buffer(const uint8_t *input_buffer, std::size_t input_size,
    std::size_t *compressed_size)
{
    uLongf          zlib_bound;
    uLongf          actual_size;
    uLong           zlib_input_size;
    unsigned char   *result_buffer;
    unsigned char   *resized_buffer;
    uint32_t        original_size;
    int             zlib_status;

    if (!compressed_size)
    {
        return (ft_nullptr);
    }
    *compressed_size = 0;
    if (!input_buffer)
    {
        return (ft_nullptr);
    }
    if (input_size > compression_max_size)
    {
        return (ft_nullptr);
    }
    if (compression_size_fits_zlib_uLong(input_size) == FT_FALSE)
        return (ft_nullptr);
    zlib_input_size = FT_ZLIB_ULONG_CAST(input_size);
    zlib_bound = compressBound(zlib_input_size);
    if (zlib_bound < zlib_input_size
        || zlib_bound > FT_SYSTEM_SIZE_MAX - sizeof(uint32_t))
        return (ft_nullptr);
    result_buffer = static_cast<unsigned char *>(cma_malloc(zlib_bound + sizeof(uint32_t)));
    if (!result_buffer)
    {
        return (ft_nullptr);
    }
    actual_size = zlib_bound;
        zlib_status = compress2(result_buffer + sizeof(uint32_t), &actual_size, input_buffer,
            zlib_input_size, Z_BEST_COMPRESSION);
    if (zlib_status != Z_OK)
    {
        cma_free(result_buffer);
        return (ft_nullptr);
    }
    original_size = static_cast<uint32_t>(input_size);
    ft_memcpy(result_buffer, &original_size, sizeof(uint32_t));
    *compressed_size = actual_size + sizeof(uint32_t);
    if (actual_size != zlib_bound)
    {
        resized_buffer = static_cast<uint8_t *>(cma_realloc(result_buffer, *compressed_size));
        if (resized_buffer != ft_nullptr)
            result_buffer = resized_buffer;
    }
    return (result_buffer);
}

unsigned char    *decompress_buffer(const unsigned char *input_buffer, std::size_t input_size, std::size_t *decompressed_size)
{
    uint32_t        expected_size;
    uLongf          actual_size;
    uLong           zlib_input_size;
    unsigned char   *result_buffer;
    int             zlib_status;

    if (!decompressed_size)
    {
        return (ft_nullptr);
    }
    *decompressed_size = 0;
    if (!input_buffer || input_size < sizeof(uint32_t))
    {
        return (ft_nullptr);
    }
    ft_memcpy(&expected_size, input_buffer, sizeof(uint32_t));
    if (static_cast<std::size_t>(expected_size) > compression_max_size)
    {
        return (ft_nullptr);
    }
    if (input_size - sizeof(uint32_t) > compression_max_size)
    {
        return (ft_nullptr);
    }
    if (compression_size_fits_zlib_uLong(input_size - sizeof(uint32_t)) == FT_FALSE)
        return (ft_nullptr);
#if defined(__linux__)
    zlib_input_size = input_size - sizeof(uint32_t);
#else
    zlib_input_size = FT_ZLIB_ULONG_CAST(input_size - sizeof(uint32_t));
#endif
    result_buffer = static_cast<unsigned char *>(cma_malloc(expected_size));
    if (!result_buffer)
    {
        return (ft_nullptr);
    }
    actual_size = static_cast<uLongf>(expected_size);
        zlib_status = uncompress(result_buffer, &actual_size, input_buffer + sizeof(uint32_t),
            zlib_input_size);
    if (zlib_status != Z_OK || actual_size != expected_size)
    {
        cma_free(result_buffer);
        return (ft_nullptr);
    }
    *decompressed_size = actual_size;
    return (result_buffer);
}

unsigned char    *ft_compress(const unsigned char *input_buffer, std::size_t input_size, std::size_t *compressed_size)
{
    return (compress_buffer(input_buffer, input_size, compressed_size));
}

unsigned char    *ft_decompress(const unsigned char *input_buffer, std::size_t input_size, std::size_t *decompressed_size)
{
    return (decompress_buffer(input_buffer, input_size, decompressed_size));
}

int ft_compress_string_to_vector(const ft_string &input, ft_vector<unsigned char> &output)
{
    const unsigned char   *input_buffer;
    std::size_t           input_length;
    unsigned char         *compressed_buffer;
    std::size_t           compressed_size;
    int                   store_status;

    {
        int string_error = compression_string_pop_error(input);

        if (string_error != FT_ERR_SUCCESS)
        {
            return (1);
        }
    }
    input_length = input.size();
    input_buffer = reinterpret_cast<const unsigned char *>(input.c_str());
    compressed_size = 0;
    compressed_buffer = ft_compress(input_buffer, input_length, &compressed_size);
    if (!compressed_buffer)
        return (1);
    store_status = compression_store_in_vector(output, compressed_buffer, compressed_size);
    cma_free(compressed_buffer);
    if (store_status != 0)
        return (1);
    return (0);
}

int ft_compress_vector_to_vector(const ft_vector<unsigned char> &input, ft_vector<unsigned char> &output)
{
    std::size_t           input_size;
    const unsigned char   *input_buffer;
    unsigned char         *compressed_buffer;
    std::size_t           compressed_size;
    int                   store_status;

    input_size = input.size();
    if (compression_vector_operation_failed(input))
        return (1);
    if (input_size == 0)
        input_buffer = &g_compression_empty_input;
    else
    {
        input_buffer = input.begin();
        if (compression_vector_operation_failed(input))
            return (1);
    }
    compressed_size = 0;
    compressed_buffer = ft_compress(input_buffer, input_size, &compressed_size);
    if (!compressed_buffer)
        return (1);
    store_status = compression_store_in_vector(output, compressed_buffer, compressed_size);
    cma_free(compressed_buffer);
    if (store_status != 0)
        return (1);
    return (0);
}

int ft_decompress_vector_to_string(const ft_vector<unsigned char> &input, ft_string &output)
{
    std::size_t           input_size;
    const unsigned char   *input_buffer;
    unsigned char         *decompressed_buffer;
    std::size_t           decompressed_size;
    int                   assign_status;

    input_size = input.size();
    if (compression_vector_operation_failed(input))
        return (1);
    if (input_size == 0)
        input_buffer = &g_compression_empty_input;
    else
    {
        input_buffer = input.begin();
        if (compression_vector_operation_failed(input))
            return (1);
    }
    decompressed_size = 0;
    decompressed_buffer = ft_decompress(input_buffer, input_size, &decompressed_size);
    if (!decompressed_buffer)
        return (1);
    assign_status = compression_assign_string(output, decompressed_buffer, decompressed_size);
    cma_free(decompressed_buffer);
    if (assign_status != 0)
        return (1);
    return (0);
}

int ft_decompress_vector_to_vector(const ft_vector<unsigned char> &input, ft_vector<unsigned char> &output)
{
    std::size_t           input_size;
    const unsigned char   *input_buffer;
    unsigned char         *decompressed_buffer;
    std::size_t           decompressed_size;
    int                   store_status;

    input_size = input.size();
    if (compression_vector_operation_failed(input))
        return (1);
    if (input_size == 0)
        input_buffer = &g_compression_empty_input;
    else
    {
        input_buffer = input.begin();
        if (compression_vector_operation_failed(input))
            return (1);
    }
    decompressed_size = 0;
    decompressed_buffer = ft_decompress(input_buffer, input_size, &decompressed_size);
    if (!decompressed_buffer)
        return (1);
    store_status = compression_store_in_vector(output, decompressed_buffer, decompressed_size);
    cma_free(decompressed_buffer);
    if (store_status != 0)
        return (1);
    return (0);
}
