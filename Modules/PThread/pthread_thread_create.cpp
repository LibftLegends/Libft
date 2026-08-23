#include <pthread.h>
#include "pthread.hpp"
#include <cerrno>
#include "../Errno/errno.hpp"
#include "pthread_lock_tracking.hpp"
#include <new>

thread_local pt_thread_id_type pt_thread_id = THREAD_ID;

struct s_pt_thread_start_payload
{
    void *(*start_routine)(void *);
    void *argument;
};

static void pt_thread_tracking_cleanup(void *argument)
{
    s_pt_thread_start_payload *payload;

    payload = static_cast<s_pt_thread_start_payload *>(argument);
    (void)pt_lock_tracking::notify_thread_exit(THREAD_ID);
    delete payload;
    return ;
}

static void *pt_thread_start_trampoline(void *argument)
{
    s_pt_thread_start_payload *payload;
    void *result;

    payload = static_cast<s_pt_thread_start_payload *>(argument);
    result = ft_nullptr;
    pthread_cleanup_push(pt_thread_tracking_cleanup, payload);
    (void)pt_lock_tracking::notify_thread_enter(THREAD_ID);
    if (payload != ft_nullptr && payload->start_routine != ft_nullptr)
        result = payload->start_routine(payload->argument);
    pthread_cleanup_pop(1);
    return (result);
}

int pt_thread_create(pthread_t *thread, const pthread_attr_t *attr,
                void *(*start_routine)(void *), void *arg)
{
    int return_value;
    void *(*null_start_routine)(void *);
    s_pt_thread_start_payload *payload;

    null_start_routine = ft_nullptr;
    if (thread == ft_nullptr || start_routine == null_start_routine)
        return (EINVAL);
    payload = new (std::nothrow) s_pt_thread_start_payload();
    if (payload == ft_nullptr)
        return (ENOMEM);
    payload->start_routine = start_routine;
    payload->argument = arg;
    return_value = pthread_create(thread, attr, pt_thread_start_trampoline,
        payload);
    if (return_value != 0)
    {
        delete payload;
        return (return_value);
    }
    return (return_value);
}
