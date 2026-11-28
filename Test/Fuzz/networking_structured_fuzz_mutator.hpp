#ifndef NETWORKING_STRUCTURED_FUZZ_MUTATOR_HPP
#define NETWORKING_STRUCTURED_FUZZ_MUTATOR_HPP

#include "../../Modules/Basic/class_nullptr.hpp"
#include <cstddef>
#include <cstdint>

static size_t networking_structured_fuzz_mutate(uint8_t *data, size_t size,
    size_t maximum_size, unsigned int seed, size_t structure_limit) noexcept
{
    uint32_t state;
    size_t mutation_count;

    if (data == ft_nullptr || maximum_size == 0U)
        return (0U);
    if (structure_limit > maximum_size)
        structure_limit = maximum_size;
    if (size > structure_limit)
        size = structure_limit;
    if (size == 0U)
    {
        size = 1U;
        data[0] = static_cast<uint8_t>(seed);
    }
    state = static_cast<uint32_t>(seed);
    if (state == 0U)
        state = 0x9e3779b9U;
    mutation_count = 1U + static_cast<size_t>(seed % 8U);
    while (mutation_count > 0U)
    {
        size_t index;

        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        index = static_cast<size_t>(state) % size;
        data[index] ^= static_cast<uint8_t>((state >> 24U) | 1U);
        mutation_count -= 1U;
    }
    if (size < structure_limit && (seed & 3U) == 0U)
    {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        data[size] = static_cast<uint8_t>(state);
        size += 1U;
    }
    return (size);
}

#endif
