#include "../../../Modules/Compatebility/compatebility_cross_process.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 500000U;
    cross_process_message source = {0U, 9088U, 8192U, 65536U, 32536U, "/voxel_benchmark"};
    cross_process_message decoded{};
    uint8_t wire[CMP_CROSS_PROCESS_WIRE_SIZE];
    uint32_t index = 0;
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        source.stack_base_address = index;
        if (cmp_cross_process_encode_wire(source, wire, sizeof(wire)) != FT_ERR_SUCCESS
            || cmp_cross_process_decode_wire(wire, sizeof(wire), decoded) != FT_ERR_SUCCESS)
            return (1);
        checksum = checksum * 31U + decoded.stack_base_address
            + wire[index % CMP_CROSS_PROCESS_WIRE_SIZE];
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("wire %.3f ns/op checksum=%" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}
