#include "variant.hpp"
#include <cstdint>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

[[maybe_unused]] static int32_t check_variant_usage()
{
    ft_variant<int32_t, const char*> variant_value;
    variant_value.emplace<int32_t>(42);
    int32_t stored_integer = variant_value.get<int32_t>();
    (void)stored_integer;
    variant_value.emplace<const char*>("hello");
    const char *stored_string = variant_value.get<const char*>();
    (void)stored_string;
    variant_value.visit([](const auto &value)
    {
        (void)value;
    });
    variant_value.reset();
    return (static_cast<int32_t>(variant_value.holds_alternative<int32_t>())
            + static_cast<int32_t>(variant_value.holds_alternative<const char*>()));
}

