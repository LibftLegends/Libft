#include "networking.hpp"
#include "socket_handle.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Template/unordered_map.hpp"
#include "../Template/vector.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Time/time.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <string_view>
#include <new>
#include <utility>
#include <errno.h>

#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netdb.h>
# include <sys/socket.h>
# include <arpa/inet.h>
#endif

struct networking_dns_cache_entry
{
    int64_t expiration_ms;
    ft_size_t address_count;
};

using networking_dns_table = ft_unordered_map<std::string_view, networking_dns_cache_entry *>;

struct networking_dns_cache
{
    networking_dns_table entries;
    uint64_t generation = 0;

    ~networking_dns_cache();
    void clear();
};

void networking_dns_cache::clear()
{
    if (this->entries.is_initialised() != FT_CLASS_STATE_INITIALISED)
        return ;
    auto entry = this->entries.begin();
    const auto end = this->entries.end();
    while (entry != end)
    {
        cma_free(entry->second);
        ++entry;
    }
    this->entries.clear();
    return ;
}

networking_dns_cache::~networking_dns_cache()
{
    this->clear();
    return ;
}

static constexpr int64_t DNS_CACHE_TTL_MS = 60000;
static std::mutex g_networking_dns_shared_mutex;
static networking_dns_cache g_networking_dns_shared_cache;
static std::atomic<ft_bool> g_networking_dns_shared_enabled{FT_TRUE};
static std::atomic<uint64_t> g_networking_dns_cache_generation{1};

static networking_dns_cache &networking_dns_get_cache(ft_bool shared)
{
    if (shared)
        return (g_networking_dns_shared_cache);
    thread_local networking_dns_cache private_cache;
    if (private_cache.generation != 0
        && private_cache.generation != g_networking_dns_cache_generation.load(std::memory_order_acquire))
    {
        private_cache.clear();
        private_cache.generation = 0;
    }
    return (private_cache);
}

