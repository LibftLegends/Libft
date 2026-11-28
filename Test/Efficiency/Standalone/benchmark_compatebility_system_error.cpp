#include "../../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    uint64_t checksum = 0;
    if (argument_count == 2 && ft_strcmp(arguments[1], "--range") == 0)
    {
        int32_t error = -2048;
        while (error <= 12600)
        {
            checksum = checksum * 31U + static_cast<uint32_t>(cmp_map_system_error_to_ft(error));
            ++error;
        }
        std::printf("range checksum=%" PRIu64 "\n", checksum);
        return (0);
    }

    constexpr uint32_t ITERATIONS = 10000000U;
    const int32_t errors[] = {0, EACCES, EPERM, EINVAL, ENOMEM, EAGAIN,
#if defined(_WIN32) || defined(_WIN64)
        ERROR_PATH_NOT_FOUND, ERROR_DISK_FULL, ERROR_INVALID_NAME,
        ERROR_NO_SYSTEM_RESOURCES, WSAEWOULDBLOCK, WSAECONNRESET, WSAECONNREFUSED,
        WSAHOST_NOT_FOUND, WSANO_DATA,
#endif
        29000};
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0;
    while (index < ITERATIONS)
    {
        const int32_t error = errors[index % (sizeof(errors) / sizeof(errors[0]))];
        checksum = checksum * 31U + static_cast<uint32_t>(cmp_map_system_error_to_ft(error));
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("error %.3f ns/op checksum=%" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}
