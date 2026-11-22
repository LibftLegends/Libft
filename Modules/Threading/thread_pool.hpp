#ifndef THREADING_THREAD_POOL_HPP
#define THREADING_THREAD_POOL_HPP

#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/pthread.hpp"
#include "thread.hpp"
#include "../Template/vector.hpp"
#include "../Template/queue.hpp"
#include "cancellation.hpp"
#include "../Template/function.hpp"
#include "../Template/move.hpp"
#include <atomic>
#include <cstddef>
#include <utility>
#include <type_traits>

class ft_thread_pool
{
    private:
        ft_vector<ft_thread>            _workers;
        ft_queue<ft_function<void()> >  _tasks;
        ft_size_t                       _configured_thread_count;
        ft_size_t                       _max_tasks;
        std::atomic<ft_bool>              _stop;
        ft_size_t                       _active;
        std::atomic<uint32_t>             _work_epoch;
        std::atomic<uint32_t>             _idle_epoch;
        mutable pt_recursive_mutex      *_work_mutex;
        mutable pt_recursive_mutex      *_thread_safe_mutex;
        uint8_t                         _initialised_state;
        static thread_local int32_t     _last_error;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t lock_internal(ft_bool *lock_acquired) const;
        void unlock_internal(ft_bool lock_acquired) const;

        static void worker_entry(ft_thread_pool *pool);
        void worker();
        int32_t stop_workers();

    public:
        ft_thread_pool(ft_size_t thread_count = 0, ft_size_t max_tasks = 0);
        ft_thread_pool(const ft_thread_pool &other) = delete;
        ft_thread_pool(ft_thread_pool &&other) = delete;
        ~ft_thread_pool();

        ft_thread_pool& operator=(const ft_thread_pool&) = delete;
        ft_thread_pool& operator=(ft_thread_pool&&) = delete;

        int32_t initialize();
        int32_t initialize(ft_size_t thread_count, ft_size_t max_tasks);
        int32_t destroy();
        int32_t move(ft_thread_pool &other) noexcept;

        template <typename Function>
        void submit(Function &&function);

        template <typename Function>
        void submit(Function &&function, const ft_cancellation_token &token);

        void wait();

        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;

        int32_t lock(ft_bool *lock_acquired) const;
        void unlock(ft_bool lock_acquired) const;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

template <typename Function>
void ft_thread_pool::submit(Function &&function)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t work_lock_error;
    ft_size_t task_count;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_thread_pool::submit");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        set_error(lock_error);
        return ;
    }
    work_lock_error = pt_recursive_mutex_lock_if_not_null(this->_work_mutex);
    if (work_lock_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        set_error(work_lock_error);
        return ;
    }
    if (this->_stop)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
        this->unlock_internal(lock_acquired);
        set_error(FT_ERR_INVALID_STATE);
        return ;
    }
    if (this->_max_tasks != 0)
    {
        task_count = this->_tasks.size();
        if (task_count >= this->_max_tasks)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
            this->unlock_internal(lock_acquired);
            set_error(FT_ERR_FULL);
            return ;
        }
    }
    {
        ft_function<void()> wrapper(ft_move(function));
        if (!wrapper)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
            this->unlock_internal(lock_acquired);
            set_error(FT_ERR_NO_MEMORY);
            return ;
        }
        this->_tasks.enqueue(ft_move(wrapper));
        if (this->_tasks.get_error() != FT_ERR_SUCCESS)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
            this->unlock_internal(lock_acquired);
            set_error(this->_tasks.get_error());
            return ;
        }
    }
    this->_work_epoch.fetch_add(1U, std::memory_order_release);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_work_mutex);
    this->unlock_internal(lock_acquired);
    (void)pt_thread_wake_one_uint32(&this->_work_epoch);
    set_error(FT_ERR_SUCCESS);
    return ;
}

template <typename Function>
void ft_thread_pool::submit(Function &&function, const ft_cancellation_token &token)
{
    if (token.is_cancellation_requested())
    {
        set_error(FT_ERR_SUCCESS);
        return ;
    }
    this->submit(std::forward<Function>(function));
    return ;
}

#endif
