#include "pthread_lock_tracking.hpp"
#include "../Errno/errno.hpp"
#include "../Time/time.hpp"
#include "mutex.hpp"
#include "recursive_mutex.hpp"
#include "pthread.hpp"
#include <pthread.h>
#include <atomic>
#include <cstdlib>
#include <new>

static thread_local bool g_registry_mutex_owned = false;
static std::atomic<uint64_t> g_pt_lock_tracking_next_generation(1);
static thread_local uint64_t g_pt_lock_tracking_generation = 0;
static pthread_key_t g_pt_lock_tracking_exit_key;
static std::atomic<ft_bool> g_pt_lock_tracking_exit_key_initialised(FT_FALSE);

static std::mutex *pt_lock_tracking_get_exit_key_mutex(void)
{
    static std::mutex *exit_key_mutex = []() -> std::mutex *
    {
        void *memory_pointer;

        memory_pointer = std::malloc(sizeof(std::mutex));
        if (memory_pointer == ft_nullptr)
            return (ft_nullptr);
        return (new (memory_pointer) std::mutex());
    }();

    return (exit_key_mutex);
}

static void pt_lock_tracking_thread_exit_key_cleanup(void *argument)
{
    if (argument == ft_nullptr)
        return ;
    (void)pthread_setspecific(g_pt_lock_tracking_exit_key, ft_nullptr);
    (void)pt_lock_tracking::notify_thread_exit(THREAD_ID);
    return ;
}

static void pt_lock_tracking_initialise_exit_key(void)
{
    std::mutex *exit_key_mutex;

    if (g_pt_lock_tracking_exit_key_initialised.load(std::memory_order_acquire)
        == FT_TRUE)
        return ;
    exit_key_mutex = pt_lock_tracking_get_exit_key_mutex();
    if (exit_key_mutex == ft_nullptr)
        return ;
    exit_key_mutex->lock();
    if (g_pt_lock_tracking_exit_key_initialised.load(std::memory_order_relaxed)
        == FT_FALSE
        && pthread_key_create(&g_pt_lock_tracking_exit_key,
            pt_lock_tracking_thread_exit_key_cleanup) == 0)
        g_pt_lock_tracking_exit_key_initialised.store(FT_TRUE,
            std::memory_order_release);
    exit_key_mutex->unlock();
    return ;
}

struct s_pt_lock_tracking_thread_exit_guard
{
    pt_thread_id_type thread_identifier;

    s_pt_lock_tracking_thread_exit_guard()
        : thread_identifier(THREAD_ID)
    {
        g_pt_lock_tracking_generation =
            g_pt_lock_tracking_next_generation.fetch_add(1,
                std::memory_order_relaxed);
        return ;
    }

    ~s_pt_lock_tracking_thread_exit_guard()
    {
        (void)pt_lock_tracking::notify_thread_exit(this->thread_identifier);
        return ;
    }
};

static thread_local s_pt_lock_tracking_thread_exit_guard
    g_pt_lock_tracking_thread_exit_guard;

static void pt_lock_tracking_arm_thread_exit_cleanup(void)
{
    pt_lock_tracking_initialise_exit_key();
    if (g_pt_lock_tracking_exit_key_initialised.load(std::memory_order_acquire)
        == FT_TRUE)
        (void)pthread_setspecific(g_pt_lock_tracking_exit_key,
            reinterpret_cast<void *>(static_cast<uintptr_t>(1U)));
    (void)g_pt_lock_tracking_thread_exit_guard;
    return ;
}

#ifdef LIBFT_TEST_BUILD
std::atomic<int> pt_lock_tracking_notify_acquired_override_error_code(
    FT_ERR_SUCCESS);
std::atomic<int> pt_lock_tracking_detect_cycle_override_error_code(
    FT_ERR_SUCCESS);
#endif

struct s_pt_mutex_owner_info
{
    const void      *mutex_pointer;
    pt_thread_id_type  owner_thread;
};

