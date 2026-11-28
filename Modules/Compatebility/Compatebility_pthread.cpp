#include "compatebility_internal.hpp"
#include <cerrno>
#include <atomic>
#include <cstdlib>
#include "../PThread/pthread.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <chrono>


static int32_t cmp_thread_map_windows_error(DWORD error_code)
{
    if (error_code == 0)
        return (FT_ERR_INTERNAL);
    return (cmp_map_system_error_to_ft(static_cast<int32_t>(error_code)));
}

int32_t cmp_thread_equal(pthread_t thread1, pthread_t thread2)
{
    return (thread1 == thread2);
}

int32_t cmp_thread_cancel(pthread_t thread)
{
    if (TerminateThread(reinterpret_cast<HANDLE>(thread), 0) == 0)
    {
        return (cmp_thread_map_windows_error(GetLastError()));
    }
    return (FT_ERR_SUCCESS);
}

int32_t cmp_thread_yield()
{
    SwitchToThread();
    return (FT_ERR_SUCCESS);
}

int32_t cmp_thread_sleep(uint32_t milliseconds)
{
    Sleep(milliseconds);
    return (FT_ERR_SUCCESS);
}

using cmp_windows_wait_function = BOOL (WINAPI *)(volatile VOID *, PVOID, SIZE_T, DWORD);
using cmp_windows_wake_function = VOID (WINAPI *)(PVOID);

struct cmp_windows_address_wait_api
{
    cmp_windows_wait_function wait;
    cmp_windows_wake_function wake_one;
    cmp_windows_wake_function wake_all;
};

static const cmp_windows_address_wait_api &cmp_get_address_wait_api()
{
    static const cmp_windows_address_wait_api api = []()
    {
        // Resolve at run time so older Windows versions can still use polling.
        const HMODULE module = LoadLibraryExW(L"api-ms-win-core-synch-l1-2-0.dll",
                ft_nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        cmp_windows_address_wait_api functions{};
        if (module == ft_nullptr)
            return (functions);
        functions.wait = reinterpret_cast<cmp_windows_wait_function>(
                reinterpret_cast<void *>(GetProcAddress(module, "WaitOnAddress")));
        functions.wake_one = reinterpret_cast<cmp_windows_wake_function>(
                reinterpret_cast<void *>(GetProcAddress(module, "WakeByAddressSingle")));
        functions.wake_all = reinterpret_cast<cmp_windows_wake_function>(
                reinterpret_cast<void *>(GetProcAddress(module, "WakeByAddressAll")));
        if (functions.wait == ft_nullptr || functions.wake_one == ft_nullptr
            || functions.wake_all == ft_nullptr)
        {
            FreeLibrary(module);
            return (cmp_windows_address_wait_api{});
        }
        return (functions);
    }();
    return (api);
}

static int32_t cmp_wait_for_uint32(std::atomic<uint32_t> *address,
    uint32_t expected_value, ft_bool timed, uint64_t timeout_ms)
{
    static_assert(std::atomic<uint32_t>::is_always_lock_free
        && sizeof(std::atomic<uint32_t>) == sizeof(uint32_t),
        "The Windows address wait requires a lock-free four-byte atomic");
    using clock = std::chrono::steady_clock;
    if (address == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (address->load() != expected_value)
        return (FT_ERR_SUCCESS);
    if (timed == FT_TRUE && timeout_ms == 0U)
        return (FT_ERR_TIMEOUT);
    const cmp_windows_address_wait_api &api = cmp_get_address_wait_api();
    clock::time_point started;
    if (timed == FT_TRUE)
        started = clock::now();
    while (address->load() == expected_value)
    {
        DWORD interval = INFINITE;
        bool final_wait = false;
        if (timed == FT_TRUE)
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    clock::now() - started).count();
            const uint64_t elapsed_ms = static_cast<uint64_t>(elapsed);
            if (elapsed_ms >= timeout_ms)
                return (FT_ERR_TIMEOUT);
            const uint64_t remaining = timeout_ms - elapsed_ms;
            final_wait = (remaining < INFINITE);
            interval = INFINITE - 1U;
            if (final_wait)
                interval = static_cast<DWORD>(remaining);
        }
        if (api.wait == ft_nullptr)
        {
            Sleep(1U);
            continue ;
        }
        if (!api.wait(address, &expected_value, sizeof(expected_value), interval))
        {
            const DWORD error = GetLastError();
            if (error != ERROR_TIMEOUT)
                return (cmp_thread_map_windows_error(error));
            if (final_wait)
            {
                if (address->load() == expected_value)
                    return (FT_ERR_TIMEOUT);
                return (FT_ERR_SUCCESS);
            }
        }
    }
    return (FT_ERR_SUCCESS);
}

