#include "../../../Modules/Template/string_view.hpp"
#include <chrono>
#include <cstdio>
#include <string>

static int32_t benchmark_compare(const char *name, const ft_string_view<char> &first,
    const ft_string_view<char> &second, int32_t expected)
{
    constexpr uint32_t ROUNDS = 20000U;
    uint32_t index = 0U;
    int64_t result_sum = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ROUNDS)
    {
        result_sum += first.compare(second);
        ++index;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (result_sum != static_cast<int64_t>(expected) * ROUNDS)
        return (1);
    std::printf("%s ns/op=%.2f\n", name,
        std::chrono::duration<double, std::nano>(elapsed).count() / ROUNDS);
    return (0);
}

int main()
{
    const std::string text(16384U, 'a');
    std::string comparison = text;
    ft_string_view<char> first(text.c_str());
    ft_string_view<char> second(comparison.c_str());
    if (benchmark_compare("string_view_equal", first, second, 0) != 0)
        return (1);
    comparison.back() = 'b';
    return (benchmark_compare("string_view_late_mismatch", first, second, -1));
}
