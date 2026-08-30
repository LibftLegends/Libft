#include <pthread.h>
#include <errno.h>
#include "pthread.hpp"
#include "../Errno/errno.hpp"

int pt_thread_detach(pthread_t thread)
{
    int return_value;

    if (!thread)
        return (ESRCH);
    return_value = pthread_detach(thread);
    if (return_value != 0)
        return (return_value);
    return (return_value);
}
