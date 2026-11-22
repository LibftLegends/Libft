#include "../CPP_class/bitset.hpp"
#include <cstdint>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

[[maybe_unused]] static int32_t compile_bitset_usage()
{
    ft_bitset bitset_value(8);
    int32_t test_value;

    bitset_value.set(1);
    test_value = 0;
    if (bitset_value.test(1))
        test_value = 1;
    return (test_value);
}

