#include "../../../Modules/System_utils/system_utils.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 8192U;
    const char *text = "Mixed Case Text With Enough Bytes To Exceed Small String Storage";
    ft_string output;
    if (output.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    ft_size_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        int32_t order;
        if (su_locale_casefold(text, "C", output) != 0
            || su_locale_compare(output.c_str(), "z", "C", &order) != 0
            || order >= 0 || output.c_str()[0] != 'm')
            return (1);
        checksum += output.size();
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("case fold and compare %.2f ns/op, checksum %" PRIu64 "\n",
        elapsed / ITERATIONS, checksum);
    return (0);
}
