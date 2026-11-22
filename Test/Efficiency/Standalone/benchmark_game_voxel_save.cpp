#include "../../../Modules/Game/game_voxel_chunk.hpp"
#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Compatebility/compatebility_internal.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;
using Samples = std::vector<double>;
static double micros(Clock::time_point begin, Clock::time_point end)
{
    return (std::chrono::duration<double, std::micro>(end - begin).count());
}
static void show(const char *name, Samples samples)
{
    std::sort(samples.begin(), samples.end());
    std::printf("%s p50=%.2f p95=%.2f max=%.2f us\n",
        name, samples[samples.size()/2], samples[samples.size()*95/100], samples.back());
    return ;
}
static int32_t run(int32_t overrides)
{
    constexpr const char *DESTINATION = "libft-save-audit.vxc";
    game_voxel_chunk chunk;
    int32_t section = 0;
    int32_t block = 0;
    int32_t rounds = 120;
    int32_t iteration = 0;

    if (file_get_type(DESTINATION) != FILE_TYPE_MISSING)
    {
        std::fprintf(stderr, "Run the voxel save benchmark in an empty working directory.\n");
        return (1);
    }
    if (chunk.initialize() != FT_ERR_SUCCESS)
        return (1);
    while (section < 8)
    {
        block = 0;
        while (block < 4096)
        {
            if (chunk.write_generated_block(block % 16, section * 16 + block / 256,
                    (block / 16) % 16, 1U + static_cast<uint32_t>((block / 7) % 13))
                != FT_ERR_SUCCESS)
                return (2);
            ++block;
        }
        ++section;
    }
    block = 0;
    while (block < overrides)
    {
        if (chunk.write_block(block % 16, block / 256, (block / 16) % 16, 42U)
            != FT_ERR_SUCCESS)
            return (3);
        ++block;
    }
    Samples codec, temp, write, close, rename, read;
    ft_size_t size = 0U;
    if (overrides > 10)
        rounds = 48;
    while (iteration < rounds)
    {
        ft_byte_buffer payload;
        ft_string staging;
        if (payload.initialize() || staging.initialize())
            return (4);
        int32_t descriptor = -1;
        const auto started = Clock::now();
        if (chunk.serialize(payload))
            return (5);
        const auto encoded = Clock::now();
        size = payload.size();
        if (file_secure_temp_file(".", "audit", &staging, &descriptor))
            return (6);
        const auto temporary_opened = Clock::now();
        const int32_t write_error = file_write_all_to_descriptor(descriptor,
            reinterpret_cast<const char *>(payload.data()), size);
        const auto written = Clock::now();
        const int32_t close_error = cmp_close(descriptor);
        const auto closed = Clock::now();
        if (write_error != FT_ERR_SUCCESS || close_error != FT_ERR_SUCCESS)
        {
            (void)file_delete(staging.c_str());
            return (7);
        }
        if (file_move(staging.c_str(), DESTINATION))
        {
            (void)file_delete(staging.c_str());
            return (9);
        }
        const auto replaced = Clock::now();
        game_voxel_chunk decoded;
        if (decoded.deserialize(payload))
        {
            (void)file_delete(DESTINATION);
            return (10);
        }
        const auto decoded_at = Clock::now();
        codec.push_back(micros(started, encoded));
        temp.push_back(micros(encoded, temporary_opened));
        write.push_back(micros(temporary_opened, written));
        close.push_back(micros(written, closed));
        rename.push_back(micros(closed, replaced));
        read.push_back(micros(replaced, decoded_at));
        ++iteration;
    }
    if (file_delete(DESTINATION))
        return (11);
    std::printf("overrides=%d bytes=" FT_UINT64_DECIMAL_FORMAT " samples=%d\n",
        overrides, size, rounds);
    show("encode", codec);
    show("temp", temp);
    show("write", write);
    show("close", close);
    show("replace", rename);
    show("decode", read);
    return (0);
}
int main()
{
    const int32_t first = run(2);
    if (first != 0)
        return (first);
    return (run(4096));
}
