Math_TARGET := Math.a
Math_DEBUG_TARGET := Math_debug.a

Math_SOURCES := math_abs.cpp \
        math_fabs.cpp \
        math_fmod.cpp \
        math_signbit.cpp \
        math_isnan.cpp \
        math_isinf.cpp \
        math_nan.cpp \
        math_infinity.cpp \
        math_negative_infinity.cpp \
        math_indeterminate.cpp \
        math_swap.cpp \
        math_clamp.cpp \
        math_gcd.cpp \
        math_big_number.cpp \
        math_lcm.cpp \
        math_max.cpp \
        math_min.cpp \
        math_factorial.cpp \
        math_absdiff.cpp \
        math_average.cpp \
        math_pow.cpp \
        math_sqrt.cpp \
        math_exp.cpp \
        math_log.cpp \
        math_acos.cpp \
        math_cos.cpp \
        math_sin.cpp \
        math_tan.cpp \
        math_deg2rad.cpp \
        math_rad2deg.cpp \
        math_statistics.cpp \
        math_roll.cpp \
        math_eval.cpp \
        math_roll_utilities.cpp \
        math_roll_parse_brackets.cpp \
        math_roll_parse_dice.cpp \
        math_roll_parse_md.cpp \
        math_roll_parse_pm.cpp \
        math_roll_parse_utils.cpp \
        math_roll_validate_string.cpp \
        math_roll_validate_utils.cpp \
        math_validate_int.cpp \
        math_autodiff.cpp \
        math_fft.cpp \
        math_interval.cpp \
        math_polynomial.cpp \
        math_linear_algebra.cpp \
        math_linear_algebra_constructors.cpp \
        math_linear_algebra_vector2.cpp \
        math_linear_algebra_vector3.cpp \
        math_linear_algebra_vector4.cpp \
        math_linear_algebra_quaternion.cpp

Math_HEADERS := math.hpp roll.hpp math_internal.hpp linear_algebra.hpp vector2.hpp vector3.hpp vector4.hpp matrix2.hpp matrix3.hpp matrix4.hpp quaternion.hpp ft_dual_number.hpp math_fft.hpp math_interval.hpp ft_cubic_spline.hpp
