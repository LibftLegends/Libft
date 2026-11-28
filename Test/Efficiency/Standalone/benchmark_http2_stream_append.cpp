#include "../../../Modules/Networking/http2_client.hpp"
#include <chrono>
#include <cstdio>

#if NETWORKING_HAS_OPENSSL
int main()
{
    constexpr char PAYLOAD[16384]{};
    constexpr uint32_t ITERATIONS = 500;
    std::chrono::steady_clock::duration elapsed{};
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        http2_stream_manager manager;
        if (manager.initialize() != FT_ERR_SUCCESS || !manager.open_stream(1U))
            return (1);
        const auto start = std::chrono::steady_clock::now();
        const ft_bool appended = manager.append_data(1U, PAYLOAD, sizeof(PAYLOAD));
        elapsed += std::chrono::steady_clock::now() - start;
        if (!appended || manager.get_local_window(1U) != 65535U - sizeof(PAYLOAD))
            return (1);
        ++iteration;
    }
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("http2_stream_append us/op=%.3f\n", microseconds / ITERATIONS);
    return (0);
}
#else
int main()
{
    std::fputs("The HTTP/2 stream benchmark requires OpenSSL headers.\n", stderr);
    return (1);
}
#endif
