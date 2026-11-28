#include "../Basic/basic.hpp"
#include <cstdint>
#include "algorithm.hpp"
#include "constructor.hpp"
#include "container_serialization.hpp"
#include "invoke.hpp"
#include "move.hpp"
#include "math.hpp"
#include "static_cast.hpp"
#include "string_view.hpp"
#include "swap.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../RNG/rng.hpp"
#include "pair.hpp"

struct s_invoke_reference_probe
{
    int32_t value;

    constexpr int32_t &get_value() & noexcept;
};

constexpr int32_t &s_invoke_reference_probe::get_value() & noexcept
{
    return (this->value);
}

static constexpr ft_bool invoke_preserves_references()
{
    s_invoke_reference_probe probe = {0};
    ft_invoke(&s_invoke_reference_probe::value, &probe) = 12;
    ft_invoke(&s_invoke_reference_probe::get_value, probe) = 42;
    ft_invoke([&probe]() -> int32_t & { return (probe.value); }) += 1;
    return (probe.value == 43);
}

static_assert(invoke_preserves_references());

static constexpr ft_bool algorithms_work_in_constant_evaluation()
{
    int32_t values[] = {3, 2, 1};
    ft_reverse(values, values + 3);
    return (values[0] == 1 && ft_binary_search(values, values + 3, 2)
        && !ft_binary_search(values, values + 3, 4));
}

static_assert(algorithms_work_in_constant_evaluation());

[[maybe_unused]] static int32_t instantiate_free_functions()
{
    int32_t data[] = {1, 2, 3};
    ft_sort(data, data + 3);
    ft_sort(data, data + 3, [](const int32_t &left, const int32_t &right)
    {
        return (left < right);
    });
    ft_binary_search(data, data + 3, 2);
    ft_binary_search(data, data + 3, 2, [](const int32_t &left, const int32_t &right)
    {
        return (left < right);
    });
    ft_shuffle(data, data + 3);
    ft_reverse(data, data + 3);
    int32_t max_value = ft_max(1, 2);
    int32_t min_value = ft_min(1, 2);
    ft_swap(data[0], data[1]);
    construct_at(&data[0], 5);
    destroy_at(&data[0]);
    int32_t sample = 7;
    auto moved_sample = ft_move(sample);
    (void)moved_sample;
    int32_t target_value = 0;
    static_cast_or_construct<int32_t>(&target_value);
    int32_t invoked_value = ft_invoke([](int32_t value)
    {
        return (value + 1);
    }, 5);
    (void)invoked_value;
    ft_string serialized;
    default_string_serializer<int32_t>(42, serialized);
    default_string_serializer<const char *>("abc", serialized);
    ft_string_view<char> view("hello");
    (void)view.size();
    ft_string_view<char> substring;
    (void)view.substr(1, substring);
    (void)substring.compare(view);
    return (max_value + min_value);
}

