#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    crypto_sha256 context;
    uint8_t digest[32] = {0U};
    ft_size_t offset;
    ft_size_t chunk_size;

    if (data == ft_nullptr && size != 0U)
        return (0);
    if (context.initialize() != FT_ERR_SUCCESS)
        return (0);
    offset = 0U;
    while (offset < size)
    {
        chunk_size = size - offset;
        if (chunk_size > 97U)
            chunk_size = 97U;
        if (context.update(data + offset, chunk_size) != FT_ERR_SUCCESS)
            break ;
        offset += chunk_size;
    }
    (void)context.final(digest);
    (void)context.destroy();
    return (0);
}
