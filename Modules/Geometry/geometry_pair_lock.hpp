#ifndef GEOMETRY_PAIR_LOCK_HPP
# define GEOMETRY_PAIR_LOCK_HPP

#include "../PThread/pthread_internal.hpp"
#include <functional>

static inline uint32_t geometry_lock_pair(const pt_recursive_mutex *&lower,
    const pt_recursive_mutex *&upper)
{
    if (lower == upper)
    {
        upper = ft_nullptr;
        return (pt_recursive_mutex_lock_if_not_null(lower));
    }
    if (lower == ft_nullptr || (upper != ft_nullptr
        && std::less<const pt_recursive_mutex *>{}(upper, lower)))
    {
        const pt_recursive_mutex *previous_lower = lower;
        lower = upper;
        upper = previous_lower;
    }
    while (FT_TRUE)
    {
        uint32_t lock_error = pt_recursive_mutex_try_lock_if_not_null(lower);
        if (lock_error == FT_ERR_SUCCESS)
        {
            lock_error = pt_recursive_mutex_try_lock_if_not_null(upper);
            if (lock_error == FT_ERR_SUCCESS)
                return (FT_ERR_SUCCESS);
            (void)pt_recursive_mutex_unlock_if_not_null(lower);
        }
        if (lock_error != FT_ERR_MUTEX_ALREADY_LOCKED)
            return (lock_error);
        (void)pt_thread_yield();
    }
}

static inline void geometry_unlock_pair(const pt_recursive_mutex *lower,
    const pt_recursive_mutex *upper)
{
    (void)pt_recursive_mutex_unlock_if_not_null(upper);
    (void)pt_recursive_mutex_unlock_if_not_null(lower);
    return ;
}

#endif
