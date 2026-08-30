#ifndef PTHREAD_INTERNAL_HPP
# define PTHREAD_INTERNAL_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include <cstdint>
#include "../Errno/errno.hpp"
#include "mutex.hpp"
#include "recursive_mutex.hpp"

static inline int    pt_mutex_lock_if_not_null(const pt_mutex *mutex_pointer)
{
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (mutex_pointer->lock());
}

static inline int    pt_mutex_unlock_if_not_null(const pt_mutex *mutex_pointer)
{
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (mutex_pointer->unlock());
}

static inline int    pt_recursive_mutex_lock_if_not_null(
    const pt_recursive_mutex *mutex_pointer)
{
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (mutex_pointer->lock());
}

static inline int    pt_recursive_mutex_unlock_if_not_null(
    const pt_recursive_mutex *mutex_pointer)
{
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (mutex_pointer->unlock());
}

static inline int32_t pt_rwlock_rdlock_if_not_null(t_pt_rwlock *rwlock)
{
    if (rwlock == ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (pt_rwlock_strategy_rdlock(rwlock));
}

static inline int32_t pt_rwlock_rdunlock_if_not_null(t_pt_rwlock *rwlock)
{
    if (rwlock == ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (pt_rwlock_strategy_rdunlock(rwlock));
}

static inline int32_t pt_rwlock_wrlock_if_not_null(t_pt_rwlock *rwlock)
{
    if (rwlock == ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (pt_rwlock_strategy_wrlock(rwlock));
}

static inline int32_t pt_rwlock_wrunlock_if_not_null(t_pt_rwlock *rwlock)
{
    if (rwlock == ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (pt_rwlock_strategy_wrunlock(rwlock));
}

#endif
