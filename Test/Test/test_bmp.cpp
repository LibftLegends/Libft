#include "../test_internal.hpp"
#include "../../Modules/BMP/bmp.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static void bmp_write_u16(uint8_t *data, uint16_t value)
{
    data[0] = static_cast<uint8_t>(value & 0xFFU);
    data[1] = static_cast<uint8_t>(value >> 8U);
    return ;
}

static void bmp_write_u32(uint8_t *data, uint32_t value)
{
    data[0] = static_cast<uint8_t>(value & 0xFFU);
    data[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    data[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    data[3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
    return ;
}

static void bmp_make_2x1(uint8_t data[62])
{
    ft_memset(data, 0, 62U);
    data[0] = 'B';
    data[1] = 'M';
    bmp_write_u32(data + 2U, 62U);
    bmp_write_u32(data + 10U, 54U);
    bmp_write_u32(data + 14U, 40U);
    bmp_write_u32(data + 18U, 2U);
    bmp_write_u32(data + 22U, 1U);
    bmp_write_u16(data + 26U, 1U);
    bmp_write_u16(data + 28U, 24U);
    bmp_write_u32(data + 34U, 8U);
    data[54] = 30U;
    data[55] = 20U;
    data[56] = 10U;
    data[57] = 60U;
    data[58] = 50U;
    data[59] = 40U;
    return ;
}

static void bmp_make_1x2_32_bit_top_down(uint8_t data[62])
{
    ft_memset(data, 0, 62U);
    data[0] = 'B';
    data[1] = 'M';
    bmp_write_u32(data + 2U, 62U);
    bmp_write_u32(data + 10U, 54U);
    bmp_write_u32(data + 14U, 40U);
    bmp_write_u32(data + 18U, 1U);
    bmp_write_u32(data + 22U, 0xFFFFFFFEU);
    bmp_write_u16(data + 26U, 1U);
    bmp_write_u16(data + 28U, 32U);
    bmp_write_u32(data + 34U, 8U);
    data[54] = 30U;
    data[55] = 20U;
    data[56] = 10U;
    data[57] = 40U;
    data[58] = 70U;
    data[59] = 60U;
    data[60] = 50U;
    data[61] = 80U;
    return ;
}

static void bmp_make_3x2_24_bit(uint8_t data[78])
{
    ft_memset(data, 0, 78U);
    data[0] = 'B';
    data[1] = 'M';
    bmp_write_u32(data + 2U, 78U);
    bmp_write_u32(data + 10U, 54U);
    bmp_write_u32(data + 14U, 40U);
    bmp_write_u32(data + 18U, 3U);
    bmp_write_u32(data + 22U, 2U);
    bmp_write_u16(data + 26U, 1U);
    bmp_write_u16(data + 28U, 24U);
    bmp_write_u32(data + 34U, 24U);
    data[54] = 3U;
    data[55] = 2U;
    data[56] = 1U;
    data[57] = 6U;
    data[58] = 5U;
    data[59] = 4U;
    data[60] = 9U;
    data[61] = 8U;
    data[62] = 7U;
    data[66] = 30U;
    data[67] = 20U;
    data[68] = 10U;
    data[69] = 60U;
    data[70] = 50U;
    data[71] = 40U;
    data[72] = 90U;
    data[73] = 80U;
    data[74] = 70U;
    return ;
}

FT_TEST(test_bmp_loads_24_bit_pixels_as_rgba)
{
    uint8_t encoded_data[62];
    bmp_image image;

    bmp_make_2x1(encoded_data);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, image.initialize(encoded_data, 62U));
    FT_ASSERT_EQ(2U, image.width());
    FT_ASSERT_EQ(1U, image.height());
    FT_ASSERT_EQ(8U, image.pixel_size());
    FT_ASSERT_EQ(10U, image.data()[0]);
    FT_ASSERT_EQ(20U, image.data()[1]);
    FT_ASSERT_EQ(30U, image.data()[2]);
    FT_ASSERT_EQ(255U, image.data()[3]);
    FT_ASSERT_EQ(40U, image.data()[4]);
    FT_ASSERT_EQ(50U, image.data()[5]);
    FT_ASSERT_EQ(60U, image.data()[6]);
    FT_ASSERT_EQ(255U, image.data()[7]);
    return (1);
}

FT_TEST(test_bmp_rejects_truncated_and_oversized_inputs)
{
    uint8_t encoded_data[62];
    bmp_image image;

    bmp_make_2x1(encoded_data);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        image.initialize(encoded_data, 61U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.initialize(encoded_data, 62U, 61U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.initialize(encoded_data, 62U,
            BMP_HARD_MAX_FILE_SIZE + 1U));
    return (1);
}

FT_TEST(test_bmp_rejects_dimension_overflow)
{
    uint8_t encoded_data[62];
    bmp_image image;

    bmp_make_2x1(encoded_data);
    bmp_write_u32(encoded_data + 18U, 0x7FFFFFFFU);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.initialize(encoded_data, 62U));
    return (1);
}

FT_TEST(test_bmp_loads_32_bit_top_down_pixels_and_alpha)
{
    uint8_t encoded_data[62];
    bmp_image image;

    bmp_make_1x2_32_bit_top_down(encoded_data);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, image.initialize(encoded_data, 62U));
    FT_ASSERT_EQ(1U, image.width());
    FT_ASSERT_EQ(2U, image.height());
    FT_ASSERT_EQ(8U, image.pixel_size());
    FT_ASSERT_EQ(10U, image.data()[0]);
    FT_ASSERT_EQ(20U, image.data()[1]);
    FT_ASSERT_EQ(30U, image.data()[2]);
    FT_ASSERT_EQ(40U, image.data()[3]);
    FT_ASSERT_EQ(50U, image.data()[4]);
    FT_ASSERT_EQ(60U, image.data()[5]);
    FT_ASSERT_EQ(70U, image.data()[6]);
    FT_ASSERT_EQ(80U, image.data()[7]);
    return (1);
}

FT_TEST(test_bmp_rejects_unsupported_format_fields)
{
    uint8_t encoded_data[62];
    bmp_image image;

    bmp_make_2x1(encoded_data);
    bmp_write_u16(encoded_data + 28U, 8U);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        image.initialize(encoded_data, 62U));
    bmp_make_2x1(encoded_data);
    bmp_write_u32(encoded_data + 30U, 1U);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        image.initialize(encoded_data, 62U));
    bmp_make_2x1(encoded_data);
    bmp_write_u32(encoded_data + 10U, 61U);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        image.initialize(encoded_data, 62U));
    return (1);
}

