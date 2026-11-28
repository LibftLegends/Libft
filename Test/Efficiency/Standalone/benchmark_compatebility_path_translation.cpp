#include "../../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t ITERATIONS = 80000;
    const ft_bool portable = argument_count == 2 && ft_strcmp(arguments[1], "--portable") == 0;
    const char sample[] = "world\\overworld/region\\block_edits/";
    char source[512];
    source[0] = '/';
    ft_size_t index = 1;
    while (index + 1 < sizeof(source))
    {
        source[index] = sample[(index - 1) % (sizeof(sample) - 1)];
        ++index;
    }
    source[index] = '\0';
    uint32_t iteration = 0;
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        char *translated = ft_nullptr;
        int32_t result;
        if (portable)
            result = cmp_translate_path_to_portable(source, &translated);
        else
            result = cmp_translate_path_to_native(source, &translated);
        if (result != FT_ERR_SUCCESS)
            return (1);
        checksum = checksum * 31U + static_cast<uint8_t>(translated[iteration % 510U]);
        cma_free(translated);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("path %.3f us/op checksum=%" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}
