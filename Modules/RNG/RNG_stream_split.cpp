#include "rng.hpp"
#include "../Errno/errno.hpp"

static uint64_t rng_splitmix64_next(uint64_t *state)
{
    *state += 0x9E3779B97F4A7C15ULL;
    uint64_t result = *state;
    result = (result ^ (result >> 30)) * 0xBF58476D1CE4E5B9ULL;
    result = (result ^ (result >> 27)) * 0x94D049BB133111EBULL;
    result = result ^ (result >> 31);
    return (result);
}

static uint64_t rng_initialize_state(uint64_t base_seed, uint64_t stream_identifier)
{
    uint64_t state = base_seed ^ 0x9E3779B97F4A7C15ULL;
    state ^= stream_identifier * 0xBF58476D1CE4E5B9ULL;
    return (state);
}

int32_t rng_stream_seed(uint64_t base_seed, uint64_t stream_identifier, uint64_t *stream_seed)
{
    if (stream_seed == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    uint64_t state = rng_initialize_state(base_seed, stream_identifier);
    *stream_seed = rng_splitmix64_next(&state);
    return (FT_ERR_SUCCESS);
}

int32_t rng_stream_seed_sequence(uint64_t base_seed, uint64_t stream_identifier, uint32_t *buffer, ft_size_t count)
{
    if (buffer == ft_nullptr && count > 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (count == 0)
        return (FT_ERR_SUCCESS);
    uint64_t state = rng_initialize_state(base_seed, stream_identifier);
    ft_size_t index = 0;
    const ft_size_t paired_count = count - (count & 1U);
    while (index < paired_count)
    {
        const uint64_t mixed_value = rng_splitmix64_next(&state);
        buffer[index] = static_cast<uint32_t>(mixed_value);
        buffer[index + 1U] = static_cast<uint32_t>(mixed_value >> 32U);
        index += 2U;
    }
    if (index < count)
        buffer[index] = static_cast<uint32_t>(rng_splitmix64_next(&state));
    return (FT_ERR_SUCCESS);
}

int32_t rng_stream_seed_from_string(const char *seed_string, uint64_t stream_identifier, uint64_t *stream_seed)
{
    if (seed_string == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    uint32_t base_seed_32 = ft_random_seed(seed_string);
    return (rng_stream_seed(static_cast<uint64_t>(base_seed_32), stream_identifier, stream_seed));
}

int32_t rng_stream_seed_sequence_from_string(const char *seed_string, uint64_t stream_identifier, uint32_t *buffer, ft_size_t count)
{
    if (seed_string == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    uint32_t base_seed_32 = ft_random_seed(seed_string);
    return (rng_stream_seed_sequence(static_cast<uint64_t>(base_seed_32), stream_identifier, buffer, count));
}
