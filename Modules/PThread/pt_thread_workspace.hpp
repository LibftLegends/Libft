#ifndef PT_THREAD_WORKSPACE_HPP
# define PT_THREAD_WORKSPACE_HPP

# include <atomic>
# include <new>
# include <pthread.h>
# include "../Errno/errno.hpp"

template <typename Workspace>
struct pt_thread_workspace
{
    static pthread_once_t initialization;
    static pthread_key_t key;
    static std::atomic<ft_bool> ready;

    static Workspace *get_existing() noexcept;
    static Workspace *get_or_create() noexcept;
    static void initialize_key() noexcept;
    static void destroy_workspace(void *workspace) noexcept;
};

template <typename Workspace>
pthread_once_t pt_thread_workspace<Workspace>::initialization = PTHREAD_ONCE_INIT;

template <typename Workspace>
pthread_key_t pt_thread_workspace<Workspace>::key;

template <typename Workspace>
std::atomic<ft_bool> pt_thread_workspace<Workspace>::ready(FT_FALSE);

template <typename Workspace>
void pt_thread_workspace<Workspace>::destroy_workspace(void *workspace) noexcept
{
    delete static_cast<Workspace *>(workspace);
    return ;
}

template <typename Workspace>
void pt_thread_workspace<Workspace>::initialize_key() noexcept
{
    if (pthread_key_create(&key, destroy_workspace) == 0)
        ready.store(FT_TRUE, std::memory_order_release);
    return ;
}

template <typename Workspace>
Workspace *pt_thread_workspace<Workspace>::get_existing() noexcept
{
    if (ready.load(std::memory_order_acquire) == FT_FALSE)
        return (nullptr);
    return (static_cast<Workspace *>(pthread_getspecific(key)));
}

template <typename Workspace>
Workspace *pt_thread_workspace<Workspace>::get_or_create() noexcept
{
    Workspace *workspace = get_existing();
    if (workspace != nullptr)
        return (workspace);
    if (pthread_once(&initialization, initialize_key) != 0
        || ready.load(std::memory_order_acquire) == FT_FALSE)
        return (nullptr);
    /* The exit callback owns this allocation independently of C++ TLS storage. */
    workspace = new (std::nothrow) Workspace();
    if (workspace != nullptr && pthread_setspecific(key, workspace) != 0)
    {
        delete workspace;
        return (nullptr);
    }
    return (workspace);
}

#endif
