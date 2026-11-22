#include "../test_internal.hpp"
#include "test_scma_shared.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/SCMA/scma_internal.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_scma_resize_grows_preserves_data)
{
    scma_handle handle;
    int write_values[4];
    int read_value;
    ft_size_t index;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(int) * 4));
    handle = scma_allocate(sizeof(int) * 2);
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    index = 0;
    while (index < 2)
    {
        write_values[index] = static_cast<int>(index * 10 + 1);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle,
                sizeof(int) * index,
                &write_values[index],
                sizeof(int)));
        index = index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_resize(handle, sizeof(int) * 4));
    FT_ASSERT_EQ(sizeof(int) * 4, scma_get_size(handle));
    index = 0;
    while (index < 2)
    {
        read_value = 0;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handle,
                sizeof(int) * index,
                &read_value,
                sizeof(int)));
        FT_ASSERT_EQ(write_values[index], read_value);
        index = index + 1;
    }
    write_values[2] = 33;
    write_values[3] = 44;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle,
            sizeof(int) * 2,
            &write_values[2],
            sizeof(int)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle,
            sizeof(int) * 3,
            &write_values[3],
            sizeof(int)));
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handle,
            sizeof(int) * 3,
            &read_value,
            sizeof(int)));
    FT_ASSERT_EQ(write_values[3], read_value);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_resize_shrinks_truncates_tail)
{
    scma_handle handle;
    int write_values[3];
    int read_value;
    ft_size_t index;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(int) * 3));
    handle = scma_allocate(sizeof(int) * 3);
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    index = 0;
    while (index < 3)
    {
        write_values[index] = static_cast<int>(index * 5 + 2);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle,
                sizeof(int) * index,
                &write_values[index],
                sizeof(int)));
        index = index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_resize(handle, sizeof(int) * 2));
    FT_ASSERT_EQ(sizeof(int) * 2, scma_get_size(handle));
    index = 0;
    while (index < 2)
    {
        read_value = 0;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handle,
                sizeof(int) * index,
                &read_value,
                sizeof(int)));
        FT_ASSERT_EQ(write_values[index], read_value);
        index = index + 1;
    }
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, scma_read(handle,
            sizeof(int) * 2,
            &read_value,
            sizeof(int)));
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_resize_updates_live_end_without_moving_other_handles)
{
    const uint32_t payload = 0x12345678U;
    uint32_t readback = 0U;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_test_initialize(64U));
    const scma_handle first = scma_allocate(8U);
    const scma_handle second = scma_allocate(8U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(first, 0U, &payload,
        sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_resize(first, 4U));
    FT_ASSERT_EQ(16U, scma_used_size_ref());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_resize(first, 12U));
    FT_ASSERT_EQ(28U, scma_used_size_ref());
    FT_ASSERT_EQ(8U, scma_blocks_data_ref()[second.index].offset);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_resize(first, 16U));
    FT_ASSERT_EQ(32U, scma_used_size_ref());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_resize(first, 4U));
    FT_ASSERT_EQ(20U, scma_used_size_ref());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_resize(first, 4U));
    FT_ASSERT_EQ(20U, scma_used_size_ref());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(first, 0U, &readback,
        sizeof(readback)));
    FT_ASSERT_EQ(payload, readback);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_resize_rejects_zero_size)
{
    scma_handle handle;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(int)));
    handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, scma_resize(handle, 0));
    scma_shutdown();
    return (1);
}
