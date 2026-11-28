#include "../../../Modules/CPP_class/bitset.hpp"
#include <chrono>
#include <cstdio>

static int32_t benchmark_bitset(ft_bool copy_bits)
{
    constexpr ft_size_t BIT_COUNT = 32U * 1024U * 1024U + 1;
    constexpr int32_t COPY_COUNT = 128;
    ft_bitset source;
    ft_bitset destination;

    if (source.initialize(BIT_COUNT) != FT_ERR_SUCCESS)
        return (1);
    source.set(0);
    source.set(BIT_COUNT - 1);
    const auto start = std::chrono::steady_clock::now();
    int32_t copy_index = 0;
    int32_t checksum = 0;
    while (copy_index < COPY_COUNT)
    {
        if (copy_bits == FT_TRUE)
        {
            if (destination.initialize(source) != FT_ERR_SUCCESS)
                return (1);
        }
        else
        {
            if (destination.destroy() != FT_ERR_SUCCESS
                || destination.initialize(BIT_COUNT) != FT_ERR_SUCCESS)
                return (1);
        }
        checksum += destination.test(0) + destination.test(BIT_COUNT - 1);
        ++copy_index;
    }
    const std::chrono::duration<double, std::micro> elapsed =
        std::chrono::steady_clock::now() - start;
    if (copy_bits == FT_TRUE)
    {
        std::printf("bitset-copy checksum %d %.3f us/operation\n",
            checksum, elapsed.count() / COPY_COUNT);
        return (checksum != COPY_COUNT * 2);
    }
    std::printf("bitset-initialize checksum %d %.3f us/operation\n",
        checksum, elapsed.count() / COPY_COUNT);
    return (checksum != 0);
}

int main()
{
    if (benchmark_bitset(FT_FALSE) != 0)
        return (1);
    return (benchmark_bitset(FT_TRUE));
}
