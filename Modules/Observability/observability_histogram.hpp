#ifndef OBSERVABILITY_HISTOGRAM_HPP
# define OBSERVABILITY_HISTOGRAM_HPP

#include "../Errno/errno.hpp"

struct ft_observability_histogram_bucket
{
    double upper_bound;
    uint64_t count;
};

struct ft_observability_histogram
{
    const char *name;
    const char *unit;
    ft_observability_histogram_bucket *buckets;
    ft_size_t bucket_count;
    double sum;
    uint64_t count;
};

typedef void (*ft_observability_histogram_exporter)(const char *chunk,
    void *user_data);

// Upper bounds must be ordered, without NaNs, and remain unchanged while recording.
int32_t observability_histogram_initialize(ft_observability_histogram *histogram,
    const char *name, const char *unit,
    ft_observability_histogram_bucket *buckets, ft_size_t bucket_count) noexcept;
void observability_histogram_clear(ft_observability_histogram *histogram) noexcept;
int32_t observability_histogram_record(ft_observability_histogram *histogram,
    double value) noexcept;
int32_t observability_histogram_export_text(
    const ft_observability_histogram *histogram,
    ft_observability_histogram_exporter exporter, void *user_data) noexcept;
// Nonfinite sums and bounds are exported as JSON null.
int32_t observability_histogram_export_json(
    const ft_observability_histogram *histogram,
    ft_observability_histogram_exporter exporter, void *user_data) noexcept;

#endif