int32_t cmp_thread_wait_uint32(std::atomic<uint32_t> *address, uint32_t expected_value)
{
    return (cmp_wait_for_uint32(address, expected_value, FT_FALSE, 0U));
}

int32_t cmp_thread_wake_one_uint32(std::atomic<uint32_t> *address)
{
    if (address == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const cmp_windows_address_wait_api &api = cmp_get_address_wait_api();
    if (api.wake_one != ft_nullptr)
        api.wake_one(address);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_thread_wake_all_uint32(std::atomic<uint32_t> *address)
{
    if (address == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const cmp_windows_address_wait_api &api = cmp_get_address_wait_api();
    if (api.wake_all != ft_nullptr)
        api.wake_all(address);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_thread_wait_uint32_timed(std::atomic<uint32_t> *address,
    uint32_t expected_value, uint64_t timeout_ms)
{
    return (cmp_wait_for_uint32(address, expected_value, FT_TRUE, timeout_ms));
}

#else
# include <sched.h>
# include <unistd.h>
# include <errno.h>
# if defined(__linux__)
#  include <linux/futex.h>
#  include <sys/syscall.h>
# else
#  include "../Basic/class_nullptr.hpp"

struct cmp_wait_entry
{
    std::atomic<uint32_t> *address;
    pthread_cond_t condition;
    uint32_t waiter_count;
    cmp_wait_entry *next;
};

static pthread_mutex_t g_wait_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static cmp_wait_entry *g_wait_list_head = ft_nullptr;

static int32_t pt_pthread_mutex_lock_with_error(pthread_mutex_t *mutex) noexcept
{
    int32_t lock_result;

    lock_result = pthread_mutex_lock(mutex);
    if (lock_result != 0)
        return (cmp_map_system_error_to_ft(lock_result));
    return (FT_ERR_SUCCESS);
}

static int32_t pt_pthread_mutex_unlock_with_error(pthread_mutex_t *mutex) noexcept
{
    int32_t unlock_result;

    unlock_result = pthread_mutex_unlock(mutex);
    if (unlock_result != 0)
        return (cmp_map_system_error_to_ft(unlock_result));
    return (FT_ERR_SUCCESS);
}

static cmp_wait_entry *cmp_wait_lookup_entry(std::atomic<uint32_t> *address) noexcept
{
    cmp_wait_entry *current_entry;

    current_entry = g_wait_list_head;
    while (current_entry != ft_nullptr)
    {
        if (current_entry->address == address)
            return (current_entry);
        current_entry = current_entry->next;
    }
    return (ft_nullptr);
}

static int32_t cmp_wait_create_entry(std::atomic<uint32_t> *address, cmp_wait_entry **entry_out) noexcept
{
    cmp_wait_entry *new_entry;
    int32_t init_result;

    if (!entry_out)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    new_entry = reinterpret_cast<cmp_wait_entry *>(std::malloc(
        sizeof(cmp_wait_entry)));
    if (!new_entry)
    {
        return (FT_ERR_NO_MEMORY);
    }
    new_entry->address = address;
    new_entry->waiter_count = 0;
    new_entry->next = g_wait_list_head;
    init_result = pthread_cond_init(&new_entry->condition, ft_nullptr);
    if (init_result != 0)
    {
        std::free(static_cast<void *>(new_entry));
        return (cmp_map_system_error_to_ft(init_result));
    }
    g_wait_list_head = new_entry;
    *entry_out = new_entry;
    return (FT_ERR_SUCCESS);
}

static int32_t cmp_wait_remove_entry(cmp_wait_entry *entry) noexcept
{
    cmp_wait_entry *current_entry;
    cmp_wait_entry *previous_entry;
    int32_t destroy_result;

    if (!entry)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    previous_entry = ft_nullptr;
    current_entry = g_wait_list_head;
    while (current_entry != ft_nullptr)
    {
        if (current_entry == entry)
        {
            destroy_result = pthread_cond_destroy(&entry->condition);
            if (destroy_result != 0)
            {
                return (cmp_map_system_error_to_ft(destroy_result));
            }
            if (previous_entry == ft_nullptr)
                g_wait_list_head = current_entry->next;
            else
                previous_entry->next = current_entry->next;
            std::free(static_cast<void *>(current_entry));
            return (FT_ERR_SUCCESS);
        }
        previous_entry = current_entry;
        current_entry = current_entry->next;
    }
    return (FT_ERR_SUCCESS);
}
# endif

int32_t cmp_thread_equal(pthread_t thread1, pthread_t thread2)
{
    return (pthread_equal(thread1, thread2));
}

int32_t cmp_thread_cancel(pthread_t thread)
{
    int32_t return_value;

    return_value = pthread_cancel(thread);
    if (return_value != 0)
    {
        return (cmp_map_system_error_to_ft(return_value));
    }
    return (FT_ERR_SUCCESS);
}

int32_t cmp_thread_yield()
{
    if (sched_yield() != 0)
    {
        return (cmp_map_system_error_to_ft(errno));
    }
    return (FT_ERR_SUCCESS);
}

int32_t cmp_thread_sleep(uint32_t milliseconds)
{
    if (usleep(milliseconds * 1000) == -1)
    {
        return (cmp_map_system_error_to_ft(errno));
    }
    return (FT_ERR_SUCCESS);
}

int32_t cmp_thread_wait_uint32(std::atomic<uint32_t> *address, uint32_t expected_value)
{
# if defined(__linux__)
    int64_t syscall_result;

    while (1)
    {
        syscall_result = syscall(SYS_futex, reinterpret_cast<uint32_t *>(address),
#  ifdef FUTEX_WAIT_PRIVATE
                FUTEX_WAIT_PRIVATE,
#  else
                FUTEX_WAIT,
#  endif
                expected_value, NULL, NULL, 0);
        if (syscall_result == 0)
        {
            return (FT_ERR_SUCCESS);
        }
        if (errno == EAGAIN)
        {
            return (FT_ERR_SUCCESS);
        }
        if (errno == EINTR)
            continue;
        return (cmp_map_system_error_to_ft(errno));
    }
# else
    int32_t lock_error;
    cmp_wait_entry *entry;
    int32_t create_error;
    int32_t wait_result;
    int32_t remove_error;
    int32_t unlock_error;
    uint32_t current_value;

    if (address == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    lock_error = pt_pthread_mutex_lock_with_error(&g_wait_list_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    entry = cmp_wait_lookup_entry(address);
    if (entry == ft_nullptr)
    {
        create_error = cmp_wait_create_entry(address, &entry);
        if (create_error != FT_ERR_SUCCESS)
        {
            unlock_error = pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
            if (unlock_error != FT_ERR_SUCCESS)
                return (unlock_error);
            return (create_error);
        }
    }
    entry->waiter_count += 1;
    while (1)
    {
        current_value = address->load();
        if (current_value != expected_value)
            break ;
        wait_result = pthread_cond_wait(&entry->condition, &g_wait_list_mutex);
        if (wait_result != 0)
        {
            if (entry->waiter_count > 0)
                entry->waiter_count -= 1;
            if (entry->waiter_count == 0)
            {
                remove_error = cmp_wait_remove_entry(entry);
                if (remove_error != FT_ERR_SUCCESS)
                {
                    unlock_error = pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
                    if (unlock_error != FT_ERR_SUCCESS)
                        return (unlock_error);
                    return (remove_error);
                }
            }
            unlock_error = pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
            if (unlock_error != FT_ERR_SUCCESS)
            {
                return (unlock_error);
            }
            return (cmp_map_system_error_to_ft(wait_result));
        }
    }
    if (entry->waiter_count > 0)
        entry->waiter_count -= 1;
    if (entry->waiter_count == 0)
    {
        remove_error = cmp_wait_remove_entry(entry);
        if (remove_error != FT_ERR_SUCCESS)
        {
            unlock_error = pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
            if (unlock_error != FT_ERR_SUCCESS)
                return (unlock_error);
            return (remove_error);
        }
    }
    unlock_error = pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
# endif
}

int32_t cmp_thread_wait_uint32_timed(std::atomic<uint32_t> *address,
    uint32_t expected_value, uint64_t timeout_ms)
{
# if defined(__linux__)
    int64_t syscall_result;
    struct timespec timeout;

    if (address == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    timeout.tv_sec = static_cast<time_t>(timeout_ms / 1000U);
    timeout.tv_nsec = static_cast<long>((timeout_ms % 1000U) * 1000000U);
    syscall_result = syscall(SYS_futex, reinterpret_cast<uint32_t *>(address),
#  ifdef FUTEX_WAIT_PRIVATE
            FUTEX_WAIT_PRIVATE,
#  else
            FUTEX_WAIT,
#  endif
            expected_value, &timeout, NULL, 0);
    if (syscall_result == 0 || errno == EAGAIN)
        return (FT_ERR_SUCCESS);
    if (errno == ETIMEDOUT)
        return (FT_ERR_TIMEOUT);
    if (errno == EINTR)
        return (FT_ERR_TIMEOUT);
    return (cmp_map_system_error_to_ft(errno));
# else
    int32_t lock_error;
    cmp_wait_entry *entry;
    int32_t create_error;
    int32_t wait_result;
    int32_t remove_error;
    int32_t unlock_error;
    uint32_t current_value;
    struct timespec deadline;

    if (address == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0)
        return (cmp_map_system_error_to_ft(errno));
    deadline.tv_sec += static_cast<time_t>(timeout_ms / 1000U);
    deadline.tv_nsec += static_cast<long>((timeout_ms % 1000U) * 1000000U);
    if (deadline.tv_nsec >= 1000000000L)
    {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }
    lock_error = pt_pthread_mutex_lock_with_error(&g_wait_list_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    entry = cmp_wait_lookup_entry(address);
    if (entry == ft_nullptr)
    {
        create_error = cmp_wait_create_entry(address, &entry);
        if (create_error != FT_ERR_SUCCESS)
        {
            (void)pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
            return (create_error);
        }
    }
    entry->waiter_count += 1;
    current_value = address->load();
    while (current_value == expected_value)
    {
        wait_result = pthread_cond_timedwait(&entry->condition,
            &g_wait_list_mutex, &deadline);
        if (wait_result != 0)
            break ;
        current_value = address->load();
    }
    if (entry->waiter_count > 0)
        entry->waiter_count -= 1;
    if (entry->waiter_count == 0)
    {
        remove_error = cmp_wait_remove_entry(entry);
        if (remove_error != FT_ERR_SUCCESS)
        {
            (void)pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
            return (remove_error);
        }
    }
    unlock_error = pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
    if (unlock_error != FT_ERR_SUCCESS)
        return (unlock_error);
    if (current_value != expected_value)
        return (FT_ERR_SUCCESS);
    if (wait_result == ETIMEDOUT)
        return (FT_ERR_TIMEOUT);
    if (wait_result != 0)
        return (cmp_map_system_error_to_ft(wait_result));
    return (FT_ERR_SUCCESS);
# endif
}

int32_t cmp_thread_wake_one_uint32(std::atomic<uint32_t> *address)
{
# if defined(__linux__)
    int64_t syscall_result;

    syscall_result = syscall(SYS_futex, reinterpret_cast<uint32_t *>(address),
#  ifdef FUTEX_WAKE_PRIVATE
            FUTEX_WAKE_PRIVATE,
#  else
            FUTEX_WAKE,
#  endif
            1, NULL, NULL, 0);
    if (syscall_result == -1)
    {
        return (cmp_map_system_error_to_ft(errno));
    }
    return (FT_ERR_SUCCESS);
# else
    int32_t lock_error;
    cmp_wait_entry *entry;
    int32_t signal_result;
    int32_t unlock_error;

    if (address == ft_nullptr)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    lock_error = pt_pthread_mutex_lock_with_error(&g_wait_list_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        return (lock_error);
    }
    entry = cmp_wait_lookup_entry(address);
    if (entry != ft_nullptr)
    {
        signal_result = pthread_cond_signal(&entry->condition);
        if (signal_result != 0)
        {
            unlock_error = pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
            if (unlock_error != FT_ERR_SUCCESS)
                return (unlock_error);
            return (cmp_map_system_error_to_ft(signal_result));
        }
    }
    unlock_error = pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
    if (unlock_error != FT_ERR_SUCCESS)
    {
        return (unlock_error);
    }
    return (FT_ERR_SUCCESS);
# endif
}

int32_t cmp_thread_wake_all_uint32(std::atomic<uint32_t> *address)
{
# if defined(__linux__)
    int64_t syscall_result;

    if (address == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    syscall_result = syscall(SYS_futex, reinterpret_cast<uint32_t *>(address),
#  ifdef FUTEX_WAKE_PRIVATE
            FUTEX_WAKE_PRIVATE,
#  else
            FUTEX_WAKE,
#  endif
            0x7FFFFFFF, NULL, NULL, 0);
    if (syscall_result == -1)
        return (cmp_map_system_error_to_ft(errno));
    return (FT_ERR_SUCCESS);
# else
    int32_t lock_error;
    cmp_wait_entry *entry;
    int32_t signal_result;
    int32_t unlock_error;

    if (address == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_error = pt_pthread_mutex_lock_with_error(&g_wait_list_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    entry = cmp_wait_lookup_entry(address);
    if (entry != ft_nullptr)
    {
        signal_result = pthread_cond_broadcast(&entry->condition);
        if (signal_result != 0)
        {
            (void)pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
            return (cmp_map_system_error_to_ft(signal_result));
        }
    }
    unlock_error = pt_pthread_mutex_unlock_with_error(&g_wait_list_mutex);
    if (unlock_error != FT_ERR_SUCCESS)
        return (unlock_error);
    return (FT_ERR_SUCCESS);
# endif
}
#endif
