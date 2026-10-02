#include "../test_internal.hpp"
#include "../../Modules/Scripting/scripting.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t scripting_manifest_native_a(
    const scripting_call_context *context, const scripting_value *arguments,
    uint32_t argument_count, scripting_value *result, void *user_data) noexcept
{
    (void)context;
    (void)arguments;
    (void)argument_count;
    (void)user_data;
    return (scripting_value_set_null(result));
}

static int32_t scripting_manifest_native_b(
    const scripting_call_context *context, const scripting_value *arguments,
    uint32_t argument_count, scripting_value *result, void *user_data) noexcept
{
    (void)context;
    (void)arguments;
    (void)argument_count;
    (void)user_data;
    return (scripting_value_set_null(result));
}

FT_TEST(test_scripting_native_manifest_hash_is_deterministic_and_pointer_independent)
{
    scripting_engine first;
    scripting_engine second;
    uint32_t native_id;
    uint32_t first_user_data;
    uint32_t second_user_data;
    uint64_t first_hash;
    uint64_t second_hash;

    first_user_data = 1U;
    second_user_data = 2U;
    native_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.register_native("alpha",
        scripting_manifest_native_a, &first_user_data, &native_id));
    FT_ASSERT_EQ(0U, native_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.register_native("beta",
        scripting_manifest_native_a, &first_user_data, &native_id));
    FT_ASSERT_EQ(1U, native_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.register_native("alpha",
        scripting_manifest_native_b, &second_user_data, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.register_native("beta",
        scripting_manifest_native_b, &second_user_data, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        first.get_native_manifest_hash(&first_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        second.get_native_manifest_hash(&second_hash));
    FT_ASSERT_EQ(first_hash, second_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_scripting_native_manifest_hash_tracks_names_and_registration_order)
{
    scripting_engine baseline;
    scripting_engine reversed;
    scripting_engine renamed;
    uint32_t native_id;
    uint64_t baseline_hash;
    uint64_t reversed_hash;
    uint64_t renamed_hash;

    native_id = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, baseline.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reversed.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, renamed.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, baseline.register_native("alpha",
        scripting_manifest_native_a, ft_nullptr, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, baseline.register_native("beta",
        scripting_manifest_native_a, ft_nullptr, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reversed.register_native("beta",
        scripting_manifest_native_a, ft_nullptr, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reversed.register_native("alpha",
        scripting_manifest_native_a, ft_nullptr, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, renamed.register_native("alpha",
        scripting_manifest_native_a, ft_nullptr, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, renamed.register_native("gamma",
        scripting_manifest_native_a, ft_nullptr, &native_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        baseline.get_native_manifest_hash(&baseline_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        reversed.get_native_manifest_hash(&reversed_hash));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        renamed.get_native_manifest_hash(&renamed_hash));
    FT_ASSERT_NEQ(baseline_hash, reversed_hash);
    FT_ASSERT_NEQ(baseline_hash, renamed_hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, baseline.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reversed.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, renamed.destroy());
    return (1);
}

FT_TEST(test_scripting_native_manifest_hash_rejects_invalid_lifecycle_and_output)
{
    scripting_engine engine;
    uint64_t hash;

    hash = 0x123456789ABCDEF0ULL;
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED,
        engine.get_native_manifest_hash(&hash));
    FT_ASSERT_EQ(0x123456789ABCDEF0ULL, hash);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        engine.get_native_manifest_hash(ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED,
        engine.get_native_manifest_hash(&hash));
    return (1);
}
