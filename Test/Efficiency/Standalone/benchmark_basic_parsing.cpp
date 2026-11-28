#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

static volatile uint64_t observed;

template <typename Operation>
static void benchmark_parsing_operation(const char *name, uint32_t rounds,
    Operation operation)
{
    const auto started = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    uint64_t result = 0U;

    while (iteration < rounds)
    {
        result += operation(iteration);
        ++iteration;
    }
    observed = result;
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    std::printf("%s ns/op=%.2f\n", name, elapsed / static_cast<double>(rounds));
    return ;
}

int main()
{
    constexpr uint32_t PARSE_ROUNDS = 500000U;
    const char *const signed_values[] = {"1234567890123456789", "-1234567890123456789"};
    const char *const unsigned_values[] = {"11446744073709551615", "18446744073709551615"};
    const char *const decimal_values[] = {"1234567890", "2147483647"};
    const char *const token_inputs[] = {"alpha,beta,gamma,delta", "alpha, beta;gamma:delta"};
    const char *const delimiters[] = {",", ", ;:"};
    const std::string input(65536U, 'a');
    std::string copy(input.size() + 1U, '\0');

    benchmark_parsing_operation("strtol", PARSE_ROUNDS, [&](uint32_t index)
    {
        return (static_cast<uint64_t>(ft_strtol(signed_values[index & 1U], ft_nullptr, 10)));
    });
    benchmark_parsing_operation("strtoul", PARSE_ROUNDS, [&](uint32_t index)
    {
        return (ft_strtoul(unsigned_values[index & 1U], ft_nullptr, 10));
    });
    benchmark_parsing_operation("parse_int64", PARSE_ROUNDS, [&](uint32_t index)
    {
        int64_t value = 0;
        const int32_t status = ft_parse_int64(signed_values[index & 1U], ft_nullptr, &value);
        return (static_cast<uint64_t>(value) + static_cast<uint64_t>(status));
    });
    benchmark_parsing_operation("parse_uint32", PARSE_ROUNDS, [&](uint32_t index)
    {
        uint32_t value = 0U;
        const int32_t status = ft_parse_uint32(decimal_values[index & 1U], ft_nullptr, &value);
        return (static_cast<uint64_t>(value) + static_cast<uint64_t>(status));
    });
    benchmark_parsing_operation("validate_int", PARSE_ROUNDS, [&](uint32_t index)
    {
        return (static_cast<uint64_t>(ft_validate_int(decimal_values[index & 1U])));
    });
    benchmark_parsing_operation("strtok_four_tokens", PARSE_ROUNDS / 4U, [&](uint32_t index)
    {
        char buffer[32];
        uint64_t token_count = 0U;
        const uint32_t sample = index & 1U;
        std::strcpy(buffer, token_inputs[sample]);
        char *token = ft_strtok(buffer, delimiters[sample]);
        while (token != ft_nullptr)
        {
            ++token_count;
            token = ft_strtok(ft_nullptr, delimiters[sample]);
        }
        return (token_count);
    });
    benchmark_parsing_operation("strlcpy_64_kib", 10000U, [&](uint32_t)
    {
        return (ft_strlcpy(&copy[0U], input.c_str(),
            ft_platform_cast<ft_size_t>(copy.size())));
    });
    copy[input.size() - 1U] = 'b';
    benchmark_parsing_operation("strnstr_late_64_kib", 1000U, [&](uint32_t)
    {
        return (reinterpret_cast<uintptr_t>(ft_strnstr(copy.c_str(), "aaaaab", input.size())));
    });
    benchmark_parsing_operation("strnstr_early_64_kib", PARSE_ROUNDS, [&](uint32_t)
    {
        return (reinterpret_cast<uintptr_t>(ft_strnstr(input.c_str(), "aaaaaa", input.size())));
    });
    return (0);
}
