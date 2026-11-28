#include "file_internal.hpp"
#include "../Encryption/encryption_digest_internal.hpp"

template <typename State, void (*Initialize)(State &) noexcept,
    void (*Update)(State &, const uint8_t *, ft_size_t) noexcept,
    void (*Final)(State &, uint8_t *) noexcept>
static int32_t file_hash_stream(const char *path, uint8_t *digest)
{
    if (path == ft_nullptr || digest == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    file_observability_emit("read_all", path, FT_OBSERVABILITY_TRACE_START,
        FT_ERR_SUCCESS, 0, 0);
    FILE *stream = ft_fopen(path, "rb");
    if (stream == ft_nullptr)
    {
        file_observability_emit("read_all", path, FT_OBSERVABILITY_TRACE_FINISH,
            FT_ERR_FILE_OPEN_FAILED, 0, 0);
        return (FT_ERR_FILE_OPEN_FAILED);
    }
    constexpr ft_size_t BUFFER_SIZE = 64 * 1024;
    uint8_t buffer[BUFFER_SIZE];
    ft_size_t bytes_read = 0;
    State state;
    Initialize(state);
    ft_size_t count;
    do
    {
        count = std::fread(buffer, 1, sizeof(buffer), stream);
        Update(state, buffer, count);
        bytes_read += count;
    }
    while (count == sizeof(buffer));
    int32_t error_code = FT_ERR_SUCCESS;
    if (std::ferror(stream) != 0)
        error_code = FT_ERR_IO;
    if (ft_fclose(stream) != FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    if (error_code == FT_ERR_SUCCESS)
        Final(state, digest);
    else
        bytes_read = 0;
    file_observability_emit("read_all", path, FT_OBSERVABILITY_TRACE_FINISH,
        error_code, bytes_read, 0);
    return (error_code);
}

int32_t file_hash_sha1(const char *path, uint8_t digest[20])
{
    return (file_hash_stream<sha1_digest_state, sha1_digest_initialize,
        sha1_digest_update, sha1_digest_final>(path, digest));
}

int32_t file_hash_sha256(const char *path, uint8_t digest[32])
{
    return (file_hash_stream<sha256_digest_state, sha256_digest_initialize,
        sha256_digest_update, sha256_digest_final>(path, digest));
}
