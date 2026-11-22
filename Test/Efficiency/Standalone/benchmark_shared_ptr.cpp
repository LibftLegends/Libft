#include "../../../Modules/Template/shared_ptr.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    constexpr uint32_t ITERATIONS = 8000000U;
    ft_sharedptr<uint32_t> source(new uint32_t(42U));
    ft_sharedptr<uint32_t> destination;
    if (destination.initialize(source) != FT_ERR_SUCCESS)
        return (1);
    const ft_sharedptr<uint32_t> *volatile source_slot = &source;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        destination = *source_slot;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (destination.use_count() != 2 || destination.get() != source.get())
        return (1);
    std::printf("shared_ptr_alias_assignment ns/op=%.2f\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}
