#include "math.hpp"
#include <array>
#include <cstddef>

static constexpr uint32_t MAX_INT32_FACTORIAL_INPUT = 12U;
static constexpr uint32_t MAX_INT64_FACTORIAL_INPUT = 20U;

static constexpr std::array<int64_t, MAX_INT64_FACTORIAL_INPUT + 1U>
    math_factorial_values()
{
    std::array<int64_t, MAX_INT64_FACTORIAL_INPUT + 1U> values{};
    values[0] = 1;
    uint32_t index = 1U;
    while (index <= MAX_INT64_FACTORIAL_INPUT)
    {
        values[index] = values[index - 1U] * static_cast<int64_t>(index);
        index += 1U;
    }
    return (values);
}

static constexpr auto FACTORIAL_VALUES = math_factorial_values();
static_assert(FACTORIAL_VALUES[MAX_INT32_FACTORIAL_INPUT] <= INT32_MAX
    && FACTORIAL_VALUES[MAX_INT32_FACTORIAL_INPUT + 1U] > INT32_MAX);
static_assert(FACTORIAL_VALUES[MAX_INT64_FACTORIAL_INPUT]
    > INT64_MAX / (MAX_INT64_FACTORIAL_INPUT + 1U));

int32_t math_factorial(int32_t number)
{
    const uint32_t index = static_cast<uint32_t>(number);
    if (index > MAX_INT32_FACTORIAL_INPUT)
        return (0);
    return (static_cast<int32_t>(FACTORIAL_VALUES[index]));
}

int64_t math_factorial(int64_t number)
{
    const uint64_t index = static_cast<uint64_t>(number);
    if (index > MAX_INT64_FACTORIAL_INPUT)
        return (0);
    return (FACTORIAL_VALUES[static_cast<std::size_t>(index)]);
}
