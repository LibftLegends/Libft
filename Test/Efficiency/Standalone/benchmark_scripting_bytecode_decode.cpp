#include "../../../Modules/Scripting/scripting.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int argc, char **argv)
{
    const ft_bool encode_large = argc == 2 && ft_strcmp(argv[1], "--encode-large") == 0;
    const ft_bool encode = encode_large || (argc == 2 && ft_strcmp(argv[1], "--encode") == 0);
    const ft_bool large = encode_large || (argc == 2 && ft_strcmp(argv[1], "--large") == 0);
    if (argc != 1 && large == FT_FALSE && encode == FT_FALSE)
        return (1);
    scripting_engine engine;
    scripting_program source{};
    scripting_program decoded{};
    uint8_t bytes[FT_SCRIPTING_MAX_SERIALIZED_PROGRAM_BYTES];
    uint32_t bytes_written;
    if (engine.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t iterations = 20000U;
    if (large)
    {
        iterations = 300U;
        source.format_version = FT_SCRIPTING_BYTECODE_VERSION;
        source.instruction_count = FT_SCRIPTING_MAX_INSTRUCTIONS;
        source.string_data_size = FT_SCRIPTING_MAX_STRING_BYTES;
        ft_memset(source.string_data, 'x', source.string_data_size);
        uint32_t index = 0U;
        while (index < source.instruction_count - 2U)
        {
            source.instructions[index++] = {SCRIPTING_OP_PUSH_NULL, 0, 0U};
            source.instructions[index++] = {SCRIPTING_OP_POP, 0, 0U};
        }
        source.instructions[index++] = {SCRIPTING_OP_PUSH_INTEGER, 7, 0U};
        source.instructions[index] = {SCRIPTING_OP_RETURN, 0, 0U};
    }
    else if (engine.compile("return \"saved\";", &source) != FT_ERR_SUCCESS)
        return (1);
    if (engine.serialize_program(source, bytes, sizeof(bytes), &bytes_written)
        != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (encode)
        {
            if (engine.serialize_program(source, bytes, sizeof(bytes), &bytes_written)
                != FT_ERR_SUCCESS)
                return (1);
        }
        else if (engine.deserialize_program(bytes, bytes_written, &decoded)
            != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (encode == FT_FALSE
        && engine.serialize_program(decoded, bytes, sizeof(bytes), &bytes_written)
            != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    index = 0U;
    while (index < bytes_written)
        checksum = checksum * UINT64_C(31) + bytes[index++];
    std::printf("bytecode checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / iterations);
    return (0);
}
