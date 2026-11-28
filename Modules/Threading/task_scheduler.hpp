#ifndef THREADING_TASK_SCHEDULER_HPP
#define THREADING_TASK_SCHEDULER_HPP

#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Template/function.hpp"
#include "../Template/vector.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Template/promise.hpp"
#include "../Template/future.hpp"
#include "../Template/queue.hpp"
#include "../Template/pair.hpp"
#include "thread.hpp"
#include "../Time/time.hpp"
#include "../PThread/condition.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "task_scheduler_tracing.hpp"

#include <pthread.h>
#include <cerrno>
#include <ctime>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "../Template/move.hpp"
template <typename ElementType>
class ft_blocking_queue
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        pt_mutex _mutex;
        pt_condition_variable _condition;
        bool _shutdown;
        std::atomic<ft_size_t> _waiting_workers;
        uint8_t _initialised_state;
        ft_queue<ElementType> _storage;
        mutable pt_mutex *_state_mutex;
        int lock_internal(bool *lock_acquired) const;
        int unlock_internal(bool lock_acquired) const;
        void teardown_thread_safety();

    public:
        ft_blocking_queue();
        ft_blocking_queue(ft_blocking_queue &&other) = delete;
        ~ft_blocking_queue();

        ft_blocking_queue(const ft_blocking_queue&) = delete;
        ft_blocking_queue &operator=(const ft_blocking_queue&) = delete;

        int initialize();
        int destroy();
        int enable_thread_safety();
        int disable_thread_safety();
        bool is_thread_safe() const;
        int lock(bool *lock_acquired) const;
        void unlock(bool lock_acquired) const;

        int push(ElementType &&value);
        bool pop(ElementType &result);
        bool wait_pop(ElementType &result, const std::atomic<bool> &running_flag);
        void shutdown();
};

class ft_task_scheduler;

class pt_mutex;
class pt_recursive_mutex;

class ft_scheduled_task_state
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        std::atomic<bool> _cancelled;
        mutable pt_mutex *_state_mutex;
        int lock_internal(bool *lock_acquired) const;
        int unlock_internal(bool lock_acquired) const;
        void teardown_thread_safety();

    public:
        ft_scheduled_task_state();
        ~ft_scheduled_task_state();

        void cancel();
        bool is_cancelled() const;

        int enable_thread_safety();
        int disable_thread_safety();
        bool is_thread_safe() const;
        int lock(bool *lock_acquired) const;
        void unlock(bool lock_acquired) const;
};

class ft_scheduled_task_handle
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_sharedptr<ft_scheduled_task_state> _state;
        ft_task_scheduler *_scheduler;
        mutable pt_mutex *_state_mutex;
        int lock_internal(bool *lock_acquired) const;
        int unlock_internal(bool lock_acquired) const;
        void teardown_thread_safety();

    public:
        ft_scheduled_task_handle();
        ft_scheduled_task_handle(ft_task_scheduler *scheduler,
                const ft_sharedptr<ft_scheduled_task_state> &state);
        ft_scheduled_task_handle(const ft_scheduled_task_handle &other);
        ft_scheduled_task_handle &operator=(const ft_scheduled_task_handle &other);
        ~ft_scheduled_task_handle();

        bool cancel();
        bool valid() const;
        const ft_scheduled_task_state *get_state() const;

        int enable_thread_safety();
        int disable_thread_safety();
        bool is_thread_safe() const;
        int lock(bool *lock_acquired) const;
        void unlock(bool lock_acquired) const;
};

