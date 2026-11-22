#include "../../../Modules/Networking/http2_client.hpp"
#include <chrono>
#include <cstdio>
#include <string>

#if NETWORKING_HAS_OPENSSL
int32_t main()
{
    constexpr uint32_t COUNT = 24;
    constexpr uint32_t ITERATIONS = 300;
    const std::string value(1024, 'v');
    ft_vector<http2_header_field> headers;
    ft_vector<http2_header_field> decoded;
    ft_string encoded;
    if (headers.initialize() != FT_ERR_SUCCESS || decoded.initialize() != FT_ERR_SUCCESS
        || encoded.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0;
    while (index < COUNT)
    {
        http2_header_field field;
        if (field.initialize() != FT_ERR_SUCCESS
            || !field.assign_from_buffers("x-bench", 7, value.data(), value.size())
            || headers.push_back(ft_move(field)) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        int32_t error_code = FT_ERR_SUCCESS;
        if (!http2_compress_headers(headers, encoded, error_code)
            || !http2_decompress_headers(encoded, decoded, error_code)
            || decoded.size() != COUNT)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("http2_header_codec us/op=%.3f\n", microseconds / ITERATIONS);
    return (0);
}
#else
int32_t main()
{
    std::fputs("The HTTP/2 header benchmark requires OpenSSL headers.\n", stderr);
    return (1);
}
#endif
