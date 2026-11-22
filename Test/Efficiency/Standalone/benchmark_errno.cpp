#include "../../../Modules/Errno/errno.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cstdio>

using benchmark_clock = std::chrono::steady_clock;
static volatile uintptr_t observed;

static void print_average(const char *name, benchmark_clock::time_point started,
    uint32_t iterations)
{
    const double elapsed = std::chrono::duration<double, std::nano>(
        benchmark_clock::now() - started).count();
    std::printf("%s ns/op=%.2f\n", name, elapsed / static_cast<double>(iterations));
    return ;
}

static void benchmark_lookup(const char *name, const int32_t *codes, uint32_t count)
{
    constexpr uint32_t ITERATIONS = 2000000U;
    uintptr_t result = 0U;
    uint32_t iteration = 0U;
    const auto started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        result += reinterpret_cast<uintptr_t>(ft_strerror(codes[iteration % count]));
        ++iteration;
    }
    observed = result;
    print_average(name, started, ITERATIONS);
    return ;
}

static int32_t benchmark_reporting()
{
    constexpr uint32_t ITERATIONS = 20000U;
#if defined(_WIN32)
    const char *destination = "NUL";
#else
    const char *destination = "/dev/null";
#endif
    if (std::freopen(destination, "w", stderr) == ft_nullptr)
        return (1);
    if (std::setvbuf(stderr, ft_nullptr, _IONBF, 0U) != 0)
        return (1);
    uint32_t iteration = 0U;
    const auto started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        if (ft_perror("could not load resource", FT_ERR_NOT_FOUND) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    print_average("perror_prefixed_unbuffered", started, ITERATIONS);
    return (0);
}

int main()
{
    const int32_t dense_codes[] = {FT_ERR_SUCCESS, FT_ERR_NO_MEMORY,
        FT_ERR_INVALID_ARGUMENT, FT_ERR_SYS_MUTEX_UNLOCK_FAILED,
        FT_ERR_RWLOCK_READER_CAPACITY, FT_ERR_SOCKET_RESOLVE_OVERFLOW};
    const int32_t sparse_codes[] = {FT_ERR_HTTP_PROTOCOL_MISMATCH,
        FT_ERR_API_CIRCUIT_OPEN, 1, -1000000};
    benchmark_lookup("strerror_dense", dense_codes,
        sizeof(dense_codes) / sizeof(dense_codes[0]));
    benchmark_lookup("strerror_sparse_or_unknown", sparse_codes,
        sizeof(sparse_codes) / sizeof(sparse_codes[0]));
    return (benchmark_reporting());
}
