#!/usr/bin/env sh

set -eu

root_directory=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
aead_source="$root_directory/Modules/Crypto/crypto_aead.cpp"
x25519_source="$root_directory/Modules/Crypto/crypto_x25519.cpp"
chacha_source="$root_directory/Modules/Crypto/crypto_chacha20.cpp"
poly1305_source="$root_directory/Modules/Crypto/crypto_poly1305.cpp"

require_text()
{
    source_file=$1
    text_value=$2
    if ! grep -F "$text_value" "$source_file" >/dev/null 2>&1; then
        printf '%s\n' "Crypto review source check failed: $source_file lacks $text_value" >&2
        exit 1
    fi
}

require_text "$aead_source" 'constant_time_equal(expected_tag, authentication_tag, 16U)'
require_text "$aead_source" 'difference = static_cast<uint8_t>(difference'
require_text "$aead_source" '| (left[index] ^ right[index]));'
if grep -E '(^|[^[:alnum:]_])(ft_)?memcmp[[:space:]]*\(' "$aead_source" \
    >/dev/null 2>&1; then
    printf '%s\n' 'Crypto review source check found a non-constant-time AEAD compare' >&2
    exit 1
fi
require_text "$x25519_source" 'bit_index = 254;'
require_text "$x25519_source" 'while (bit_index >= 0)'
require_text "$x25519_source" 'conditional_swap(x2, x3, swap);'
require_text "$x25519_source" 'conditional_swap(z2, z3, swap);'
require_text "$x25519_source" 'mask = 0U - swap;'
require_text "$x25519_source" 'difference = (left[index] ^ right[index]) & mask;'
require_text "$chacha_source" 'crypto_secure_wipe(state, sizeof(state))'
require_text "$poly1305_source" 'crypto_secure_wipe(&r0, sizeof(r0))'

scalar_function=$(awk '
    /static int32_t scalar_multiply\(/ { in_function = 1 }
    in_function { print }
    in_function && /^    }[[:space:]]*$/ { exit }
' "$x25519_source")
scalar_ladder=$(printf '%s\n' "$scalar_function" | awk '
    /while \(bit_index >= 0\)/ { in_loop = 1 }
    in_loop { print }
    in_loop && /bit_index -= 1;/ { exit }
')
if [ -z "$scalar_ladder" ] \
    || printf '%s\n' "$scalar_ladder" \
        | grep -E '^[[:space:]]*(if|switch)[[:space:]]*\(|[?]|(^|[^[:alnum:]_])(break|return)[[:space:]]*;' \
            >/dev/null 2>&1; then
    printf '%s\n' \
        'Crypto review found a branch or early exit in the X25519 secret ladder' >&2
    exit 1
fi
if printf '%s\n' "$scalar_ladder" \
    | grep -E '\[[^]]*([^[:alnum:]_])(bit|swap)([^[:alnum:]_]|$)[^]]*\]' \
        >/dev/null 2>&1; then
    printf '%s\n' \
        'Crypto review found secret-derived array indexing in the X25519 ladder' >&2
    exit 1
fi

comparison_function=$(awk '
    /static ft_bool constant_time_equal\(/ { in_function = 1 }
    in_function { print }
    in_function && /^    }[[:space:]]*$/ { exit }
' "$aead_source")
comparison_loop=$(printf '%s\n' "$comparison_function" | awk '
    /while \(index < length\)/ { in_loop = 1 }
    in_loop { print }
    in_loop && /index \+= 1U;/ { exit }
')
if [ -z "$comparison_loop" ] \
    || printf '%s\n' "$comparison_loop" \
        | grep -E '^[[:space:]]*(if|switch)[[:space:]]*\(|[?]|(^|[^[:alnum:]_])break[[:space:]]*;|(^|[^[:alnum:]_])return[[:space:]]*\(' \
            >/dev/null 2>&1; then
    printf '%s\n' \
        'Crypto review found an early exit or branch in the AEAD tag comparison loop' >&2
    exit 1
fi

printf '%s\n' 'Crypto source review checks passed; this is not an independent cryptographic review.'