int32_t networking_dns_enable_thread_safety(void) noexcept
{
    g_networking_dns_shared_enabled.store(FT_TRUE, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t networking_dns_disable_thread_safety(void) noexcept
{
    g_networking_dns_shared_enabled.store(FT_FALSE, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

ft_bool networking_dns_is_thread_safe(void) noexcept
{
    return (g_networking_dns_shared_enabled.load(std::memory_order_acquire));
}

static void networking_push_failure(int32_t error_code) noexcept
{
    (void)error_code;
    return ;
}

static std::string_view networking_dns_build_key(char *buffer, ft_size_t capacity,
    ft_string &large_key, const char *host, const char *service,
    int32_t family, int32_t socktype, int32_t protocol, int32_t flags)
{
    const int32_t settings[] = {family, socktype, protocol, flags};
    const ft_size_t host_size = ft_strlen_size_t(host);
    ft_size_t service_size = 0;
    if (service)
        service_size = ft_strlen_size_t(service);
    constexpr ft_size_t FIXED_SIZE = sizeof(settings) + 1U;
    constexpr ft_size_t MAX_TEXT_SIZE = FT_SYSTEM_SIZE_MAX - FIXED_SIZE - 2U;
    if (host_size > MAX_TEXT_SIZE || service_size > MAX_TEXT_SIZE - host_size)
        return (std::string_view{});
    const ft_size_t key_size = FIXED_SIZE + host_size + service_size;
    if (key_size > capacity)
    {
        if (large_key.initialize() != FT_ERR_SUCCESS
            || large_key.resize_length(key_size) != FT_ERR_SUCCESS)
            return (std::string_view{});
        buffer = large_key.data();
    }
    ft_memcpy_fixed<sizeof(settings)>(buffer, settings);
    char *const text = buffer + sizeof(settings);
    ft_memcpy(text, host, host_size);
    text[host_size] = '\0';
    if (service_size != 0)
        ft_memcpy(text + host_size + 1U, service, service_size);
    return (std::string_view(buffer, key_size));
}

static const networking_resolved_address *networking_dns_entry_addresses(
    const networking_dns_cache_entry *entry)
{
    return (reinterpret_cast<const networking_resolved_address *>(entry + 1));
}

static int32_t networking_dns_copy_addresses(const networking_dns_cache_entry &entry,
    ft_vector<networking_resolved_address> &destination)
{
    destination.reserve(entry.address_count);
    const int32_t reserve_error = destination.get_error();
    if (reserve_error != FT_ERR_SUCCESS)
        return (reserve_error);
    const auto *addresses = networking_dns_entry_addresses(&entry);
    ft_size_t index = 0;
    while (index < entry.address_count)
    {
        const int32_t copy_error = destination.push_back(addresses[index]);
        if (copy_error != FT_ERR_SUCCESS)
        {
            destination.clear();
            return (copy_error);
        }
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

static void networking_dns_cache_store(networking_dns_cache &cache, std::string_view key,
    const ft_vector<networking_resolved_address> &addresses)
{
    const ft_size_t count = addresses.size();
    if (key.size() > FT_SYSTEM_SIZE_MAX - sizeof(networking_dns_cache_entry)
        || count > (FT_SYSTEM_SIZE_MAX - sizeof(networking_dns_cache_entry) - key.size())
            / sizeof(networking_resolved_address))
        return ;
    auto &entries = cache.entries;
    if (entries.is_initialised() != FT_CLASS_STATE_INITIALISED
        && entries.initialize() != FT_ERR_SUCCESS)
        return ;
#ifdef LIBFT_TEST_BUILD
    (void)cma_untrack_leak(entries._data);
    (void)cma_untrack_leak(entries._occupied);
#endif
    if (entries.find(key) != entries.end())
        return ;

    static_assert(alignof(networking_dns_cache_entry) >= alignof(networking_resolved_address));
    const ft_size_t addresses_bytes = count * sizeof(networking_resolved_address);
    void *const storage = cma_malloc(sizeof(networking_dns_cache_entry) + addresses_bytes + key.size());
    if (!storage)
        return ;
    auto *entry = new (storage) networking_dns_cache_entry{};
    entry->address_count = count;
    const int64_t now_ms = time_monotonic();
    entry->expiration_ms = INT64_MAX;
    if (now_ms <= INT64_MAX - DNS_CACHE_TTL_MS)
        entry->expiration_ms = now_ms + DNS_CACHE_TTL_MS;
    auto *stored_addresses = reinterpret_cast<networking_resolved_address *>(entry + 1);
    std::uninitialized_copy_n(addresses.begin(), count, stored_addresses);
    char *const stored_key = reinterpret_cast<char *>(stored_addresses) + addresses_bytes;
    ft_memcpy(stored_key, key.data(), key.size());
    entries.insert(std::string_view(stored_key, key.size()), entry);
    if (entries.get_error() != FT_ERR_SUCCESS)
        cma_free(entry);
#ifdef LIBFT_TEST_BUILD
    else
        (void)cma_untrack_leak(entry);
    (void)cma_untrack_leak(entries._data);
    (void)cma_untrack_leak(entries._occupied);
#endif
    return ;
}

void networking_dns_set_error(int32_t resolver_status) noexcept
{
#ifdef EAI_BADFLAGS
    if (resolver_status == EAI_BADFLAGS)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_BAD_FLAGS);
        return ;
    }
#endif
#ifdef EAI_AGAIN
    if (resolver_status == EAI_AGAIN)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_AGAIN);
        return ;
    }
#endif
#ifdef EAI_FAIL
    if (resolver_status == EAI_FAIL)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_FAIL);
        return ;
    }
#endif
#ifdef EAI_FAMILY
    if (resolver_status == EAI_FAMILY)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_FAMILY);
        return ;
    }
#endif
#ifdef EAI_ADDRFAMILY
    if (resolver_status == EAI_ADDRFAMILY)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_FAMILY);
        return ;
    }
