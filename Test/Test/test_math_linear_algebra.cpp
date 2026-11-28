#include "../test_internal.hpp"
#include "../../Modules/Math/math.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <cmath>
#include <limits>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

extern std::atomic<int32_t> pt_recursive_mutex_lock_override_error_code;

FT_TEST(test_vector3_dot)
{
    vector3 a(1.0, 0.0, 0.0);
    vector3 b(0.0, 1.0, 0.0);

    FT_ASSERT(math_fabs(a.dot(b)) < 0.000001);
    return (1);
}

FT_TEST(test_vector3_get_components_transfers_a_single_locked_snapshot)
{
    vector3 value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(1.5, -2.0, 3.25));
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), value.enable_thread_safety());
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_components(x, y, z));
    FT_ASSERT_EQ(1.5, x);
    FT_ASSERT_EQ(-2.0, y);
    FT_ASSERT_EQ(3.25, z);
    return (1);
}

FT_TEST(test_vector3_cross)
{
    vector3 a(1.0, 0.0, 0.0);
    vector3 b(0.0, 1.0, 0.0);
    vector3 c = a.cross(b);

    FT_ASSERT(math_fabs(c.get_x()) < 0.000001);
    FT_ASSERT(math_fabs(c.get_y()) < 0.000001);
    FT_ASSERT(math_fabs(c.get_z() - 1.0) < 0.000001);
    return (1);
}

FT_TEST(test_matrix4_identity_transform)
{
    matrix4 identity(1.0, 0.0, 0.0, 0.0,
                     0.0, 1.0, 0.0, 0.0,
                     0.0, 0.0, 1.0, 0.0,
                     0.0, 0.0, 0.0, 1.0);
    vector4 v(1.0, 2.0, 3.0, 1.0);
    vector4 r = identity.transform(v);

    FT_ASSERT(math_fabs(r.get_x() - 1.0) < 0.000001);
    FT_ASSERT(math_fabs(r.get_y() - 2.0) < 0.000001);
    FT_ASSERT(math_fabs(r.get_z() - 3.0) < 0.000001);
    FT_ASSERT(math_fabs(r.get_w() - 1.0) < 0.000001);
    return (1);
}

FT_TEST(test_matrix2_operations)
{
    matrix2 matrix(4.0, 7.0,
                   2.0, 6.0);
    matrix2 inverse = matrix.invert();
    matrix2 identity = matrix.multiply(inverse);
    vector2 vector_value(3.0, 5.0);
    vector2 transformed = identity.transform(vector_value);

    FT_ASSERT(math_fabs(transformed.get_x() - 3.0) < 0.000001);
    FT_ASSERT(math_fabs(transformed.get_y() - 5.0) < 0.000001);
    return (1);
}

FT_TEST(test_matrix3_operations)
{
    matrix3 matrix(1.0, 2.0, 3.0,
                   0.0, 1.0, 4.0,
                   5.0, 6.0, 0.0);
    matrix3 inverse = matrix.invert();
    matrix3 identity = matrix.multiply(inverse);
    vector3 vector_value(1.0, 2.0, 3.0);
    vector3 transformed = identity.transform(vector_value);

    FT_ASSERT(math_fabs(transformed.get_x() - 1.0) < 0.000001);
    FT_ASSERT(math_fabs(transformed.get_y() - 2.0) < 0.000001);
    FT_ASSERT(math_fabs(transformed.get_z() - 3.0) < 0.000001);
    return (1);
}

FT_TEST(test_vector_lengths_handle_extreme_finite_scales)
{
    const double smallest = std::numeric_limits<double>::denorm_min();
    const vector2 tiny(smallest, 0.0);
    const vector3 big(1e300, 2e300, 2e300);
    const vector4 wide(1e308, 1e308, 0.0, 0.0);
    FT_ASSERT_EQ(smallest, tiny.length());
    FT_ASSERT(std::fabs(big.length() / 3e300 - 1.0) < 1e-14);
    FT_ASSERT(std::fabs(wide.length() / 1e308 - std::sqrt(2.0)) < 1e-14);
    return (1);
}

