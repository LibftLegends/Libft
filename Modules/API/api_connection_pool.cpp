#include "api_internal.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/mutex.hpp"
#include "../CMA/CMA.hpp"
#include "../Logger/logger.hpp"
#include "../Networking/networking.hpp"
#include "../System_utils/system_utils.hpp"
#include <atomic>
#include <charconv>
#include <cstddef>
#include <set>
#include <new>
#include <errno.h>

#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
# include <sys/types.h>
#endif

struct api_pooled_connection
{
    ft_string key;
    ft_socket socket;
#if NETWORKING_HAS_OPENSSL
    SSL *tls_session;
    SSL_CTX *tls_context;
#endif
    ft_bool uses_tls;
    ft_bool negotiated_http2;
    int64_t idle_timestamp_ms;
    api_pooled_connection *next;

    static void *operator new(std::size_t size) noexcept
    {
        void *pointer;

        pointer = cma_malloc(size);
        return (pointer);
    }

    static void *operator new(std::size_t size, const std::nothrow_t&) noexcept
    {
        return (api_pooled_connection::operator new(size));
    }

    static void operator delete(void *pointer) noexcept
    {
        if (pointer)
            cma_free(pointer);
        return ;
    }

    static void operator delete(void *pointer, const std::nothrow_t&) noexcept
    {
        api_pooled_connection::operator delete(pointer);
        return ;
    }
};

struct api_connection_pool_storage
{
    api_pooled_connection *oldest;
    api_pooled_connection *newest;
    ft_size_t entry_count;
};

static void api_connection_pool_dispose_entry(api_pooled_connection &entry);

static void api_connection_pool_remove_entry(api_connection_pool_storage &storage,
    api_pooled_connection *previous, api_pooled_connection *entry)
{
    if (previous)
        previous->next = entry->next;
    else
        storage.oldest = entry->next;
    if (storage.newest == entry)
        storage.newest = previous;
    --storage.entry_count;
    delete entry;
    return ;
}

static api_connection_pool_storage &api_connection_pool_get_storage(void)
{
    static api_connection_pool_storage storage{};
    return (storage);
}

#if NETWORKING_HAS_OPENSSL
static std::set<SSL*> &api_connection_pool_get_tls_registry(void)
{
    static std::set<SSL*> tls_registry;

    return (tls_registry);
}

static pt_mutex &api_connection_pool_get_tls_mutex(void)
{
    static pt_mutex *const tls_mutex = []() noexcept
    {
        pt_mutex *const mutex = new (std::nothrow) pt_mutex();
        if (mutex == ft_nullptr)
            su_abort();
        const int32_t initialize_error = mutex->initialize();
        if (initialize_error != FT_ERR_SUCCESS)
            su_abort();
#ifdef LIBFT_TEST_BUILD
        (void)cma_untrack_leak(mutex);
        if (mutex->_native_mutex != ft_nullptr)
            (void)cma_untrack_leak(mutex->_native_mutex);
#endif
        return (mutex);
    }();
    return (*tls_mutex);
}

