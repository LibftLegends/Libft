#include "../../../Modules/CPP_class/class_big_number.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>

static int32_t measure_relations(const char *label, uint32_t digits, ft_bool equal)
{
    constexpr uint32_t ITERATIONS = 200000U;
    char left_digits[1025];
    char right_digits[1025];
    std::memset(left_digits, '7', digits);
    left_digits[digits] = '\0';
    std::memcpy(right_digits, left_digits, digits + 1U);
    if (equal == FT_FALSE)
        right_digits[digits - 1U] = '8';
    ft_big_number left;
    ft_big_number right;
    if (left.initialize() != FT_ERR_SUCCESS || right.initialize() != FT_ERR_SUCCESS)
        return (1);
    left.assign(left_digits);
    right.assign(right_digits);
    uint32_t checksum = 0U;
    uint32_t index = 0U;
    const auto started = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        checksum += left <= right;
        checksum += 2U * (left >= right);
        checksum += 4U * (left != right);
        index += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - started).count();
    uint32_t expected_mask = 3U;
    if (equal == FT_FALSE)
        expected_mask = 5U;
    if (checksum != ITERATIONS * expected_mask)
        return (1);
    std::printf("%s: %.2f ns/relation, checksum %u\n",
        label, elapsed / (3.0 * ITERATIONS), checksum);
    return (0);
}

int32_t main()
{
    if (measure_relations("equal short operands", 20U, FT_TRUE)
        || measure_relations("equal wide operands", 1024U, FT_TRUE))
        return (1);
    return (measure_relations("wide late mismatch", 1024U, FT_FALSE));
}
