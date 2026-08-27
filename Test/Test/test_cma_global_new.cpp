#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include <new>
#include <cstdint>
#include "../../Modules/Basic/limits.hpp"
#if __has_include(<valgrind/valgrind.h>)
# include <valgrind/valgrind.h>
#endif

#ifndef LIBFT_TEST_BUILD
#endif

struct alignas(64) aligned_trivial_type
{
    int value;
};

struct alignas(128) aligned_large_type
{
    int value;
};

FT_TEST(test_cma_global_new_preserves_alignment)
{
    aligned_trivial_type *instance;
    std::uintptr_t instance_address;
    std::uintptr_t alignment_mask;

    cma_set_alloc_limit(0);
    instance = new (std::nothrow) aligned_trivial_type;
    if (instance == ft_nullptr)
        return (0);
    instance_address = reinterpret_cast<std::uintptr_t>(instance);
    alignment_mask = alignof(aligned_trivial_type) - 1;
    FT_ASSERT_EQ(instance_address & alignment_mask, 0);
    delete instance;
    return (1);
}

FT_TEST(test_cma_global_new_alignment_failure_sets_errno)
{
    void *instance;

    cma_clear_backend();
    cma_set_alloc_limit(0);
    cma_set_alloc_limit(15);
    instance = ::operator new(static_cast<std::size_t>(1),
            static_cast<std::align_val_t>(16), std::nothrow);
    FT_ASSERT_EQ(ft_nullptr, instance);
    cma_clear_backend();
    cma_set_alloc_limit(0);
    return (1);
}
