#include "../test_internal.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <thread>

static ft_bool test_pthread_rwlock_excludes_readers(t_pt_rwlock_strategy strategy)
{
    t_pt_rwlock lock;
    if (pt_rwlock_strategy_init(&lock, strategy) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    std::atomic<uint32_t> readers{0U};
    std::atomic<ft_bool> writer{FT_FALSE};
    std::atomic<ft_bool> failed{FT_FALSE};
    uint32_t writes = 0U;
    std::thread reader([&]()
    {
        uint32_t index = 0U;
        while (index < 20000U)
        {
            if (pt_rwlock_strategy_rdlock(&lock) != FT_ERR_SUCCESS)
            {
                failed.store(FT_TRUE);
                return ;
            }
            readers.fetch_add(1U);
            if (writer.load() == FT_TRUE)
                failed.store(FT_TRUE);
            readers.fetch_sub(1U);
            if (pt_rwlock_strategy_rdunlock(&lock) != FT_ERR_SUCCESS)
                failed.store(FT_TRUE);
            ++index;
        }
        return ;
    });
    uint32_t index = 0U;
    while (index < 20000U)
    {
        int32_t result;
        if (index % 2U == 0U)
            result = pt_rwlock_strategy_try_wrlock(&lock);
        else
            result = pt_rwlock_strategy_wrlock(&lock);
        if (result == FT_ERR_SUCCESS)
        {
            writer.store(FT_TRUE);
            if (readers.load() != 0U)
                failed.store(FT_TRUE);
            writer.store(FT_FALSE);
            if (pt_rwlock_strategy_wrunlock(&lock) != FT_ERR_SUCCESS)
                failed.store(FT_TRUE);
            ++writes;
        }
        else if (index % 2U != 0U || result != FT_ERR_THREAD_BUSY)
            failed.store(FT_TRUE);
        ++index;
    }
    reader.join();
    const int32_t destroyed = pt_rwlock_strategy_destroy(&lock);
    return (failed.load() == FT_FALSE && writes != 0U && destroyed == FT_ERR_SUCCESS);
}

FT_TEST(test_pthread_rwlock_writer_excludes_fast_readers_in_both_strategies)
{
    FT_ASSERT(test_pthread_rwlock_excludes_readers(PT_RWLOCK_STRATEGY_READER_PRIORITY));
    FT_ASSERT(test_pthread_rwlock_excludes_readers(PT_RWLOCK_STRATEGY_WRITER_PRIORITY));
    return (1);
}
