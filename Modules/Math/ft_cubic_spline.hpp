#ifndef MATH_POLYNOMIAL_HPP
# define MATH_POLYNOMIAL_HPP

#include "../Template/move.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "linear_algebra.hpp"
#include <cstddef>
#include <cstdint>

typedef double (*math_unary_function)(double value, void *user_data);

class ft_cubic_spline
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        friend double ft_cubic_spline_evaluate(const ft_cubic_spline &spline,
            double input_value) noexcept;

        mutable pt_recursive_mutex *_mutex = ft_nullptr;
        uint8_t                    _initialised_state = FT_CLASS_STATE_UNINITIALISED;

        void                        abort_lifecycle_error(const char *method_name,
                                        const char *reason) const noexcept;
        void                        abort_if_not_initialised(const char *method_name) const noexcept;

    public:
        ft_vector<double> _x_values;
        ft_vector<double> _a_coefficients;
        ft_vector<double> _b_coefficients;
        ft_vector<double> _c_coefficients;
        ft_vector<double> _d_coefficients;

        ft_cubic_spline() noexcept;
        ft_cubic_spline(const ft_cubic_spline &other) noexcept = delete;
        ft_cubic_spline(ft_cubic_spline &&other) noexcept = delete;
        ft_cubic_spline &operator=(const ft_cubic_spline &other) = delete;
        ft_cubic_spline &operator=(ft_cubic_spline &&other) noexcept = delete;
        ~ft_cubic_spline() noexcept;
        int32_t initialize() noexcept;
        int32_t initialize(const ft_cubic_spline &other) noexcept;
        int32_t initialize(ft_cubic_spline &&other) noexcept;
        int32_t destroy() noexcept;
        int32_t move(ft_cubic_spline &other) noexcept;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
};

int32_t math_polynomial_evaluate(const ft_vector<double> &coefficients,
        double input_value,
        double &result) noexcept;
int32_t math_polynomial_find_root_newton(const ft_vector<double> &coefficients,
        double initial_guess,
        double tolerance,
        ft_size_t max_iterations,
        double &root) noexcept;
int32_t math_polynomial_solve_quadratic(double coefficient_a,
        double coefficient_b,
        double coefficient_c,
        double &root_one,
        double &root_two) noexcept;
int32_t math_polynomial_lagrange_interpolate(const ft_vector<double> &x_values_input,
        const ft_vector<double> &y_values,
        double input_value,
        double &result) noexcept;
int32_t math_bezier_evaluate(const ft_vector<double> &control_points,
        double parameter,
        double &result) noexcept;
int32_t math_bezier_evaluate_vector2(const ft_vector<vector2> &control_points,
        double parameter,
        vector2 &result) noexcept;
ft_cubic_spline *ft_cubic_spline_build(const ft_vector<double> &x_values_input,
        const ft_vector<double> &y_values) noexcept;
double ft_cubic_spline_evaluate(const ft_cubic_spline &spline,
        double input_value) noexcept;
int32_t math_integrate_trapezoidal(math_unary_function function,
        void *user_data,
        double lower_bound,
        double upper_bound,
        ft_size_t subdivisions,
        double &result) noexcept;
int32_t math_integrate_simpson(math_unary_function function,
        void *user_data,
        double lower_bound,
        double upper_bound,
        double tolerance,
        double &result) noexcept;

#endif
