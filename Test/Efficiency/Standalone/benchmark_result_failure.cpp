#include "../../../Modules/Template/result.hpp"
#include <chrono>
#include <cstdio>

struct large_result_value
{
    uint8_t bytes[65536U];
};

int32_t main()
{
    using result_type = ft_result<large_result_value>;
    constexpr uint32_t ITERATIONS = 200000U;
    // Call through a pointer to measure constructing a result that escapes the factory.
    auto *volatile make_failure = &result_type::failure;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        result_type *result = make_failure(FT_ERR_NOT_FOUND);
        if (result == ft_nullptr)
            return (1);
        const ft_bool valid = result->get_error() == FT_ERR_NOT_FOUND
            && result->get_value() == ft_nullptr;
        delete result;
        if (!valid)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("large_result_failure ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}
