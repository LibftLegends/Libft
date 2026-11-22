#include "../../../Modules/Networking/http2_client.hpp"
#include <chrono>
#include <cstdio>

#if NETWORKING_HAS_OPENSSL
int32_t main()
{
    constexpr uint32_t STREAMS = 4096;
    constexpr uint32_t ITERATIONS = 30;
    http2_stream_manager manager;
    if (manager.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t stream_identifier = 1;
    while (stream_identifier <= STREAMS)
    {
        if (!manager.open_stream(stream_identifier))
            return (1);
        ++stream_identifier;
    }
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        if (!manager.update_local_initial_window(30000U + iteration % 2U))
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (manager.get_local_window(STREAMS) != 30001U)
        return (1);
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("http2_stream_windows us/op=%.3f\n", microseconds / ITERATIONS);
    return (0);
}
#else
int32_t main()
{
    std::fputs("The HTTP/2 stream window benchmark requires OpenSSL headers.\n", stderr);
    return (1);
}
#endif