FT_TEST(test_bmp_loads_larger_padded_rows)
{
    uint8_t encoded_data[78];
    bmp_image image;

    bmp_make_3x2_24_bit(encoded_data);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, image.initialize(encoded_data, 78U));
    FT_ASSERT_EQ(3U, image.width());
    FT_ASSERT_EQ(2U, image.height());
    FT_ASSERT_EQ(10U, image.data()[0]);
    FT_ASSERT_EQ(20U, image.data()[1]);
    FT_ASSERT_EQ(30U, image.data()[2]);
    FT_ASSERT_EQ(40U, image.data()[4]);
    FT_ASSERT_EQ(70U, image.data()[8]);
    FT_ASSERT_EQ(1U, image.data()[12]);
    FT_ASSERT_EQ(4U, image.data()[16]);
    FT_ASSERT_EQ(7U, image.data()[20]);
    return (1);
}

FT_TEST(test_bmp_loads_sample_through_file_api)
{
    uint8_t encoded_data[62];
    const char *file_path;
    bmp_image image;

    file_path = "Test/tmp_bmp_sample.bmp";
    bmp_make_2x1(encoded_data);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        file_write_all(file_path, reinterpret_cast<const char *>(encoded_data),
            62U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, image.initialize(file_path, 62U));
    FT_ASSERT_EQ(2U, image.width());
    FT_ASSERT_EQ(1U, image.height());
    FT_ASSERT_EQ(10U, image.data()[0]);
    FT_ASSERT_EQ(60U, image.data()[6]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(file_path));
    return (1);
}

FT_TEST(test_bmp_initializes_from_rgb_values)
{
    const uint8_t rgb_data[6] = {10U, 20U, 30U, 40U, 50U, 60U};
    bmp_image image;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.initialize_rgb(rgb_data, 2U, 1U, FT_FALSE));
    FT_ASSERT_EQ(2U, image.width());
    FT_ASSERT_EQ(1U, image.height());
    FT_ASSERT_EQ(8U, image.pixel_size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.get_pixel(0U, 0U, &red, &green, &blue, &alpha));
    FT_ASSERT_EQ(10U, red);
    FT_ASSERT_EQ(20U, green);
    FT_ASSERT_EQ(30U, blue);
    FT_ASSERT_EQ(255U, alpha);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.get_pixel(1U, 0U, &red, &green, &blue, &alpha));
    FT_ASSERT_EQ(40U, red);
    FT_ASSERT_EQ(50U, green);
    FT_ASSERT_EQ(60U, blue);
    FT_ASSERT_EQ(255U, alpha);
    return (1);
}

