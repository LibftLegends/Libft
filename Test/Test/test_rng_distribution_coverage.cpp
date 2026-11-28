#include "../test_internal.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_rng_test_utils.hpp"
#include <cmath>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_rng_gamma_pdf_cdf_reference)
{
    double pdf_value;
    double cdf_value;
    double expected_pdf;
    double expected_cdf;
    double difference;

    expected_pdf = 0.19196788093577974;
    expected_cdf = 0.45058404864721977;
    pdf_value = rng_gamma_pdf(2.5, 1.5, 3.0);
    difference = pdf_value - expected_pdf;
    if (difference < 0.0)
        difference = -difference;
    if (difference > 0.000001)
        return (0);
    cdf_value = rng_gamma_cdf(2.5, 1.5, 3.0);
    difference = cdf_value - expected_cdf;
    if (difference < 0.0)
        difference = -difference;
    if (difference > 0.01)
        return (0);
    return (1);
}

FT_TEST(test_rng_beta_pdf_cdf_reference)
{
    double pdf_value;
    double cdf_value;
    double expected_pdf;
    double expected_cdf;
    double difference;

    expected_pdf = 1.5552;
    expected_cdf = 0.76672;
    pdf_value = rng_beta_pdf(2.0, 5.0, 0.4);
    difference = pdf_value - expected_pdf;
    if (difference < 0.0)
        difference = -difference;
    if (difference > 0.0001)
        return (0);
    cdf_value = rng_beta_cdf(2.0, 5.0, 0.4);
    difference = cdf_value - expected_cdf;
    if (difference < 0.0)
        difference = -difference;
    if (difference > 0.01)
        return (0);
    return (1);
}

FT_TEST(test_rng_distribution_cdf_singular_endpoints_and_segment_cap)
{
    const double gamma_cdf = rng_gamma_cdf(0.5, 2.0, 1.0);
    const double beta_cdf = rng_beta_cdf(0.5, 0.5, 0.5);
    const double concentrated_beta_cdf = rng_beta_cdf(1.0, 1000000000.0, 0.000000001);

    FT_ASSERT(std::isinf(rng_gamma_pdf(0.5, 2.0, 0.0)));
    FT_ASSERT(std::isinf(rng_beta_pdf(0.5, 0.5, 0.0)));
    FT_ASSERT(std::isinf(rng_beta_pdf(0.5, 0.5, 1.0)));
    FT_ASSERT(std::fabs(gamma_cdf - std::erf(std::sqrt(0.5))) < 0.02);
    FT_ASSERT(std::fabs(beta_cdf - 0.5) < 0.02);
    FT_ASSERT(std::fabs(concentrated_beta_cdf - (1.0 - std::exp(-1.0))) < 0.00001);
    return (1);
}

FT_TEST(test_rng_chi_squared_pdf_cdf_reference)
{
    double pdf_value;
    double cdf_value;
    double expected_pdf;
    double expected_cdf;
    double difference;

    expected_pdf = 0.08790080915922291;
    expected_cdf = 0.7602705204748466;
    pdf_value = rng_chi_squared_pdf(4.0, 5.5);
    difference = pdf_value - expected_pdf;
    if (difference < 0.0)
        difference = -difference;
    if (difference > 0.0001)
        return (0);
    cdf_value = rng_chi_squared_cdf(4.0, 5.5);
    difference = cdf_value - expected_cdf;
    if (difference < 0.0)
        difference = -difference;
    if (difference > 0.01)
        return (0);
    return (1);
}

FT_TEST(test_rng_gamma_sampling_mean)
{
    const int sample_total = 4096;
    double sum;
    int index;
    double mean_value;
    double difference;

    ft_rng_test_seed_engine(424242u, "rng_gamma_sampling_mean");
    sum = 0.0;
    index = 0;
    while (index < sample_total)
    {
        sum = sum + static_cast<double>(ft_random_gamma(2.0f, 3.0f));
        index = index + 1;
    }
    mean_value = sum / static_cast<double>(sample_total);
    difference = mean_value - 6.0;
    if (difference < 0.0)
        difference = -difference;
    if (difference > 0.2)
        return (0);
    return (1);
}

FT_TEST(test_rng_beta_sampling_mean)
{
    const int sample_total = 4096;
    double sum;
    int index;
    double mean_value;
    double expected;
    double difference;

    ft_rng_test_seed_engine(987654u, "rng_beta_sampling_mean");
    sum = 0.0;
    index = 0;
    while (index < sample_total)
    {
        sum = sum + static_cast<double>(ft_random_beta(2.0f, 5.0f));
        index = index + 1;
    }
    mean_value = sum / static_cast<double>(sample_total);
    expected = 2.0 / 7.0;
    difference = mean_value - expected;
    if (difference < 0.0)
        difference = -difference;
    if (difference > 0.02)
        return (0);
    return (1);
}

FT_TEST(test_rng_chi_squared_sampling_mean)
{
    const int sample_total = 4096;
    double sum;
    int index;
    double mean_value;
    double difference;

    ft_rng_test_seed_engine(246813u, "rng_chi_squared_sampling_mean");
    sum = 0.0;
    index = 0;
    while (index < sample_total)
    {
        sum = sum + static_cast<double>(ft_random_chi_squared(5.0f));
        index = index + 1;
    }
    mean_value = sum / static_cast<double>(sample_total);
    difference = mean_value - 5.0;
    if (difference < 0.0)
        difference = -difference;
    if (difference > 0.3)
        return (0);
    return (1);
}
