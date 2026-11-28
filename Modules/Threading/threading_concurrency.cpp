#include "concurrency.hpp"
#include "../Errno/errno.hpp"
#include <chrono>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

ft_once::ft_once() noexcept
    : _mutex(), _called(FT_FALSE), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_once::~ft_once() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_once::initialize() noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_called = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_once::destroy() noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    this->_called = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_once::call(const ft_function<int32_t()> &function) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);
    int32_t error_code;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_called == FT_TRUE)
        return (FT_ERR_SUCCESS);
    if (!function)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = function();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->_called = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t ft_once::call_void(const ft_function<void()> &function) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_called == FT_TRUE)
        return (FT_ERR_SUCCESS);
    if (!function)
        return (FT_ERR_INVALID_ARGUMENT);
    function();
    this->_called = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

ft_bool ft_once::has_run() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    return (this->_called);
}

ft_countdown_latch::ft_countdown_latch() noexcept
    : _mutex(), _condition(), _count(0), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_countdown_latch::~ft_countdown_latch() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_countdown_latch::initialize(uint32_t count) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_count = count;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    if (this->_count == 0)
        this->_condition.notify_all();
    return (FT_ERR_SUCCESS);
}

int32_t ft_countdown_latch::destroy() noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    this->_count = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->_condition.notify_all();
    return (FT_ERR_SUCCESS);
}

int32_t ft_countdown_latch::count_down() noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_count == 0)
        return (FT_ERR_SUCCESS);
    --this->_count;
    if (this->_count == 0)
        this->_condition.notify_all();
    return (FT_ERR_SUCCESS);
}

int32_t ft_countdown_latch::wait() noexcept
{
    std::unique_lock<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    while (this->_count != 0 && this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->_condition.wait(lock);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_TERMINATED);
    return (FT_ERR_SUCCESS);
}

int32_t ft_countdown_latch::wait_for(uint32_t timeout_milliseconds) noexcept
{
    std::unique_lock<std::mutex> lock(this->_mutex);
    std::chrono::milliseconds timeout(timeout_milliseconds);

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_count != 0U && this->_condition.wait_for(lock, timeout, [this]()
            { return (this->_count == 0
                    || this->_initialised_state != FT_CLASS_STATE_INITIALISED); }) == false)
        return (FT_ERR_TIMEOUT);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_TERMINATED);
    return (FT_ERR_SUCCESS);
}

int32_t ft_countdown_latch::wait(const ft_cancellation_token &token) noexcept
{
    if (token.is_valid() == FT_FALSE)
        return (this->wait());
    std::unique_lock<std::mutex> lock(this->_mutex);

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    while (this->_count != 0 && this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        if (token.is_cancellation_requested() == FT_TRUE)
            return (FT_ERR_TERMINATED);
        this->_condition.wait_for(lock, std::chrono::milliseconds(10));
    }
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_TERMINATED);
    return (FT_ERR_SUCCESS);
}

uint32_t ft_countdown_latch::get_count() const noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    return (this->_count);
}

ft_barrier::ft_barrier() noexcept
    : _mutex(), _condition(), _participant_count(0), _waiting_count(0),
    _generation(0), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_barrier::~ft_barrier() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_barrier::initialize(uint32_t participant_count) noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (participant_count == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_participant_count = participant_count;
    this->_waiting_count = 0;
    this->_generation = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_barrier::destroy() noexcept
{
    std::lock_guard<std::mutex> lock(this->_mutex);

    this->_participant_count = 0;
    this->_waiting_count = 0;
    ++this->_generation;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->_condition.notify_all();
    return (FT_ERR_SUCCESS);
}

int32_t ft_barrier::arrive_and_wait() noexcept
{
    std::unique_lock<std::mutex> lock(this->_mutex);
    uint32_t generation;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    generation = this->_generation;
    ++this->_waiting_count;
    if (this->_waiting_count == this->_participant_count)
    {
        this->_waiting_count = 0;
        ++this->_generation;
        this->_condition.notify_all();
        return (FT_ERR_SUCCESS);
    }
    while (generation == this->_generation
        && this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->_condition.wait(lock);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_TERMINATED);
    return (FT_ERR_SUCCESS);
}

int32_t ft_barrier::arrive_and_wait(const ft_cancellation_token &token) noexcept
{
    if (token.is_valid() == FT_FALSE)
        return (this->arrive_and_wait());
    std::unique_lock<std::mutex> lock(this->_mutex);
    uint32_t generation;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    generation = this->_generation;
    ++this->_waiting_count;
    if (this->_waiting_count == this->_participant_count)
    {
        this->_waiting_count = 0;
        ++this->_generation;
        this->_condition.notify_all();
        return (FT_ERR_SUCCESS);
    }
    while (generation == this->_generation
        && this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        if (token.is_cancellation_requested() == FT_TRUE)
        {
            --this->_waiting_count;
            this->_condition.notify_all();
            return (FT_ERR_TERMINATED);
        }
        this->_condition.wait_for(lock, std::chrono::milliseconds(10));
    }
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_TERMINATED);
    return (FT_ERR_SUCCESS);
}
