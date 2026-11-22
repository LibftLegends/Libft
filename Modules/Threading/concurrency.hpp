#ifndef THREADING_CONCURRENCY_HPP
# define THREADING_CONCURRENCY_HPP

#include "../PThread/pthread.hpp"
#include "thread.hpp"
#include "cancellation.hpp"
#include "../Template/function.hpp"
#include "../Template/promise.hpp"
#include "../Template/move.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>

class ft_once
{
    private:
        mutable std::mutex _mutex;
        ft_bool _called;
        uint8_t _initialised_state;

    public:
        ft_once() noexcept;
        ft_once(const ft_once &other) noexcept = delete;
        ft_once(ft_once &&other) noexcept = delete;
        ~ft_once() noexcept;

        ft_once &operator=(const ft_once &other) noexcept = delete;
        ft_once &operator=(ft_once &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t call(const ft_function<int32_t()> &function) noexcept;
        int32_t call_void(const ft_function<void()> &function) noexcept;
        ft_bool has_run() const noexcept;
};

class ft_countdown_latch
{
    private:
        mutable std::mutex _mutex;
        std::condition_variable _condition;
        uint32_t _count;
        uint8_t _initialised_state;

    public:
        ft_countdown_latch() noexcept;
        ft_countdown_latch(const ft_countdown_latch &other) noexcept = delete;
        ft_countdown_latch(ft_countdown_latch &&other) noexcept = delete;
        ~ft_countdown_latch() noexcept;

        ft_countdown_latch &operator=(const ft_countdown_latch &other) noexcept = delete;
        ft_countdown_latch &operator=(ft_countdown_latch &&other) noexcept = delete;

        int32_t initialize(uint32_t count) noexcept;
        int32_t destroy() noexcept;
        int32_t count_down() noexcept;
        int32_t wait() noexcept;
        int32_t wait_for(uint32_t timeout_milliseconds) noexcept;
        int32_t wait(const ft_cancellation_token &token) noexcept;
        uint32_t get_count() const noexcept;
};

class ft_barrier
{
    private:
        mutable std::mutex _mutex;
        std::condition_variable _condition;
        uint32_t _participant_count;
        uint32_t _waiting_count;
        uint32_t _generation;
        uint8_t _initialised_state;

    public:
        ft_barrier() noexcept;
        ft_barrier(const ft_barrier &other) noexcept = delete;
        ft_barrier(ft_barrier &&other) noexcept = delete;
        ~ft_barrier() noexcept;

        ft_barrier &operator=(const ft_barrier &other) noexcept = delete;
        ft_barrier &operator=(ft_barrier &&other) noexcept = delete;

        int32_t initialize(uint32_t participant_count) noexcept;
        int32_t destroy() noexcept;
        int32_t arrive_and_wait() noexcept;
        int32_t arrive_and_wait(const ft_cancellation_token &token) noexcept;
};

template <typename Type>
class ft_thread_safe_queue
{
    private:
        mutable std::mutex _mutex;
        std::condition_variable _condition;
        std::deque<Type> _queue;
        uint8_t _initialised_state;

    public:
        ft_thread_safe_queue() noexcept;
        ft_thread_safe_queue(const ft_thread_safe_queue<Type> &other) noexcept = delete;
        ft_thread_safe_queue(ft_thread_safe_queue<Type> &&other) noexcept = delete;
        ~ft_thread_safe_queue() noexcept;

