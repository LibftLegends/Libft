#include "../test_internal.hpp"
#include "../../Modules/Template/pool.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_pool_resize_preserves_live_slots_and_free_indices)
{
    Pool<uint32_t> pool;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.initialize());
    pool.resize(1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.get_error());
    {
        auto retained = pool.acquire(11U);
        FT_ASSERT(static_cast<ft_bool>(retained));
        uint32_t *address = retained.operator->();
        pool.resize(3U);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.get_error());
        pool.resize(3U);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.get_error());
        {
            auto high_slot = pool.acquire(22U);
            auto middle_slot = pool.acquire(33U);
            FT_ASSERT(static_cast<ft_bool>(high_slot));
            FT_ASSERT(static_cast<ft_bool>(middle_slot));
            auto empty = pool.acquire(44U);
            FT_ASSERT(!static_cast<ft_bool>(empty));
            FT_ASSERT_EQ(FT_ERR_EMPTY, pool.get_error());
            pool.resize(2U);
            FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, pool.get_error());
            FT_ASSERT_EQ(22U, *high_slot.operator->());
        }
        pool.resize(1U);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.get_error());
        pool.resize(FT_SYSTEM_SIZE_MAX);
        FT_ASSERT_EQ(FT_ERR_NO_MEMORY, pool.get_error());
        FT_ASSERT_EQ(address, retained.operator->());
        FT_ASSERT_EQ(11U, *address);
        auto empty = pool.acquire(55U);
        FT_ASSERT(!static_cast<ft_bool>(empty));
        FT_ASSERT_EQ(FT_ERR_EMPTY, pool.get_error());
    }
    auto reused = pool.acquire(66U);
    FT_ASSERT(static_cast<ft_bool>(reused));
    FT_ASSERT_EQ(66U, *reused.operator->());
    return (1);
}

FT_TEST(test_pool_failed_growth_and_acquire_leave_free_slot_available)
{
    Pool<uint32_t> pool;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.initialize());
    pool.resize(1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.get_error());
    cma_set_alloc_limit(1U);
    pool.resize(128U);
    const int32_t growth_error = pool.get_error();
    auto failed = pool.acquire(1U);
    const int32_t acquire_error = pool.get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, growth_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, acquire_error);
    FT_ASSERT(!static_cast<ft_bool>(failed));
    auto allocated = pool.acquire(19U);
    FT_ASSERT(static_cast<ft_bool>(allocated));
    FT_ASSERT_EQ(19U, *allocated.operator->());
    auto exhausted = pool.acquire(20U);
    FT_ASSERT(!static_cast<ft_bool>(exhausted));
    FT_ASSERT_EQ(FT_ERR_EMPTY, pool.get_error());
    return (1);
}
