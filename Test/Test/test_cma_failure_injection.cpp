#include "../test_internal.hpp"
#include "test_cma_failure_injection.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
# include <malloc.h>
#endif

static int32_t test_cma_failure_find_slot(
    const test_cma_failure_controller *controller,
    const void *memory_pointer) noexcept
{
    ft_size_t allocation_index;

    if (controller == ft_nullptr || memory_pointer == ft_nullptr)
        return (-1);
    allocation_index = 0U;
    while (allocation_index < controller->allocation_slots)
    {
        if (controller->allocations[allocation_index].memory_pointer
            == memory_pointer)
            return (static_cast<int32_t>(allocation_index));
        allocation_index += 1U;
    }
    return (-1);
}

static void test_cma_failure_release_pointer(void *memory_pointer,
    ft_bool aligned) noexcept
{
    if (memory_pointer == ft_nullptr)
        return ;
#ifdef _WIN32
    if (aligned == FT_TRUE)
        _aligned_free(memory_pointer);
    else
        std::free(memory_pointer);
#else
    (void)aligned;
    std::free(memory_pointer);
#endif
    return ;
}

static void test_cma_failure_remove_slot(
    test_cma_failure_controller *controller, ft_size_t allocation_index)
    noexcept
{
    if (controller == ft_nullptr
        || allocation_index >= controller->allocation_slots)
        return ;
    controller->allocations[allocation_index]
        = controller->allocations[controller->allocation_slots - 1U];
    controller->allocations[controller->allocation_slots - 1U].memory_pointer
        = ft_nullptr;
    controller->allocations[controller->allocation_slots - 1U].size = 0U;
    controller->allocations[controller->allocation_slots - 1U].alignment = 0U;
    controller->allocations[controller->allocation_slots - 1U].aligned
        = FT_FALSE;
    controller->allocation_slots -= 1U;
    return ;
}

static ft_size_t test_cma_failure_min_size(ft_size_t first_size,
    ft_size_t second_size) noexcept
{
    if (first_size < second_size)
        return (first_size);
    return (second_size);
}

static ft_bool test_cma_failure_should_fail(
    const test_cma_failure_controller *controller,
    test_cma_failure_operation operation, ft_size_t call_number) noexcept
{
    if (controller == ft_nullptr)
        return (FT_FALSE);
    if (operation == TEST_CMA_FAILURE_ALLOCATE)
        return (controller->allocation_failure_call != 0U
            && controller->allocation_failure_call == call_number);
    if (operation == TEST_CMA_FAILURE_REALLOCATE)
        return (controller->reallocation_failure_call != 0U
            && controller->reallocation_failure_call == call_number);
    if (operation == TEST_CMA_FAILURE_ALIGNED_ALLOCATE)
        return (controller->aligned_failure_call != 0U
            && controller->aligned_failure_call == call_number);
    return (FT_FALSE);
}