typedef pt_buffer<s_pt_mutex_owner_info> pt_mutex_owner_vector;

struct s_pt_owned_mutex_buffer_cache
{
    pt_mutex_vector buffer;

    s_pt_owned_mutex_buffer_cache()
    {
        pt_buffer_init(this->buffer);
        return ;
    }

    ~s_pt_owned_mutex_buffer_cache()
    {
        pt_buffer_destroy(this->buffer);
        return ;
    }
};

static s_pt_owned_mutex_buffer_cache *pt_lock_tracking_get_owned_mutex_buffer_cache(void)
{
    static s_pt_owned_mutex_buffer_cache *owned_mutex_buffer_cache =
        new (std::nothrow) s_pt_owned_mutex_buffer_cache();

    return (owned_mutex_buffer_cache);
}

int pt_lock_tracking::lock_registry_mutex(void)
{
    std::mutex *registry_mutex;

    registry_mutex = pt_lock_tracking::get_registry_mutex();
    if (registry_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    registry_mutex->lock();
    return (FT_ERR_SUCCESS);
}

int pt_lock_tracking::unlock_registry_mutex(void)
{
    std::mutex *registry_mutex;

    registry_mutex = pt_lock_tracking::get_registry_mutex();
    if (registry_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    registry_mutex->unlock();
    return (FT_ERR_SUCCESS);
}

static std::mutex *pt_lock_tracking_create_registry_mutex(void)
{
    void *memory_pointer;

    memory_pointer = std::malloc(sizeof(std::mutex));
    if (memory_pointer == ft_nullptr)
        return (ft_nullptr);
    return (new (memory_pointer) std::mutex());
}

std::mutex *pt_lock_tracking::get_registry_mutex(void)
{
    static std::mutex *registry_mutex =
        pt_lock_tracking_create_registry_mutex();

    return (registry_mutex);
}

bool pt_lock_tracking::ensure_registry_mutex_initialised(int *error_code)
{
    if (pt_lock_tracking::get_registry_mutex() == ft_nullptr)
    {
        if (error_code)
            *error_code = FT_ERR_NO_MEMORY;
        return (false);
    }
    if (error_code)
        *error_code = FT_ERR_SUCCESS;
    return (true);
}

pt_buffer<s_pt_thread_lock_info> *pt_lock_tracking::get_thread_infos(int *error_code)
{
    static pt_buffer<s_pt_thread_lock_info> *thread_infos_pointer = ft_nullptr;
    void *memory_pointer;

    if (thread_infos_pointer != ft_nullptr)
    {
        if (error_code)
            *error_code = FT_ERR_SUCCESS;
        return (thread_infos_pointer);
    }
    memory_pointer = std::malloc(sizeof(pt_buffer<s_pt_thread_lock_info>));
    if (memory_pointer == ft_nullptr)
    {
        if (error_code)
            *error_code = FT_ERR_INVALID_STATE;
        return (ft_nullptr);
    }
    thread_infos_pointer =
        static_cast<pt_buffer<s_pt_thread_lock_info> *>(memory_pointer);
    new (thread_infos_pointer) pt_buffer<s_pt_thread_lock_info>();
    pt_buffer_init(*thread_infos_pointer);
    if (error_code)
        *error_code = FT_ERR_SUCCESS;
    return (thread_infos_pointer);
}

static pt_mutex_owner_vector *pt_lock_tracking_get_mutex_owners(int *error_code)
{
    static pt_mutex_owner_vector *mutex_owners_pointer = ft_nullptr;
    void *memory_pointer;

    if (mutex_owners_pointer != ft_nullptr)
    {
        if (error_code)
            *error_code = FT_ERR_SUCCESS;
        return (mutex_owners_pointer);
    }
    memory_pointer = std::malloc(sizeof(pt_mutex_owner_vector));
    if (memory_pointer == ft_nullptr)
    {
        if (error_code)
            *error_code = FT_ERR_INVALID_STATE;
        return (ft_nullptr);
    }
    mutex_owners_pointer = static_cast<pt_mutex_owner_vector *>(memory_pointer);
    new (mutex_owners_pointer) pt_mutex_owner_vector();
    pt_buffer_init(*mutex_owners_pointer);
    if (error_code)
        *error_code = FT_ERR_SUCCESS;
    return (mutex_owners_pointer);
}

static s_pt_mutex_owner_info *pt_lock_tracking_lookup_mutex_owner(
    const void *mutex_pointer, int *error_code)
{
    pt_mutex_owner_vector *mutex_owners;
    ft_size_t index;

    mutex_owners = pt_lock_tracking_get_mutex_owners(error_code);
    if (mutex_owners == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < mutex_owners->size)
    {
        if (mutex_owners->data[index].mutex_pointer == mutex_pointer)
        {
            if (error_code)
                *error_code = FT_ERR_SUCCESS;
            return (&mutex_owners->data[index]);
        }
        index += 1;
    }
    if (error_code)
        *error_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

static int pt_lock_tracking_set_mutex_owner(const void *mutex_pointer,
    pt_thread_id_type owner_thread)
{
    pt_mutex_owner_vector *mutex_owners;
    s_pt_mutex_owner_info *owner_info;
    s_pt_mutex_owner_info new_owner_info;
    int error_code;
    int push_error;

    error_code = FT_ERR_SUCCESS;
    owner_info = pt_lock_tracking_lookup_mutex_owner(mutex_pointer, &error_code);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (owner_info != ft_nullptr)
    {
        owner_info->owner_thread = owner_thread;
        return (FT_ERR_SUCCESS);
    }
    mutex_owners = pt_lock_tracking_get_mutex_owners(&error_code);
    if (mutex_owners == ft_nullptr)
        return (error_code);
    new_owner_info.mutex_pointer = mutex_pointer;
    new_owner_info.owner_thread = owner_thread;
    push_error = pt_buffer_push(*mutex_owners, new_owner_info);
    if (push_error != FT_ERR_SUCCESS)
        return (push_error);
    return (FT_ERR_SUCCESS);
}

static void pt_lock_tracking_clear_mutex_owner(const void *mutex_pointer)
{
    pt_mutex_owner_vector *mutex_owners;
    ft_size_t index;
    int error_code;

    error_code = FT_ERR_SUCCESS;
    mutex_owners = pt_lock_tracking_get_mutex_owners(&error_code);
    if (mutex_owners == ft_nullptr)
        return ;
    index = 0;
    while (index < mutex_owners->size)
    {
        if (mutex_owners->data[index].mutex_pointer == mutex_pointer)
        {
            pt_buffer_erase(*mutex_owners, index);
            return ;
        }
        index += 1;
    }
    return ;
}

static void pt_lock_tracking_clear_mutex_owners_for_thread(
    pt_thread_id_type thread_identifier)
{
    pt_mutex_owner_vector *mutex_owners;
    ft_size_t index;
    int error_code;

    error_code = FT_ERR_SUCCESS;
    mutex_owners = pt_lock_tracking_get_mutex_owners(&error_code);
    if (mutex_owners == ft_nullptr)
        return ;
    index = 0;
    while (index < mutex_owners->size)
    {
        if (mutex_owners->data[index].owner_thread == thread_identifier)
        {
            pt_buffer_erase(*mutex_owners, index);
            continue ;
        }
        index += 1;
    }
    return ;
}

pt_lock_tracking::pt_lock_tracking()
{
    return ;
}

pt_lock_tracking::~pt_lock_tracking()
{
    return ;
}

pt_mutex_vector pt_lock_tracking::get_owned_mutexes
        (pt_thread_id_type thread_identifier, int *error_code_out)
{
    pt_mutex_vector owned_mutexes;
    s_pt_thread_lock_info *info;
    int lock_error;
    bool lock_acquired;
    int error_code;

    pt_buffer_init(owned_mutexes);
    error_code = FT_ERR_SUCCESS;
    if (!pt_lock_tracking::ensure_registry_mutex_initialised(&error_code))
    {
        if (error_code_out)
            *error_code_out = error_code;
        return (owned_mutexes);
    }
    lock_acquired = false;
    if (!g_registry_mutex_owned)
    {
        lock_error = pt_lock_tracking::lock_registry_mutex();
        if (lock_error != FT_ERR_SUCCESS)
        {
            if (error_code_out)
                *error_code_out = lock_error;
            return (owned_mutexes);
        }
        g_registry_mutex_owned = true;
        lock_acquired = true;
    }
    info = pt_lock_tracking::lookup_thread_info(thread_identifier, &error_code);
    if (info == ft_nullptr && error_code != FT_ERR_SUCCESS)
    {
        if (lock_acquired)
        {
            pt_lock_tracking::unlock_registry_mutex();
            g_registry_mutex_owned = false;
        }
        if (error_code_out)
            *error_code_out = error_code;
        return (owned_mutexes);
    }
    if (info == ft_nullptr)
    {
        if (lock_acquired)
        {
            pt_lock_tracking::unlock_registry_mutex();
            g_registry_mutex_owned = false;
        }
        if (error_code_out)
            *error_code_out = FT_ERR_SUCCESS;
        return (owned_mutexes);
    }
    pt_buffer_init(owned_mutexes);
    int copy_error = pt_buffer_copy(owned_mutexes, info->owned_mutexes);
    if (copy_error != FT_ERR_SUCCESS)
    {
        if (lock_acquired)
        {
            pt_lock_tracking::unlock_registry_mutex();
            g_registry_mutex_owned = false;
        }
        if (error_code_out)
            *error_code_out = copy_error;
        pt_buffer_destroy(owned_mutexes);
        return (owned_mutexes);
    }
    if (lock_acquired)
    {
        pt_lock_tracking::unlock_registry_mutex();
        g_registry_mutex_owned = false;
    }
    if (error_code_out)
        *error_code_out = FT_ERR_SUCCESS;
    return (owned_mutexes);
}

s_pt_thread_lock_info *pt_lock_tracking::find_thread_info
    (pt_thread_id_type thread_identifier, int *error_code)
{
    ft_size_t index;
    pt_buffer<s_pt_thread_lock_info> *thread_infos;
    s_pt_owned_mutex_buffer_cache *owned_mutex_buffer_cache;
    s_pt_thread_lock_info *info;
    s_pt_thread_lock_info new_info;

    thread_infos = pt_lock_tracking::get_thread_infos(error_code);
    if (thread_infos == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < thread_infos->size)
    {
        info = &thread_infos->data[index];
        if (info->thread_identifier == thread_identifier)
        {
            if (info->generation == g_pt_lock_tracking_generation)
                return (info);
            pt_lock_tracking_clear_mutex_owners_for_thread(
                info->thread_identifier);
            pt_buffer_destroy(info->owned_mutexes);
            pt_buffer_erase(*thread_infos, index);
            continue ;
        }
        index += 1;
    }
    owned_mutex_buffer_cache =
        pt_lock_tracking_get_owned_mutex_buffer_cache();
    if (owned_mutex_buffer_cache == ft_nullptr)
    {
        if (error_code)
            *error_code = FT_ERR_NO_MEMORY;
        return (ft_nullptr);
    }
    new_info.thread_identifier = thread_identifier;
    new_info.generation = g_pt_lock_tracking_generation;
    new_info.owned_mutexes = owned_mutex_buffer_cache->buffer;
    pt_buffer_clear(new_info.owned_mutexes);
    pt_buffer_init(owned_mutex_buffer_cache->buffer);
    new_info.waiting_mutex = ft_nullptr;
    new_info.wait_started_ms = 0;
    int push_error = pt_buffer_push(*thread_infos, new_info);
    if (push_error != FT_ERR_SUCCESS)
    {
        owned_mutex_buffer_cache->buffer = new_info.owned_mutexes;
        if (error_code)
            *error_code = push_error;
        return (ft_nullptr);
    }
    if (error_code)
        *error_code = FT_ERR_SUCCESS;
    return (&thread_infos->data[thread_infos->size - 1]);
}

s_pt_thread_lock_info *pt_lock_tracking::lookup_thread_info(pt_thread_id_type
        thread_identifier, int *error_code)
{
    ft_size_t index;
    pt_buffer<s_pt_thread_lock_info> *thread_infos;
    s_pt_thread_lock_info *info;

    thread_infos = pt_lock_tracking::get_thread_infos(error_code);
    if (thread_infos == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < thread_infos->size)
    {
        info = &thread_infos->data[index];
        if (info->thread_identifier == thread_identifier)
            return (info);
        index += 1;
    }
    if (error_code)
        *error_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

bool pt_lock_tracking::vector_contains_mutex(const pt_mutex_vector &mutexes,
        const void *mutex_pointer)
{
    ft_size_t index;

    index = 0;
    while (index < mutexes.size)
    {
        if (mutexes.data[index] == mutex_pointer)
            return (true);
        index += 1;
    }
    return (false);
}

bool pt_lock_tracking::vector_contains_thread(const pt_thread_vector
        &thread_identifiers, pt_thread_id_type thread_identifier)
{
    ft_size_t index;

    index = 0;
    while (index < thread_identifiers.size)
    {
        if (thread_identifiers.data[index] == thread_identifier)
            return (true);
        index += 1;
    }
    return (false);
}

int pt_lock_tracking::detect_cycle(const s_pt_thread_lock_info *origin,
        const void *requested_mutex, pt_mutex_vector *visited_mutexes,
        pt_thread_vector *visited_threads, bool *cycle_detected)
{
    pt_buffer<s_pt_thread_lock_info> *thread_infos;
    s_pt_mutex_owner_info *owner_info;
    s_pt_thread_lock_info *info;
    int error_code;

    *cycle_detected = false;
    if (pt_lock_tracking::vector_contains_mutex(*visited_mutexes, requested_mutex))
        return (FT_ERR_SUCCESS);
#ifdef LIBFT_TEST_BUILD
    error_code = pt_lock_tracking_detect_cycle_override_error_code.load();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
#endif
    if (pt_buffer_push(*visited_mutexes, requested_mutex) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    error_code = FT_ERR_SUCCESS;
    owner_info = pt_lock_tracking_lookup_mutex_owner(requested_mutex, &error_code);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (owner_info == ft_nullptr)
        return (FT_ERR_SUCCESS);
    if (owner_info->owner_thread == origin->thread_identifier)
        return (FT_ERR_SUCCESS);
    thread_infos = pt_lock_tracking::get_thread_infos(&error_code);
    if (thread_infos == ft_nullptr)
        return (error_code);
    info = pt_lock_tracking::lookup_thread_info(owner_info->owner_thread, &error_code);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (info == ft_nullptr)
        return (FT_ERR_SUCCESS);
    if (info->waiting_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    if (pt_lock_tracking::vector_contains_mutex(origin->owned_mutexes,
                info->waiting_mutex))
    {
        *cycle_detected = true;
        return (FT_ERR_SUCCESS);
    }
    if (!pt_lock_tracking::vector_contains_thread(*visited_threads,
                info->thread_identifier))
    {
#ifdef LIBFT_TEST_BUILD
        error_code = pt_lock_tracking_detect_cycle_override_error_code.load();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
#endif
        if (pt_buffer_push(*visited_threads, info->thread_identifier)
                != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
        error_code = pt_lock_tracking::detect_cycle(origin, info->waiting_mutex,
                visited_mutexes, visited_threads, cycle_detected);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

int pt_lock_tracking::notify_wait(pt_thread_id_type thread_identifier,
        const void *requested_mutex)
{
    int lock_error;
    s_pt_thread_lock_info *info;
    bool lock_acquired = false;
    int error_code = FT_ERR_SUCCESS;
    int result_code = FT_ERR_SUCCESS;

    pt_lock_tracking_arm_thread_exit_cleanup();
    if (!pt_lock_tracking::ensure_registry_mutex_initialised(&error_code))
        return (error_code);
    if (!g_registry_mutex_owned)
    {
        lock_error = pt_lock_tracking::lock_registry_mutex();
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        g_registry_mutex_owned = true;
        lock_acquired = true;
    }
    info = pt_lock_tracking::find_thread_info(thread_identifier, &error_code);
    if (info == ft_nullptr)
    {
        if (!lock_acquired && error_code == FT_ERR_SUCCESS)
            result_code = FT_ERR_INVALID_STATE;
        else
            result_code = error_code;
    }
    else
    {
        bool cycle_detected;
        int detect_error;

        if (info->waiting_mutex != requested_mutex)
            info->wait_started_ms = time_now_ms();
        if (info->wait_started_ms == 0)
            info->wait_started_ms = time_now_ms();
        info->waiting_mutex = requested_mutex;
        pt_mutex_vector visited_mutexes_inner;
        pt_thread_vector visited_threads_inner;
        pt_buffer_init(visited_mutexes_inner);
        pt_buffer_init(visited_threads_inner);
        detect_error = pt_lock_tracking::detect_cycle(info, requested_mutex,
                &visited_mutexes_inner, &visited_threads_inner, &cycle_detected);
        if (detect_error != FT_ERR_SUCCESS)
            result_code = detect_error;
        else if (cycle_detected)
            result_code = FT_ERR_MUTEX_ALREADY_LOCKED;
        else
            result_code = FT_ERR_SUCCESS;
        pt_buffer_destroy(visited_threads_inner);
        pt_buffer_destroy(visited_mutexes_inner);
    }
    if (lock_acquired)
    {
        pt_lock_tracking::unlock_registry_mutex();
        g_registry_mutex_owned = false;
    }
    return (result_code);
}

int pt_lock_tracking::notify_acquired(pt_thread_id_type thread_identifier,
        const void *mutex_pointer)
{
    int lock_error;
    s_pt_thread_lock_info *info;
    bool lock_acquired = false;
    int error_code = FT_ERR_SUCCESS;
    int result_code = FT_ERR_SUCCESS;

    pt_lock_tracking_arm_thread_exit_cleanup();
    if (!pt_lock_tracking::ensure_registry_mutex_initialised(&error_code))
        return (error_code);
#ifdef LIBFT_TEST_BUILD
    result_code = pt_lock_tracking_notify_acquired_override_error_code.load();
    if (result_code != FT_ERR_SUCCESS)
        return (result_code);
#endif
    if (!g_registry_mutex_owned)
    {
        lock_error = pt_lock_tracking::lock_registry_mutex();
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        g_registry_mutex_owned = true;
        lock_acquired = true;
    }
    info = pt_lock_tracking::find_thread_info(thread_identifier, &error_code);
    if (info == ft_nullptr)
    {
        if (!lock_acquired && error_code == FT_ERR_SUCCESS)
            result_code = FT_ERR_INVALID_STATE;
        else
            result_code = error_code;
        goto cleanup_acquired;
    }
    if (!pt_lock_tracking::vector_contains_mutex(info->owned_mutexes, mutex_pointer))
    {
        int push_error = pt_buffer_push(info->owned_mutexes, mutex_pointer);
        if (push_error != FT_ERR_SUCCESS)
        {
            result_code = push_error;
            goto cleanup_acquired;
        }
    }
    result_code = pt_lock_tracking_set_mutex_owner(mutex_pointer, thread_identifier);
    if (result_code != FT_ERR_SUCCESS)
        goto cleanup_acquired;
    info->waiting_mutex = ft_nullptr;
    info->wait_started_ms = 0;
cleanup_acquired:
    if (lock_acquired)
    {
        pt_lock_tracking::unlock_registry_mutex();
        g_registry_mutex_owned = false;
    }
    return (result_code);
}

int pt_lock_tracking::notify_thread_exit(pt_thread_id_type thread_identifier)
{
    int lock_error;
    pt_buffer<s_pt_thread_lock_info> *thread_infos;
    ft_size_t thread_index;
    bool lock_acquired;
    int error_code;

    error_code = FT_ERR_SUCCESS;
    lock_acquired = false;
    if (!pt_lock_tracking::ensure_registry_mutex_initialised(&error_code))
        return (error_code);
    if (!g_registry_mutex_owned)
    {
        lock_error = pt_lock_tracking::lock_registry_mutex();
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        g_registry_mutex_owned = true;
        lock_acquired = true;
    }
    pt_lock_tracking_clear_mutex_owners_for_thread(thread_identifier);
    thread_infos = pt_lock_tracking::get_thread_infos(&error_code);
    if (thread_infos == ft_nullptr)
    {
        if (lock_acquired)
        {
            pt_lock_tracking::unlock_registry_mutex();
            g_registry_mutex_owned = false;
        }
        return (error_code);
    }
    thread_index = 0;
    while (thread_index < thread_infos->size)
    {
        if (thread_infos->data[thread_index].thread_identifier
                == thread_identifier)
        {
            pt_buffer_destroy(thread_infos->data[thread_index].owned_mutexes);
            pt_buffer_erase(*thread_infos, thread_index);
            break ;
        }
        thread_index += 1;
    }
    if (lock_acquired)
    {
        pt_lock_tracking::unlock_registry_mutex();
        g_registry_mutex_owned = false;
    }
    return (FT_ERR_SUCCESS);
}

int pt_lock_tracking::notify_released(pt_thread_id_type thread_identifier,
        const void *mutex_pointer)
{
    int lock_error;
    s_pt_thread_lock_info *info;
    ft_size_t index;
    bool lock_acquired = false;
    int error_code = FT_ERR_SUCCESS;
    int result_code = FT_ERR_SUCCESS;

    pt_lock_tracking_arm_thread_exit_cleanup();
    if (!pt_lock_tracking::ensure_registry_mutex_initialised(&error_code))
        return (error_code);
    if (!g_registry_mutex_owned)
    {
        lock_error = pt_lock_tracking::lock_registry_mutex();
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        g_registry_mutex_owned = true;
        lock_acquired = true;
    }
    info = pt_lock_tracking::find_thread_info(thread_identifier, &error_code);
    if (info == ft_nullptr)
    {
        if (!lock_acquired && error_code == FT_ERR_SUCCESS)
            result_code = FT_ERR_INVALID_STATE;
        else
            result_code = error_code;
        goto cleanup_released;
    }
    index = 0;
    while (index < info->owned_mutexes.size)
    {
        if (info->owned_mutexes.data[index] == mutex_pointer)
        {
            pt_buffer_erase(info->owned_mutexes, index);
            break ;
        }
        index += 1;
    }
    pt_lock_tracking_clear_mutex_owner(mutex_pointer);
    if (info->waiting_mutex == mutex_pointer)
    {
        info->waiting_mutex = ft_nullptr;
        info->wait_started_ms = 0;
    }
cleanup_released:
    if (lock_acquired)
    {
        pt_lock_tracking::unlock_registry_mutex();
        g_registry_mutex_owned = false;
    }
    return (result_code);
}

int pt_lock_tracking::snapshot_waiters(pt_lock_wait_snapshot_vector &snapshot)
{
    int lock_error;
    bool lock_acquired = false;
    pt_buffer<s_pt_thread_lock_info> *thread_infos;
    ft_size_t index;
    int error_code = FT_ERR_SUCCESS;
    int result_code = FT_ERR_SUCCESS;

    pt_buffer_clear(snapshot);
    if (!pt_lock_tracking::ensure_registry_mutex_initialised(&error_code))
        return (error_code);
    if (!g_registry_mutex_owned)
    {
        lock_error = pt_lock_tracking::lock_registry_mutex();
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        g_registry_mutex_owned = true;
        lock_acquired = true;
    }
    thread_infos = pt_lock_tracking::get_thread_infos(&error_code);
    if (thread_infos == ft_nullptr)
    {
        if (error_code != FT_ERR_SUCCESS)
            result_code = error_code;
        else
            result_code = FT_ERR_INVALID_STATE;
        goto cleanup_snapshot;
    }
    index = 0;
    while (index < thread_infos->size)
    {
        s_pt_thread_lock_info *info = &thread_infos->data[index];

        if (info->waiting_mutex != ft_nullptr)
        {
            s_pt_lock_wait_snapshot entry;
            s_pt_mutex_owner_info *owner_info;

            entry.mutex_pointer = info->waiting_mutex;
            entry.waiting_thread = info->thread_identifier;
            entry.owner_thread = 0;
            entry.wait_started_ms = info->wait_started_ms;
            owner_info = pt_lock_tracking_lookup_mutex_owner(info->waiting_mutex,
                    &error_code);
            if (error_code != FT_ERR_SUCCESS)
            {
                result_code = error_code;
                goto cleanup_snapshot;
            }
            if (owner_info != ft_nullptr)
                entry.owner_thread = owner_info->owner_thread;
            int push_error = pt_buffer_push(snapshot, entry);
            if (push_error != FT_ERR_SUCCESS)
            {
                result_code = push_error;
                goto cleanup_snapshot;
            }
        }
        index += 1;
    }
    result_code = FT_ERR_SUCCESS;
cleanup_snapshot:
    if (lock_acquired)
    {
        pt_lock_tracking::unlock_registry_mutex();
        g_registry_mutex_owned = false;
    }
    return (result_code);
}

int pt_lock_tracking::get_thread_state(pt_thread_id_type thread_identifier,
        s_pt_lock_tracking_thread_state &state)
{
    int lock_error;
    s_pt_thread_lock_info *info;
    int copy_error;
    bool lock_acquired = false;
    int error_code = FT_ERR_SUCCESS;
    int result_code = FT_ERR_SUCCESS;

    if (!pt_lock_tracking::ensure_registry_mutex_initialised(&error_code))
        return (error_code);
    pt_buffer_clear(state.owned_mutexes);
    state.thread_identifier = thread_identifier;
    state.waiting_mutex = ft_nullptr;
    state.wait_started_ms = 0;
    if (!g_registry_mutex_owned)
    {
        lock_error = pt_lock_tracking::lock_registry_mutex();
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
        g_registry_mutex_owned = true;
        lock_acquired = true;
    }
    info = pt_lock_tracking::lookup_thread_info(thread_identifier, &error_code);
    if (info == ft_nullptr)
    {
        if (error_code != FT_ERR_SUCCESS)
            result_code = error_code;
        goto cleanup_state;
    }
    state.thread_identifier = info->thread_identifier;
    state.waiting_mutex = info->waiting_mutex;
    state.wait_started_ms = info->wait_started_ms;
    copy_error = pt_buffer_copy(state.owned_mutexes, info->owned_mutexes);
    if (copy_error != FT_ERR_SUCCESS)
        result_code = copy_error;
cleanup_state:
    if (lock_acquired)
    {
        pt_lock_tracking::unlock_registry_mutex();
        g_registry_mutex_owned = false;
    }
    return (result_code);
}
