#include "../test_internal.hpp"
#include "../../Modules/Template/optional.hpp"
#include "../../Modules/Template/variant.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>
#include <utility>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_optional_reports_empty_state)
{
    ft_optional<int> optional_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, optional_value.initialize());
    FT_ASSERT_EQ(false, optional_value.has_value());
    int &fallback_value = optional_value.value();

    FT_ASSERT_EQ(0, fallback_value);
    FT_ASSERT_EQ(FT_ERR_EMPTY, optional_value.get_error());
    return (1);
}

FT_TEST(test_ft_optional_holds_value_and_resets)
{
    ft_optional<int> optional_value(42);

    FT_ASSERT(optional_value.has_value());
    FT_ASSERT_EQ(42, optional_value.value());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, optional_value.get_error());
    optional_value.reset();
    FT_ASSERT_EQ(false, optional_value.has_value());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, optional_value.get_error());
    return (1);
}

FT_TEST(test_ft_optional_move_transfers_state)
{
    ft_optional<int> source_optional(99);
    ft_optional<int> destination_optional;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_optional.enable_thread_safety());
    FT_ASSERT_EQ(0, destination_optional.move(source_optional));
    FT_ASSERT(destination_optional.has_value());
    FT_ASSERT_EQ(99, destination_optional.value());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_optional.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_optional.disable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_optional.get_error());
    return (1);
}

FT_TEST(test_ft_optional_move_constructor_preserves_thread_safety)
{
    ft_optional<int> source_optional(123);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_optional.enable_thread_safety());

    ft_optional<int> moved_optional;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_optional.move(source_optional));

    FT_ASSERT(moved_optional.has_value());
    FT_ASSERT_EQ(123, moved_optional.value());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_optional.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_optional.disable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_optional.destroy());
    return (1);
}

FT_TEST(test_ft_variant_emplace_and_get)
{
    ft_variant<int, const char*> variant_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, variant_value.initialize());
    variant_value.emplace<int>(17);
    FT_ASSERT(variant_value.holds_alternative<int>());
    FT_ASSERT_EQ(17, variant_value.get<int>());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, variant_value.get_error());
    variant_value.emplace<const char*>("hello");
    FT_ASSERT(variant_value.holds_alternative<const char*>());
    const char *string_value = variant_value.get<const char*>();
    FT_ASSERT_EQ(0, std::strcmp("hello", string_value));
    int wrong_access = variant_value.get<int>();
    FT_ASSERT_EQ(0, wrong_access);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, variant_value.get_error());
    return (1);
}

FT_TEST(test_ft_variant_visit_and_reset)
{
    ft_variant<int, long> variant_value(12);
    long visit_sum = 0;

    variant_value.visit([&visit_sum](const auto &value){ visit_sum += value; });
    FT_ASSERT_EQ(12L, visit_sum);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, variant_value.get_error());
    variant_value.reset();
    FT_ASSERT_EQ(false, variant_value.holds_alternative<int>());
    visit_sum = 5;
    variant_value.visit([&visit_sum](const auto &value){ visit_sum += value; });
    FT_ASSERT_EQ(5L, visit_sum);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, variant_value.get_error());
    return (1);
}

struct template_fallback_test_value
{
    static inline uint32_t default_constructions = 0U;
    uint32_t value;

    template_fallback_test_value() : value(0U)
    {
        ++default_constructions;
        return ;
    }
};

FT_TEST(test_ft_optional_constructs_value_fallbacks_only_on_failure)
{
    template_fallback_test_value seed;
    seed.value = 42U;
    const uint32_t initial_constructions = template_fallback_test_value::default_constructions;
    ft_optional<template_fallback_test_value> optional_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, optional_value.initialize(seed));
    const ft_optional<template_fallback_test_value> &const_optional = optional_value;
    FT_ASSERT_EQ(42U, optional_value.value().value);
    FT_ASSERT_EQ(42U, const_optional.value().value);
    FT_ASSERT_EQ(initial_constructions, template_fallback_test_value::default_constructions);

    optional_value.reset();
    template_fallback_test_value &mutable_fallback = optional_value.value();
    FT_ASSERT_EQ(FT_ERR_EMPTY, optional_value.get_error());
    FT_ASSERT_EQ(initial_constructions + 1U,
        template_fallback_test_value::default_constructions);
    const template_fallback_test_value &const_fallback = const_optional.value();
    FT_ASSERT_EQ(FT_ERR_EMPTY, const_optional.get_error());
    FT_ASSERT_EQ(initial_constructions + 2U,
        template_fallback_test_value::default_constructions);
    FT_ASSERT_EQ(&mutable_fallback, &optional_value.value());
    FT_ASSERT_EQ(&const_fallback, &const_optional.value());
    FT_ASSERT_EQ(initial_constructions + 2U,
        template_fallback_test_value::default_constructions);
    return (1);
}

FT_TEST(test_ft_variant_constructs_value_fallbacks_only_on_failure)
{
    template_fallback_test_value seed;
    seed.value = 73U;
    const uint32_t initial_constructions = template_fallback_test_value::default_constructions;
    ft_variant<template_fallback_test_value, uint64_t> variant;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, variant.initialize(seed));
    const ft_variant<template_fallback_test_value, uint64_t> &const_variant = variant;
    const auto stored_proxy = variant.get<template_fallback_test_value>();
    const auto const_stored_proxy = const_variant.get<template_fallback_test_value>();
    template_fallback_test_value &stored_value = stored_proxy;
    const template_fallback_test_value &const_stored_value = const_stored_proxy;
    FT_ASSERT_EQ(73U, stored_value.value);
    FT_ASSERT_EQ(&stored_value, &const_stored_value);
    FT_ASSERT_EQ(initial_constructions, template_fallback_test_value::default_constructions);

    variant.reset();
    const auto mutable_proxy = variant.get<template_fallback_test_value>();
    const auto const_proxy = const_variant.get<template_fallback_test_value>();
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, mutable_proxy.get_error());
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, const_proxy.get_error());
    template_fallback_test_value &mutable_fallback = mutable_proxy;
    const template_fallback_test_value &const_fallback = const_proxy;
    FT_ASSERT_EQ(0U, mutable_fallback.value);
    FT_ASSERT_EQ(0U, const_fallback.value);
    FT_ASSERT_EQ(initial_constructions + 2U,
        template_fallback_test_value::default_constructions);
    return (1);
}
