#include "../../../Modules/Networking/networking.hpp"
#include <charconv>
#include <chrono>
#include <cstdio>

static int32_t measure_cache_hit(const char *scenario, const char *service,
    uint32_t iterations)
{
    networking_resolved_address address{};
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0;
    while (index < iterations)
    {
        if (!networking_dns_resolve_first("127.0.0.1", service, AF_INET,
                SOCK_STREAM, IPPROTO_TCP, 0, address))
            return (1);
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (address.address.ss_family != AF_INET)
        return (1);
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("api_networking_dns_cache_%s us/op=%.5f\n", scenario, microseconds / iterations);
    return (0);
}

int main()
{
    constexpr uint32_t DESTINATIONS = 1024;
    networking_resolved_address address{};
    if (!networking_dns_resolve_first("127.0.0.1", "25565", AF_INET,
            SOCK_STREAM, IPPROTO_TCP, 0, address)
        || measure_cache_hit("single", "25565", 200000U) != 0)
        return (1);
    networking_dns_clear_cache();
    uint32_t index = 0;
    while (index < DESTINATIONS)
    {
        char service[6]{};
        (void)std::to_chars(service, service + sizeof(service) - 1U, 30000U + index);
        if (!networking_dns_resolve_first("127.0.0.1", service, AF_INET,
                SOCK_STREAM, IPPROTO_TCP, 0, address))
            return (1);
        ++index;
    }
    const int32_t result = measure_cache_hit("1024", "31023", 30000U);
    networking_dns_clear_cache();
    return (result);
}
