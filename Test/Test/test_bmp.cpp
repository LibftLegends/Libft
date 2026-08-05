#include "../test_internal.hpp"
#include "../../Modules/BMP/bmp.hpp"
#include "../../Modules/Errno/errno.hpp"
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

FT_TEST(test_bmp_loads_24_bit_pixels_as_rgba)
{
    uint8_t encoded_data[62];
    ft_bmp_image image;

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
    ft_bmp_image image;

    bmp_make_2x1(encoded_data);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        image.initialize(encoded_data, 61U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.initialize(encoded_data, 62U, 61U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.initialize(encoded_data, 62U,
            FT_BMP_HARD_MAX_FILE_SIZE + 1U));
    return (1);
}

FT_TEST(test_bmp_rejects_dimension_overflow)
{
    uint8_t encoded_data[62];
    ft_bmp_image image;

    bmp_make_2x1(encoded_data);
    bmp_write_u32(encoded_data + 18U, 0x7FFFFFFFU);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        image.initialize(encoded_data, 62U));
    return (1);
}
