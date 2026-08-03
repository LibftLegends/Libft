#include <atomic>
#include <system_error>
#include "../Basic/limits.hpp"
#include "../Errno/errno.hpp"
#include "../System_utils/system_utils.hpp"

#include "mutex.hpp"
#ifdef LIBFT_TEST_BUILD
std::atomic<int> pt_recursive_mutex_lock_override_error_code(FT_ERR_SUCCESS);
#endif

#include "pthread.hpp"
#include "recursive_mutex.hpp"
#include "pthread_lock_tracking.hpp"

int pt_recursive_mutex::lock() const
{
    int ensure_error = this->ensure_native_mutex();
    int notify_error;
    int mutex_error;
    bool acquired_without_wait;

    if (ensure_error != FT_ERR_SUCCESS)
        return (ensure_error);
#ifdef LIBFT_TEST_BUILD
    int override_error = pt_recursive_mutex_lock_override_error_code.load(std::memory_order_acquire);
    if (override_error != FT_ERR_SUCCESS)
        return (override_error);
#endif

    pt_thread_id_type thread_id = pt_thread_self();
    if (this->_lock.load(std::memory_order_acquire))
    {
        pt_thread_id_type owner = this->_owner.load(std::memory_order_relaxed);
        if (pt_thread_equal(owner, thread_id))
        {
            this->_lock_depth.fetch_add(1, std::memory_order_relaxed);
            return (FT_ERR_SUCCESS);
        }
    }

    const void *mutex_handle = static_cast<const void *>(this);
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
            this->_lock_depth.store(1, std::memory_order_relaxed);
            notify_error = pt_lock_tracking::notify_acquired(thread_id,
                    mutex_handle);
            if (notify_error != FT_ERR_SUCCESS)
            {
                this->_lock.store(false, std::memory_order_release);
                this->_owner.store(0, std::memory_order_release);
                this->_lock_depth.store(0, std::memory_order_relaxed);
                try
                {
                    this->_native_mutex->unlock();
                }
                catch (const std::system_error &)
                {
                    su_abort();
                }
                pt_lock_tracking::notify_released(thread_id, mutex_handle);
                return (notify_error);
            }
            return (FT_ERR_SUCCESS);
        }
    }

    int wait_result = pt_lock_tracking::notify_wait(thread_id,
            mutex_handle);
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
        pt_lock_tracking::notify_released(thread_id, mutex_handle);
        return (mutex_error);
    }

    this->_owner.store(thread_id, std::memory_order_relaxed);
    this->_lock.store(true, std::memory_order_release);
    this->_lock_depth.store(1, std::memory_order_relaxed);

    notify_error = pt_lock_tracking::notify_acquired(thread_id,
            mutex_handle);
    if (notify_error != FT_ERR_SUCCESS)
    {
        this->_lock.store(false, std::memory_order_release);
        this->_owner.store(0, std::memory_order_release);
        this->_lock_depth.store(0, std::memory_order_relaxed);
        try
        {
            this->_native_mutex->unlock();
        }
        catch (const std::system_error &)
        {
            su_abort();
        }
        pt_lock_tracking::notify_released(thread_id, mutex_handle);
        return (notify_error);
    }

    return (FT_ERR_SUCCESS);
}
