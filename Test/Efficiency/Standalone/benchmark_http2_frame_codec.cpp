#include "../../../Modules/Networking/http2_client.hpp"
#include <chrono>
#include <cstdio>

#if NETWORKING_HAS_OPENSSL
int main()
{
    constexpr ft_size_t PAYLOAD_SIZE = 16384;
    constexpr uint32_t ITERATIONS = 750;
    char payload[PAYLOAD_SIZE]{};
    http2_frame outgoing;
    http2_frame incoming;
    ft_string encoded;
    if (outgoing.initialize() != FT_ERR_SUCCESS
        || incoming.initialize() != FT_ERR_SUCCESS
        || encoded.initialize() != FT_ERR_SUCCESS
        || !outgoing.set_payload_from_buffer(payload, PAYLOAD_SIZE))
        return (1);
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        ft_size_t offset = 0;
        int32_t error_code = FT_ERR_SUCCESS;
        if (!http2_encode_frame(outgoing, encoded, error_code)
            || !http2_decode_frame(reinterpret_cast<const unsigned char *>(encoded.c_str()),
                encoded.size(), offset, incoming, error_code)
            || offset != PAYLOAD_SIZE + 9U)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("http2_frame_codec us/op=%.3f\n", microseconds / ITERATIONS);
    return (0);
}
#else
int main()
{
    std::fputs("The HTTP/2 codec benchmark requires OpenSSL headers.\n", stderr);
    return (1);
}
#endif
