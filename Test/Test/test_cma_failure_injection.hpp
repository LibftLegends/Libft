#ifndef TEST_CMA_FAILURE_INJECTION_HPP
# define TEST_CMA_FAILURE_INJECTION_HPP

#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include <stdint.h>

#define TEST_CMA_FAILURE_ALLOCATION_CAPACITY 4096U

enum test_cma_failure_operation
{
    TEST_CMA_FAILURE_ALLOCATE = 1,
    TEST_CMA_FAILURE_REALLOCATE = 2,
    TEST_CMA_FAILURE_ALIGNED_ALLOCATE = 3
};

struct test_cma_failure_allocation
{
    void *memory_pointer;
    ft_size_t size;
    ft_size_t alignment;
    ft_bool aligned;
};

struct test_cma_failure_controller
{
    cma_backend_hooks hooks;
    test_cma_failure_allocation allocations[
        TEST_CMA_FAILURE_ALLOCATION_CAPACITY];
    ft_size_t allocation_slots;
    ft_size_t allocation_attempts;
    ft_size_t reallocation_attempts;
    ft_size_t aligned_allocation_attempts;
    ft_size_t allocation_failure_call;
    ft_size_t reallocation_failure_call;
    ft_size_t aligned_failure_call;
    ft_bool active;
};

int32_t test_cma_failure_controller_initialize(
    test_cma_failure_controller &controller) noexcept;
int32_t test_cma_failure_controller_begin(
    test_cma_failure_controller &controller) noexcept;
int32_t test_cma_failure_controller_end(
    test_cma_failure_controller &controller) noexcept;
int32_t test_cma_failure_controller_destroy(
    test_cma_failure_controller &controller) noexcept;
int32_t test_cma_failure_controller_fail_next(
    test_cma_failure_controller &controller,
    test_cma_failure_operation operation) noexcept;
int32_t test_cma_failure_controller_fail_after(
    test_cma_failure_controller &controller,
    test_cma_failure_operation operation,
    ft_size_t successful_calls) noexcept;
int32_t test_cma_failure_controller_fail_on_call(
    test_cma_failure_controller &controller,
    test_cma_failure_operation operation,
    ft_size_t call_number) noexcept;
ft_size_t test_cma_failure_controller_attempt_count(
    const test_cma_failure_controller &controller,
    test_cma_failure_operation operation) noexcept;

#endif
