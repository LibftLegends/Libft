#include "../../../Modules/Math/math.hpp"
#include <chrono>
#include <cstdio>

static void benchmark_operation(const char *label,
    double (*operation)(const double *, int32_t), const double *values,
    int32_t size, uint32_t iterations)
{
    double checksum = 0.0;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        checksum += operation(values, size);
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %.17g\n",
        label, elapsed / iterations, checksum);
    return ;
}

int main()
{
    constexpr int32_t LARGE_SIZE = 4096;
    constexpr int32_t SMALL_SIZE = 32;
    double ordered[LARGE_SIZE];
    double shuffled[LARGE_SIZE];
    int32_t index = 0;
    while (index < LARGE_SIZE)
    {
        ordered[index] = static_cast<double>(index / 3);
        const int32_t shuffled_index = (index * 4093 + 37) % LARGE_SIZE;
        shuffled[shuffled_index] = ordered[index];
        index += 1;
    }
    benchmark_operation("median small", ft_median, shuffled, SMALL_SIZE, 32768U);
    benchmark_operation("mode small", ft_mode, shuffled, SMALL_SIZE, 32768U);
    benchmark_operation("median unsorted", ft_median, shuffled, LARGE_SIZE, 16U);
    benchmark_operation("mode unsorted", ft_mode, shuffled, LARGE_SIZE, 16U);
    benchmark_operation("median ordered", ft_median, ordered, LARGE_SIZE, 16U);
    benchmark_operation("mode ordered", ft_mode, ordered, LARGE_SIZE, 16U);
    return (0);
}