static ft_bool api_connection_pool_tls_register(SSL *tls_session)
{
    std::set<SSL*> &tls_registry = api_connection_pool_get_tls_registry();
    if (!tls_session)
        return (FT_TRUE);
    pt_mutex &mutex = api_connection_pool_get_tls_mutex();
    if (mutex.lock() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    try
    {
        tls_registry.insert(tls_session);
    }
    catch (const std::bad_alloc &)
    {
        (void)mutex.unlock();
        return (FT_FALSE);
    }
    (void)mutex.unlock();
    return (FT_TRUE);
}

static ft_bool api_connection_pool_tls_unregister(SSL *tls_session)
{
    std::set<SSL*> &tls_registry = api_connection_pool_get_tls_registry();
    if (!tls_session)
        return (FT_FALSE);
    pt_mutex &mutex = api_connection_pool_get_tls_mutex();
    if (mutex.lock() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const ft_bool removed = tls_registry.erase(tls_session) != 0;
    (void)mutex.unlock();
    return (removed);
}
#endif

static ft_bool api_connection_pool_socket_is_alive(ft_socket &socket)
{
    int32_t poll_descriptor;
    int32_t poll_result;
    char peek_byte;
    ssize_t peek_result;

    poll_descriptor = socket.get_file_descriptor();
    if (poll_descriptor < 0)
        return (FT_FALSE);
    // Silence on an idle socket is expected. Only inspect data or EOF already available.
    poll_result = nw_poll(&poll_descriptor, 1, ft_nullptr, 0, 0);
    if (poll_result < 0)
        return (FT_FALSE);
    if (poll_result == 0)
        return (FT_TRUE);
    if (poll_descriptor == -1)
        return (FT_FALSE);
    peek_byte = 0;
#ifdef _WIN32
    peek_result = socket.receive_data(&peek_byte, 1, MSG_PEEK);
    if (peek_result < 0)
    {
        int32_t last_error;

        last_error = WSAGetLastError();
        if (last_error == WSAEWOULDBLOCK || last_error == WSAEINTR)
            return (FT_TRUE);
        return (FT_FALSE);
    }
#else
    peek_result = socket.receive_data(&peek_byte, 1, MSG_PEEK | MSG_DONTWAIT);
    if (peek_result > 0)
        return (FT_FALSE);
    if (peek_result == 0)
        return (FT_FALSE);
    if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
        return (FT_TRUE);
#endif
    return (FT_FALSE);
}

#if NETWORKING_HAS_OPENSSL
ft_bool api_connection_pool_track_tls_session(SSL *tls_session)
{
    return (api_connection_pool_tls_register(tls_session));
}

ft_bool api_connection_pool_untrack_tls_session(SSL *tls_session)
{
    return (api_connection_pool_tls_unregister(tls_session));
}
#endif

static void api_connection_pool_clear_storage(api_connection_pool_storage &storage)
{
    while (storage.oldest)
    {
        api_connection_pool_dispose_entry(*storage.oldest);
        api_connection_pool_remove_entry(storage, ft_nullptr, storage.oldest);
    }
    return ;
}

static pt_mutex &api_connection_pool_get_mutex(void)
{
    static pt_mutex *const connection_pool_mutex = []() noexcept
    {
        pt_mutex *const mutex = new (std::nothrow) pt_mutex();
        if (mutex == ft_nullptr)
            su_abort();
        const int32_t initialize_error = mutex->initialize();
        if (initialize_error != FT_ERR_SUCCESS)
            su_abort();
#ifdef LIBFT_TEST_BUILD
        (void)cma_untrack_leak(mutex);
        if (mutex->_native_mutex != ft_nullptr)
            (void)cma_untrack_leak(mutex->_native_mutex);
#endif
        return (mutex);
    }();
    return (*connection_pool_mutex);
}
static std::atomic<ft_size_t> g_api_connection_max_idle{8};
static std::atomic<int64_t> g_api_connection_idle_timeout_ms{60000};
static std::atomic<ft_size_t> g_api_connection_pool_acquire_calls{0};
static std::atomic<ft_size_t> g_api_connection_pool_reuse_hits{0};
static std::atomic<ft_size_t> g_api_connection_pool_acquire_misses{0};
static std::atomic<ft_bool> g_api_connection_pool_enabled{FT_TRUE};

static void api_connection_pool_record_dispose_snapshot(
    ft_bool socket_cleanup_allowed,
    ft_bool socket_is_open,
    int32_t socket_error,
    ft_size_t client_count)
{
    int32_t cleanup_allowed_flag;
    int32_t socket_open_flag;

    cleanup_allowed_flag = 0;
    if (socket_cleanup_allowed)
        cleanup_allowed_flag = 1;
    socket_open_flag = 0;
    if (socket_is_open)
        socket_open_flag = 1;
    ft_log_debug(
        "api_connection_pool_dispose_entry snapshot cleanup_allowed=%d socket_open=%d socket_error=%d client_count=%llu",
        cleanup_allowed_flag, socket_open_flag, socket_error, client_count);
    return ;
}

static int32_t api_connection_pool_build_key(const char *host, uint16_t port,
        api_connection_security_mode security_mode,
        const char *security_identity, ft_string &key)
{
    int32_t error_code;
    if (key.is_initialised() == FT_FALSE)
        error_code = key.initialize();
    else
        error_code = key.clear();
    if (error_code == FT_ERR_SUCCESS && host)
        error_code = key.append(host);
    if (error_code == FT_ERR_SUCCESS)
        error_code = key.append(':');
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    char port_buffer[5];
    const auto port_text = std::to_chars(port_buffer, port_buffer + sizeof(port_buffer), port);
    if (port_text.ec != std::errc{})
        return (FT_ERR_OUT_OF_RANGE);
    error_code = key.append(port_buffer, static_cast<ft_size_t>(port_text.ptr - port_buffer));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (security_mode == api_connection_security_mode::TLS)
        error_code = key.append(":tls:", 5U);
    else
        error_code = key.append(":plain:", 7U);
    if (error_code == FT_ERR_SUCCESS && security_identity && security_identity[0] != '\0')
        error_code = key.append(security_identity);
    return (error_code);
}

#if NETWORKING_HAS_OPENSSL
static void api_connection_pool_free_tls(SSL *tls_session, SSL_CTX *tls_context)
{
    if (tls_session)
        SSL_free(tls_session);
    if (tls_context)
        SSL_CTX_free(tls_context);
    return ;
}
#endif

static void api_connection_pool_dispose_entry(api_pooled_connection &entry)
{
    ft_bool socket_is_open;
    ft_size_t client_count;
    int32_t socket_error;

    socket_is_open = (entry.socket.get_file_descriptor() >= 0);
    client_count = 0;
    socket_error = FT_ERR_SUCCESS;
    if (socket_is_open)
        client_count = entry.socket.get_client_count();
    {
        entry.socket.disconnect_all_clients();
    }
#if NETWORKING_HAS_OPENSSL
    if (entry.uses_tls && entry.tls_session)
    {
        if (api_connection_pool_tls_unregister(entry.tls_session))
            api_connection_pool_free_tls(entry.tls_session, entry.tls_context);
    }
    entry.tls_session = ft_nullptr;
    entry.tls_context = ft_nullptr;
#endif
    entry.uses_tls = FT_FALSE;
    entry.negotiated_http2 = FT_FALSE;
    entry.idle_timestamp_ms = 0;
    api_connection_pool_record_dispose_snapshot(
        FT_TRUE,
        socket_is_open,
        socket_error,
        client_count);
    if (socket_is_open)
        entry.socket.close_socket();
    return ;
}

ft_bool api_connection_pool_acquire(api_connection_pool_handle &handle,
        const char *host, uint16_t port,
        api_connection_security_mode security_mode,
        const char *security_identity)
{
    ft_bool handle_lock_acquired;
    int32_t lock_error;

    handle_lock_acquired = FT_FALSE;
    if (handle.lock(&handle_lock_acquired) != FT_ERR_SUCCESS)
    {
        g_api_connection_pool_acquire_misses.fetch_add(1U, std::memory_order_relaxed);
        return (FT_FALSE);
    }

    if (api_connection_pool_build_key(host, port, security_mode,
            security_identity, handle.key) != FT_ERR_SUCCESS)
    {
        handle.unlock(handle_lock_acquired);
        g_api_connection_pool_acquire_misses.fetch_add(1U, std::memory_order_relaxed);
        return (FT_FALSE);
    }
    handle.security_mode = security_mode;
#if NETWORKING_HAS_OPENSSL
    handle.tls_session = ft_nullptr;
    handle.tls_context = ft_nullptr;
#endif
    handle.has_socket = FT_FALSE;
    handle.from_pool = FT_FALSE;
    handle.should_store = FT_FALSE;
    handle.negotiated_http2 = FT_FALSE;
    handle.plain_socket_timed_out = FT_FALSE;
    handle.plain_socket_validated = FT_FALSE;
    g_api_connection_pool_acquire_calls.fetch_add(1U, std::memory_order_relaxed);
    // TLS sessions are evicted instead of entering the shared idle pool.
    if (security_mode == api_connection_security_mode::TLS
        || !g_api_connection_pool_enabled.load(std::memory_order_relaxed))
    {
        handle.unlock(handle_lock_acquired);
        g_api_connection_pool_acquire_misses.fetch_add(1U, std::memory_order_relaxed);
        return (FT_FALSE);
    }
    handle.should_store = FT_TRUE;
    lock_error = api_connection_pool_get_mutex().lock();
    if (lock_error != FT_ERR_SUCCESS)
    {
        handle.unlock(handle_lock_acquired);
        g_api_connection_pool_acquire_misses.fetch_add(1U, std::memory_order_relaxed);
        return (FT_FALSE);
    }
    if (!g_api_connection_pool_enabled.load(std::memory_order_relaxed))
    {
        handle.should_store = FT_FALSE;
        (void)api_connection_pool_get_mutex().unlock();
        handle.unlock(handle_lock_acquired);
        g_api_connection_pool_acquire_misses.fetch_add(1U, std::memory_order_relaxed);
        return (FT_FALSE);
    }
    const int64_t idle_timeout_ms = g_api_connection_idle_timeout_ms.load(std::memory_order_relaxed);
    api_connection_pool_storage &storage = api_connection_pool_get_storage();
    if (storage.oldest && idle_timeout_ms > 0)
    {
        const int64_t now_ms = time_monotonic();
        while (storage.oldest && now_ms - storage.oldest->idle_timestamp_ms > idle_timeout_ms)
            api_connection_pool_remove_entry(storage, ft_nullptr, storage.oldest);
    }
    const char *requested_key = handle.key.c_str();
    api_pooled_connection *previous = ft_nullptr;
    api_pooled_connection *entry = storage.oldest;
    while (entry)
    {
        api_pooled_connection *next = entry->next;
        if (ft_strcmp(entry->key.c_str(), requested_key) != 0)
        {
            previous = entry;
            entry = next;
            continue ;
        }
        if (!api_connection_pool_socket_is_alive(entry->socket))
        {
            api_connection_pool_remove_entry(storage, previous, entry);
            entry = next;
            continue ;
        }
        if (handle.socket.move(entry->socket) != FT_ERR_SUCCESS)
        {
            api_connection_pool_remove_entry(storage, previous, entry);
            (void)api_connection_pool_get_mutex().unlock();
            handle.unlock(handle_lock_acquired);
            g_api_connection_pool_acquire_misses.fetch_add(1U, std::memory_order_relaxed);
            return (FT_FALSE);
        }
#if NETWORKING_HAS_OPENSSL
        handle.tls_session = entry->tls_session;
        handle.tls_context = entry->tls_context;
        entry->tls_session = ft_nullptr;
        entry->tls_context = ft_nullptr;
#endif
        handle.has_socket = FT_TRUE;
        handle.from_pool = FT_TRUE;
        handle.should_store = FT_TRUE;
        handle.negotiated_http2 = entry->negotiated_http2;
        handle.plain_socket_timed_out = FT_FALSE;
        handle.plain_socket_validated = FT_FALSE;
        api_connection_pool_remove_entry(storage, previous, entry);
        (void)api_connection_pool_get_mutex().unlock();
        handle.unlock(handle_lock_acquired);
        g_api_connection_pool_reuse_hits.fetch_add(1U, std::memory_order_relaxed);
        return (FT_TRUE);
    }
    (void)api_connection_pool_get_mutex().unlock();
    handle.unlock(handle_lock_acquired);
    g_api_connection_pool_acquire_misses.fetch_add(1U, std::memory_order_relaxed);
    return (FT_FALSE);
}

void api_connection_pool_mark_idle(api_connection_pool_handle &handle)
{
    api_connection_pool_storage &storage = api_connection_pool_get_storage();
    api_pooled_connection *entry;
    ft_bool handle_lock_acquired;
    int32_t lock_error;

    handle_lock_acquired = FT_FALSE;
    if (handle.lock(&handle_lock_acquired) != FT_ERR_SUCCESS)
        return ;
    if (!handle.has_socket)
    {
        handle.unlock(handle_lock_acquired);
        return ;
    }
    if (handle.plain_socket_timed_out)
    {
        handle.unlock(handle_lock_acquired);
        api_connection_pool_evict(handle);
        return ;
    }
    if (!handle.should_store)
    {
        handle.unlock(handle_lock_acquired);
        api_connection_pool_evict(handle);
        return ;
    }
    if (!g_api_connection_pool_enabled.load(std::memory_order_relaxed))
    {
        handle.unlock(handle_lock_acquired);
        api_connection_pool_evict(handle);
        return ;
    }
    if (handle.security_mode == api_connection_security_mode::TLS)
    {
        handle.unlock(handle_lock_acquired);
        api_connection_pool_evict(handle);
        return ;
    }
    if (g_api_connection_max_idle.load(std::memory_order_relaxed) == 0)
    {
        handle.unlock(handle_lock_acquired);
        api_connection_pool_evict(handle);
        return ;
    }
    if (!api_connection_pool_socket_is_alive(handle.socket))
    {
        handle.unlock(handle_lock_acquired);
        api_connection_pool_evict(handle);
        return ;
    }
    entry = new (std::nothrow) api_pooled_connection();
    if (entry == ft_nullptr)
    {
        handle.unlock(handle_lock_acquired);
        api_connection_pool_evict(handle);
        return ;
    }
    if (entry->key.initialize(handle.key) != FT_ERR_SUCCESS)
    {
        delete entry;
        handle.unlock(handle_lock_acquired);
        api_connection_pool_evict(handle);
        return ;
    }
    if (entry->socket.move(handle.socket) != FT_ERR_SUCCESS)
    {
        delete entry;
        handle.unlock(handle_lock_acquired);
        api_connection_pool_evict(handle);
        return ;
    }
#if NETWORKING_HAS_OPENSSL
    entry->tls_session = handle.tls_session;
    entry->tls_context = handle.tls_context;
    handle.tls_session = ft_nullptr;
    handle.tls_context = ft_nullptr;
#endif
    entry->uses_tls = (handle.security_mode == api_connection_security_mode::TLS);
    entry->negotiated_http2 = handle.negotiated_http2;
    handle.has_socket = FT_FALSE;
    handle.from_pool = FT_FALSE;
    handle.should_store = FT_FALSE;
    handle.negotiated_http2 = FT_FALSE;
    handle.plain_socket_timed_out = FT_FALSE;
    handle.plain_socket_validated = FT_FALSE;
    lock_error = api_connection_pool_get_mutex().lock();
    if (lock_error != FT_ERR_SUCCESS)
    {
        handle.unlock(handle_lock_acquired);
        api_connection_pool_dispose_entry(*entry);
        delete entry;
        return ;
    }
    const ft_size_t max_idle = g_api_connection_max_idle.load(std::memory_order_relaxed);
    if (!g_api_connection_pool_enabled.load(std::memory_order_relaxed) || max_idle == 0)
    {
        (void)api_connection_pool_get_mutex().unlock();
        handle.unlock(handle_lock_acquired);
        api_connection_pool_dispose_entry(*entry);
        delete entry;
        return ;
    }
    while (storage.entry_count >= max_idle)
        api_connection_pool_remove_entry(storage, ft_nullptr, storage.oldest);
    // Timestamp entries under the lock so expiry can stop at the first live entry.
    entry->idle_timestamp_ms = time_monotonic();
    entry->next = ft_nullptr;
    if (storage.newest)
        storage.newest->next = entry;
    else
        storage.oldest = entry;
    storage.newest = entry;
    storage.entry_count += 1;
    (void)api_connection_pool_get_mutex().unlock();
    handle.unlock(handle_lock_acquired);
    return ;
}

void api_connection_pool_evict(api_connection_pool_handle &handle)
{
    ft_bool handle_lock_acquired;

    handle_lock_acquired = FT_FALSE;
    if (handle.lock(&handle_lock_acquired) != FT_ERR_SUCCESS)
        return ;
    if (!handle.has_socket)
    {
        handle.unlock(handle_lock_acquired);
        return ;
    }
    if (handle.security_mode == api_connection_security_mode::TLS)
    {
#if NETWORKING_HAS_OPENSSL
        ft_bool tls_removed;

        tls_removed = api_connection_pool_untrack_tls_session(handle.tls_session);
        if (tls_removed)
            api_connection_pool_free_tls(handle.tls_session, handle.tls_context);
        handle.tls_session = ft_nullptr;
        handle.tls_context = ft_nullptr;
#endif
    }
    handle.socket.disconnect_all_clients();
    handle.socket.close_socket();
    handle.has_socket = FT_FALSE;
    handle.from_pool = FT_FALSE;
    handle.should_store = FT_FALSE;
    handle.negotiated_http2 = FT_FALSE;
    handle.plain_socket_timed_out = FT_FALSE;
    handle.plain_socket_validated = FT_FALSE;
    handle.unlock(handle_lock_acquired);
    return ;
}

void api_connection_pool_disable_store(api_connection_pool_handle &handle)
{
    ft_bool handle_lock_acquired;

    handle_lock_acquired = FT_FALSE;
    if (handle.lock(&handle_lock_acquired) != FT_ERR_SUCCESS)
        return ;
    handle.should_store = FT_FALSE;
    handle.negotiated_http2 = FT_FALSE;
    handle.unlock(handle_lock_acquired);
    return ;
}

void api_connection_pool_flush(void)
{
    api_connection_pool_storage &storage = api_connection_pool_get_storage();
    int32_t lock_error;

    lock_error = api_connection_pool_get_mutex().lock();
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    api_connection_pool_clear_storage(storage);
    (void)api_connection_pool_get_mutex().unlock();
    return ;
}

void api_connection_pool_set_enabled(ft_bool enabled)
{
    api_connection_pool_storage &storage = api_connection_pool_get_storage();
    int32_t lock_error;

    lock_error = api_connection_pool_get_mutex().lock();
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    g_api_connection_pool_enabled.store(enabled, std::memory_order_relaxed);
    if (!enabled)
        api_connection_pool_clear_storage(storage);
    (void)api_connection_pool_get_mutex().unlock();
    return ;
}

ft_bool api_connection_pool_is_enabled(void)
{
    return (g_api_connection_pool_enabled.load(std::memory_order_relaxed));
}

void api_connection_pool_set_max_idle(ft_size_t max_idle)
{
    g_api_connection_max_idle.store(max_idle, std::memory_order_relaxed);
    return ;
}

ft_size_t api_connection_pool_get_max_idle(void)
{
    return (g_api_connection_max_idle.load(std::memory_order_relaxed));
}

void api_connection_pool_set_idle_timeout(int64_t idle_timeout_ms)
{
    g_api_connection_idle_timeout_ms.store(idle_timeout_ms, std::memory_order_relaxed);
    return ;
}

int64_t api_connection_pool_get_idle_timeout(void)
{
    return (g_api_connection_idle_timeout_ms.load(std::memory_order_relaxed));
}

void api_debug_reset_connection_pool_counters(void)
{
    g_api_connection_pool_acquire_calls.store(0, std::memory_order_relaxed);
    g_api_connection_pool_reuse_hits.store(0, std::memory_order_relaxed);
    g_api_connection_pool_acquire_misses.store(0, std::memory_order_relaxed);
    return ;
}

ft_size_t api_debug_get_connection_pool_acquires(void)
{
    return (g_api_connection_pool_acquire_calls.load(std::memory_order_relaxed));
}

ft_size_t api_debug_get_connection_pool_reuses(void)
{
    return (g_api_connection_pool_reuse_hits.load(std::memory_order_relaxed));
}

ft_size_t api_debug_get_connection_pool_misses(void)
{
    return (g_api_connection_pool_acquire_misses.load(std::memory_order_relaxed));
}
