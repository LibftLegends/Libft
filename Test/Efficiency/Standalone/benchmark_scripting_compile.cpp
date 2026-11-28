#include "../../../Modules/Scripting/scripting.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <string>

int main(int argc, char **argv)
{
    const ft_bool large = argc == 2 && ft_strcmp(argv[1], "--large") == 0;
    if (argc != 1 && large == FT_FALSE)
        return (1);
    uint32_t iterations = 20000U;
    const char *source = "let base = 9; let bigger = base * 2; return bigger + 6;";
    std::string long_source;
    if (large)
    {
        iterations = 200U;
        uint32_t index = 0U;
        while (index < FT_SCRIPTING_MAX_INSTRUCTIONS / 2U - 1U)
        {
            long_source += "1;";
            ++index;
        }
        long_source += "24";
        source = long_source.c_str();
    }
    scripting_engine engine;
    scripting_program program;
    scripting_value result;
    if (engine.initialize() != FT_ERR_SUCCESS
        || engine.set_operation_limit(FT_SCRIPTING_MAX_OPERATIONS) != FT_ERR_SUCCESS
        || engine.compile(source, &program) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto compile_start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (engine.compile(source, &program) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double compile_elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - compile_start).count();
    iteration = 0U;
    const auto verify_start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (engine.verify_program(program) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double verify_elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - verify_start).count();
    iteration = 0U;
    const auto execute_start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (engine.execute_program(program, &result) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double execute_elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - execute_start).count();
    if (result.type != SCRIPTING_VALUE_INTEGER || result.integer_value != 24)
        return (1);
    iteration = 0U;
    const auto run_start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (engine.execute(source, &result) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double run_elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - run_start).count();
    if (result.type != SCRIPTING_VALUE_INTEGER || result.integer_value != 24)
        return (1);
    std::printf("compile %.6f\nverify %.6f\nexecute %.6f\nsource_run %.6f\n",
        compile_elapsed / iterations, verify_elapsed / iterations, execute_elapsed / iterations,
        run_elapsed / iterations);
    uint8_t serialized[FT_SCRIPTING_MAX_SERIALIZED_PROGRAM_BYTES];
    uint32_t serialized_size;
    if (engine.serialize_program(program, serialized, sizeof(serialized), &serialized_size)
        != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    while (index < serialized_size)
        checksum = checksum * UINT64_C(31) + serialized[index++];
    std::printf("compile checksum %" PRIu64 " %.6f us/operation\n",
        checksum, compile_elapsed / iterations);
    return (0);
}
