#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/Basic/utf8.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static volatile uintptr_t observed;

template <typename Operation>
static void benchmark_text_operation(const char *name, uint32_t length,
    uint32_t rounds, Operation operation)
{
    const auto start = std::chrono::steady_clock::now();
    uintptr_t result = 0U;
    uint32_t iteration = 0U;

    while (iteration < rounds)
    {
        result += operation(iteration);
        ++iteration;
    }
    observed = result;
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s length=%u ns/op=%.2f\n", name, length,
        elapsed / static_cast<double>(rounds));
    return ;
}

int main()
{
    constexpr uint32_t LENGTHS[] = {32U, 4096U, 65536U};
    constexpr uint32_t ROUNDS[] = {500000U, 3000U, 300U};
    uint32_t sample = 0U;

    while (sample < sizeof(LENGTHS) / sizeof(LENGTHS[0]))
    {
        const uint32_t length = LENGTHS[sample];
        const uint32_t rounds = ROUNDS[sample];
        std::string input(length, 'a');
        input.back() = 'b';
        const char *string = input.c_str();
        const char *volatile runtime_string = string;
        benchmark_text_operation("strlen", length, rounds, [&runtime_string](uint32_t iteration)
        {
            return (ft_strlen_size_t(runtime_string + (iteration & 1U)));
        });
        benchmark_text_operation("strchr", length, rounds, [string](uint32_t)
        {
            return (reinterpret_cast<uintptr_t>(ft_strchr(string, 'b')));
        });
        benchmark_text_operation("strrchr", length, rounds, [string](uint32_t)
        {
            return (reinterpret_cast<uintptr_t>(ft_strrchr(string, 'q')));
        });
        benchmark_text_operation("strstr", length, rounds, [string](uint32_t)
        {
            return (reinterpret_cast<uintptr_t>(ft_strstr(string, "aaaaab")));
        });
        benchmark_text_operation("str_contains", length, rounds, [string](uint32_t)
        {
            return (ft_str_contains(string, "aaaab"));
        });
        benchmark_text_operation("utf8_count", length, rounds, [string](uint32_t)
        {
            ft_size_t count = 0U;
            const int32_t status = ft_utf8_count(string, &count);
            return (count + static_cast<ft_size_t>(status));
        });
        benchmark_text_operation("utf8_validate", length, rounds, [string, length](uint32_t)
        {
            return (static_cast<uintptr_t>(ft_utf8_validate(string, length)));
        });
        ++sample;
    }
    return (0);
}