static int32_t test_cma_failure_track_allocation(
    test_cma_failure_controller *controller, void *memory_pointer,
    ft_size_t size, ft_size_t alignment, ft_bool aligned) noexcept
{
    if (memory_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (controller == ft_nullptr
        || controller->allocation_slots
            >= TEST_CMA_FAILURE_ALLOCATION_CAPACITY)
    {
        test_cma_failure_release_pointer(memory_pointer, aligned);
        return (FT_ERR_OUT_OF_RANGE);
    }
    controller->allocations[controller->allocation_slots].memory_pointer
        = memory_pointer;
    controller->allocations[controller->allocation_slots].size = size;
    controller->allocations[controller->allocation_slots].alignment = alignment;
    controller->allocations[controller->allocation_slots].aligned = aligned;
    controller->allocation_slots += 1U;
    return (FT_ERR_SUCCESS);
}

static void *test_cma_failure_allocate(ft_size_t size, void *user_data)
{
    test_cma_failure_controller *controller
        = static_cast<test_cma_failure_controller *>(user_data);
    void *memory_pointer;

    if (controller == ft_nullptr)
        return (ft_nullptr);
    controller->allocation_attempts += 1U;
    if (test_cma_failure_should_fail(controller, TEST_CMA_FAILURE_ALLOCATE,
            controller->allocation_attempts) == FT_TRUE)
        return (ft_nullptr);
    memory_pointer = std::malloc(size);
    if (test_cma_failure_track_allocation(controller, memory_pointer, size,
            0U, FT_FALSE) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    return (memory_pointer);
}

static void *test_cma_failure_aligned_allocate(ft_size_t alignment,
    ft_size_t size, void *user_data)
{
    test_cma_failure_controller *controller
        = static_cast<test_cma_failure_controller *>(user_data);
    void *memory_pointer = ft_nullptr;

    if (controller == ft_nullptr)
        return (ft_nullptr);
    controller->aligned_allocation_attempts += 1U;
    if (test_cma_failure_should_fail(controller,
            TEST_CMA_FAILURE_ALIGNED_ALLOCATE,
            controller->aligned_allocation_attempts) == FT_TRUE)
        return (ft_nullptr);
#ifdef _WIN32
    memory_pointer = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&memory_pointer, alignment, size) != 0)
        memory_pointer = ft_nullptr;
#endif
    if (test_cma_failure_track_allocation(controller, memory_pointer, size,
            alignment, FT_TRUE) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    return (memory_pointer);
}

static void *test_cma_failure_reallocate(void *memory_pointer,
    ft_size_t size, void *user_data)
{
    test_cma_failure_controller *controller
        = static_cast<test_cma_failure_controller *>(user_data);
    int32_t allocation_index;
    void *new_pointer;

    if (controller == ft_nullptr)
        return (ft_nullptr);
    controller->reallocation_attempts += 1U;
    if (test_cma_failure_should_fail(controller,
            TEST_CMA_FAILURE_REALLOCATE,
            controller->reallocation_attempts) == FT_TRUE)
        return (ft_nullptr);
    if (memory_pointer == ft_nullptr)
        return (test_cma_failure_allocate(size, user_data));
    allocation_index = test_cma_failure_find_slot(controller, memory_pointer);
    if (size == 0U)
    {
        if (allocation_index >= 0)
        {
            test_cma_failure_release_pointer(memory_pointer,
                controller->allocations[allocation_index].aligned);
            test_cma_failure_remove_slot(controller,
                static_cast<ft_size_t>(allocation_index));
        }
        return (ft_nullptr);
    }
    if (allocation_index < 0)
        return (ft_nullptr);
    if (controller->allocations[allocation_index].aligned == FT_TRUE)
    {
        new_pointer = test_cma_failure_aligned_allocate(
            controller->allocations[allocation_index].alignment, size,
            user_data);
        if (new_pointer == ft_nullptr)
            return (ft_nullptr);
        std::memcpy(new_pointer, memory_pointer,
            test_cma_failure_min_size(
                controller->allocations[allocation_index].size, size));
        test_cma_failure_release_pointer(memory_pointer, FT_TRUE);
        test_cma_failure_remove_slot(controller,
            static_cast<ft_size_t>(allocation_index));
        return (new_pointer);
    }
    new_pointer = std::realloc(memory_pointer, size);
    if (new_pointer == ft_nullptr)
        return (ft_nullptr);
    controller->allocations[allocation_index].memory_pointer = new_pointer;
    controller->allocations[allocation_index].size = size;
    return (new_pointer);
}

static void test_cma_failure_deallocate(void *memory_pointer, void *user_data)
{
    test_cma_failure_controller *controller
        = static_cast<test_cma_failure_controller *>(user_data);
    int32_t allocation_index;

    if (controller == ft_nullptr || memory_pointer == ft_nullptr)
        return ;
    allocation_index = test_cma_failure_find_slot(controller, memory_pointer);
    if (allocation_index >= 0)
    {
        test_cma_failure_release_pointer(memory_pointer,
            controller->allocations[allocation_index].aligned);
        test_cma_failure_remove_slot(controller,
            static_cast<ft_size_t>(allocation_index));
    }
    return ;
}

static ft_size_t test_cma_failure_get_allocation_size(
    const void *memory_pointer, void *user_data)
{
    test_cma_failure_controller *controller
        = static_cast<test_cma_failure_controller *>(user_data);
    int32_t allocation_index;

    allocation_index = test_cma_failure_find_slot(controller, memory_pointer);
    if (allocation_index < 0)
        return (0U);
    return (controller->allocations[allocation_index].size);
}

static ft_bool test_cma_failure_owns_allocation(const void *memory_pointer,
    void *user_data)
{
    if (test_cma_failure_find_slot(
            static_cast<test_cma_failure_controller *>(user_data),
            memory_pointer) >= 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t test_cma_failure_controller_initialize(
    test_cma_failure_controller &controller) noexcept
{
    ft_size_t allocation_index;

    allocation_index = 0U;
    while (allocation_index < TEST_CMA_FAILURE_ALLOCATION_CAPACITY)
    {
        controller.allocations[allocation_index].memory_pointer = ft_nullptr;
        controller.allocations[allocation_index].size = 0U;
        controller.allocations[allocation_index].alignment = 0U;
        controller.allocations[allocation_index].aligned = FT_FALSE;
        allocation_index += 1U;
    }
    controller.allocation_slots = 0U;
    controller.allocation_attempts = 0U;
    controller.reallocation_attempts = 0U;
    controller.aligned_allocation_attempts = 0U;
    controller.allocation_failure_call = 0U;
    controller.reallocation_failure_call = 0U;
    controller.aligned_failure_call = 0U;
    controller.active = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t test_cma_failure_controller_begin(
    test_cma_failure_controller &controller) noexcept
{
    if (controller.active == FT_TRUE)
        return (FT_ERR_INVALID_STATE);
    controller.hooks.allocate = &test_cma_failure_allocate;
    controller.hooks.reallocate = &test_cma_failure_reallocate;
    controller.hooks.deallocate = &test_cma_failure_deallocate;
    controller.hooks.aligned_allocate = &test_cma_failure_aligned_allocate;
    controller.hooks.get_allocation_size = &test_cma_failure_get_allocation_size;
    controller.hooks.owns_allocation = &test_cma_failure_owns_allocation;
    controller.hooks.user_data = &controller;
    cma_set_alloc_limit(0U);
    if (cma_set_backend(&controller.hooks) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    controller.active = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t test_cma_failure_controller_end(
    test_cma_failure_controller &controller) noexcept
{
    while (controller.allocation_slots > 0U)
    {
        test_cma_failure_deallocate(
            controller.allocations[controller.allocation_slots - 1U]
                .memory_pointer, &controller);
    }
    if (controller.active == FT_TRUE)
    {
        cma_clear_backend();
        controller.active = FT_FALSE;
    }
    cma_set_alloc_limit(0U);
    return (FT_ERR_SUCCESS);
}

int32_t test_cma_failure_controller_destroy(
    test_cma_failure_controller &controller) noexcept
{
    return (test_cma_failure_controller_end(controller));
}

static ft_size_t *test_cma_failure_call_counter(
    test_cma_failure_controller &controller,
    test_cma_failure_operation operation) noexcept
{
    if (operation == TEST_CMA_FAILURE_ALLOCATE)
        return (&controller.allocation_attempts);
    if (operation == TEST_CMA_FAILURE_REALLOCATE)
        return (&controller.reallocation_attempts);
    if (operation == TEST_CMA_FAILURE_ALIGNED_ALLOCATE)
        return (&controller.aligned_allocation_attempts);
    return (ft_nullptr);
}

static ft_size_t *test_cma_failure_failure_call(
    test_cma_failure_controller &controller,
    test_cma_failure_operation operation) noexcept
{
    if (operation == TEST_CMA_FAILURE_ALLOCATE)
        return (&controller.allocation_failure_call);
    if (operation == TEST_CMA_FAILURE_REALLOCATE)
        return (&controller.reallocation_failure_call);
    if (operation == TEST_CMA_FAILURE_ALIGNED_ALLOCATE)
        return (&controller.aligned_failure_call);
    return (ft_nullptr);
}

int32_t test_cma_failure_controller_fail_next(
    test_cma_failure_controller &controller,
    test_cma_failure_operation operation) noexcept
{
    ft_size_t *counter;
    ft_size_t *failure_call;

    counter = test_cma_failure_call_counter(controller, operation);
    failure_call = test_cma_failure_failure_call(controller, operation);
    if (counter == ft_nullptr || failure_call == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *failure_call = *counter + 1U;
    return (FT_ERR_SUCCESS);
}

int32_t test_cma_failure_controller_fail_after(
    test_cma_failure_controller &controller,
    test_cma_failure_operation operation, ft_size_t successful_calls) noexcept
{
    ft_size_t *counter;
    ft_size_t *failure_call;

    counter = test_cma_failure_call_counter(controller, operation);
    failure_call = test_cma_failure_failure_call(controller, operation);
    if (counter == ft_nullptr || failure_call == ft_nullptr
        || *counter >= FT_SYSTEM_SIZE_MAX
        || successful_calls > FT_SYSTEM_SIZE_MAX - *counter - 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    *failure_call = *counter + successful_calls + 1U;
    return (FT_ERR_SUCCESS);
}

int32_t test_cma_failure_controller_fail_on_call(
    test_cma_failure_controller &controller,
    test_cma_failure_operation operation, ft_size_t call_number) noexcept
{
    ft_size_t *failure_call;

    failure_call = test_cma_failure_failure_call(controller, operation);
    if (failure_call == ft_nullptr || call_number == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    *failure_call = call_number;
    return (FT_ERR_SUCCESS);
}

ft_size_t test_cma_failure_controller_attempt_count(
    const test_cma_failure_controller &controller,
    test_cma_failure_operation operation) noexcept
{
    if (operation == TEST_CMA_FAILURE_ALLOCATE)
        return (controller.allocation_attempts);
    if (operation == TEST_CMA_FAILURE_REALLOCATE)
        return (controller.reallocation_attempts);
    if (operation == TEST_CMA_FAILURE_ALIGNED_ALLOCATE)
        return (controller.aligned_allocation_attempts);
    return (0U);
}

FT_TEST(test_cma_failure_controller_targets_predictable_events)
{
    test_cma_failure_controller controller;
    void *memory_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    memory_pointer = cma_malloc(64U);
    FT_ASSERT_EQ(ft_nullptr, memory_pointer);
    FT_ASSERT_EQ(1U, test_cma_failure_controller_attempt_count(controller,
        TEST_CMA_FAILURE_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_after(controller,
            TEST_CMA_FAILURE_ALLOCATE, 1U));
    memory_pointer = cma_malloc(64U);
    FT_ASSERT(memory_pointer != ft_nullptr);
    cma_free(memory_pointer);
    memory_pointer = cma_malloc(64U);
    FT_ASSERT_EQ(ft_nullptr, memory_pointer);
    FT_ASSERT_EQ(3U, test_cma_failure_controller_attempt_count(controller,
        TEST_CMA_FAILURE_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_REALLOCATE));
    memory_pointer = cma_malloc(64U);
    FT_ASSERT(memory_pointer != ft_nullptr);
    FT_ASSERT_EQ(ft_nullptr, cma_realloc(memory_pointer, 128U));
    cma_free(memory_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALIGNED_ALLOCATE));
    FT_ASSERT_EQ(ft_nullptr, cma_aligned_alloc(16U, 64U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    return (1);
}
