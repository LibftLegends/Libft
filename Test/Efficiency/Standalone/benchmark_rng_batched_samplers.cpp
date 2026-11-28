#include "../../../Modules/RNG/rng.hpp"
#include "../../../Modules/RNG/rng_internal.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cstdio>

template<typename Sampler>
static void measure_sampler(const char *name, uint32_t iterations, const Sampler &sample)
{
    ft_seed_random_engine(187192U);
    double checksum = 0.0;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        checksum += sample();
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns per sample, checksum %a, next draw %d\n",
        name, elapsed_ns / iterations, checksum, ft_random_int());
    return ;
}

int main()
{
    if (rng_enable_random_engine_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    measure_sampler("normal", 280000U, ft_random_normal);
    measure_sampler("binomial 1", 240000U, []()
    {
        return (ft_random_binomial(1, 0.36));
    });
    measure_sampler("binomial 257", 16000U, []()
    {
        return (ft_random_binomial(257, 0.36));
    });
    measure_sampler("exponential", 230000U, []()
    {
        return (ft_random_exponential(2.0f));
    });
    measure_sampler("geometric", 14000U, []()
    {
        return (ft_random_geometric(0.004));
    });
    measure_sampler("poisson", 200000U, []()
    {
        return (ft_random_poisson(10.0));
    });
    measure_sampler("poisson 128", 200000U, []()
    {
        return (ft_random_poisson(128.0));
    });
    measure_sampler("poisson 1200", 200000U, []()
    {
        return (ft_random_poisson(1200.0));
    });
    if (rng_disable_random_engine_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    return (0);
}
