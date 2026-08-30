#!/usr/bin/env sh

set -eu

policy_paths=":(glob)Modules/Crypto/**/*.cpp :(glob)Modules/Crypto/**/*.hpp :(glob)Modules/Networking/**/*.cpp :(glob)Modules/Networking/**/*.hpp Modules/Voxel/voxel_json.cpp Modules/Voxel/voxel_block_registry.cpp"
failed=0

check_pattern()
{
    pattern=$1
    description=$2
    if git grep -nE "$pattern" -- $policy_paths; then
        printf 'AGENTS policy violation: %s\n' "$description" >&2
        failed=1
    fi
}

check_pattern 'std::(memcpy|memset)' 'use ft_memcpy/ft_memset'
check_pattern '(^|[^?])\?([^?]|$)' 'ternary operators are forbidden'
check_pattern '\b(try|catch|switch)[[:space:]]*([({]|$)' \
    'try/catch and switch statements are forbidden'

if git grep -nE '#include[[:space:]]*[<"]([^">/]+/)*voxel\.hpp[>"]' \
    -- ':(glob)Modules/**/*.cpp' ':(glob)Modules/**/*.hpp' \
    ':(glob)Test/**/*.cpp' ':(glob)Test/**/*.hpp'; then
    printf '%s\n' 'AGENTS policy violation: direct voxel.hpp dependency' >&2
    failed=1
fi

if [ "$failed" -ne 0 ]; then
    exit 1
fi
printf '%s\n' 'AGENTS policy scan passed'
