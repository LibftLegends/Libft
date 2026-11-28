# Math

The `Math` module contains scalar math, statistics, dice/roll parsing, interpolation, integration, polynomial solvers, automatic differentiation, FFT helpers, intervals, vectors, matrices, and quaternions.

## Scalar Math and Statistics

- `ft_abs`, `ft_sqrt`, `ft_pow`, `ft_exp`, `ft_log`, `ft_log10`, `ft_sin`, `ft_cos`, `ft_tan`, `ft_asin`, `ft_acos`, `ft_atan`, and related helpers - Project math wrappers.
- `ft_floor`, `ft_ceil`, `ft_round`, `ft_fmod`, `ft_fabs`, `ft_isnan`, `ft_isinf`, `ft_isfinite`, and `ft_signbit` - Floating-point classification and rounding helpers.
- `ft_clamp`, `ft_min`, `ft_max`, and `ft_swap` - Generic scalar utility helpers.
- `ft_mean`, `ft_median`, `ft_mode`, `ft_geometric_mean`, and `ft_harmonic_mean` - Basic statistics over numeric arrays.
- Angle conversion helpers - Convert between degrees and radians.

## Roll and Dice Helpers

- `math_eval` evaluates an integer expression with signed operands, parentheses,
  and the usual precedence for `+`, `-`, `*`, and `/`. It returns null for
  invalid syntax, out-of-range intermediate values, division by zero, or dice notation.
- `math_roll` also evaluates dice expressions such as `2d6` and the one-die shorthand `d6`. Both functions return
  a CMA-allocated integer on success. The caller releases it with `cma_free`.

## Interpolation, Integration, and Polynomials

- `ft_cubic_spline` - Lifecycle class storing spline x values and cubic coefficients.
- `ft_cubic_spline_build(...)` - Allocates and builds a cubic spline from x/y samples.
- `ft_cubic_spline_evaluate(...)` - Evaluates a spline at an input value.
- `math_bezier_evaluate(...)` / `math_bezier_evaluate_vector2(...)` - Evaluate scalar or `vector2` Bezier curves.
- `math_integrate_trapezoidal(...)` / `math_integrate_simpson(...)` - Numerically integrate a callback over a finite range. Simpson's method reuses sampled values between refinements, so the callback should return the same value for the same input during a call.
- `math_polynomial_evaluate(...)` - Evaluate a polynomial.
- `math_polynomial_solve_quadratic(...)` - Solve a quadratic equation.
- `math_polynomial_find_root_newton(...)` - Find a polynomial root with Newton iteration.
- `math_polynomial_lagrange_interpolate(...)` - Interpolate a value from sample points.

## Automatic Differentiation

- `ft_dual_number` - Lifecycle value type storing a value and derivative.
- `ft_dual_number::constant(...)` / `variable(...)` - Allocate constant or variable dual numbers.
- `value()` / `derivative()` - Read dual-number components.
- Operators `+`, `-`, `*`, `/` - Combine dual numbers while propagating derivatives.
- `apply_sin`, `apply_cos`, `apply_exp`, and `apply_log` - Apply differentiable unary functions.
- `ft_dual_number_proxy` - Error-propagating proxy for dual-number operator chains.
- `math_autodiff_univariate(...)` - Computes value and derivative for a unary callback.
- `math_autodiff_gradient(...)` - Computes multivariate value and gradient. The point
  and gradient may be the same vector. Errors leave the supplied outputs intact.
- Both differentiation callbacks return an owning pointer to a heap-allocated dual
  number. The differentiation routine deletes it after reading the result.

## Linear Algebra

- `vector2`, `vector3`, and `vector4` - Lifecycle vector classes with constructors, lifecycle methods, `get_*` accessors, `add`, `subtract`, `dot`, `length`, `normalize`, error accessors, and optional thread-safety methods. `vector3` also provides `cross`.
- `matrix2`, `matrix3`, and `matrix4` - Lifecycle matrix classes with constructors, lifecycle methods, `transform`, `multiply`, `invert`, error accessors, and optional thread-safety methods. `matrix4` also provides translation, scale, and axis-rotation factory methods.
- `quaternion` - Lifecycle quaternion class with component getters, `add`, `multiply`, `conjugate`, `length`, `normalize`, error accessors, and optional thread-safety methods.
- `linear_algebra.hpp` - Umbrella header for the vector, matrix, and quaternion types.

## FFT and Intervals

- `math_fft.hpp` - `math_fft` and `math_ifft` transform equally sized, nonempty vectors
  whose length is a power of two. Their two output vectors must be distinct. Each
  output may alias an input, including for an in-place transform.
- `math_convolution` convolves two nonempty real vectors into a vector of length
  `first.size() + second.size() - 1`. The result may alias either input.
  Both APIs take initialized vectors and return Libft status codes.
- `s_ft_interval` - Interval with `lower`, `upper`, and error code fields.
- `math_interval.hpp` helpers - Create and evaluate interval ranges and interval operations.
  Multiplication and division find the extrema of the four cross-endpoint results.
  Division rejects intervals containing zero, and both operations reject undefined
  endpoint results such as zero times infinity.

All lifecycle math classes expose `initialize`, copy/move initialization, `destroy`, `move`, `enable_thread_safety`, `disable_thread_safety`, and `is_thread_safe` unless noted by the class API.
