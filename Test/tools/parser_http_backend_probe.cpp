#include "../../Modules/Parser/document_backend.hpp"
#include "../../Modules/Networking/http_client.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include <cstdio>
#include <string>

int main(int argument_count, char **arguments)
{
    if (argument_count != 3)
        return (1);
    const ft_bool use_ssl = arguments[2][0] == '1';
    const char *host = "127.0.0.1";
    if (use_ssl)
        host = "localhost";
    const char *port = arguments[1];
    ft_http_document_source source;
    ft_http_document_sink sink;
    ft_string output;
    if (source.initialize() != FT_ERR_SUCCESS || sink.initialize() != FT_ERR_SUCCESS
        || source.enable_thread_safety() != FT_ERR_SUCCESS)
        return (2);

    const char expected[] = {'a', '\0', 'b'};
    source.configure(host, "/binary", use_ssl, port);
    const int32_t get_status = source.read_all(output);
    if (get_status != FT_ERR_SUCCESS)
    {
        std::fprintf(stderr, "GET transport error %d\n", get_status);
        return (3);
    }
    if (output.size() != sizeof(expected) || ft_memcmp(output.c_str(), expected, sizeof(expected)) != 0)
        return (4);
    source.configure(host, "/missing", use_ssl, port);
    cma_set_alloc_limit(16384);
    const int32_t rejected_get = source.read_all(output);
    cma_set_alloc_limit(0);
    if (rejected_get != FT_ERR_INVALID_OPERATION || output.size() != sizeof(expected)
        || ft_memcmp(output.c_str(), expected, sizeof(expected)) != 0)
        return (5);
    source.configure(host, "/empty", use_ssl, port);
    if (source.read_all(output) != FT_ERR_SUCCESS || !output.empty())
        return (6);

    std::string payload(32768, 'x');
    payload[13] = '\0';
    sink.configure(host, "/post", use_ssl, port);
    cma_set_alloc_limit(16384);
    const int32_t post_status = sink.write_all(payload.data(), payload.size());
    cma_set_alloc_limit(0);
    if (post_status != FT_ERR_SUCCESS)
    {
        std::fprintf(stderr, "POST transport error %d\n", post_status);
        return (7);
    }
    sink.configure(host, "/missing", use_ssl, port);
    if (sink.write_all(expected, sizeof(expected)) != FT_ERR_INVALID_OPERATION)
        return (8);
    if (http_get(host, "/binary", output, use_ssl, port) != FT_ERR_SUCCESS
        || output.size() <= sizeof(expected) || ft_memcmp(output.c_str(), "HTTP/1.1", 8) != 0)
        return (9);
    ft_string legacy_body;
    if (legacy_body.initialize() != FT_ERR_SUCCESS
        || legacy_body.assign(expected, sizeof(expected)) != FT_ERR_SUCCESS
        || http_post(host, "/legacy", legacy_body, output, use_ssl, port) != FT_ERR_SUCCESS
        || output.size() < 12 || ft_memcmp(output.c_str(), "HTTP/1.1 404", 12) != 0)
        return (10);
    http_client_pool_flush();
    return (0);
}
