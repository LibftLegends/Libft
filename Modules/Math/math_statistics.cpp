#include "math.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

static ft_bool math_statistics_is_ordered(const double *values, int32_t array_size)
{
    int32_t index = 1;
    while (index < array_size)
    {
        if (values[index] < values[index - 1])
            return (FT_FALSE);
        index += 1;
    }
    return (FT_TRUE);
}

static constexpr int32_t STATISTICS_SMALL_ARRAY_SIZE = 32;

static double *math_statistics_copy(const double *values, int32_t array_size,
    double *local_copy, std::vector<double> &allocated_copy)
{
    if (array_size <= STATISTICS_SMALL_ARRAY_SIZE)
    {
        std::copy_n(values, array_size, local_copy);
        return (local_copy);
    }
    allocated_copy.assign(values, values + array_size);
    return (allocated_copy.data());
}

static void math_statistics_sort_run(double *first, double *last)
{
    if (last - first > STATISTICS_SMALL_ARRAY_SIZE)
    {
        std::stable_sort(first, last);
        return ;
    }
    if (first == last)
        return ;
    double *next = first + 1;
    while (next != last)
    {
        const double value = *next;
        double *position = next;
        while (position != first && value < position[-1])
        {
            *position = position[-1];
            position -= 1;
        }
        *position = value;
        next += 1;
    }
    return ;
}

static void math_statistics_sort(double *first, double *last)
{
    // The previous adjacent swaps left NaNs in place and preserved equal-value order.
    double *run_start = first;
    while (first != last)
    {
        if (std::isnan(*first))
        {
            math_statistics_sort_run(run_start, first);
            run_start = first + 1;
        }
        first += 1;
    }
    math_statistics_sort_run(run_start, last);
    return ;
}

static ft_bool math_statistics_needs_stable_median(const double *first,
    const double *last)
{
    while (first != last)
    {
        if (std::isnan(*first) || (std::fpclassify(*first) == FP_ZERO && std::signbit(*first)))
            return (FT_TRUE);
        first += 1;
    }
    return (FT_FALSE);
}

double ft_mean(const double *values, int32_t array_size)
{
    int32_t array_index;
    double sum;

    if (array_size <= 0)
        return (0.0);
    array_index = 0;
    sum = 0.0;
    while (array_index < array_size)
    {
        sum += values[array_index];
        array_index++;
    }
    return (sum / array_size);
}

static double math_statistics_middle(const double *values, int32_t array_size)
{
    const int32_t middle = array_size / 2;
    if (array_size % 2 != 0)
        return (values[middle]);
    return (ft_mean(values + middle - 1, 2));
}

double ft_median(const double *values, int32_t array_size)
{
    if (array_size <= 0)
        return (0.0);
    if (math_statistics_is_ordered(values, array_size))
        return (math_statistics_middle(values, array_size));

    double local_copy[STATISTICS_SMALL_ARRAY_SIZE];
    std::vector<double> allocated_copy;
    double *first = math_statistics_copy(values, array_size, local_copy, allocated_copy);
    double *last = first + array_size;
    if (math_statistics_needs_stable_median(first, last))
    {
        math_statistics_sort(first, last);
        return (math_statistics_middle(first, array_size));
    }
    double *middle = first + array_size / 2;
    std::nth_element(first, middle, last);
    if (array_size % 2 != 0)
        return (*middle);
    const double ends[2] = {*std::max_element(first, middle), *middle};
    return (ft_mean(ends, 2));
}

double ft_mode(const double *values, int32_t array_size)
{
    constexpr double EPSILON = 0.000001;

    if (array_size <= 0)
        return (0.0);

    double local_copy[STATISTICS_SMALL_ARRAY_SIZE];
    std::vector<double> allocated_copy;
    if (!math_statistics_is_ordered(values, array_size))
    {
        double *first = math_statistics_copy(values, array_size, local_copy, allocated_copy);
        math_statistics_sort(first, first + array_size);
        values = first;
    }
    int32_t array_index = 1;
    int32_t current_count = 1;
    int32_t best_count = 1;
    double mode_value = values[0];
    while (array_index < array_size)
    {
        // Each pair is ordered or contains a NaN, which breaks the run.
        if (values[array_index] - values[array_index - 1] <= EPSILON)
        {
            current_count += 1;
            if (current_count > best_count)
            {
                best_count = current_count;
                mode_value = values[array_index];
            }
        }
        else
            current_count = 1;
        array_index += 1;
    }
    return (mode_value);
}

double ft_variance(const double *values, int32_t array_size)
{
    int32_t array_index;
    double mean_value;
    double sum;
    double difference;

    if (array_size <= 0)
        return (0.0);
    mean_value = ft_mean(values, array_size);
    array_index = 0;
    sum = 0.0;
    while (array_index < array_size)
    {
        difference = values[array_index] - mean_value;
        sum += difference * difference;
        array_index++;
    }
    return (sum / array_size);
}

double ft_stddev(const double *values, int32_t array_size)
{
    double variance_value;

    variance_value = ft_variance(values, array_size);
    return (math_sqrt(variance_value));
}

double ft_geometric_mean(const double *values, int32_t array_size)
{
    int32_t array_index;
    double log_sum;

    if (array_size <= 0)
        return (0.0);
    array_index = 0;
    log_sum = 0.0;
    while (array_index < array_size)
    {
        if (values[array_index] <= 0.0)
            return (0.0);
        log_sum = log_sum + math_log(values[array_index]);
        array_index++;
    }
    return (math_exp(log_sum / array_size));
}

double ft_harmonic_mean(const double *values, int32_t array_size)
{
    int32_t array_index;
    double reciprocal_sum;

    if (array_size <= 0)
        return (0.0);
    array_index = 0;
    reciprocal_sum = 0.0;
    while (array_index < array_size)
    {
        if (math_fabs(values[array_index]) <= DBL_EPSILON)
            return (0.0);
        reciprocal_sum = reciprocal_sum + (1.0 / values[array_index]);
        array_index++;
    }
    return ((static_cast<double>(array_size)) / reciprocal_sum);
}
