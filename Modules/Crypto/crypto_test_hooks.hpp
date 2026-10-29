#ifndef CRYPTO_MODULE_TEST_HOOKS_HPP
#define CRYPTO_MODULE_TEST_HOOKS_HPP

#include "../Errno/errno.hpp"
#include <cstdint>

#ifdef LIBFT_TEST_BUILD

int32_t crypto_test_random_seed(uint64_t seed) noexcept;
int32_t crypto_test_random_clear() noexcept;
int32_t crypto_test_random_fail_next() noexcept;
int32_t crypto_test_random_fail_after(uint64_t successful_calls) noexcept;
uint64_t crypto_test_random_attempt_count() noexcept;
uint64_t crypto_test_random_failure_count() noexcept;
ft_bool crypto_test_random_should_fail() noexcept;
ft_bool crypto_test_random_bytes(uint8_t *output, ft_size_t length) noexcept;

#endif

#endif
