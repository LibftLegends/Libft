#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/CMA/cma_internal.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/limits.hpp"
FT_TEST(test_cma_arena_small_malloc_uses_normal_cma_api)
{
    void *first_pointer;
    void *second_pointer;

    first_pointer = cma_malloc(24);
    second_pointer = cma_malloc(32);
    FT_ASSERT(first_pointer != ft_nullptr);
    FT_ASSERT(second_pointer != ft_nullptr);
    FT_ASSERT(first_pointer != second_pointer);
    FT_ASSERT_EQ(32, cma_alloc_size(first_pointer));
    FT_ASSERT_EQ(32, cma_alloc_size(second_pointer));
    cma_free(first_pointer);
    cma_free(second_pointer);
    return (1);
}

FT_TEST(test_cma_arena_small_allocations_reset_after_all_freed)
{
    void *first_pointer;
    void *second_pointer;

    first_pointer = cma_malloc(16);
    FT_ASSERT(first_pointer != ft_nullptr);
    cma_free(first_pointer);
    second_pointer = cma_malloc(16);
    FT_ASSERT(second_pointer != ft_nullptr);
    FT_ASSERT_EQ(16, cma_alloc_size(second_pointer));
    cma_free(second_pointer);
    return (1);
}

FT_TEST(test_cma_arena_reuses_freed_size_class_while_live)
{
    void *first_pointer;
    void *second_pointer;
    void *reused_pointer;

    first_pointer = cma_malloc(64);
    second_pointer = cma_malloc(64);
    FT_ASSERT(first_pointer != ft_nullptr);
    FT_ASSERT(second_pointer != ft_nullptr);
    cma_free(first_pointer);
    reused_pointer = cma_malloc(64);
    FT_ASSERT_EQ(first_pointer, reused_pointer);
    cma_free(reused_pointer);
    cma_free(second_pointer);
    return (1);
}

FT_TEST(test_cma_arena_reuses_every_small_size_class_lifo)
{
    void *first_pointer;
    void *second_pointer;
    void *first_reused;
    void *second_reused;
    ft_size_t size;

    size = 16;
    while (size <= 256)
    {
        first_pointer = cma_malloc(size);
        second_pointer = cma_malloc(size);
        FT_ASSERT(first_pointer != ft_nullptr);
        FT_ASSERT(second_pointer != ft_nullptr);
        cma_free(first_pointer);
        cma_free(second_pointer);
        second_reused = cma_malloc(size);
        first_reused = cma_malloc(size);
        FT_ASSERT_EQ(second_pointer, second_reused);
        FT_ASSERT_EQ(first_pointer, first_reused);
        cma_free(first_reused);
        cma_free(second_reused);
        size += 16;
    }
    return (1);
}

FT_TEST(test_cma_arena_double_free_does_not_duplicate_free_list_entry)
{
    void *first_pointer;
    void *second_pointer;
    void *reused_pointer;
    void *next_pointer;

    first_pointer = cma_malloc(64);
    second_pointer = cma_malloc(64);
    FT_ASSERT(first_pointer != ft_nullptr);
    FT_ASSERT(second_pointer != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_checked_free(first_pointer));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cma_checked_free(first_pointer));
    reused_pointer = cma_malloc(64);
    FT_ASSERT_EQ(first_pointer, reused_pointer);
    next_pointer = cma_malloc(64);
    FT_ASSERT(next_pointer != reused_pointer);
    cma_free(reused_pointer);
    cma_free(next_pointer);
    cma_free(second_pointer);
    return (1);
}

FT_TEST(test_cma_free_bins_reuse_large_block)
{
    void *first_pointer;
    void *second_pointer;
    void *reused_pointer;

    first_pointer = cma_malloc(512);
    second_pointer = cma_malloc(512);
    FT_ASSERT(first_pointer != ft_nullptr);
    FT_ASSERT(second_pointer != ft_nullptr);
    cma_free(first_pointer);
    reused_pointer = cma_malloc(480);
    FT_ASSERT_EQ(first_pointer, reused_pointer);
    cma_free(reused_pointer);
    cma_free(second_pointer);
    return (1);
}

FT_TEST(test_cma_single_thread_mode_keeps_allocator_checks)
{
    void *memory_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_thread_safety(FT_FALSE));
    FT_ASSERT_EQ(FT_FALSE, cma_is_thread_safe_enabled());
    memory_pointer = cma_malloc(512);
    FT_ASSERT(memory_pointer != ft_nullptr);
    FT_ASSERT_EQ(512, cma_alloc_size(memory_pointer));
    cma_free(memory_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_thread_safety(FT_TRUE));
    FT_ASSERT_EQ(FT_TRUE, cma_is_thread_safe_enabled());
    return (1);
}

FT_TEST(test_cma_thread_safety_transition_requires_quiescence)
{
    ft_bool lock_acquired;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_thread_safety(FT_TRUE));
    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_lock_allocator(&lock_acquired));
    FT_ASSERT_EQ(FT_TRUE, lock_acquired);
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, cma_set_thread_safety(FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_unlock_allocator(lock_acquired));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_thread_safety(FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_thread_safety(FT_TRUE));
    return (1);
}

FT_TEST(test_cma_arena_small_realloc_copies_data)
{
    char *memory_pointer;
    char *new_pointer;

    memory_pointer = static_cast<char *>(cma_malloc(8));
    FT_ASSERT(memory_pointer != ft_nullptr);
    memory_pointer[0] = 'a';
    memory_pointer[1] = 'r';
    memory_pointer[2] = 'e';
    memory_pointer[3] = 'n';
    memory_pointer[4] = 'a';
    memory_pointer[5] = '\0';
    new_pointer = static_cast<char *>(cma_realloc(memory_pointer, 64));
    FT_ASSERT(new_pointer != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(new_pointer, "arena"));
    FT_ASSERT_EQ(64, cma_alloc_size(new_pointer));
    cma_free(new_pointer);
    return (1);
}

FT_TEST(test_cma_arena_small_realloc_to_large_copies_data)
{
    char *memory_pointer;
    char *new_pointer;

    memory_pointer = static_cast<char *>(cma_malloc(8));
    FT_ASSERT(memory_pointer != ft_nullptr);
    memory_pointer[0] = 'c';
    memory_pointer[1] = 'm';
    memory_pointer[2] = 'a';
    memory_pointer[3] = '\0';
    new_pointer = static_cast<char *>(cma_realloc(memory_pointer, 1024));
    FT_ASSERT(new_pointer != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(new_pointer, "cma"));
    FT_ASSERT_EQ(1024, cma_alloc_size(new_pointer));
    cma_free(new_pointer);
    return (1);
}

FT_TEST(test_cma_arena_small_aligned_alloc_uses_normal_cma_api)
{
    void *memory_pointer;
    uintptr_t address;

    memory_pointer = cma_aligned_alloc(64, 48);
    FT_ASSERT(memory_pointer != ft_nullptr);
    address = reinterpret_cast<uintptr_t>(memory_pointer);
    FT_ASSERT_EQ(0, address % 64);
    FT_ASSERT_EQ(48, cma_alloc_size(memory_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_checked_free(memory_pointer));
    return (1);
}