FT_TEST(test_bmp_initializes_from_rgba_values_and_edits_pixels)
{
    const uint8_t rgba_data[8] = {10U, 20U, 30U, 40U,
        50U, 60U, 70U, 80U};
    bmp_image image;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.initialize_rgb(rgba_data, 2U, 1U, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.set_pixel(1U, 0U, 100U, 110U, 120U, 130U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.get_pixel(1U, 0U, &red, &green, &blue, &alpha));
    FT_ASSERT_EQ(100U, red);
    FT_ASSERT_EQ(110U, green);
    FT_ASSERT_EQ(120U, blue);
    FT_ASSERT_EQ(130U, alpha);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.get_pixel(0U, 0U, &red, &green, &blue, &alpha));
    FT_ASSERT_EQ(10U, red);
    FT_ASSERT_EQ(20U, green);
    FT_ASSERT_EQ(30U, blue);
    FT_ASSERT_EQ(40U, alpha);
    return (1);
}

FT_TEST(test_bmp_rejects_rgb_input_errors)
{
    const uint8_t rgb_data[3] = {10U, 20U, 30U};
    bmp_image image;

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        image.initialize_rgb(ft_nullptr, 1U, 1U, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        image.initialize_rgb(rgb_data, 0U, 1U, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        image.initialize_rgb(rgb_data, 1U, 0U, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        image.initialize_rgb(rgb_data, 1U, 1U, static_cast<ft_bool>(2U)));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.initialize_rgb(rgb_data, BMP_HARD_MAX_FILE_SIZE, 1U,
            FT_FALSE));
    return (1);
}

FT_TEST(test_bmp_rejects_invalid_pixel_coordinates_and_outputs)
{
    const uint8_t rgb_data[3] = {10U, 20U, 30U};
    bmp_image image;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.initialize_rgb(rgb_data, 1U, 1U, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER,
        image.get_pixel(0U, 0U, ft_nullptr, &green, &blue, &alpha));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.get_pixel(1U, 0U, &red, &green, &blue, &alpha));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.get_pixel(0U, 1U, &red, &green, &blue, &alpha));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.set_pixel(1U, 0U, 1U, 2U, 3U, 4U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.set_pixel(0U, 1U, 1U, 2U, 3U, 4U));
    return (1);
}

FT_TEST(test_bmp_saves_rgb_values_and_round_trips)
{
    const uint8_t rgba_data[16] = {10U, 20U, 30U, 40U, 50U, 60U,
        70U, 80U, 90U, 100U, 110U, 120U, 130U, 140U, 150U, 160U};
    const char *file_path;
    bmp_image source_image;
    bmp_image loaded_image;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;

    file_path = "Test/tmp_bmp_rgb_round_trip.bmp";
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_image.initialize_rgb(rgba_data, 2U, 2U, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_image.save(file_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_image.initialize(file_path));
    FT_ASSERT_EQ(2U, loaded_image.width());
    FT_ASSERT_EQ(2U, loaded_image.height());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        loaded_image.get_pixel(0U, 0U, &red, &green, &blue, &alpha));
    FT_ASSERT_EQ(10U, red);
    FT_ASSERT_EQ(20U, green);
    FT_ASSERT_EQ(30U, blue);
    FT_ASSERT_EQ(40U, alpha);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        loaded_image.get_pixel(0U, 1U, &red, &green, &blue, &alpha));
    FT_ASSERT_EQ(90U, red);
    FT_ASSERT_EQ(100U, green);
    FT_ASSERT_EQ(110U, blue);
    FT_ASSERT_EQ(120U, alpha);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(file_path));
    return (1);
}

FT_TEST(test_bmp_rgb_operations_with_thread_safety)
{
    const uint8_t rgb_data[3] = {10U, 20U, 30U};
    bmp_image image;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.initialize_rgb(rgb_data, 1U, 1U, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, image.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.set_pixel(0U, 0U, 40U, 50U, 60U, 70U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        image.get_pixel(0U, 0U, &red, &green, &blue, &alpha));
    FT_ASSERT_EQ(40U, red);
    FT_ASSERT_EQ(50U, green);
    FT_ASSERT_EQ(60U, blue);
    FT_ASSERT_EQ(70U, alpha);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, image.disable_thread_safety());
    return (1);
}

FT_TEST(test_bmp_move_transfers_decoded_pixels)
{
    uint8_t encoded_data[62];
    bmp_image source_image;
    bmp_image destination_image;

    bmp_make_2x1(encoded_data);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_image.initialize(encoded_data, 62U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_image.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_image.move(source_image));
    FT_ASSERT_EQ(FT_FALSE, source_image.is_initialised());
    FT_ASSERT_EQ(2U, destination_image.width());
    FT_ASSERT_EQ(8U, destination_image.pixel_size());
    FT_ASSERT_EQ(10U, destination_image.data()[0]);
    return (1);
}

FT_TEST(test_bmp_thread_safety_controls)
{
    bmp_image image;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, image.initialize());
    FT_ASSERT_EQ(FT_FALSE, image.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, image.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, image.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, image.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, image.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, image.is_thread_safe());
    return (1);
}
