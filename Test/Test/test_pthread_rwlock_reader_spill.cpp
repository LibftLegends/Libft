#include "../test_internal.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_pt_rwlock_strategy_reader_ownership_spills_beyond_inline_cache)
{
    t_pt_rwlock locks[65];
    uint32_t lock_index;

    lock_index = 0U;
    while (lock_index < 65U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_init(&locks[lock_index],
            PT_RWLOCK_STRATEGY_READER_PRIORITY));
        lock_index += 1U;
    }
    lock_index = 0U;
    while (lock_index < 65U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            pt_rwlock_strategy_rdlock(&locks[lock_index]));
        lock_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&locks[0U]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&locks[64U]));
    lock_index = 1U;
    while (lock_index < 63U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            pt_rwlock_strategy_rdunlock(&locks[lock_index]));
        lock_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pt_rwlock_strategy_rdunlock(&locks[63U]));
    lock_index = 0U;
    while (lock_index < 65U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS,
            pt_rwlock_strategy_destroy(&locks[lock_index]));
        lock_index += 1U;
    }
    return (1);
}
