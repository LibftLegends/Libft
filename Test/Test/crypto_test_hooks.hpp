#ifndef CRYPTO_TEST_HOOKS_HPP
#define CRYPTO_TEST_HOOKS_HPP

#include "../../Modules/Errno/errno.hpp"
#include <cstdint>

int32_t crypto_test_random_seed(uint64_t seed) noexcept;
int32_t crypto_test_random_clear() noexcept;
int32_t crypto_test_random_fail_next() noexcept;
ft_bool crypto_test_random_should_fail() noexcept;
ft_bool crypto_test_random_bytes(uint8_t *output, ft_size_t length) noexcept;

#endif
