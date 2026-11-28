#include "../test_internal.hpp"
#include "../../Modules/Compression/compression.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_compress_string_to_vector_round_trip)
{
    ft_string                  input;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize("In-memory compression helper"));
    ft_vector<unsigned char>   compressed;
    ft_string                  output;
    int                        result;
    int                        comparison_result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed.initialize());
    result = ft_compress_string_to_vector(input, compressed);
    if (result != 0)
        return (0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    result = ft_decompress_vector_to_string(compressed, output);
    if (result != 0)
        return (0);
    if (output.size() != input.size())
        return (0);
    comparison_result = ft_memcmp(output.c_str(), input.c_str(), input.size());
    if (comparison_result != 0)
        return (0);
    return (1);
}

FT_TEST(test_ft_compress_vector_to_vector_round_trip)
{
    unsigned char              sample_bytes[6];
    ft_vector<unsigned char>   input;
    ft_vector<unsigned char>   compressed;
    ft_vector<unsigned char>   output;
    std::size_t                sample_count;
    std::size_t                index;
    int                        result;
    std::size_t                input_size;
    std::size_t                output_size;
    const unsigned char        *input_data;
    const unsigned char        *output_data;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    sample_bytes[0] = 0;
    sample_bytes[1] = 1;
    sample_bytes[2] = 0;
    sample_bytes[3] = 255;
    sample_bytes[4] = 128;
    sample_bytes[5] = 64;
    sample_count = sizeof(sample_bytes) / sizeof(sample_bytes[0]);
    index = 0;
    while (index < sample_count)
    {
        input.push_back(sample_bytes[index]);
        if (input.get_error() != FT_ERR_SUCCESS)
            return (0);
        index++;
    }
    result = ft_compress_vector_to_vector(input, compressed);
    if (result != 0)
        return (0);
    result = ft_decompress_vector_to_vector(compressed, output);
    if (result != 0)
        return (0);
    input_size = input.size();
    if (input.get_error() != FT_ERR_SUCCESS)
        return (0);
    output_size = output.size();
    if (output.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (input_size != output_size)
        return (0);
    if (input_size == 0)
        return (1);
    input_data = input.begin();
    if (input.get_error() != FT_ERR_SUCCESS)
        return (0);
    output_data = output.begin();
    if (output.get_error() != FT_ERR_SUCCESS)
        return (0);
    if (ft_memcmp(input_data, output_data, input_size) != 0)
        return (0);
    return (1);
}

FT_TEST(test_ft_decompress_vector_to_string_reports_allocator_failure)
{
    ft_string                  input;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize("allocator failure exercise"));
    ft_vector<unsigned char>   compressed;
    ft_string                  output;
    int                        result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, compressed.initialize());
    if (ft_compress_string_to_vector(input, compressed) != 0)
        return (0);
    cma_set_alloc_limit(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    result = ft_decompress_vector_to_string(compressed, output);
    cma_set_alloc_limit(0);
    if (result == 0)
        return (0);
    return (1);
}

FT_TEST(test_compression_vector_round_trip_in_place_matches_buffer_format)
{
    const uint8_t expected[] = {0U, 1U, 0xFFU, 0x00U, 0xFFU, 5U};
    ft_vector<uint8_t> bytes;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bytes.initialize());
    ft_size_t index = 0U;
    while (index < sizeof(expected))
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, bytes.push_back(expected[index]));
        ++index;
    }
    ft_size_t compressed_size = 0U;
    uint8_t *compressed = ft_compress(expected, sizeof(expected), &compressed_size);
    FT_ASSERT(compressed != ft_nullptr);
    FT_ASSERT_EQ(0, ft_compress_vector_to_vector(bytes, bytes));
    const ft_bool same_bytes = compressed_size == bytes.size()
        && ft_memcmp(compressed, bytes.begin(), compressed_size) == 0;
    cma_free(compressed);
    FT_ASSERT(same_bytes == FT_TRUE);
    FT_ASSERT_EQ(0, ft_decompress_vector_to_vector(bytes, bytes));
    FT_ASSERT_EQ(sizeof(expected), bytes.size());
    FT_ASSERT_EQ(0, ft_memcmp(expected, bytes.begin(), sizeof(expected)));
    return (1);
}
