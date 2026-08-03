#include "pthread.hpp"
#include "mutex.hpp"
#include "pthread_lock_tracking.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include <atomic>
#include <system_error>

#include "../Basic/limits.hpp"
#include "recursive_mutex.hpp"
#ifdef LIBFT_TEST_BUILD
std::atomic<int> pt_mutex_lock_override_error_code(FT_ERR_SUCCESS);
#endif

int pt_mutex::lock() const
{
    pt_thread_id_type thread_id = pt_thread_self();
    int ensure_error = this->ensure_native_mutex();
    int notify_error;
    int mutex_error;
    bool acquired_without_wait;

    if (ensure_error != FT_ERR_SUCCESS)
        return (ensure_error);
#ifdef LIBFT_TEST_BUILD
    int override_error = pt_mutex_lock_override_error_code.load(std::memory_order_acquire);
    if (override_error != FT_ERR_SUCCESS)
        return (override_error);
#endif

    pthread_t owner = this->_owner.load(std::memory_order_relaxed);
    if (this->_lock.load(std::memory_order_acquire)
            && pt_thread_equal(owner, thread_id))
        return (FT_ERR_MUTEX_ALREADY_LOCKED);
    acquired_without_wait = false;
    mutex_error = FT_ERR_SUCCESS;
    if (!this->_lock.load(std::memory_order_acquire))
    {
        try
        {
            acquired_without_wait = this->_native_mutex->try_lock();
        }
        catch (const std::system_error &error)
        {
            mutex_error = cmp_map_system_error_to_ft(error.code().value());
        }
        if (mutex_error != FT_ERR_SUCCESS)
            return (mutex_error);
        if (acquired_without_wait)
        {
            this->_owner.store(thread_id, std::memory_order_relaxed);
            this->_lock.store(true, std::memory_order_release);
            notify_error = pt_lock_tracking::notify_acquired(thread_id,
                    static_cast<const void *>(this));
            if (notify_error != FT_ERR_SUCCESS)
            {
                this->_lock.store(false, std::memory_order_release);
                this->_owner.store(0, std::memory_order_release);
                try
                {
                    this->_native_mutex->unlock();
                }
                catch (const std::system_error &)
                {
                    su_abort();
                }
                pt_lock_tracking::notify_released(thread_id,
                        static_cast<const void *>(this));
                return (notify_error);
            }
            return (FT_ERR_SUCCESS);
        }
    }

    int wait_result = pt_lock_tracking::notify_wait(thread_id,
            static_cast<const void *>(this));
    if (wait_result != FT_ERR_SUCCESS)
        return (wait_result);

    mutex_error = FT_ERR_SUCCESS;
    try
    {
        this->_native_mutex->lock();
    }
    catch (const std::system_error &error)
    {
        mutex_error = cmp_map_system_error_to_ft(error.code().value());
    }
    if (mutex_error != FT_ERR_SUCCESS)
    {
        pt_lock_tracking::notify_released(thread_id,
                static_cast<const void *>(this));
        return (mutex_error);
    }

    this->_owner.store(thread_id, std::memory_order_relaxed);
    this->_lock.store(true, std::memory_order_release);

    notify_error = pt_lock_tracking::notify_acquired(thread_id,
            static_cast<const void *>(this));
    if (notify_error != FT_ERR_SUCCESS)
    {
        this->_lock.store(false, std::memory_order_release);
        this->_owner.store(0, std::memory_order_release);
        try
        {
            this->_native_mutex->unlock();
        }
        catch (const std::system_error &)
        {
            su_abort();
        }
        pt_lock_tracking::notify_released(thread_id,
                static_cast<const void *>(this));
        return (notify_error);
    }

    return (FT_ERR_SUCCESS);
}
