#include "../../../Modules/API/api_http_common.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static int32_t measure_chunk_scan(const std::string &body, ft_bool resume)
{
    constexpr ft_size_t READ_SIZE = 256;
    constexpr uint32_t ITERATIONS = 200;
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        api_http_chunk_scan scan;
        ft_size_t available = 0;
        ft_bool complete = FT_FALSE;
        ft_size_t consumed = 0;
        while (available < body.size() && !complete)
        {
            const ft_size_t remaining = body.size() - available;
            if (remaining < READ_SIZE)
                available += remaining;
            else
                available += READ_SIZE;
            if (resume)
            {
                complete = api_http_chunked_body_complete(body.data(), available, scan);
                consumed = scan.position;
            }
            else
                complete = api_http_chunked_body_complete(body.data(), available, consumed);
        }
        if (!complete || consumed != body.size())
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double microseconds = std::chrono::duration<double, std::micro>(elapsed).count();
    std::printf("api_http_chunk_scan resume=%u us/op=%.3f\n",
        static_cast<uint32_t>(resume), microseconds / ITERATIONS);
    return (0);
}

int main()
{
    constexpr uint32_t CHUNKS = 4096;
    std::string body;
    uint32_t chunk_index = 0;
    while (chunk_index < CHUNKS)
    {
        body += "10\r\nabcdefghijklmnop\r\n";
        ++chunk_index;
    }
    body += "0\r\nX: ok\r\n\r\n";
    if (measure_chunk_scan(body, FT_FALSE) != 0)
        return (1);
    return (measure_chunk_scan(body, FT_TRUE));
}