        ft_thread_safe_queue<Type> &operator=(const ft_thread_safe_queue<Type> &other) noexcept = delete;
        ft_thread_safe_queue<Type> &operator=(ft_thread_safe_queue<Type> &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t push(const Type &value) noexcept;
        int32_t push(Type &&value) noexcept;
        int32_t try_pop(Type *output) noexcept;
        int32_t blocking_pop(Type *output) noexcept;
        int32_t blocking_pop_for(Type *output, uint32_t timeout_milliseconds) noexcept;
        int32_t blocking_pop(Type *output, const ft_cancellation_token &token) noexcept;
        ft_bool empty() const noexcept;
        ft_size_t size() const noexcept;
};

template <typename ValueType, typename Function>
int pt_async(ft_promise<ValueType> &promise, const ft_cancellation_token &token,
    Function function)
{
    if (token.is_cancellation_requested() == FT_TRUE)
    {
        promise.set_value(ValueType());
        return (FT_ERR_TERMINATED);
    }
    return (pt_async(promise, [function, token]() mutable
    {
        if (token.is_cancellation_requested() == FT_TRUE)
            return (ValueType());
        return (function());
    }));
}

template <typename Function>
int pt_async(ft_promise<void> &promise, const ft_cancellation_token &token,
    Function function)
{
    if (token.is_cancellation_requested() == FT_TRUE)
    {
        promise.set_value();
        return (FT_ERR_TERMINATED);
    }
    return (pt_async(promise, [function, token]() mutable
    {
        if (token.is_cancellation_requested() == FT_FALSE)
            function();
        return ;
    }));
}

template <typename Type>
ft_thread_safe_queue<Type>::ft_thread_safe_queue() noexcept
    : _mutex(), _condition(), _queue(), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

template <typename Type>
ft_thread_safe_queue<Type>::~ft_thread_safe_queue() noexcept
{
    (void)this->destroy();
    return ;
}

template <typename Type>
int32_t ft_thread_safe_queue<Type>::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_queue.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

template <typename Type>
int32_t ft_thread_safe_queue<Type>::destroy() noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    this->_queue.clear();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->_condition.notify_all();
    return (FT_ERR_SUCCESS);
}

template <typename Type>
int32_t ft_thread_safe_queue<Type>::push(const Type &value) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    try
    {
        this->_queue.push_back(value);
    }
    catch (...)
    {
        return (FT_ERR_NO_MEMORY);
    }
    this->_condition.notify_one();
    return (FT_ERR_SUCCESS);
}

template <typename Type>
int32_t ft_thread_safe_queue<Type>::push(Type &&value) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    try
    {
        this->_queue.push_back(ft_move(value));
    }
    catch (...)
    {
        return (FT_ERR_NO_MEMORY);
    }
    this->_condition.notify_one();
    return (FT_ERR_SUCCESS);
}

template <typename Type>
int32_t ft_thread_safe_queue<Type>::try_pop(Type *output) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (output == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_queue.empty())
        return (FT_ERR_EMPTY);
    *output = ft_move(this->_queue.front());
    this->_queue.pop_front();
    return (FT_ERR_SUCCESS);
}

template <typename Type>
int32_t ft_thread_safe_queue<Type>::blocking_pop(Type *output) noexcept
{
    std::unique_lock<std::mutex> lock(this->_mutex);

    if (output == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    while (this->_queue.empty() && this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->_condition.wait(lock);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_TERMINATED);
    *output = ft_move(this->_queue.front());
    this->_queue.pop_front();
    return (FT_ERR_SUCCESS);
}

template <typename Type>
int32_t ft_thread_safe_queue<Type>::blocking_pop_for(Type *output,
    uint32_t timeout_milliseconds) noexcept
{
    std::unique_lock<std::mutex> lock(this->_mutex);
    std::chrono::milliseconds timeout(timeout_milliseconds);

    if (output == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_queue.empty() && this->_condition.wait_for(lock, timeout, [this]()
            { return (!this->_queue.empty()
                    || this->_initialised_state != FT_CLASS_STATE_INITIALISED); }) == false)
        return (FT_ERR_TIMEOUT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_TERMINATED);
    *output = ft_move(this->_queue.front());
    this->_queue.pop_front();
    return (FT_ERR_SUCCESS);
}

template <typename Type>
int32_t ft_thread_safe_queue<Type>::blocking_pop(Type *output,
    const ft_cancellation_token &token) noexcept
{
    if (token.is_valid() == FT_FALSE)
        return (this->blocking_pop(output));
    std::unique_lock<std::mutex> lock(this->_mutex);

    if (output == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    while (this->_queue.empty() && this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        if (token.is_cancellation_requested() == FT_TRUE)
            return (FT_ERR_TERMINATED);
        this->_condition.wait_for(lock, std::chrono::milliseconds(10));
    }
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_TERMINATED);
    *output = ft_move(this->_queue.front());
    this->_queue.pop_front();
    return (FT_ERR_SUCCESS);
}

template <typename Type>
ft_bool ft_thread_safe_queue<Type>::empty() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_queue.empty())
        return (FT_TRUE);
    return (FT_FALSE);
}

template <typename Type>
ft_size_t ft_thread_safe_queue<Type>::size() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    return (static_cast<ft_size_t>(this->_queue.size()));
}

#endif
