#include <system_error>
#include "../Basic/limits.hpp"

#include "pthread.hpp"
#include "recursive_mutex.hpp"
#include "pthread_lock_tracking.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno.hpp"
#include "mutex.hpp"

static const uint32_t PT_RECURSIVE_MUTEX_TRY_LOCK_ATTEMPTS = 2U;

int pt_recursive_mutex::try_lock(pt_thread_id_type thread_id) const
{
    int ensure_error = this->ensure_native_mutex();
    const void *mutex_handle;
    int mutex_error;
    bool acquired;
    uint32_t attempt_count;

    if (ensure_error != FT_ERR_SUCCESS)
        return (ensure_error);
    mutex_handle = static_cast<const void *>(this);

    if (this->_lock.load(std::memory_order_acquire))
    {
        pt_thread_id_type owner = this->_owner.load(std::memory_order_relaxed);
        if (pt_thread_equal(owner, thread_id))
        {
            this->_lock_depth.fetch_add(1, std::memory_order_relaxed);
            return (FT_ERR_SUCCESS);
        }
    }

    mutex_error = FT_ERR_SUCCESS;
    acquired = false;
    attempt_count = 0U;
    while (attempt_count < PT_RECURSIVE_MUTEX_TRY_LOCK_ATTEMPTS && !acquired)
    {
        try
        {
            acquired = this->_native_mutex->try_lock();
        }
        catch (const std::system_error &error)
        {
            mutex_error = cmp_map_system_error_to_ft(error.code().value());
        }
        if (mutex_error != FT_ERR_SUCCESS)
            return (mutex_error);
        attempt_count++;
    }
    if (!acquired)
        return (FT_ERR_MUTEX_ALREADY_LOCKED);

    this->_owner.store(thread_id, std::memory_order_relaxed);
    this->_lock.store(true, std::memory_order_release);
    this->_lock_depth.store(1, std::memory_order_relaxed);

    int notify_error = pt_lock_tracking::notify_acquired(thread_id,
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