class ft_task_scheduler
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        struct task_queue_entry
        {
            ft_function<void()> _function;
            unsigned long long _trace_id;
            unsigned long long _parent_id;
            const char *_label;
        };

        struct scheduled_task
        {
            t_monotonic_time_point _time;
            long long _interval_ms;
            ft_function<void()> _function;
            ft_sharedptr<ft_scheduled_task_state> _state;
            unsigned long long _trace_id;
            unsigned long long _parent_id;
            const char *_label;
            uint8_t _initialised_state;
            static const uint8_t _state_uninitialised = 0;
            static const uint8_t _state_destroyed = 1;
            static const uint8_t _state_initialised = 2;

            scheduled_task();
            ~scheduled_task();
            scheduled_task(const scheduled_task &other);
            scheduled_task(scheduled_task &&other);
            scheduled_task &operator=(const scheduled_task &other);
            scheduled_task &operator=(scheduled_task &&other);
            int initialize();
            int initialize(const scheduled_task &other);
            int initialize_move(scheduled_task &other);
            int destroy();
            int is_initialised() const;
        };

        ft_blocking_queue<task_queue_entry> _queue;
        ft_vector<ft_thread> _workers;
        ft_thread _timer_thread;
        ft_vector<scheduled_task> _scheduled;
        mutable pt_mutex _scheduled_mutex;
        pt_condition_variable _scheduled_condition;
        std::atomic<bool> _running;
        mutable pt_mutex _queue_metrics_mutex;
        mutable pt_mutex _worker_metrics_mutex;
        long long _queue_size_counter;
        long long _scheduled_size_counter;
        long long _worker_active_counter;
        long long _worker_idle_counter;
        size_t _worker_total_count;
        size_t _configured_thread_count;
        mutable pt_recursive_mutex *_state_mutex;
        mutable uint8_t _initialised_state;
        static const uint8_t _state_uninitialised = 0;
        static const uint8_t _state_destroyed = 1;
        static const uint8_t _state_initialised = 2;

        bool cancel_task_state(const ft_sharedptr<ft_scheduled_task_state> &state);
        bool scheduled_remove_index(size_t index);
        void worker_loop();
        void timer_loop();
        bool scheduled_heap_push(scheduled_task &&task);
        bool scheduled_heap_pop(scheduled_task &task);
        void scheduled_heap_sift_up(size_t index);
        void scheduled_heap_sift_down(size_t index);
        bool update_queue_size(long long delta);
        bool update_worker_counters(long long active_delta, long long idle_delta);
        bool update_worker_total(long long delta);
        void trace_emit_event(e_ft_task_trace_phase phase, unsigned long long trace_id,
                unsigned long long parent_id, const char *label, bool timer_thread);
        bool capture_metrics(ft_task_trace_event &event) const;
        int lock_internal(bool *lock_acquired) const;
        int unlock_internal(bool lock_acquired) const;
        void teardown_thread_safety();
        void abort_if_not_initialised(const char *method_name) const;

    public:
        friend class ft_scheduled_task_handle;

        ft_task_scheduler(size_t thread_count = 0);
        ~ft_task_scheduler();

        ft_task_scheduler(const ft_task_scheduler &) = delete;
        ft_task_scheduler &operator=(const ft_task_scheduler &) = delete;
        ft_task_scheduler(ft_task_scheduler &&) = delete;
        ft_task_scheduler &operator=(ft_task_scheduler &&) = delete;
        int initialize(size_t thread_count = 0);
        int destroy();

        template <typename FunctionType, typename... Args>
        int32_t submit(ft_future<typename std::invoke_result<FunctionType, Args...>::type> &future_value,
                FunctionType function, Args... args);

        template <typename Rep, typename Period, typename FunctionType, typename... Args>
        auto schedule_after(std::chrono::duration<Rep, Period> delay,
                FunctionType function, Args... args)
            -> Pair<ft_future<typename std::invoke_result<FunctionType, Args...>::type>,
                    ft_scheduled_task_handle>;

        template <typename Rep, typename Period, typename FunctionType, typename... Args>
        ft_scheduled_task_handle schedule_every(std::chrono::duration<Rep, Period> interval,
                FunctionType function, Args... args);

        long long get_queue_size() const;
        int get_queue_size(long long *queue_size) const;
        long long get_scheduled_task_count() const;
        int get_scheduled_task_count(long long *scheduled_count) const;
        long long get_worker_active_count() const;
        int get_worker_active_count(long long *active_count) const;
        long long get_worker_idle_count() const;
        int get_worker_idle_count(long long *idle_count) const;
        size_t get_worker_total_count() const;
        int get_worker_total_count(size_t *total_count) const;

        int enable_thread_safety();
        int disable_thread_safety();
        bool is_thread_safe() const;
        int lock(bool *lock_acquired) const;
        int unlock(bool lock_acquired) const;
};