FT_TEST(test_vectors_normalize_when_unscaled_length_overflows)
{
    const vector2 large_pair(1e308, -1e308);
    const vector3 large_triple(1e308, 1e308, -1e308);
    const vector4 large_quad(1e308, -1e308, 1e308, -1e308);
    const vector2 unit_pair = large_pair.normalize();
    const vector3 unit_triple = large_triple.normalize();
    const vector4 unit_quad = large_quad.normalize();
    FT_ASSERT(std::fabs(unit_pair.get_x() - std::sqrt(0.5)) < 1e-14);
    FT_ASSERT(std::fabs(unit_triple.get_z() + 1.0 / std::sqrt(3.0)) < 1e-14);
    FT_ASSERT_EQ(-0.5, unit_quad.get_w());
    FT_ASSERT_EQ(1.0, unit_quad.length());
    return (1);
}

FT_TEST(test_vector_normalize_retains_near_zero_policy)
{
    const vector3 tiny(1e-8, 0.0, 0.0);
    const vector3 normalized = tiny.normalize();
    FT_ASSERT_EQ(FT_CLASS_STATE_UNINITIALISED, normalized._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tiny.get_error());
    return (1);
}

FT_TEST(test_quaternion_multiply_keeps_hamilton_product_order)
{
    quaternion first(1.0, 2.0, 3.0, 4.0);
    quaternion second(5.0, 6.0, 7.0, 8.0);
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), first.enable_thread_safety());
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), second.enable_thread_safety());
    const quaternion product = first.multiply(second);
    const quaternion reversed = second.multiply(first);
    FT_ASSERT_EQ(-60.0, product.get_w());
    FT_ASSERT_EQ(12.0, product.get_x());
    FT_ASSERT_EQ(30.0, product.get_y());
    FT_ASSERT_EQ(24.0, product.get_z());
    FT_ASSERT_EQ(20.0, reversed.get_x());
    FT_ASSERT_EQ(14.0, reversed.get_y());
    FT_ASSERT_EQ(32.0, reversed.get_z());
    return (1);
}

FT_TEST(test_quaternion_length_and_normalize_handle_extreme_scales)
{
    const double smallest = std::numeric_limits<double>::denorm_min();
    const quaternion tiny(0.0, smallest, 0.0, 0.0);
    FT_ASSERT_EQ(smallest, tiny.length());

    quaternion large(1e308, -1e308, 1e308, -1e308);
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), large.enable_thread_safety());
    const quaternion unit = large.normalize();
    FT_ASSERT_EQ(0.5, unit.get_w());
    FT_ASSERT_EQ(-0.5, unit.get_x());
    FT_ASSERT_EQ(0.5, unit.get_y());
    FT_ASSERT_EQ(-0.5, unit.get_z());
    FT_ASSERT_EQ(1.0, unit.length());
    return (1);
}

FT_TEST(test_matrix4_multiply_preserves_transform_order)
{
    const matrix4 translation = matrix4::make_translation(5.0, 7.0, 11.0);
    const matrix4 scale = matrix4::make_scale(2.0, 3.0, 4.0);
    const vector4 point(1.0, 2.0, 3.0, 1.0);
    const vector4 translated_after_scale = translation.multiply(scale).transform(point);
    const vector4 scaled_after_translation = scale.multiply(translation).transform(point);
    FT_ASSERT_EQ(7.0, translated_after_scale.get_x());
    FT_ASSERT_EQ(13.0, translated_after_scale.get_y());
    FT_ASSERT_EQ(23.0, translated_after_scale.get_z());
    FT_ASSERT_EQ(1.0, translated_after_scale.get_w());
    FT_ASSERT_EQ(12.0, scaled_after_translation.get_x());
    FT_ASSERT_EQ(27.0, scaled_after_translation.get_y());
    FT_ASSERT_EQ(56.0, scaled_after_translation.get_z());
    return (1);
}

FT_TEST(test_matrix4_transform_propagates_vector_snapshot_failure)
{
    const matrix4 translation = matrix4::make_translation(5.0, 7.0, 11.0);
    vector4 point(1.0, 2.0, 3.0, 1.0);
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), point.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_INTERNAL);
    const vector4 failed = translation.transform(point);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_INTERNAL, translation.get_error());
    FT_ASSERT_EQ(FT_CLASS_STATE_UNINITIALISED, failed._initialised_state);

    const vector4 transformed = translation.transform(point);
    FT_ASSERT_EQ(6.0, transformed.get_x());
    FT_ASSERT_EQ(9.0, transformed.get_y());
    FT_ASSERT_EQ(14.0, transformed.get_z());
    return (1);
}

