#include "../test_internal.hpp"
#include "../../Modules/PThread/pt_buffer.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_pthread_buffer_growth_copies_aliased_element_and_preserves_order)
{
    pt_buffer<uint64_t> buffer;
    pt_buffer<uint64_t> copied;
    pt_buffer_init(buffer);
    pt_buffer_init(copied);
    uint64_t value = 0U;
    while (value < 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_buffer_push(buffer, value));
        ++value;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_buffer_push(buffer, buffer.data[1]));
    pt_buffer_erase(buffer, 1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_buffer_copy(copied, buffer));
    const uint64_t expected[] = {0U, 2U, 3U, 1U};
    const ft_bool valid = copied.size == 4U
        && std::memcmp(expected, copied.data, sizeof(expected)) == 0;
    const int32_t overflow = pt_buffer_reserve(buffer, FT_SYSTEM_SIZE_MAX);
    pt_buffer_destroy(buffer);
    pt_buffer_destroy(copied);
    FT_ASSERT_EQ(FT_TRUE, valid);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, overflow);
    return (1);
}