template <typename ElementType>
ft_blocking_queue<ElementType>::ft_blocking_queue()
    : _mutex(), _condition(), _shutdown(false),
      _waiting_workers(0U),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _storage(), _state_mutex(ft_nullptr)
{
    return ;
}

template <typename ElementType>
ft_blocking_queue<ElementType>::~ft_blocking_queue()
{
    (void)this->destroy();
    return ;
}

template <typename ElementType>
int ft_blocking_queue<ElementType>::initialize()
{
    int mutex_initialize_error;
    int storage_initialize_error;
    int condition_enable_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    this->_shutdown = false;
    this->_waiting_workers = 0U;
    mutex_initialize_error = this->_mutex.initialize();
    if (mutex_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (mutex_initialize_error);
    }
    storage_initialize_error = this->_storage.initialize();
    if (storage_initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_mutex.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (storage_initialize_error);
    }
    condition_enable_error = this->_condition.enable_thread_safety();
    if (condition_enable_error != FT_ERR_SUCCESS)
    {
        (void)this->_storage.destroy();
        (void)this->_mutex.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (condition_enable_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
int ft_blocking_queue<ElementType>::destroy()
{
    int first_error;
    int storage_destroy_error;
    int condition_disable_error;
    int mutex_destroy_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    first_error = FT_ERR_SUCCESS;
    this->shutdown();
    this->teardown_thread_safety();
    storage_destroy_error = this->_storage.destroy();
    if (first_error == FT_ERR_SUCCESS && storage_destroy_error != FT_ERR_SUCCESS)
        first_error = storage_destroy_error;
    condition_disable_error = this->_condition.disable_thread_safety();
    if (first_error == FT_ERR_SUCCESS && condition_disable_error != FT_ERR_SUCCESS)
        first_error = condition_disable_error;
    mutex_destroy_error = this->_mutex.destroy();
    if (first_error == FT_ERR_SUCCESS && mutex_destroy_error != FT_ERR_SUCCESS)
        first_error = mutex_destroy_error;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

template <typename ElementType>
int ft_blocking_queue<ElementType>::lock_internal(bool *lock_acquired) const
{
    int state_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    state_error = pt_mutex_lock_if_not_null(this->_state_mutex);
    if (state_error != FT_ERR_SUCCESS)
    {
        if (state_error == FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            bool state_lock_acquired;

            state_lock_acquired = false;
            if (this->_state_mutex->lock_state(&state_lock_acquired) == 0)
                this->_state_mutex->unlock_state(state_lock_acquired);
            return (FT_ERR_SUCCESS);
        }
        return (state_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
int ft_blocking_queue<ElementType>::unlock_internal(bool lock_acquired) const
{
    if (!lock_acquired || this->_state_mutex == ft_nullptr)
    {
        return (FT_ERR_SUCCESS);
    }
    (void)pt_mutex_unlock_if_not_null(this->_state_mutex);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
void ft_blocking_queue<ElementType>::teardown_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
    {
        int destroy_error;

        destroy_error = this->_state_mutex->destroy();
        (void)destroy_error;
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
    }
    this->_condition.disable_thread_safety();
    return ;
}

template <typename ElementType>
int ft_blocking_queue<ElementType>::enable_thread_safety()
{
    if (this->_state_mutex != ft_nullptr)
    {
        if (this->_condition.enable_thread_safety() != 0)
            return (-1);
        return (0);
    }
    int mutex_error;
    pt_mutex *mutex_pointer;

    mutex_pointer = new (std::nothrow) pt_mutex();
    if (mutex_pointer == ft_nullptr)
        return (-1);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (-1);
    }
    this->_state_mutex = mutex_pointer;

    if (this->_condition.enable_thread_safety() != 0)
    {
        int destroy_error;

        destroy_error = this->_state_mutex->destroy();
        (void)destroy_error;
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
        return (-1);
    }
    return (0);
}

template <typename ElementType>
int ft_blocking_queue<ElementType>::disable_thread_safety()
{
    int condition_error;

    condition_error = this->_condition.disable_thread_safety();
    if (condition_error != FT_ERR_SUCCESS)
        return (condition_error);
    if (this->_state_mutex != ft_nullptr)
    {
        int destroy_error;

        destroy_error = this->_state_mutex->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        delete this->_state_mutex;
        this->_state_mutex = ft_nullptr;
    }
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
bool ft_blocking_queue<ElementType>::is_thread_safe() const
{
    bool enabled;

    enabled = (this->_state_mutex != ft_nullptr);
    return (enabled);
}

template <typename ElementType>
int ft_blocking_queue<ElementType>::lock(bool *lock_acquired) const
{
    int operation_result = this->lock_internal(lock_acquired);
    if (operation_result != FT_ERR_SUCCESS)
        return (-1);
    return (0);
}

template <typename ElementType>
void ft_blocking_queue<ElementType>::unlock(bool lock_acquired) const
{
    (void)this->unlock_internal(lock_acquired);
    return ;
}

template <typename ElementType>
int ft_blocking_queue<ElementType>::push(ElementType &&value)
{
    bool state_lock_acquired;
    int state_lock_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    state_lock_acquired = false;
    state_lock_error = this->lock_internal(&state_lock_acquired);
    if (state_lock_error != FT_ERR_SUCCESS)
        return (state_lock_error);
    int mutex_error = this->_mutex.lock();

    if (mutex_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(state_lock_acquired);
        return (mutex_error);
    }
    this->_storage.enqueue(ft_move(value));
    const int storage_error = this->_storage.get_error();
    if (storage_error != FT_ERR_SUCCESS)
    {
        this->_mutex.unlock();
        this->unlock_internal(state_lock_acquired);
        return (storage_error);
    }
    if (this->_waiting_workers.load(std::memory_order_relaxed) != 0U)
    {
        if (this->_condition.signal() != 0)
        {
            mutex_error = this->_mutex.unlock();
            this->unlock_internal(state_lock_acquired);
            if (mutex_error != FT_ERR_SUCCESS)
                return (mutex_error);
            return (FT_ERR_SYSTEM);
        }
    }
    mutex_error = this->_mutex.unlock();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(state_lock_acquired);
        return (mutex_error);
    }
    this->unlock_internal(state_lock_acquired);
    return (FT_ERR_SUCCESS);
}

template <typename ElementType>
bool ft_blocking_queue<ElementType>::pop(ElementType &result)
{
    bool is_empty;
    ElementType value;
    bool state_lock_acquired;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (false);
    state_lock_acquired = false;
    int state_lock_error = this->lock_internal(&state_lock_acquired);
    if (state_lock_error != FT_ERR_SUCCESS)
        return (false);
    int mutex_error = this->_mutex.lock();

    if (mutex_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(state_lock_acquired);
        return (false);
    }
    is_empty = this->_storage.empty();
    int storage_error = this->_storage.get_error();
    if (storage_error != FT_ERR_SUCCESS)
    {
        this->_mutex.unlock();
        this->unlock_internal(state_lock_acquired);
        return (false);
    }
    if (is_empty)
    {
        this->_mutex.unlock();
        this->unlock_internal(state_lock_acquired);
        return (false);
    }
    value = this->_storage.dequeue();
    int dequeue_error = this->_storage.get_error();
    if (dequeue_error != FT_ERR_SUCCESS)
    {
        this->_mutex.unlock();
        this->unlock_internal(state_lock_acquired);
        return (false);
    }
    mutex_error = this->_mutex.unlock();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(state_lock_acquired);
        return (false);
    }
    result = ft_move(value);
    this->unlock_internal(state_lock_acquired);
    return (true);
}

template <typename ElementType>
bool ft_blocking_queue<ElementType>::wait_pop(ElementType &result, const std::atomic<bool> &running_flag)
{
    bool is_empty;
    ElementType value;
    bool state_lock_acquired;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (false);
    state_lock_acquired = false;
    int state_lock_error = this->lock_internal(&state_lock_acquired);
    if (state_lock_error != FT_ERR_SUCCESS)
        return (false);
    int mutex_error = this->_mutex.lock();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(state_lock_acquired);
        return (false);
    }
    while (true)
    {
        is_empty = this->_storage.empty();
        int storage_error = this->_storage.get_error();
        if (storage_error != FT_ERR_SUCCESS)
        {
            this->_mutex.unlock();
            this->unlock_internal(state_lock_acquired);
            return (false);
        }
        if (!is_empty)
            break ;
        if (!running_flag.load() || this->_shutdown)
        {
            this->_mutex.unlock();
            this->unlock_internal(state_lock_acquired);
            return (false);
        }
        this->unlock_internal(state_lock_acquired);
        state_lock_acquired = false;
        this->_waiting_workers.fetch_add(1U, std::memory_order_relaxed);
        const int wait_error = this->_condition.wait(this->_mutex);
        this->_waiting_workers.fetch_sub(1U, std::memory_order_relaxed);
        if (wait_error != 0)
        {
            this->_mutex.unlock();
            return (false);
        }
        if (this->_state_mutex != ft_nullptr)
        {
            if (this->_mutex.unlock() != FT_ERR_SUCCESS)
                return (false);
            if (this->lock_internal(&state_lock_acquired) != FT_ERR_SUCCESS)
                return (false);
            if (this->_mutex.lock() != FT_ERR_SUCCESS)
            {
                this->unlock_internal(state_lock_acquired);
                return (false);
            }
        }
    }
    value = this->_storage.dequeue();
    int dequeue_error = this->_storage.get_error();
    if (dequeue_error != FT_ERR_SUCCESS)
    {
        this->_mutex.unlock();
        this->unlock_internal(state_lock_acquired);
        return (false);
    }
    mutex_error = this->_mutex.unlock();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(state_lock_acquired);
        return (false);
    }
    result = ft_move(value);
    this->unlock_internal(state_lock_acquired);
    return (true);
}

template <typename ElementType>
void ft_blocking_queue<ElementType>::shutdown()
{
    bool state_lock_acquired;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return ;
    state_lock_acquired = false;
    int state_lock_error = this->lock_internal(&state_lock_acquired);
    if (state_lock_error != FT_ERR_SUCCESS)
        return ;
    int mutex_error = this->_mutex.lock();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(state_lock_acquired);
        return ;
    }
    this->_shutdown = true;
    if (this->_condition.broadcast() != 0)
    {
        mutex_error = this->_mutex.unlock();
        this->unlock_internal(state_lock_acquired);
        if (mutex_error != FT_ERR_SUCCESS)
            return ;
        return ;
    }
    mutex_error = this->_mutex.unlock();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        this->unlock_internal(state_lock_acquired);
        return ;
    }
    this->unlock_internal(state_lock_acquired);
    return ;
}

template <typename FunctionType, typename... Args>
int32_t ft_task_scheduler::submit(
    ft_future<typename std::invoke_result<FunctionType, Args...>::type> &future_value,
    FunctionType function, Args... args)
{
    this->abort_if_not_initialised("ft_task_scheduler::submit");
    using return_type = typename std::invoke_result<FunctionType, Args...>::type;
    using promise_type = ft_promise<return_type>;

    promise_type *promise_raw;
    ft_sharedptr<promise_type> promise_shared;
    std::shared_ptr<ft_sharedptr<promise_type> > promise_capture;
    int promise_shared_initialize_error;
    int future_initialize_error;

    promise_shared_initialize_error = promise_shared.initialize();
    if (promise_shared_initialize_error != FT_ERR_SUCCESS)
        return (promise_shared_initialize_error);
    promise_raw = new (std::nothrow) promise_type();
    if (!promise_raw)
        return (FT_ERR_NO_MEMORY);
    promise_shared.reset(promise_raw, 1, false);
    int promise_error = promise_shared.get_error();

    if (promise_error != FT_ERR_SUCCESS)
        return (promise_error);
    future_initialize_error = future_value.initialize(promise_shared);
    if (future_initialize_error != FT_ERR_SUCCESS)
        return (future_initialize_error);
    promise_capture.reset(new (std::nothrow) ft_sharedptr<promise_type>());
    if (!promise_capture)
        return (FT_ERR_NO_MEMORY);
    promise_shared_initialize_error = promise_capture->move(promise_shared);
    if (promise_shared_initialize_error != FT_ERR_SUCCESS)
        return (promise_shared_initialize_error);
    auto task_body = [promise_capture, function, args...]() mutable
    {
        if (!*promise_capture)
        {
            if constexpr (std::is_void_v<return_type>)
                function(args...);
            else
                (void)function(args...);
            return ;
        }
        if constexpr (std::is_void_v<return_type>)
        {
            function(args...);
            (*promise_capture)->set_value();
        }
        else
        {
            return_type result_value;

            result_value = function(args...);
            (*promise_capture)->set_value(ft_move(result_value));
        }
        return ;
    };
    ft_function<void()> wrapper(task_body);
    task_queue_entry queue_entry;
    unsigned long long parent_span;
    unsigned long long trace_id;
    bool metrics_updated;

    parent_span = task_scheduler_trace_current_span();
    trace_id = task_scheduler_trace_generate_span_id();
    queue_entry._function = ft_move(wrapper);
    queue_entry._trace_id = trace_id;
    queue_entry._parent_id = parent_span;
    queue_entry._label = g_ft_task_trace_label_async;
    this->trace_emit_event(FT_TASK_TRACE_PHASE_SUBMITTED, trace_id, parent_span,
            g_ft_task_trace_label_async, false);
    this->trace_emit_event(FT_TASK_TRACE_PHASE_ENQUEUED, trace_id, parent_span,
            g_ft_task_trace_label_async, false);
    int queue_push_error = this->_queue.push(ft_move(queue_entry));
    if (queue_push_error != FT_ERR_SUCCESS)
    {
        this->trace_emit_event(FT_TASK_TRACE_PHASE_CANCELLED, trace_id, parent_span,
                g_ft_task_trace_label_async, false);
        return (queue_push_error);
    }
    metrics_updated = this->update_queue_size(1);
    if (!metrics_updated)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

template <typename Rep, typename Period, typename FunctionType, typename... Args>
auto ft_task_scheduler::schedule_after(std::chrono::duration<Rep, Period> delay,
        FunctionType function, Args... args)
    -> Pair<ft_future<typename std::invoke_result<FunctionType, Args...>::type>,
            ft_scheduled_task_handle>
{
    this->abort_if_not_initialised("ft_task_scheduler::schedule_after");
    using return_type = typename std::invoke_result<FunctionType, Args...>::type;
    using promise_type = ft_promise<return_type>;

    Pair<ft_future<return_type>, ft_scheduled_task_handle> result_pair;
    promise_type *promise_raw;
    ft_sharedptr<promise_type> promise_shared;
    ft_scheduled_task_state *state_raw;
    ft_sharedptr<ft_scheduled_task_state> state_shared;
    int shared_initialize_error;
    int state_shared_initialize_error;
    int result_future_initialize_error;

    shared_initialize_error = promise_shared.initialize();
    if (shared_initialize_error != FT_ERR_SUCCESS)
        return (result_pair);
    promise_raw = new (std::nothrow) promise_type();
    if (!promise_raw)
    {
        return (result_pair);
    }
    promise_shared.reset(promise_raw, 1, false);
    int promise_error = promise_shared.get_error();

    if (promise_error != FT_ERR_SUCCESS)
    {
        return (result_pair);
    }
    result_future_initialize_error = result_pair.key.initialize(promise_shared);
    if (result_future_initialize_error != FT_ERR_SUCCESS)
    {
        return (result_pair);
    }
    {
        int future_error = promise_shared.get_error();
        if (future_error != FT_ERR_SUCCESS)
        {
            return (result_pair);
        }
    }
    state_raw = new (std::nothrow) ft_scheduled_task_state();
    if (!state_raw)
    {
        return (result_pair);
    }
    state_shared_initialize_error = state_shared.initialize();
    if (state_shared_initialize_error != FT_ERR_SUCCESS)
        return (result_pair);
    state_shared.reset(state_raw, 1, false);
    int state_error = state_shared.get_error();

    if (state_error != FT_ERR_SUCCESS)
    {
        return (result_pair);
    }
    destroy_at(&result_pair.value);
    construct_at(&result_pair.value, this, state_shared);
    if (!result_pair.value.valid())
    {
        return (result_pair);
    }
    scheduled_task task_entry;
    std::chrono::milliseconds delay_duration;
    long long delay_milliseconds;
    t_monotonic_time_point start_point;

    delay_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delay);
    delay_milliseconds = delay_duration.count();
    start_point = time_monotonic_point_now();
    task_entry._time = time_monotonic_point_add_ms(start_point, delay_milliseconds);
    task_entry._interval_ms = 0;
    task_entry._state = ft_move(state_shared);
    std::shared_ptr<ft_sharedptr<ft_promise<return_type> > > promise_capture(
        new (std::nothrow) ft_sharedptr<ft_promise<return_type> >());
    if (!promise_capture)
    {
        return (result_pair);
    }
    if (promise_capture->move(promise_shared) != FT_ERR_SUCCESS)
    {
        return (result_pair);
    }
    auto task_body = [promise_capture, function, args...]() mutable
    {
        if (!*promise_capture)
        {
            if constexpr (std::is_void_v<return_type>)
                function(args...);
            else
                (void)function(args...);
            return ;
        }
        if constexpr (std::is_void_v<return_type>)
        {
            function(args...);
            (*promise_capture)->set_value();
        }
        else
        {
            return_type result_value;

            result_value = function(args...);
            (*promise_capture)->set_value(ft_move(result_value));
        }
        return ;
    };
    task_entry._function = ft_function<void()>(task_body);
    if (!task_entry._function)
    {
        task_body();
        return (result_pair);
    }
    unsigned long long parent_span;
    unsigned long long trace_id;

    parent_span = task_scheduler_trace_current_span();
    trace_id = task_scheduler_trace_generate_span_id();
    task_entry._trace_id = trace_id;
    task_entry._parent_id = parent_span;
    task_entry._label = g_ft_task_trace_label_schedule_once;
    int scheduled_mutex_error = this->_scheduled_mutex.lock();

    if (scheduled_mutex_error != FT_ERR_SUCCESS)
    {
        return (result_pair);
    }
    bool push_success;

    push_success = this->scheduled_heap_push(ft_move(task_entry));
    if (!push_success)
    {
        scheduled_mutex_error = this->_scheduled_mutex.unlock();
        task_body();
        {
            ft_bool scheduled_lock_acquired;

            scheduled_lock_acquired = FT_FALSE;
            if (this->_scheduled.lock(&scheduled_lock_acquired) == FT_ERR_SUCCESS)
                this->_scheduled.unlock(scheduled_lock_acquired);
        }
        this->trace_emit_event(FT_TASK_TRACE_PHASE_CANCELLED, trace_id, parent_span,
                g_ft_task_trace_label_schedule_once, false);
        return (result_pair);
    }
    scheduled_mutex_error = this->_scheduled_mutex.unlock();
    if (scheduled_mutex_error != FT_ERR_SUCCESS)
    {
        return (result_pair);
    }
    if (this->_scheduled_condition.signal() != 0)
        return (result_pair);
    this->trace_emit_event(FT_TASK_TRACE_PHASE_TIMER_REGISTERED, trace_id, parent_span,
            g_ft_task_trace_label_schedule_once, false);
    return (result_pair);
}

template <typename Rep, typename Period, typename FunctionType, typename... Args>
ft_scheduled_task_handle ft_task_scheduler::schedule_every(std::chrono::duration<Rep, Period> interval,
        FunctionType function, Args... args)
{
    this->abort_if_not_initialised("ft_task_scheduler::schedule_every");
    ft_scheduled_task_handle handle_result;
    scheduled_task task_entry;
    std::chrono::milliseconds interval_duration;
    long long interval_milliseconds;
    t_monotonic_time_point start_point;
    ft_scheduled_task_state *state_raw;
    ft_sharedptr<ft_scheduled_task_state> state_shared;
    int state_shared_initialize_error;

    interval_duration = std::chrono::duration_cast<std::chrono::milliseconds>(interval);
    interval_milliseconds = interval_duration.count();
    start_point = time_monotonic_point_now();
    task_entry._time = time_monotonic_point_add_ms(start_point, interval_milliseconds);
    task_entry._interval_ms = interval_milliseconds;
    state_raw = new (std::nothrow) ft_scheduled_task_state();
    if (!state_raw)
    {
        return (handle_result);
    }
    state_shared_initialize_error = state_shared.initialize();
    if (state_shared_initialize_error != FT_ERR_SUCCESS)
        return (handle_result);
    state_shared.reset(state_raw, 1, false);
    int state_error = state_shared.get_error();

    if (state_error != FT_ERR_SUCCESS)
    {
        return (handle_result);
    }
    handle_result = ft_scheduled_task_handle(this, state_shared);
    if (!handle_result.valid())
    {
        return (handle_result);
    }
    task_entry._state = ft_move(state_shared);
    task_entry._function = ft_function<void()>([function, args...]()
    {
        function(args...);
        return ;
    });
    if (!task_entry._function)
    {
        return (handle_result);
    }
    unsigned long long parent_span;
    unsigned long long trace_id;

    parent_span = task_scheduler_trace_current_span();
    trace_id = task_scheduler_trace_generate_span_id();
    task_entry._trace_id = trace_id;
    task_entry._parent_id = parent_span;
    task_entry._label = g_ft_task_trace_label_schedule_repeat;
    int scheduled_mutex_error = this->_scheduled_mutex.lock();

    if (scheduled_mutex_error != FT_ERR_SUCCESS)
    {
        return (handle_result);
    }
    bool push_success;

    push_success = this->scheduled_heap_push(ft_move(task_entry));
    if (!push_success)
    {
        scheduled_mutex_error = this->_scheduled_mutex.unlock();
        this->trace_emit_event(FT_TASK_TRACE_PHASE_CANCELLED, trace_id, parent_span,
                g_ft_task_trace_label_schedule_repeat, false);
        {
            ft_bool scheduled_lock_acquired;

            scheduled_lock_acquired = FT_FALSE;
            if (this->_scheduled.lock(&scheduled_lock_acquired) == FT_ERR_SUCCESS)
                this->_scheduled.unlock(scheduled_lock_acquired);
        }
        return (handle_result);
    }
    scheduled_mutex_error = this->_scheduled_mutex.unlock();
    if (scheduled_mutex_error != FT_ERR_SUCCESS)
    {
        return (handle_result);
    }
    if (this->_scheduled_condition.signal() != 0)
        return (handle_result);
    this->trace_emit_event(FT_TASK_TRACE_PHASE_TIMER_REGISTERED, trace_id, parent_span,
            g_ft_task_trace_label_schedule_repeat, false);
    return (handle_result);
}

#endif
