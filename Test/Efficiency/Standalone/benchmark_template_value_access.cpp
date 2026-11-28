#include "../../../Modules/Template/optional.hpp"
#include "../../../Modules/Template/tuple.hpp"
#include "../../../Modules/Template/variant.hpp"
#include <chrono>
#include <cstdio>

struct template_access_value
{
    uint32_t number;
    static inline uint32_t default_constructions = 0U;

    template_access_value() : number(1U)
    {
        ++default_constructions;
        return ;
    }
};

static uint32_t read_optional(const ft_optional<template_access_value> &value)
{
    return (value.value().number);
}

static uint32_t read_tuple(const ft_tuple<template_access_value, uint64_t> &value)
{
    return (value.get<0>().number);
}

static uint32_t read_variant(const ft_variant<template_access_value, uint64_t> &value)
{
    const auto proxy = value.get<template_access_value>();
    const template_access_value &element = proxy;
    return (element.number);
}

template <typename Container>
static int32_t measure_reads(const char *name, const Container &value,
    uint32_t (*read_value)(const Container &))
{
    constexpr uint32_t ITERATIONS = 3000000U;
    const uint32_t previous_constructions = template_access_value::default_constructions;
    auto *volatile read_element = read_value;
    uint32_t iteration = 0U;
    uint32_t sum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        sum += read_element(value);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (sum != ITERATIONS)
        return (1);
    std::printf("%s fallback_constructions=%u ns/op=%.2f\n", name,
        template_access_value::default_constructions - previous_constructions,
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}

int main()
{
    template_access_value seed;
    ft_optional<template_access_value> optional;
    ft_variant<template_access_value, uint64_t> variant;
    ft_tuple<template_access_value, uint64_t> tuple(seed, uint64_t{0});
    if (optional.initialize(seed) != FT_ERR_SUCCESS
        || variant.initialize(seed) != FT_ERR_SUCCESS)
        return (1);
    if (measure_reads("optional_value_read", optional, read_optional) != 0
        || measure_reads("variant_value_read", variant, read_variant) != 0)
        return (1);
    return (measure_reads("tuple_value_read", tuple, read_tuple));
}
