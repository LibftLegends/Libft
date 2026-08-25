#!/usr/bin/env sh

set -eu

script_directory=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
source_directory=$(CDPATH= cd -- "$script_directory/.." && pwd)
temporary_root=$(mktemp -d "${TMPDIR:-/tmp}/libft-incremental-build.XXXXXX")
checkout_directory="$temporary_root/checkout"
log_directory="$temporary_root/logs"
make_jobs=${LIBFT_INCREMENTAL_JOBS:-4}
mkdir -p "$log_directory"

cleanup()
{
    if [ "${LIBFT_KEEP_INCREMENTAL_WORKTREE:-0}" = "1" ]; then
        printf '%s\n' "incremental-build worktree retained: $temporary_root"
    else
        rm -rf "$temporary_root"
    fi
}

trap cleanup EXIT INT TERM HUP

copy_checkout()
{
    mkdir -p "$checkout_directory"
    cp -a "$source_directory/." "$checkout_directory/"
    rm -rf "$checkout_directory/build"
    rm -f "$checkout_directory/Full_Libft.a"
    rm -f "$checkout_directory/Full_Libft_debug.a"
    rm -f "$checkout_directory/Test/Full_Libft_test.a"
    rm -f "$checkout_directory/Test/Full_Libft_test_debug.a"
    rm -f "$checkout_directory/Test/libft_tests" \
        "$checkout_directory/Test/libft_tests.exe"
}

run_make()
{
    log_name=$1
    shift
    if ! make --no-print-directory "-j$make_jobs" "$@" \
        >"$log_directory/$log_name.log" 2>&1; then
        printf '%s\n' "make failed: $*" >&2
        cat "$log_directory/$log_name.log" >&2
        exit 1
    fi
}

run_optional_sanitizer_build()
{
    log_name=$1
    sanitizer_name=$2
    if make --no-print-directory -n "SANITIZERS=$sanitizer_name" \
        global-all >"$log_directory/$log_name.probe.log" 2>&1; then
        run_make "$log_name" "SANITIZERS=$sanitizer_name" global-all
    else
        printf '%s\n' "sanitizer unavailable on this toolchain: $sanitizer_name" \
            >"$log_directory/$log_name.log"
        cat "$log_directory/$log_name.probe.log" \
            >>"$log_directory/$log_name.log"
    fi
}

build_plan()
{
    log_name=$1
    shift
    if ! make --no-print-directory -n BUILD_PLAN_MODE=1 "$@" \
        >"$log_directory/$log_name.plan" 2>&1; then
        printf '%s\n' "build plan failed: $*" >&2
        cat "$log_directory/$log_name.plan" >&2
        exit 1
    fi
}

