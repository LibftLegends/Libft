#include "observability_histogram.hpp"

#include <cstdio>
#include <charconv>
#include <cmath>
#include "../Basic/class_nullptr.hpp"

static int32_t observability_histogram_export_emit(
    ft_observability_histogram_exporter exporter, void *user_data,
    const char *chunk) noexcept
{
    if (exporter == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    exporter(chunk, user_data);
    return (FT_ERR_SUCCESS);
}

static int32_t observability_histogram_export_emit_cstring(
    ft_observability_histogram_exporter exporter, void *user_data,
    const char *string) noexcept
{
    if (string == ft_nullptr)
        return (observability_histogram_export_emit(exporter, user_data, ""));
    return (observability_histogram_export_emit(exporter, user_data, string));
}

static int32_t observability_histogram_export_emit_number(
    ft_observability_histogram_exporter exporter, void *user_data,
    double value) noexcept
{
    char number_buffer[64];
    int32_t number_length;

    number_length = std::snprintf(number_buffer, sizeof(number_buffer), "%.17g", value);
    if (number_length < 0)
        return (FT_ERR_SYSTEM);
    if (static_cast<size_t>(number_length) >= sizeof(number_buffer))
        return (FT_ERR_OUT_OF_RANGE);
    return (observability_histogram_export_emit(exporter, user_data, number_buffer));
}

static int32_t observability_histogram_export_emit_uint64(
    ft_observability_histogram_exporter exporter, void *user_data,
    uint64_t value) noexcept
{
    char number_buffer[32];
    const auto conversion = std::to_chars(number_buffer, number_buffer + sizeof(number_buffer) - 1, value);
    if (conversion.ec != std::errc())
        return (FT_ERR_OUT_OF_RANGE);
    *conversion.ptr = '\0';
    return (observability_histogram_export_emit(exporter, user_data, number_buffer));
}

static int32_t observability_histogram_export_json_number(
    ft_observability_histogram_exporter exporter, void *user_data, double value) noexcept
{
    if (!std::isfinite(value))
        return (observability_histogram_export_emit(exporter, user_data, "null"));
    return (observability_histogram_export_emit_number(exporter, user_data, value));
}

static int32_t observability_histogram_export_json_string(
    ft_observability_histogram_exporter exporter, void *user_data,
    const char *string) noexcept
{
    if (exporter == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (string == ft_nullptr)
        string = "";
    constexpr char HEX_DIGITS[] = "0123456789abcdef";
    constexpr char CONTROL_ESCAPES[32] = {0, 0, 0, 0, 0, 0, 0, 0, 'b', 't', 'n', 0, 'f', 'r'};
    char buffer[256];
    ft_size_t used = 0;
    buffer[used++] = '"';
    const auto *character = reinterpret_cast<const unsigned char *>(string);
    while (*character != 0)
    {
        if (used >= sizeof(buffer) - 7)
        {
            buffer[used] = '\0';
            exporter(buffer, user_data);
            used = 0;
        }
        const unsigned char byte = *character++;
        if (byte == '"' || byte == '\\')
        {
            buffer[used++] = '\\';
            buffer[used++] = static_cast<char>(byte);
        }
        else if (byte < 0x20U)
        {
            buffer[used++] = '\\';
            if (CONTROL_ESCAPES[byte] != 0)
                buffer[used++] = CONTROL_ESCAPES[byte];
            else
            {
                buffer[used++] = 'u';
                buffer[used++] = '0';
                buffer[used++] = '0';
                buffer[used++] = HEX_DIGITS[byte >> 4U];
                buffer[used++] = HEX_DIGITS[byte & 0x0fU];
            }
        }
        else
            buffer[used++] = static_cast<char>(byte);
    }
    buffer[used++] = '"';
    buffer[used] = '\0';
    exporter(buffer, user_data);
    return (FT_ERR_SUCCESS);
}

int32_t observability_histogram_initialize(ft_observability_histogram *histogram,
    const char *name, const char *unit,
    ft_observability_histogram_bucket *buckets, ft_size_t bucket_count) noexcept
{
    ft_size_t index;

    if (histogram == ft_nullptr || buckets == ft_nullptr || bucket_count == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0;
    while (index < bucket_count)
    {
        if (std::isnan(buckets[index].upper_bound)
            || (index > 0 && buckets[index].upper_bound < buckets[index - 1].upper_bound))
            return (FT_ERR_INVALID_ARGUMENT);
        ++index;
    }
    histogram->name = name;
    histogram->unit = unit;
    histogram->buckets = buckets;
    histogram->bucket_count = bucket_count;
    observability_histogram_clear(histogram);
    return (FT_ERR_SUCCESS);
}

void observability_histogram_clear(ft_observability_histogram *histogram) noexcept
{
    ft_size_t index;

    if (histogram == ft_nullptr || histogram->buckets == ft_nullptr)
        return ;
    histogram->sum = 0.0;
    histogram->count = 0;
    index = 0;
    while (index < histogram->bucket_count)
    {
        histogram->buckets[index].count = 0;
        index += 1;
    }
    return ;
}

int32_t observability_histogram_record(ft_observability_histogram *histogram,
    double value) noexcept
{
    if (histogram == ft_nullptr || histogram->buckets == ft_nullptr
        || histogram->bucket_count == 0 || std::isnan(value))
        return (FT_ERR_INVALID_ARGUMENT);

    ft_size_t index = 0;
    // The last bucket also absorbs values above every configured bound.
    ft_size_t upper = histogram->bucket_count - 1;
    while (index < upper)
    {
        const ft_size_t middle = index + (upper - index) / 2;
        if (histogram->buckets[middle].upper_bound < value)
            index = middle + 1;
        else
            upper = middle;
    }
    while (index < histogram->bucket_count)
    {
        histogram->buckets[index].count += 1;
        index += 1;
    }
    histogram->sum += value;
    histogram->count += 1;
    return (FT_ERR_SUCCESS);
}

int32_t observability_histogram_export_text(
    const ft_observability_histogram *histogram,
    ft_observability_histogram_exporter exporter, void *user_data) noexcept
{
    ft_size_t index;
    int32_t error_code;

    if (histogram == ft_nullptr || histogram->buckets == ft_nullptr
        || histogram->bucket_count == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = observability_histogram_export_emit(exporter, user_data, "name=");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit_cstring(exporter, user_data,
            histogram->name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit(exporter, user_data, " unit=");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit_cstring(exporter, user_data,
            histogram->unit);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit(exporter, user_data, " count=");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit_uint64(exporter, user_data,
            histogram->count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit(exporter, user_data, " sum=");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit_number(exporter, user_data,
            histogram->sum);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit(exporter, user_data,
            " buckets=[");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0;
    while (index < histogram->bucket_count)
    {
        if (index > 0)
        {
            error_code = observability_histogram_export_emit(exporter, user_data, ",");
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        error_code = observability_histogram_export_emit(exporter, user_data, "{le=");
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = observability_histogram_export_emit_number(exporter, user_data,
                histogram->buckets[index].upper_bound);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = observability_histogram_export_emit(exporter, user_data, ",count=");
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = observability_histogram_export_emit_uint64(exporter, user_data,
                histogram->buckets[index].count);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = observability_histogram_export_emit(exporter, user_data, "}");
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1;
    }
    error_code = observability_histogram_export_emit(exporter, user_data, "]");
    return (error_code);
}

int32_t observability_histogram_export_json(
    const ft_observability_histogram *histogram,
    ft_observability_histogram_exporter exporter, void *user_data) noexcept
{
    ft_size_t index;
    int32_t error_code;

    if (histogram == ft_nullptr || histogram->buckets == ft_nullptr
        || histogram->bucket_count == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = observability_histogram_export_emit(exporter, user_data, "{");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit(exporter, user_data, "\"name\":");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_json_string(exporter, user_data,
            histogram->name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit(exporter, user_data, ",\"unit\":");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_json_string(exporter, user_data,
            histogram->unit);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit(exporter, user_data,
            ",\"count\":");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit_uint64(exporter, user_data,
            histogram->count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit(exporter, user_data,
            ",\"sum\":");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_json_number(exporter, user_data,
            histogram->sum);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = observability_histogram_export_emit(exporter, user_data,
            ",\"buckets\":[");
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0;
    while (index < histogram->bucket_count)
    {
        if (index > 0)
        {
            error_code = observability_histogram_export_emit(exporter, user_data, ",");
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        error_code = observability_histogram_export_emit(exporter, user_data, "{");
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = observability_histogram_export_emit(exporter, user_data,
                "\"upper_bound\":");
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = observability_histogram_export_json_number(exporter, user_data,
                histogram->buckets[index].upper_bound);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = observability_histogram_export_emit(exporter, user_data,
                ",\"count\":");
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = observability_histogram_export_emit_uint64(exporter, user_data,
                histogram->buckets[index].count);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = observability_histogram_export_emit(exporter, user_data, "}");
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1;
    }
    error_code = observability_histogram_export_emit(exporter, user_data, "]}");
    return (error_code);
}
