#ifndef BUFFER_ENDIAN_HPP
# define BUFFER_ENDIAN_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include <cstdint>
#include <type_traits>

template <typename UInt>
constexpr void buffer_store_le(uint8_t *bytes, UInt value) noexcept
{
    static_assert(std::is_unsigned<UInt>::value && sizeof(UInt) <= sizeof(uint64_t));
    uint32_t index = 0U;
    while (index < sizeof(UInt))
    {
        bytes[index] = static_cast<uint8_t>(static_cast<uint64_t>(value) >> (index * 8U));
        ++index;
    }
    return ;
}

template <typename UInt>
constexpr UInt buffer_load_le(const uint8_t *bytes) noexcept
{
    static_assert(std::is_unsigned<UInt>::value && sizeof(UInt) <= sizeof(uint64_t));
    uint64_t value = 0U;
    uint32_t index = 0U;
    while (index < sizeof(UInt))
    {
        value |= static_cast<uint64_t>(bytes[index]) << (index * 8U);
        ++index;
    }
    return (static_cast<UInt>(value));
}

#endif