dependency_source_from_file()
{
    dependency_path=$1
    relative_path=${dependency_path#"$release_root/"}
    relative_path=${relative_path%.d}
    for extension in cpp c mm; do
        if [ -f "$checkout_directory/$relative_path.$extension" ]; then
            printf '%s\n' "$relative_path.$extension"
            return 0
        fi
    done
    return 1
}

dependent_sources()
{
    header_name=$1
    header_basename=${header_name##*/}
    find "$release_root" -type f -name '*.d' -print \
        | while IFS= read -r dependency_path; do
            if grep -F "$header_basename" "$dependency_path" \
                >/dev/null 2>&1; then
                dependency_source_from_file "$dependency_path" || true
            fi
        done
}

assert_plan_matches_dependencies()
{
    header_name=$1
    plan_path=$2
    header_basename=${header_name##*/}
    expected_path="$plan_path.expected"
    actual_path="$plan_path.actual"
    dependency_path=''
    dependent_sources "$header_name" | sort -u >"$expected_path"
    sed -n 's/.*"__BUILD_PLAN__|compile|libft|[^|]*|\([^" ]*\)".*/\1/p' \
        "$plan_path" \
        | sort -u >"$actual_path"
    if ! diff -u "$expected_path" "$actual_path" \
        >"$plan_path.diff" 2>&1; then
        printf '%s\n' "dependency closure mismatch for $header_name" >&2
        cat "$plan_path.diff" >&2
        cat "$plan_path" >&2
        exit 1
    fi
}

archive_has_member()
{
    archive_path=$1
    member_name=$2
    ar t "$archive_path" | tr -d '\r' | grep -Fx "$member_name" \
        >/dev/null 2>&1
}

archive_members_are_unique()
{
    archive_path=$1
    duplicate_members=$(ar t "$archive_path" | tr -d '\r' \
        | sort | uniq -d)
    if [ -n "$duplicate_members" ]; then
        printf '%s\n' "duplicate members in $archive_path" >&2
        printf '%s\n' "$duplicate_members" >&2
        exit 1
    fi
}

copy_checkout
cd "$checkout_directory"

run_make baseline global-all
release_root=$(find build/libft -type d -name release -print | head -n 1)
if [ -z "$release_root" ]; then
    printf '%s\n' 'could not locate release object root' >&2
    exit 1
fi

# A module-private header must rebuild exactly its dependency closure.
private_header='Modules/Crypto/crypto_random.hpp'
touch "$private_header"
build_plan private_header_plan global-all
assert_plan_matches_dependencies "$private_header" \
    "$log_directory/private_header_plan.plan"
run_make private_header_build global-all

# A common header must rebuild every dependent release object and nothing else.
common_header='Modules/Basic/basic.hpp'
sleep 1
touch "$common_header"
build_plan common_header_plan global-all
assert_plan_matches_dependencies "$common_header" \
    "$log_directory/common_header_plan.plan"
run_make common_header_build global-all

# Removing an object must restore it and relink both archive levels.
deleted_object="$release_root/Modules/Crypto/crypto_random.o"
rm -f "$deleted_object"
run_make deleted_object_build global-all
if [ ! -f "$deleted_object" ] || \
    ! archive_has_member Modules/Crypto/crypto.a crypto_random.o || \
    ! archive_has_member Full_Libft.a crypto_random.o; then
    printf '%s\n' 'deleted object was not restored to both archives' >&2
    exit 1
fi

# Removing an archive member and touching its object forces the archive rule to
# restore the member without changing any source file.
ar d Modules/Crypto/crypto.a crypto_random.o >/dev/null
sleep 1
touch "$deleted_object"
run_make deleted_member_build global-all
if ! archive_has_member Modules/Crypto/crypto.a crypto_random.o; then
    printf '%s\n' 'deleted archive member was not restored' >&2
    exit 1
fi

# Removing a manifest source removes the stale member from module and aggregate
# archives.  Restore the manifest before continuing with the next scenario.
manifest='mk/modules/Crypto.mk'
cp "$manifest" "$manifest.incremental-backup"
sed '/crypto_random\.cpp \\/d' "$manifest" >"$manifest.incremental-rewritten"
mv "$manifest.incremental-rewritten" "$manifest"
run_make manifest_removal_build global-all
if archive_has_member Modules/Crypto/crypto.a crypto_random.o || \
    archive_has_member Full_Libft.a crypto_random.o; then
    printf '%s\n' 'stale member survived manifest removal' >&2
    exit 1
fi
mv "$manifest.incremental-backup" "$manifest"
run_make manifest_restore_build global-all

# A source rename must remove the old member and add the new one.
mv Modules/Crypto/crypto_random.cpp Modules/Crypto/crypto_random_renamed.cpp
cp "$manifest" "$manifest.incremental-backup"
sed 's/crypto_random\.cpp/crypto_random_renamed.cpp/' \
    "$manifest" >"$manifest.incremental-rewritten"
mv "$manifest.incremental-rewritten" "$manifest"
run_make source_rename_build global-all
if archive_has_member Modules/Crypto/crypto.a crypto_random.o || \
    ! archive_has_member Modules/Crypto/crypto.a crypto_random_renamed.o; then
    printf '%s\n' 'source rename archive membership is incorrect' >&2
    exit 1
fi
mv Modules/Crypto/crypto_random_renamed.cpp Modules/Crypto/crypto_random.cpp
mv "$manifest.incremental-backup" "$manifest"
run_make source_restore_build global-all

# Configuration fingerprints must isolate compile-flag changes.
run_make opt1_build OPT_LEVEL=1 global-all
opt1_root=$(find build/libft -maxdepth 1 -type d -name '*_opt1*' -print \
    | head -n 1)
if [ -z "$opt1_root" ] || [ "$opt1_root" = "$release_root" ]; then
    printf '%s\n' 'OPT_LEVEL did not select a separate object root' >&2
    exit 1
fi
run_make debug_build global-debug
run_make test_build tests
run_make test_debug_build debug-tests
run_optional_sanitizer_build asan_build address
run_optional_sanitizer_build ubsan_build undefined

# Multiple dependency roots in a parallel build must leave unique archives.
touch Modules/Crypto/crypto_random.hpp Modules/Networking/message_transport.hpp
parallel_jobs=$make_jobs
make_jobs=32
run_make parallel32_build global-all
make_jobs=$parallel_jobs
for archive_path in Modules/Crypto/crypto.a Modules/Networking/networking.a \
    Full_Libft.a; do
    archive_members_are_unique "$archive_path"
done

# Interrupt/restart recovery is best-effort on hosts where the build completes
# before the signal can be delivered; the restart is authoritative.
touch Modules/Crypto/crypto_random.hpp Modules/Networking/message_transport.hpp
make --no-print-directory -j32 global-all \
    >"$log_directory/interrupted_build.log" 2>&1 &
build_pid=$!
sleep 1
kill -TERM "$build_pid" >/dev/null 2>&1 || true
wait "$build_pid" >/dev/null 2>&1 || true
run_make interrupted_restart global-all

# Two parent-style consumers must be able to build concurrently with separate
# roots.  The copies intentionally live under paths containing spaces so the
# command and archive handling is exercised with that path shape as well.
parent_one="$temporary_root/parent one"
parent_two="$temporary_root/parent two"
cp -a . "$parent_one"
cp -a . "$parent_two"
(cd "$parent_one" && make --no-print-directory "-j$make_jobs" global-all \
    >"$log_directory/parent_one.log" 2>&1) &
parent_one_pid=$!
(cd "$parent_two" && make --no-print-directory "-j$make_jobs" global-all \
    >"$log_directory/parent_two.log" 2>&1) &
parent_two_pid=$!
if ! wait "$parent_one_pid"; then
    cat "$log_directory/parent_one.log" >&2
    exit 1
fi
if ! wait "$parent_two_pid"; then
    cat "$log_directory/parent_two.log" >&2
    exit 1
fi
for parent_archive in "$parent_one/Full_Libft.a" \
    "$parent_two/Full_Libft.a"; do
    if [ ! -f "$parent_archive" ]; then
        printf '%s\n' "concurrent parent archive missing: $parent_archive" >&2
        exit 1
    fi
    archive_members_are_unique "$parent_archive"
done

if ! git diff --check -- . >"$log_directory/diff_check.log" 2>&1; then
    cat "$log_directory/diff_check.log" >&2
    exit 1
fi

printf '%s\n' 'incremental build graph checks passed'
