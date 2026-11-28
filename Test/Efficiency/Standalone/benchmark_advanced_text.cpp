#include "../../../Modules/Advanced/advanced.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/CPP_class/class_string.hpp"
#include <chrono>
#include <cstdio>
#include <string>

template <typename Operation>
static int32_t benchmark_operation(const char *name, uint32_t rounds, Operation operation)
{
    const auto started = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < rounds)
    {
        if (operation() == FT_FALSE)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    std::printf("%s ns/op=%.2f\n", name, elapsed / static_cast<double>(rounds));
    return (0);
}

int main()
{
    const std::string input(65536U, 'A');
    std::string output(input.size() + 1U, '\0');
    const std::u16string wide(16384U, u'A');
    if (benchmark_operation("strdup_64_kib", 3000U, [&]()
    {
        char *result = adv_strdup(input.c_str());
        const ft_bool valid = static_cast<ft_bool>(
                result != ft_nullptr && result[input.size() - 1U] == 'A');
        cma_free(result);
        return (valid);
    }) != 0 || benchmark_operation("substr_16_from_64_kib", 10000U, [&]()
    {
        char *result = adv_substr(input.c_str(), 16U, 16U);
        const ft_bool valid = static_cast<ft_bool>(
                result != ft_nullptr && result[15U] == 'A' && result[16U] == '\0');
        cma_free(result);
        return (valid);
    }) != 0 || benchmark_operation("join_variadic_4", 25000U, []()
    {
        char *result = adv_strjoin_multiple(4, "alpha", "beta", "gamma", "delta");
        const ft_bool valid = static_cast<ft_bool>(
                result != ft_nullptr && result[18U] == 'a');
        cma_free(result);
        return (valid);
    }) != 0 || benchmark_operation("utf8_ascii_transform_64_kib", 1500U, [&]()
    {
        return (static_cast<ft_bool>(ft_utf8_transform(input.c_str(),
                    ft_platform_cast<ft_size_t>(input.size()), &output[0U],
                    ft_platform_cast<ft_size_t>(output.size()), ft_utf8_case_ascii_lower)
                == FT_ERR_SUCCESS && output[input.size() - 1U] == 'a'));
    }) != 0 || benchmark_operation("utf16_to_utf8_16384_ascii_units", 1500U, [&]()
    {
        ft_string *result = adv_utf16_to_utf8(wide.c_str(),
                ft_platform_cast<ft_size_t>(wide.size()));
        const ft_bool valid = static_cast<ft_bool>(
                result != ft_nullptr && result->size() == wide.size());
        delete result;
        return (valid);
    }) != 0 || benchmark_operation("utf8_to_utf16_64_kib_ascii", 1000U, [&]()
    {
        ft_size_t length = 0U;
        char16_t *result = ft_utf8_to_utf16(input.c_str(),
                ft_platform_cast<ft_size_t>(input.size()), &length);
        const ft_bool valid = static_cast<ft_bool>(
                result != ft_nullptr && length == input.size()
                && result[length - 1U] == u'A');
        cma_free(result);
        return (valid);
    }) != 0)
        return (1);
    return (0);
}
