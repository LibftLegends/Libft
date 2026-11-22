#include "optional.hpp"
#include <cstdint>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

[[maybe_unused]] static int32_t check_optional_usage()
{
    ft_optional<int32_t> initial_value(21);
    ft_optional<int32_t> moved_value;
    (void)moved_value.move(initial_value);

    if (moved_value.has_value())
    {
        int32_t result = moved_value.value();
        (void)result;
    }
    moved_value.reset();
    return (static_cast<int32_t>(initial_value.has_value())
            + static_cast<int32_t>(moved_value.has_value()));
}

