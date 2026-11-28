#include "../../../Modules/Scripting/scripting.hpp"
#include <chrono>
#include <cstdio>

int32_t main()
{
    constexpr uint32_t ITERATIONS = 20000U;
    const char *source = "let base = 9; let bigger = base * 2; return bigger + 6;";
    scripting_engine engine;
    scripting_program program;
    scripting_value result;
    if (engine.initialize() != FT_ERR_SUCCESS
        || engine.compile(source, &program) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto compile_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (engine.compile(source, &program) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double compile_elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - compile_start).count();
    iteration = 0U;
    const auto verify_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (engine.verify_program(program) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double verify_elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - verify_start).count();
    iteration = 0U;
    const auto execute_start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
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
    while (iteration < ITERATIONS)
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
        compile_elapsed / ITERATIONS, verify_elapsed / ITERATIONS, execute_elapsed / ITERATIONS,
        run_elapsed / ITERATIONS);
    return (0);
}
