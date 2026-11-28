#include "../../../Modules/URI/uri.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>
#include <string>

template <typename Operation>
static void benchmark_operation(const char *label, uint32_t iterations,
    Operation operation)
{
    uint32_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        checksum += operation();
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %u\n",
        label, elapsed / iterations, checksum);
    return ;
}

static uint32_t normalized_checksum(const char *input, ft_size_t size)
{
    char *output = uri_normalize(input, size);
    if (output == ft_nullptr)
        return (static_cast<uint32_t>(uri_get_error()));
    uint32_t checksum = 0U;
    const char *cursor = output;
    while (*cursor != '\0')
    {
        checksum = checksum * 31U + static_cast<uint8_t>(*cursor);
        cursor += 1U;
    }
    cma_free(output);
    return (checksum);
}

static int32_t check_path_grammar()
{
    const char *tokens[] = {"", ".", "..", "dir", "%2e", "%2e%2e", "%2f",
        "%2f.", "%2b", "%zz"};
    constexpr uint32_t TOKEN_COUNT = sizeof(tokens) / sizeof(tokens[0]);
    constexpr uint32_t COMBINATIONS = TOKEN_COUNT * TOKEN_COUNT * TOKEN_COUNT * TOKEN_COUNT;
    uint32_t combination = 0U;
    uint32_t checksum = 0U;
    while (combination < COMBINATIONS)
    {
        uint32_t digits = combination;
        std::string path = "HtTp://HOST";
        uint32_t segment = 0U;
        while (segment < 4U)
        {
            path += '/';
            path += tokens[digits % TOKEN_COUNT];
            digits /= TOKEN_COUNT;
            segment += 1U;
        }
        path += "?x=%ef#frag";
        checksum = checksum * 31U + normalized_checksum(path.data(), path.size());
        combination += 1U;
    }
    std::printf("%u paths, checksum %u\n", COMBINATIONS, checksum);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--check-paths") == 0)
        return (check_path_grammar());
    if (argument_count != 1)
        return (1);
    std::string query;
    uint32_t pair = 0U;
    while (pair < 64U)
    {
        query += "field%32" + std::to_string(pair) + "=several%20bytes&";
        pair += 1U;
    }
    query += "a%20b=last%20value";
    benchmark_operation("query existence", 1024U, [&query]()
    {
        return (uri_query_has_key(query.data(), query.size(), "a b", 3U));
    });
    benchmark_operation("query decode", 1024U, [&query]()
    {
        char *value = uri_query_get_value(query.data(), query.size(), "a b", 3U);
        const uint32_t checksum = static_cast<uint32_t>(ft_strlen_size_t(value));
        cma_free(value);
        return (checksum);
    });
    const char *input = "HtTp://User@[ABCD::1234]:80/a%2fb/../other/%7e/final?abc#frag";
    benchmark_operation("normalize URI", 4096U, [input]()
    {
        return (normalized_checksum(input, ft_strlen_size_t(input)));
    });
    return (0);
}
