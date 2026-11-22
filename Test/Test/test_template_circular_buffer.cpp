#include "../test_internal.hpp"
#include "../../Modules/Template/circular_buffer.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_circular_buffer_push_pop_cycle)
{
    ft_circular_buffer<int> buffer_instance(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.initialize());

    FT_ASSERT_EQ(false, buffer_instance.is_full());
    FT_ASSERT_EQ(true, buffer_instance.is_empty());
    FT_ASSERT_EQ(0UL, buffer_instance.size());
    FT_ASSERT_EQ(3UL, buffer_instance.capacity());

    buffer_instance.push(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());
    buffer_instance.push(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());
    buffer_instance.push(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());

    FT_ASSERT(buffer_instance.is_full());
    FT_ASSERT_EQ(false, buffer_instance.is_empty());

    int first_value = buffer_instance.pop();
    FT_ASSERT_EQ(1, first_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());

    buffer_instance.push(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());

    int second_value = buffer_instance.pop();
    FT_ASSERT_EQ(2, second_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());

    int third_value = buffer_instance.pop();
    FT_ASSERT_EQ(3, third_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());

    int fourth_value = buffer_instance.pop();
    FT_ASSERT_EQ(4, fourth_value);
    FT_ASSERT(buffer_instance.is_empty());
    FT_ASSERT_EQ(0UL, buffer_instance.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());
    return (1);
}

FT_TEST(test_ft_circular_buffer_error_states)
{
    ft_circular_buffer<int> buffer_instance(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.initialize());

    buffer_instance.push(10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());
    buffer_instance.push(20);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());

    buffer_instance.push(30);
    FT_ASSERT_EQ(FT_ERR_FULL, buffer_instance.get_error());

    buffer_instance.clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());
    FT_ASSERT(buffer_instance.is_empty());

    int empty_value = buffer_instance.pop();
    FT_ASSERT_EQ(0, empty_value);
    FT_ASSERT_EQ(FT_ERR_EMPTY, buffer_instance.get_error());

    buffer_instance.push(50);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());
    buffer_instance.push(60);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());

    buffer_instance.pop();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());
    buffer_instance.pop();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());
    return (1);
}

FT_TEST(test_ft_circular_buffer_wraparound_preserves_order)
{
    ft_circular_buffer<int> buffer_instance(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.initialize());
    buffer_instance.push(9);
    buffer_instance.push(8);
    FT_ASSERT_EQ(9, buffer_instance.pop());
    buffer_instance.push(7);
    FT_ASSERT_EQ(8, buffer_instance.pop());
    FT_ASSERT_EQ(7, buffer_instance.pop());
    FT_ASSERT(buffer_instance.is_empty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer_instance.get_error());
    return (1);
}

FT_TEST(test_ft_circular_buffer_handles_zero_capacity_and_rejects_byte_overflow)
{
    ft_circular_buffer<uint64_t> oversized(FT_SYSTEM_SIZE_MAX / sizeof(uint64_t) + 1U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, oversized.initialize());
    ft_circular_buffer<uint64_t> empty(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty.initialize());
    const uint64_t value = 8U;
    empty.push(value);
    FT_ASSERT_EQ(FT_ERR_FULL, empty.get_error());
    FT_ASSERT_EQ(0U, empty.pop());
    FT_ASSERT_EQ(FT_ERR_EMPTY, empty.get_error());
    empty.clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty.get_error());
    return (1);
}

struct s_circular_clear_probe
{
    uint32_t *destroyed_mask = ft_nullptr;
    uint32_t bit = 0U;

    ~s_circular_clear_probe();
};

s_circular_clear_probe::~s_circular_clear_probe()
{
    if (this->destroyed_mask != ft_nullptr)
        *this->destroyed_mask |= this->bit;
    return ;
}

FT_TEST(test_ft_circular_buffer_clear_destroys_both_wrapped_segments)
{
    uint32_t destroyed_mask = 0U;
    const s_circular_clear_probe first = {&destroyed_mask, 1U};
    const s_circular_clear_probe second = {&destroyed_mask, 2U};
    const s_circular_clear_probe third = {&destroyed_mask, 4U};
    ft_circular_buffer<s_circular_clear_probe> values(2U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    values.push(first);
    values.push(second);
    FT_ASSERT_EQ(1U, values.pop().bit);
    values.push(third);
    destroyed_mask = 0U;
    values.clear();
    FT_ASSERT_EQ(6U, destroyed_mask);
    FT_ASSERT(values.is_empty());
    return (1);
}
