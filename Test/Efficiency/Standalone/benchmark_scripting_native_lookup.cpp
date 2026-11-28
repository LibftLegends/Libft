#include "../../../Modules/Scripting/scripting.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t empty_native(const scripting_call_context *, const scripting_value *,
    uint32_t, scripting_value *result, void *) noexcept
{
    return (scripting_value_set_null(result));
}

int main()
{
    constexpr uint32_t REGISTRATIONS = 128U;
    constexpr uint32_t REGISTRATION_RUNS = 200U;
    constexpr uint32_t LOOKUPS = 100000U;
    char names[REGISTRATIONS][sizeof("voxel_generate_feature_000")];
    uint32_t index = 0U;
    while (index < REGISTRATIONS)
    {
        if (std::snprintf(names[index], sizeof(names[index]),
                "voxel_generate_feature_%03" PRIu32, index) < 0)
            return (1);
        ++index;
    }
    scripting_engine engine;
    uint32_t run = 0U;
    const auto register_start = std::chrono::steady_clock::now();
    while (run < REGISTRATION_RUNS)
    {
        if (engine.destroy() != FT_ERR_SUCCESS || engine.initialize() != FT_ERR_SUCCESS)
            return (1);
        index = 0U;
        while (index < REGISTRATIONS)
        {
            uint32_t native_id;
            if (engine.register_native(names[index], &empty_native, ft_nullptr, &native_id)
                != FT_ERR_SUCCESS || native_id != index)
                return (1);
            ++index;
        }
        ++run;
    }
    const double register_elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - register_start).count();
    run = 0U;
    const auto lookup_start = std::chrono::steady_clock::now();
    while (run < LOOKUPS)
    {
        uint32_t native_id;
        index = (run * 29U) % REGISTRATIONS;
        if (engine.find_native(names[index], sizeof(names[index]) - 1U, &native_id)
            != FT_ERR_SUCCESS || native_id != index)
            return (1);
        ++run;
    }
    const double lookup_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - lookup_start).count();
    uint64_t manifest_hash;
    if (engine.get_native_manifest_hash(&manifest_hash) != FT_ERR_SUCCESS)
        return (1);
    std::printf("register %.6f\nlookup %.6f\nmanifest %" PRIu64 "\n",
        register_elapsed / REGISTRATION_RUNS, lookup_elapsed / LOOKUPS, manifest_hash);
    return (0);
}
