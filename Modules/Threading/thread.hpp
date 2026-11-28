#ifndef THREADING_THREAD_HPP
#define THREADING_THREAD_HPP

#include "../Basic/class_nullptr.hpp"
#include "../PThread/pthread.hpp"
#include <memory>
#include <new>
#include <utility>
#include "../Template/function.hpp"
#include "../Template/invoke.hpp"
#include "../Template/move.hpp"
#include "../Errno/errno.hpp"
#include "../CMA/cma_internal.hpp"

class pt_mutex;

class ft_thread
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        struct start_payload
        {
            ft_function<void()> function;

            start_payload();
            ~start_payload();
        };

        pthread_t _thread;
        bool _joinable;
        std::shared_ptr<start_payload> _start_payload;
        mutable pt_mutex *_state_mutex;
        bool _thread_safe_enabled;

        static void *start_routine(void *data);
        int lock_internal(bool *lock_acquired) const;
        void unlock_internal(bool lock_acquired) const;
        void teardown_thread_safety();
        int detach_locked();

    public:
        ft_thread();
        template <typename FunctionType, typename... Args>
        ft_thread(FunctionType function, Args... args);
        ~ft_thread();

        ft_thread(const ft_thread &) = delete;
        ft_thread &operator=(const ft_thread &) = delete;
        ft_thread(ft_thread &&other);
        ft_thread &operator=(ft_thread &&other);

        bool joinable() const;
        void join();
        void detach();

        int enable_thread_safety();
        int disable_thread_safety();
        bool is_thread_safe() const;
        int lock(bool *lock_acquired) const;
        void unlock(bool lock_acquired) const;
};

template <typename FunctionType, typename... Args>
ft_thread::ft_thread(FunctionType function, Args... args)
    : _thread(), _joinable(false), _start_payload(),
      _state_mutex(ft_nullptr), _thread_safe_enabled(false)
{
    start_payload *payload_raw;
    std::shared_ptr<start_payload> payload;
    std::shared_ptr<start_payload> *shared_capsule;

    payload_raw = new (std::nothrow) start_payload();
    if (!payload_raw)
        return ;
    payload = std::shared_ptr<start_payload>(payload_raw);
    this->_start_payload = payload;
    payload->function = ft_function<void()>([function, args...]() mutable
    {
        ft_invoke(function, args...);
        return ;
    });
    if (!payload->function)
    {
        this->_start_payload.reset();
        return ;
    }
    shared_capsule = new (std::nothrow) std::shared_ptr<start_payload>(ft_move(payload));
    if (!shared_capsule)
    {
        this->_start_payload.reset();
        return ;
    }
    int create_result;

    create_result = pt_thread_create(&this->_thread, ft_nullptr,
            &ft_thread::start_routine, shared_capsule);
    if (create_result != 0)
    {
        delete shared_capsule;
        this->_start_payload.reset();
        return ;
    }
    this->_joinable = true;
    return ;
}

#endif
