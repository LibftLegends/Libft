#include "rng.hpp"
#include <cmath>
#include <limits>

static constexpr double RNG_DENSITY_EPSILON = 0.000000000001;

static int32_t rng_simpson_segment_total(double additional_segments)
{
    constexpr int32_t MIN_SEGMENTS = 256;
    constexpr int32_t MAX_SEGMENTS = 8192;

    if (additional_segments >= MAX_SEGMENTS - MIN_SEGMENTS)
        return (MAX_SEGMENTS);
    if (!(additional_segments > 0.0))
        return (MIN_SEGMENTS);
    const int32_t segment_total = MIN_SEGMENTS + static_cast<int32_t>(additional_segments);
    return (segment_total + (segment_total & 1));
}

template<typename Integrand>
static double rng_simpson_integral(double end, int32_t segment_total,
    const Integrand &integrand)
{
    const double step = end / static_cast<double>(segment_total);
    double sum = 0.0;
    int32_t index = 0;
    while (index <= segment_total)
    {
        const double point = step * static_cast<double>(index);
        double weight;
        if (index == 0 || index == segment_total)
            weight = 1.0;
        else if ((index & 1) == 0)
            weight = 2.0;
        else
            weight = 4.0;
        double value = integrand(point);
        if (std::isfinite(value) == FT_FALSE)
        {
            double adjustment = step * 0.5;
            if (adjustment <= 0.0)
                adjustment = 0.0000000001;
            double adjusted_point = point;
            if (index == 0)
            {
                adjusted_point = point + adjustment;
                if (adjusted_point > end)
                    adjusted_point = end;
            }
            else if (index == segment_total)
            {
                adjusted_point = point - adjustment;
                if (adjusted_point < 0.0)
                    adjusted_point = 0.0;
            }
            value = integrand(adjusted_point);
            if (std::isfinite(value) == FT_FALSE)
                value = 0.0;
        }
        sum = sum + weight * value;
        index += 1;
    }
    const double integral = (step / 3.0) * sum;
    if (integral < 0.0)
        return (0.0);
    if (integral > 1.0)
        return (1.0);
    return (integral);
}

static double rng_gamma_lower_density(double shape, double scale)
{
    if (shape < 1.0)
        return (std::numeric_limits<double>::infinity());
    if (std::fabs(shape - 1.0) <= RNG_DENSITY_EPSILON)
        return (1.0 / scale);
    return (0.0);
}

static double rng_gamma_density(double value, double shape_minus_one, double scale,
    double scaled_log_scale, double log_gamma)
{
    double log_component = shape_minus_one * std::log(value) - (value / scale);
    log_component = log_component - scaled_log_scale;
    log_component = log_component - log_gamma;
    const double result = std::exp(log_component);
    if (std::isfinite(result) == FT_FALSE)
        return (0.0);
    return (result);
}

double rng_gamma_pdf(double shape, double scale, double value)
{
    if (shape <= 0.0 || scale <= 0.0 || value < 0.0)
        return (0.0);
    if (std::fabs(value) <= RNG_DENSITY_EPSILON)
        return (rng_gamma_lower_density(shape, scale));
    return (rng_gamma_density(value, shape - 1.0, scale,
        shape * std::log(scale), std::lgamma(shape)));
}

double rng_gamma_cdf(double shape, double scale, double value)
{
    if (shape <= 0.0 || scale <= 0.0 || value <= 0.0)
        return (0.0);
    const int32_t segment_total = rng_simpson_segment_total((value / scale) * 128.0);
    const double lower_density = rng_gamma_lower_density(shape, scale);
    const double shape_minus_one = shape - 1.0;
    const double scaled_log_scale = shape * std::log(scale);
    const double log_gamma = std::lgamma(shape);
    const auto integrand = [=](double point)
    {
        if (std::fabs(point) <= RNG_DENSITY_EPSILON)
            return (lower_density);
        return (rng_gamma_density(point, shape_minus_one, scale, scaled_log_scale, log_gamma));
    };
    return (rng_simpson_integral(value, segment_total, integrand));
}

static double rng_beta_boundary_density(double boundary_shape, double other_shape)
{
    if (boundary_shape < 1.0)
        return (std::numeric_limits<double>::infinity());
    if (std::fabs(boundary_shape - 1.0) <= RNG_DENSITY_EPSILON)
        return (other_shape);
    return (0.0);
}

static double rng_beta_log_denominator(double alpha, double beta)
{
    return (std::lgamma(alpha) + std::lgamma(beta) - std::lgamma(alpha + beta));
}

static double rng_beta_density(double value, double alpha_minus_one, double beta_minus_one,
    double log_denominator)
{
    const double log_component = alpha_minus_one * std::log(value)
        + beta_minus_one * std::log(1.0 - value);
    const double result = std::exp(log_component - log_denominator);
    if (std::isfinite(result) == FT_FALSE)
        return (0.0);
    return (result);
}

double rng_beta_pdf(double alpha, double beta, double value)
{
    if (alpha <= 0.0 || beta <= 0.0)
        return (0.0);
    if (value <= 0.0)
        return (rng_beta_boundary_density(alpha, beta));
    if (value >= 1.0)
        return (rng_beta_boundary_density(beta, alpha));
    return (rng_beta_density(value, alpha - 1.0, beta - 1.0,
        rng_beta_log_denominator(alpha, beta)));
}

double rng_beta_cdf(double alpha, double beta, double value)
{
    if (alpha <= 0.0 || beta <= 0.0 || value <= 0.0)
        return (0.0);
    if (value >= 1.0)
        return (1.0);
    const int32_t segment_total = rng_simpson_segment_total((alpha + beta) * 64.0);
    const double lower_density = rng_beta_boundary_density(alpha, beta);
    const double upper_density = rng_beta_boundary_density(beta, alpha);
    const double alpha_minus_one = alpha - 1.0;
    const double beta_minus_one = beta - 1.0;
    const double log_denominator = rng_beta_log_denominator(alpha, beta);
    const auto integrand = [=](double point)
    {
        if (point <= 0.0)
            return (lower_density);
        if (point >= 1.0)
            return (upper_density);
        return (rng_beta_density(point, alpha_minus_one, beta_minus_one, log_denominator));
    };
    return (rng_simpson_integral(value, segment_total, integrand));
}

double rng_chi_squared_pdf(double degrees_of_freedom, double value)
{
    if (degrees_of_freedom <= 0.0)
        return (0.0);
    return (rng_gamma_pdf(degrees_of_freedom * 0.5, 2.0, value));
}

double rng_chi_squared_cdf(double degrees_of_freedom, double value)
{
    if (degrees_of_freedom <= 0.0)
        return (0.0);
    return (rng_gamma_cdf(degrees_of_freedom * 0.5, 2.0, value));
}