FT_TEST(test_matrix4_inverse_pivots_and_round_trips_dense_points)
{
    matrix4 matrix(0.0, 2.0, 1.0, 1.0,
                   3.0, 0.0, 1.0, 4.0,
                   2.0, 1.0, 5.0, 6.0,
                   1.0, 3.0, 1.0, 2.0);
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), matrix.enable_thread_safety());
    const matrix4 inverse = matrix.invert();
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, inverse._initialised_state);
    const vector4 point(1.0, 2.0, 3.0, 4.0);
    const vector4 restored = inverse.transform(matrix.transform(point));
    FT_ASSERT(std::fabs(restored.get_x() - 1.0) < 1e-12);
    FT_ASSERT(std::fabs(restored.get_y() - 2.0) < 1e-12);
    FT_ASSERT(std::fabs(restored.get_z() - 3.0) < 1e-12);
    FT_ASSERT(std::fabs(restored.get_w() - 4.0) < 1e-12);
    return (1);
}

FT_TEST(test_matrix4_inverse_keeps_singular_result_policy)
{
    const matrix4 singular = matrix4::make_scale(2.0, 0.0, 4.0);
    const matrix4 inverse = singular.invert();
    FT_ASSERT_EQ(FT_CLASS_STATE_UNINITIALISED, inverse._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, singular.get_error());
    return (1);
}

FT_TEST(test_matrix4_rotation_factories_preserve_axis_orientation)
{
    constexpr double QUARTER_TURN = 1.57079632679489661923;
    const vector4 x(1.0, 0.0, 0.0, 1.0);
    const vector4 y(0.0, 1.0, 0.0, 1.0);
    const vector4 z(0.0, 0.0, 1.0, 1.0);
    const vector4 rotated_x = matrix4::make_rotation_z(QUARTER_TURN).transform(x);
    const vector4 rotated_y = matrix4::make_rotation_x(QUARTER_TURN).transform(y);
    const vector4 rotated_z = matrix4::make_rotation_y(QUARTER_TURN).transform(z);
    FT_ASSERT(std::fabs(rotated_x.get_x()) < 1e-12);
    FT_ASSERT(std::fabs(rotated_x.get_y() - 1.0) < 1e-12);
    FT_ASSERT(std::fabs(rotated_y.get_y()) < 1e-12);
    FT_ASSERT(std::fabs(rotated_y.get_z() - 1.0) < 1e-12);
    FT_ASSERT(std::fabs(rotated_z.get_z()) < 1e-12);
    FT_ASSERT(std::fabs(rotated_z.get_x() - 1.0) < 1e-12);
    return (1);
}

FT_TEST(test_vector3_move_initialization_keeps_source_mutex)
{
    vector3 source(1.0, 2.0, 3.0);
    vector3 destination(9.0, 8.0, 7.0);
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), source.enable_thread_safety());
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), destination.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(static_cast<vector3 &&>(source)));
    FT_ASSERT_EQ(1.0, destination.get_x());
    FT_ASSERT_EQ(2.0, destination.get_y());
    FT_ASSERT_EQ(3.0, destination.get_z());
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());
    FT_ASSERT_EQ(0.0, source.get_x());
    FT_ASSERT_EQ(0.0, source.get_y());
    FT_ASSERT_EQ(0.0, source.get_z());
    return (1);
}

FT_TEST(test_matrix4_copy_and_move_lock_failure_keep_source)
{
    matrix4 source = matrix4::make_translation(2.0, 3.0, 4.0);
    matrix4 copied = matrix4::make_scale(9.0, 8.0, 7.0);
    matrix4 moved = matrix4::make_scale(6.0, 5.0, 4.0);
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), source.enable_thread_safety());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_INTERNAL);
    const int32_t copy_error = copied.initialize(source);
    const int32_t move_error = moved.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_INTERNAL, copy_error);
    FT_ASSERT_EQ(FT_ERR_INTERNAL, move_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, copied._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, moved._initialised_state);

    const vector4 point(1.0, 2.0, 3.0, 1.0);
    const vector4 transformed = source.transform(point);
    FT_ASSERT_EQ(3.0, transformed.get_x());
    FT_ASSERT_EQ(5.0, transformed.get_y());
    FT_ASSERT_EQ(7.0, transformed.get_z());
    return (1);
}

FT_TEST(test_vector4_value_constructor_preserves_last_error)
{
    vector4 existing(1.0, 2.0, 3.0, 4.0);
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), existing.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_INTERNAL);
    (void)existing.get_x();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS);
    {
        const vector4 value(3.0, 5.0, 7.0, 11.0);
        FT_ASSERT_EQ(FT_ERR_INTERNAL, value.get_error());
    }
    FT_ASSERT_EQ(FT_ERR_INTERNAL, existing.get_error());
    return (1);
}
