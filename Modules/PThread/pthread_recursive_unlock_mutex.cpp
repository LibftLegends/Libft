#include <system_error>
#include "pthread.hpp"
#include "recursive_mutex.hpp"
#include "pthread_lock_tracking.hpp"
#include "../Errno/errno.hpp"
#include "mutex.hpp"
#if defined(LIBFT_ENABLE_ANALYTICS)
# include "../Analytics/analytics.hpp"
# include <cstdio>
#endif

int pt_recursive_mutex::unlock_uninstrumented() const
{
    int ensure_error = this->ensure_native_mutex();
    pt_thread_id_type thread_id;
    pt_thread_id_type owner;
    std::size_t current_depth;
    int mutex_error;

    if (ensure_error != FT_ERR_SUCCESS)
        return (ensure_error);
    thread_id = pt_thread_self();
    if (!this->_lock.load(std::memory_order_acquire))
        return (FT_ERR_INVALID_STATE);
    owner = this->_owner.load(std::memory_order_relaxed);
    if (!pt_thread_equal(owner, thread_id))
        return (FT_ERR_INVALID_ARGUMENT);
    current_depth = this->_lock_depth.load(std::memory_order_relaxed);
    if (current_depth > 1)
    {
        this->_lock_depth.store(current_depth - 1, std::memory_order_relaxed);
        return (FT_ERR_SUCCESS);
    }
    this->_lock_depth.store(0, std::memory_order_relaxed);
    this->_lock.store(false, std::memory_order_release);
    this->_owner.store(0, std::memory_order_release);
    mutex_error = FT_ERR_SUCCESS;
    try
    {
        this->_native_mutex->unlock();
    }
    catch (const std::system_error &error)
    {
        mutex_error = cmp_map_system_error_to_ft(error.code().value());
    }
    if (mutex_error != FT_ERR_SUCCESS)
    {
        this->_owner.store(thread_id, std::memory_order_release);
        this->_lock.store(true, std::memory_order_release);
        this->_lock_depth.store(1, std::memory_order_relaxed);
        return (mutex_error);
    }
    (void)pt_lock_tracking::notify_released(thread_id,
            static_cast<const void *>(this));
    return (FT_ERR_SUCCESS);
}

int pt_recursive_mutex::unlock() const
{
#if defined(LIBFT_ENABLE_ANALYTICS)
    analytics_runtime_scope_token token;
    int analytics_error;
    int result;

    analytics_error = analytics_runtime_scope_begin(
        analytics_runtime_region::PT_RECURSIVE_MUTEX_UNLOCK, &token);
    if (analytics_error != FT_ERR_SUCCESS)
        return (this->unlock_uninstrumented());
    result = this->unlock_uninstrumented();
    analytics_error = analytics_runtime_scope_end(&token);
    if (analytics_error != FT_ERR_SUCCESS)
        std::fprintf(stderr,
            "[LIBFT][Analytics] pt_recursive_mutex_unlock scope failed: %d\n",
            analytics_error);
    return (result);
#else
    return (this->unlock_uninstrumented());
#endif
}
