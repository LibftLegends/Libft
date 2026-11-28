#include "../Errno/errno.hpp"
#include "../RNG/rng.hpp"
#include "encryption.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"

#ifdef LIBFT_TEST_BUILD
static ft_bool g_force_mod_inverse_failure = FT_FALSE;
#endif

static uint64_t rsa_gcd(uint64_t first_value, uint64_t second_value)
{
    while (second_value != 0)
    {
        uint64_t temp_value = second_value;
        second_value = first_value % second_value;
        first_value = temp_value;
    }
    return (first_value);
}

static int32_t rsa_mod_inverse(uint64_t value, uint64_t modulus,
    uint64_t *inverse_value)
{
    int64_t coefficient_value = 0;
    int64_t next_coefficient_value = 1;
    int64_t remainder_value = static_cast<int64_t>(modulus);
    int64_t next_remainder_value = static_cast<int64_t>(value);
    while (next_remainder_value != 0)
    {
        int64_t quotient_value = remainder_value / next_remainder_value;
        int64_t temporary_coefficient_value = coefficient_value
            - quotient_value * next_coefficient_value;
        coefficient_value = next_coefficient_value;
        next_coefficient_value = temporary_coefficient_value;
        int64_t temporary_remainder_value = remainder_value
            - quotient_value * next_remainder_value;
        remainder_value = next_remainder_value;
        next_remainder_value = temporary_remainder_value;
    }
    if (remainder_value > 1)
        return (FT_ERR_INVALID_STATE);
    if (coefficient_value < 0)
        coefficient_value += modulus;
    *inverse_value = static_cast<uint64_t>(coefficient_value);
    return (FT_ERR_SUCCESS);
}

static ft_bool rsa_is_prime(uint64_t value)
{
    if (value < 2)
        return (FT_FALSE);
    uint64_t divisor_value = 2;
    while (divisor_value * divisor_value <= value)
    {
        if (value % divisor_value == 0)
            return (FT_FALSE);
        divisor_value = divisor_value + 1;
    }
    return (FT_TRUE);
}

static uint64_t rsa_generate_prime(uint64_t limit)
{
    uint64_t prime_candidate = 0;
    while (prime_candidate < 2 || rsa_is_prime(prime_candidate) == FT_FALSE)
    {
        prime_candidate = static_cast<uint64_t>(ft_random_int()) % limit;
        if ((prime_candidate & 1) == 0)
            prime_candidate = prime_candidate + 1;
    }
    return (prime_candidate);
}

static uint64_t rsa_mod_pow(uint64_t base_value, uint64_t exponent_value, uint64_t modulus_value)
{
    uint64_t result_value = 1;
    base_value = base_value % modulus_value;
    while (exponent_value > 0)
    {
        if ((exponent_value & 1) == 1)
            result_value = (result_value * base_value) % modulus_value;
        exponent_value = exponent_value >> 1;
        base_value = (base_value * base_value) % modulus_value;
    }
    return (result_value);
}

int32_t rsa_generate_key_pair(uint64_t *public_key, uint64_t *private_key,
    uint64_t *modulus, int32_t bit_size)
{
    if (!public_key || !private_key || !modulus)
        return (FT_ERR_INVALID_ARGUMENT);
    uint64_t limit_value = static_cast<uint64_t>(1) << (bit_size / 2);
    uint64_t prime_one = rsa_generate_prime(limit_value);
    uint64_t prime_two = rsa_generate_prime(limit_value);
    while (prime_two == prime_one)
        prime_two = rsa_generate_prime(limit_value);
    uint64_t phi_value = (prime_one - 1) * (prime_two - 1);
    *modulus = prime_one * prime_two;
    uint64_t public_exponent = 65537;
    if (rsa_gcd(public_exponent, phi_value) != 1)
    {
        public_exponent = 3;
        while (rsa_gcd(public_exponent, phi_value) != 1)
            public_exponent = public_exponent + 2;
    }
    uint64_t private_exponent = 0;
    int32_t inverse_result;
#ifdef LIBFT_TEST_BUILD
    if (g_force_mod_inverse_failure == FT_TRUE)
        inverse_result = FT_ERR_INVALID_STATE;
    else
#endif
        inverse_result = rsa_mod_inverse(public_exponent, phi_value,
            &private_exponent);
    if (inverse_result != FT_ERR_SUCCESS)
        return (inverse_result);
    *public_key = public_exponent;
    *private_key = private_exponent;
    return (FT_ERR_SUCCESS);
}

#ifdef LIBFT_TEST_BUILD
void rsa_set_force_mod_inverse_failure(ft_bool enable)
{
    if (enable != FT_FALSE)
        g_force_mod_inverse_failure = FT_TRUE;
    else
        g_force_mod_inverse_failure = FT_FALSE;
    return ;
}
#endif

uint64_t rsa_encrypt(uint64_t message, uint64_t public_key, uint64_t modulus)
{
    return (rsa_mod_pow(message, public_key, modulus));
}

uint64_t rsa_decrypt(uint64_t cipher, uint64_t private_key, uint64_t modulus)
{
    return (rsa_mod_pow(cipher, private_key, modulus));
}
