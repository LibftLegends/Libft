#include <system_error>
#include "pthread.hpp"
#include "mutex.hpp"
#include "../Basic/limits.hpp"
#include "pthread_lock_tracking.hpp"
#include "../Errno/errno.hpp"
#include "../System_utils/system_utils.hpp"
#include "recursive_mutex.hpp"

int pt_mutex::unlock() const
{
    int ensure_error = this->ensure_native_mutex();
    pt_thread_id_type thread_id;
    pthread_t owner;
    int unlock_error;

    if (ensure_error != FT_ERR_SUCCESS)
        return (ensure_error);
    thread_id = pt_thread_self();
    if (!this->_lock.load(std::memory_order_acquire))
        return (FT_ERR_INVALID_STATE);
    owner = this->_owner.load(std::memory_order_relaxed);
    if (!pt_thread_equal(owner, thread_id))
        return (FT_ERR_INVALID_ARGUMENT);
    this->_lock.store(false, std::memory_order_release);
    this->_owner.store(0, std::memory_order_release);
    unlock_error = FT_ERR_SUCCESS;
    try
    {
        this->_native_mutex->unlock();
    }
    catch (const std::system_error &error)
    {
        unlock_error = cmp_map_system_error_to_ft(error.code().value());
    }
    if (unlock_error != FT_ERR_SUCCESS)
    {
        this->_owner.store(thread_id, std::memory_order_release);
        this->_lock.store(true, std::memory_order_release);
        return (unlock_error);
    }
    (void)pt_lock_tracking::notify_released(thread_id,
            static_cast<const void *>(this));
    return (FT_ERR_SUCCESS);
}
