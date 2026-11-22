#include "../../../Modules/API/api_http_internal.hpp"
#include <chrono>
#include <cstdio>
#include <string>

int32_t main()
{
    constexpr uint32_t CHUNKS = 1024;
    constexpr uint32_t ITERATIONS = 500;
    std::string encoded;
    uint32_t chunk_index = 0;
    while (chunk_index < CHUNKS)
    {
        encoded += "10\r\nabcdefghijklmnop\r\n";
        ++chunk_index;
    }
    encoded += "0\r\nX: ok\r\n\r\n";
    ft_string buffer;
    if (buffer.initialize() != FT_ERR_SUCCESS)
        return (1);
    std::chrono::steady_clock::duration elapsed{};
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        if (buffer.assign(encoded.data(), encoded.size()) != FT_ERR_SUCCESS)
            return (1);
        int64_t remaining = -1;
        ft_bool trailers = FT_FALSE;
        ft_bool final = FT_FALSE;
        int32_t error = FT_ERR_SUCCESS;
        const auto start = std::chrono::steady_clock::now();
        const ft_bool result = api_http_stream_process_chunked_buffer(buffer,
            remaining, trailers, final, ft_nullptr, error);
        elapsed += std::chrono::steady_clock::now() - start;
        if (!result || !final || buffer.size() != 0)
            return (1);
        ++iteration;
    }
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("api_http_stream_chunked us/op=%.3f\n", microseconds / ITERATIONS);
    return (0);
}