#endif
#ifdef EAI_SOCKTYPE
    if (resolver_status == EAI_SOCKTYPE)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_SOCKTYPE);
        return ;
    }
#endif
#ifdef EAI_SERVICE
    if (resolver_status == EAI_SERVICE)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_SERVICE);
        return ;
    }
#endif
#ifdef EAI_MEMORY
    if (resolver_status == EAI_MEMORY)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_MEMORY);
        return ;
    }
#endif
#ifdef EAI_NONAME
    if (resolver_status == EAI_NONAME)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_NO_NAME);
        return ;
    }
#endif
#ifdef EAI_NODATA
    if (resolver_status == EAI_NODATA)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_NO_NAME);
        return ;
    }
#endif
#ifdef EAI_OVERFLOW
    if (resolver_status == EAI_OVERFLOW)
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_OVERFLOW);
        return ;
    }
#endif
#ifdef EAI_SYSTEM
    if (resolver_status == EAI_SYSTEM)
    {
#ifdef _WIN32
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_FAIL);
#else
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_FAIL);
#endif
        return ;
    }
#endif
    networking_push_failure(FT_ERR_SOCKET_RESOLVE_FAILED);
    return ;
}

static ft_bool networking_dns_resolve_query(const char *host, const char *service,
    int32_t family, int32_t socktype, int32_t protocol, int32_t flags,
    ft_vector<networking_resolved_address> *out_addresses,
    networking_resolved_address *out_first) noexcept
{
    if (out_addresses)
        out_addresses->clear();
    if (!host || host[0] == '\0')
        return (FT_FALSE);

    char key_buffer[512];
    ft_string large_key;
    const std::string_view cache_key = networking_dns_build_key(key_buffer, sizeof(key_buffer),
        large_key, host, service, family, socktype, protocol, flags);
    const ft_bool use_shared_cache = networking_dns_is_thread_safe();
    const uint64_t generation = g_networking_dns_cache_generation.load(std::memory_order_acquire);
    if (!cache_key.empty())
    {
        if (use_shared_cache)
            g_networking_dns_shared_mutex.lock();
        auto &cache = networking_dns_get_cache(use_shared_cache);
        auto &entries = cache.entries;
        if (entries.is_initialised() == FT_CLASS_STATE_INITIALISED)
        {
            const auto cached = entries.find(cache_key);
            if (cached != entries.end())
            {
                auto *entry = cached->second;
                if (entry->expiration_ms >= time_monotonic())
                {
                    int32_t copy_error = FT_ERR_SUCCESS;
                    if (out_first)
                        *out_first = *networking_dns_entry_addresses(entry);
                    else
                        copy_error = networking_dns_copy_addresses(*entry, *out_addresses);
                    if (use_shared_cache)
                        g_networking_dns_shared_mutex.unlock();
                    return (copy_error == FT_ERR_SUCCESS);
                }
                entries.erase(cache_key);
                if (entries.get_error() == FT_ERR_SUCCESS)
                    cma_free(entry);
            }
        }
        if (use_shared_cache)
            g_networking_dns_shared_mutex.unlock();
    }

    ft_vector<networking_resolved_address> temporary_addresses;
    if (out_first)
    {
        if (temporary_addresses.initialize() != FT_ERR_SUCCESS)
            return (FT_FALSE);
        out_addresses = &temporary_addresses;
    }
    if (service && service[0] == '\0')
        service = ft_nullptr;
    struct addrinfo hints{};
    struct addrinfo *results = ft_nullptr;
    hints.ai_family = family;
    hints.ai_socktype = socktype;
    hints.ai_protocol = protocol;
    hints.ai_flags = flags;
    if (ft_socket_runtime_acquire() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const int32_t resolver_status = getaddrinfo(host, service, &hints, &results);
    if (resolver_status != 0)
    {
        networking_dns_set_error(resolver_status);
        if (results)
            freeaddrinfo(results);
        ft_socket_runtime_release();
        return (FT_FALSE);
    }
    const struct addrinfo *current = results;
    int32_t copy_error = FT_ERR_SUCCESS;
    while (current && copy_error == FT_ERR_SUCCESS)
    {
        networking_resolved_address address{};
        if (current->ai_addrlen <= sizeof(address.address))
        {
            ft_memcpy(&address.address, current->ai_addr, current->ai_addrlen);
            address.length = FT_NETWORKING_SOCKLEN_CAST(current->ai_addrlen);
            copy_error = out_addresses->push_back(address);
        }
        current = current->ai_next;
    }
    freeaddrinfo(results);
    ft_socket_runtime_release();
    if (copy_error != FT_ERR_SUCCESS || out_addresses->empty())
    {
        out_addresses->clear();
        return (FT_FALSE);
    }
    if (out_first)
        *out_first = (*out_addresses)[0];
    if (!cache_key.empty())
    {
        if (use_shared_cache)
            g_networking_dns_shared_mutex.lock();
        if (generation == g_networking_dns_cache_generation.load(std::memory_order_acquire))
        {
            auto &cache = networking_dns_get_cache(use_shared_cache);
            cache.generation = generation;
            networking_dns_cache_store(cache, cache_key, *out_addresses);
        }
        if (use_shared_cache)
            g_networking_dns_shared_mutex.unlock();
    }
    return (FT_TRUE);
}

ft_bool networking_dns_resolve(const char *host, const char *service,
    int32_t family, int32_t socktype, int32_t protocol, int32_t flags,
    ft_vector<networking_resolved_address> &out_addresses) noexcept
{
    return (networking_dns_resolve_query(host, service, family, socktype, protocol,
        flags, &out_addresses, ft_nullptr));
}

ft_bool networking_dns_resolve_first(const char *host, const char *service,
    int32_t family, int32_t socktype, int32_t protocol, int32_t flags,
    networking_resolved_address &out_address) noexcept
{
    return (networking_dns_resolve_query(host, service, family, socktype, protocol,
        flags, ft_nullptr, &out_address));
}

ft_bool networking_resolved_address_to_string(
    const networking_resolved_address &address, char *buffer,
    ft_size_t buffer_size) noexcept
{
    const void *source_address;
    int32_t family;

    if (buffer == ft_nullptr || buffer_size == 0)
    {
        networking_push_failure(FT_ERR_INVALID_POINTER);
        return (FT_FALSE);
    }
    buffer[0] = '\0';
    family = address.address.ss_family;
    if (family == AF_INET)
    {
        source_address = &reinterpret_cast<const sockaddr_in *>(&address.address)->sin_addr;
    }
    else if (family == AF_INET6)
    {
        source_address = &reinterpret_cast<const sockaddr_in6 *>(&address.address)->sin6_addr;
    }
    else
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_FAMILY);
        return (FT_FALSE);
    }
    if (!inet_ntop(family, source_address, buffer,
            static_cast<socklen_t>(buffer_size)))
    {
        networking_push_failure(FT_ERR_SOCKET_RESOLVE_FAILED);
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

void networking_dns_clear_cache(void) noexcept
{
    g_networking_dns_shared_mutex.lock();
    (void)g_networking_dns_cache_generation.fetch_add(1, std::memory_order_acq_rel);
    g_networking_dns_shared_cache.clear();
    g_networking_dns_shared_mutex.unlock();
    if (!networking_dns_is_thread_safe())
        (void)networking_dns_get_cache(FT_FALSE);
    return ;
}

#ifdef LIBFT_TEST_BUILD
void networking_dns_destroy_cache_for_tests(void) noexcept
{
    networking_dns_clear_cache();
    g_networking_dns_shared_mutex.lock();
    (void)g_networking_dns_shared_cache.entries.destroy();
    g_networking_dns_shared_mutex.unlock();
    return ;
}
#endif
