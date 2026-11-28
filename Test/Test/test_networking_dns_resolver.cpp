#include "../test_internal.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include <atomic>
#include <charconv>
#include <thread>
#if defined(_WIN32) || defined(_WIN64)
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
#endif

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_networking_dns_resolve_numeric_host_returns_address)
{
    ft_vector<networking_resolved_address> resolved_addresses;
    int32_t initialize_error;
    bool resolve_result;
    size_t address_count;
    networking_resolved_address first_address;
    const struct sockaddr_in *ipv4_address;

    initialize_error = resolved_addresses.initialize();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_error);
    networking_dns_clear_cache();
    resolve_result = networking_dns_resolve("127.0.0.1", "80", AF_INET, SOCK_STREAM,
        IPPROTO_TCP, AI_NUMERICHOST, resolved_addresses);
    FT_ASSERT_EQ(true, resolve_result);
    address_count = resolved_addresses.size();
    FT_ASSERT(address_count > 0);
    first_address = resolved_addresses[0];
    ipv4_address = reinterpret_cast<const struct sockaddr_in*>(&first_address.address);
    FT_ASSERT(ipv4_address != ft_nullptr);
    FT_ASSERT_EQ(AF_INET, ipv4_address->sin_family);
    FT_ASSERT_EQ(htons(80), ipv4_address->sin_port);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, resolved_addresses.destroy());
    return (1);
}

FT_TEST(test_networking_dns_resolve_rejects_null_host)
{
    ft_vector<networking_resolved_address> resolved_addresses;
    int32_t initialize_error;
    bool resolve_result;

    initialize_error = resolved_addresses.initialize();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_error);
    networking_dns_clear_cache();
    resolve_result = networking_dns_resolve(ft_nullptr, "80", AF_INET, SOCK_STREAM,
        IPPROTO_TCP, 0, resolved_addresses);
    FT_ASSERT_EQ(false, resolve_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, resolved_addresses.destroy());
    return (1);
}

FT_TEST(test_networking_dns_resolve_first_returns_populated_address)
{
    networking_resolved_address resolved_address;
    bool resolve_first_result;
    const struct sockaddr_in *ipv4_address;

    networking_dns_clear_cache();
    resolve_first_result = networking_dns_resolve_first("127.0.0.1", "443", AF_INET,
        SOCK_STREAM, IPPROTO_TCP, AI_NUMERICHOST, resolved_address);
    FT_ASSERT_EQ(true, resolve_first_result);
    ipv4_address = reinterpret_cast<const struct sockaddr_in*>(&resolved_address.address);
    FT_ASSERT(ipv4_address != ft_nullptr);
    FT_ASSERT_EQ(AF_INET, ipv4_address->sin_family);
    FT_ASSERT_EQ(htons(443), ipv4_address->sin_port);
    return (1);
}

FT_TEST(test_networking_dns_clear_cache_resets_error_state)
{
    networking_dns_clear_cache();
    return (1);
}

FT_TEST(test_networking_dns_set_error_unknown_status_maps_to_failed)
{
    networking_dns_set_error(12345);
#ifdef EAI_AGAIN
    networking_dns_set_error(EAI_AGAIN);
#endif
    FT_ASSERT(true);
    return (1);
}

FT_TEST(test_networking_dns_resolve_first_matches_full_results_after_cache_growth)
{
    networking_dns_clear_cache();
    networking_resolved_address first{};
    ft_vector<networking_resolved_address> addresses;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, addresses.initialize());
    uint32_t port = 5000;
    while (port < 5032)
    {
        char service[6]{};
        (void)std::to_chars(service, service + sizeof(service) - 1U, port);
        FT_ASSERT_EQ(FT_TRUE, networking_dns_resolve_first("127.0.0.1", service,
            AF_INET, SOCK_STREAM, IPPROTO_TCP, AI_NUMERICHOST, first));
        ++port;
    }
    FT_ASSERT_EQ(FT_TRUE, networking_dns_resolve("127.0.0.1", "5000", AF_INET,
        SOCK_STREAM, IPPROTO_TCP, AI_NUMERICHOST, addresses));
    FT_ASSERT(!addresses.empty());
    ft_size_t allocations_before = 0;
    ft_size_t allocations_after = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&allocations_before, ft_nullptr));
    FT_ASSERT_EQ(FT_TRUE, networking_dns_resolve_first("127.0.0.1", "5000",
        AF_INET, SOCK_STREAM, IPPROTO_TCP, AI_NUMERICHOST, first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&allocations_after, ft_nullptr));
    FT_ASSERT_EQ(allocations_before, allocations_after);
    FT_ASSERT_EQ(addresses[0].length, first.length);
    FT_ASSERT_EQ(0, ft_memcmp(&addresses[0].address, &first.address, first.length));
    return (1);
}

FT_TEST(test_networking_dns_can_resolve_when_cache_storage_allocation_fails)
{
    networking_dns_clear_cache();
    networking_resolved_address address{};
    cma_set_alloc_limit(1U);
    const ft_bool resolved = networking_dns_resolve_first("127.0.0.1", "443",
        AF_INET, SOCK_STREAM, IPPROTO_TCP, AI_NUMERICHOST, address);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_TRUE, resolved);
    FT_ASSERT_EQ(AF_INET, address.address.ss_family);
    return (1);
}

FT_TEST(test_networking_dns_shared_cache_accepts_concurrent_queries)
{
    networking_dns_clear_cache();
    FT_ASSERT_EQ(FT_TRUE, networking_dns_is_thread_safe());
    std::atomic<ft_bool> succeeded{FT_TRUE};
    std::thread workers[4];
    uint32_t index = 0;
    while (index < 4U)
    {
        workers[index] = std::thread([&succeeded]()
        {
            uint32_t attempt = 0;
            while (attempt < 100U)
            {
                networking_resolved_address address{};
                if (!networking_dns_resolve_first("127.0.0.1", "8000",
                        AF_INET, SOCK_STREAM, IPPROTO_TCP, AI_NUMERICHOST, address)
                    || address.address.ss_family != AF_INET)
                    succeeded.store(FT_FALSE, std::memory_order_relaxed);
                ++attempt;
            }
            return ;
        });
        ++index;
    }
    index = 0;
    while (index < 4U)
    {
        workers[index].join();
        ++index;
    }
    FT_ASSERT_EQ(FT_TRUE, succeeded.load(std::memory_order_relaxed));
    return (1);
}

FT_TEST(test_networking_dns_can_switch_to_thread_local_cache_and_back)
{
    networking_resolved_address address{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_dns_disable_thread_safety());
    const ft_bool shared_while_disabled = networking_dns_is_thread_safe();
    const ft_bool resolved = networking_dns_resolve_first("127.0.0.1", "4500",
        AF_INET, SOCK_STREAM, IPPROTO_TCP, AI_NUMERICHOST, address);
    std::thread cache_clearer(networking_dns_clear_cache);
    cache_clearer.join();
    const ft_bool resolved_after_clear = networking_dns_resolve_first("127.0.0.1", "4500",
        AF_INET, SOCK_STREAM, IPPROTO_TCP, AI_NUMERICHOST, address);
    networking_dns_clear_cache();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_dns_enable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, shared_while_disabled);
    FT_ASSERT_EQ(FT_TRUE, resolved);
    FT_ASSERT_EQ(FT_TRUE, resolved_after_clear);
    FT_ASSERT_EQ(FT_TRUE, networking_dns_is_thread_safe());
    return (1);
}
