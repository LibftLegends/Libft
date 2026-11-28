#include "../../../Modules/RNG/rng.hpp"
#include "../../../Modules/RNG/rng_internal.hpp"
#include <chrono>
#include <cstdio>

template<typename Sampler>
static void measure_distribution(const char *name, const Sampler &sample)
{
    constexpr uint32_t ITERATIONS = 350000U;
    ft_seed_random_engine(78213U);
    double checksum = 0.0;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        checksum += sample();
        index += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns per sample, checksum %a, next draw %d\n",
        name, elapsed_ns / ITERATIONS, checksum, ft_random_int());
    return ;
}

int32_t main()
{
    if (rng_enable_random_engine_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    measure_distribution("gamma", []()
    {
        return (ft_random_gamma(0.4f, 1.3f));
    });
    measure_distribution("beta", []()
    {
        return (ft_random_beta(2.1f, 4.6f));
    });
    measure_distribution("chi-squared", []()
    {
        return (ft_random_chi_squared(5.5f));
    });
    if (rng_disable_random_engine_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    return (0);
}
