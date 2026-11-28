#include "rng.hpp"
#include "../Errno/errno.hpp"
#include <random>
#include <exception>

static int32_t rng_fill_from_random_device(unsigned char *buffer, ft_size_t length)
{
    ft_size_t offset;

    try
    {
        std::random_device random_device;
        offset = 0;
        while (offset < length)
        {
            uint32_t random_value = random_device();
            ft_size_t byte_index = 0;
            while (byte_index < sizeof(uint32_t) && offset < length)
            {
                uint32_t shifted_value = random_value >> (byte_index * 8);
                buffer[offset] = static_cast<unsigned char>(shifted_value & 0xFFu);
                offset++;
                byte_index++;
            }
        }
    }
    catch (const std::exception &)
    {
        return (FT_ERR_INTERNAL);
    }
    return (FT_ERR_SUCCESS);
}

int32_t rng_secure_bytes_with_fallback(unsigned char *buffer, ft_size_t length, int32_t *fallback_used)
{
    if (fallback_used != ft_nullptr)
        *fallback_used = 0;
    if (length == 0)
        return (FT_ERR_SUCCESS);
    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (rng_secure_bytes(buffer, length) == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    const int32_t fallback_result = rng_fill_from_random_device(buffer, length);
    if (fallback_result != FT_ERR_SUCCESS)
        return (fallback_result);
    if (fallback_used != ft_nullptr)
        *fallback_used = 1;
    return (FT_ERR_SUCCESS);
}

template<typename IntegerType>
static int32_t rng_secure_integer(IntegerType *value, int32_t *fallback_used)
{
    if (value == ft_nullptr)
    {
        if (fallback_used != ft_nullptr)
            *fallback_used = 0;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    unsigned char buffer[sizeof(IntegerType)];
    const int32_t result = rng_secure_bytes_with_fallback(buffer, sizeof(buffer), fallback_used);
    if (result != FT_ERR_SUCCESS)
        return (result);
    ft_size_t index = 0;
    IntegerType assembled_value = 0U;
    while (index < sizeof(buffer))
    {
        assembled_value |= static_cast<IntegerType>(buffer[index]) << (index * 8U);
        index += 1U;
    }
    *value = assembled_value;
    return (FT_ERR_SUCCESS);
}

int32_t rng_secure_uint64(uint64_t *value, int32_t *fallback_used)
{
    return (rng_secure_integer(value, fallback_used));
}

int32_t rng_secure_uint32(uint32_t *value, int32_t *fallback_used)
{
    return (rng_secure_integer(value, fallback_used));
}
