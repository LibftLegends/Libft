#!/usr/bin/env sh

set -eu

script_directory=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
source_directory=$(CDPATH= cd -- "$script_directory/.." && pwd)
temporary_root=$(mktemp -d "${TMPDIR:-/tmp}/libft-incremental-build.XXXXXX")
checkout_directory="$temporary_root/checkout"
log_directory="$temporary_root/logs"
make_jobs=${LIBFT_INCREMENTAL_JOBS:-4}
# A clean Windows test graph contains hundreds of test translation units and
# can legitimately exceed fifteen minutes on a four-job runner.  Keep the
# timeout bounded, but avoid classifying a slow clean build as a hang.  CI can
# still override this explicitly with LIBFT_INCREMENTAL_TIMEOUT_SECONDS.
if [ "${OS:-}" = "Windows_NT" ]; then
    make_timeout_seconds=${LIBFT_INCREMENTAL_TIMEOUT_SECONDS:-3600}
else
    make_timeout_seconds=${LIBFT_INCREMENTAL_TIMEOUT_SECONDS:-900}
fi
heartbeat_seconds=${LIBFT_CI_HEARTBEAT_SECONDS:-30}
mkdir -p "$log_directory"

cleanup()
{
    cleanup_status=$?
    if [ "${LIBFT_KEEP_INCREMENTAL_WORKTREE:-0}" = "1" ]; then
        printf '%s\n' "incremental-build worktree retained: $temporary_root"
        return 0
    fi
    if [ "$cleanup_status" -ne 0 ]; then
        printf '%s\n' "incremental-build failed; worktree retained: $temporary_root" >&2
        return 0
    fi
    rm -rf "$temporary_root"
}

trap cleanup EXIT INT TERM HUP

copy_checkout_to()
{
    destination_directory=$1
    mkdir -p "$destination_directory"
    (
        cd "$source_directory"
        tar -cf - \
            --exclude='./.git' \
            --exclude='./build' \
            --exclude='*.a' \
            --exclude='*.d' \
            --exclude='*.o' \
            --exclude='*.exe' \
            --exclude='./Test/libft_tests' \
            --exclude='./Test/libft_tests_debug' \
            --exclude='./Test/*_fuzz' \
            --exclude='*.log' \
            --exclude='*.jsonl' \
            .
    ) | (
        cd "$destination_directory"
        tar -xf -
    )
}

capture_build_manifest()
{
    manifest_path=$1
    : >"$manifest_path"
    find build/libft -type f \
        \( -name '*.o' -o -name '*.d' -o -name '*.a' \) -print \
        | LC_ALL=C sort \
        | while IFS= read -r build_artifact; do
            if [ -f "$build_artifact" ]; then
                artifact_checksum=$(cksum "$build_artifact" \
                    | awk '{print $1 ":" $2}')
                printf '%s|%s\n' "$build_artifact" "$artifact_checksum"
            fi
        done >"$manifest_path"
    find Modules -type f -name '*.a' -print | LC_ALL=C sort \
        | while IFS= read -r archive_path; do
            if [ -f "$archive_path" ]; then
                archive_checksum=$(cksum "$archive_path" \
                    | awk '{print $1 ":" $2}')
                printf '%s|%s\n' "$archive_path" "$archive_checksum"
            fi
        done >>"$manifest_path"
    if [ -f Full_Libft.a ]; then
        archive_checksum=$(cksum Full_Libft.a | awk '{print $1 ":" $2}')
        printf '%s|%s\n' Full_Libft.a "$archive_checksum" \
            >>"$manifest_path"
    fi
}

run_make()
{
    log_name=$1
    shift
    evidence_path="$log_directory/$log_name.evidence"
    printf 'command=make --no-print-directory -j%s %s\n' \
        "$make_jobs" "$*" >"$evidence_path"
    printf 'CXX=%s\nCC=%s\nCOMPILE_FLAGS=%s\nCFLAGS=%s\n' \
        "${CXX:-}" "${CC:-}" "${COMPILE_FLAGS:-}" "${CFLAGS:-}" \
        >>"$evidence_path"
    printf 'CPPFLAGS=%s\nSANITIZERS=%s\nOPT_LEVEL=%s\n' \
        "${CPPFLAGS:-}" "${SANITIZERS:-}" "${OPT_LEVEL:-}" \
        >>"$evidence_path"
    if ! make --no-print-directory -n BUILD_PLAN_MODE=1 "$@" \
        >"$log_directory/$log_name.plan" 2>&1; then
        printf '%s\n' "build plan failed before $log_name" >&2
        cat "$log_directory/$log_name.plan" >&2
        exit 1
    fi
    capture_build_manifest "$log_directory/$log_name.manifest.before"
    printf '%s\n' "incremental-build: starting $log_name: make --no-print-directory -j$make_jobs $*" >&2
    if ! LIBFT_CI_HEARTBEAT_SECONDS="$heartbeat_seconds" \
        sh "$script_directory/ci_run_with_timeout.sh" "$make_timeout_seconds" \
        make --no-print-directory "-j$make_jobs" "$@" \
        >"$log_directory/$log_name.log" 2>&1; then
        capture_build_manifest "$log_directory/$log_name.manifest.after"
        printf '%s\n' "make failed: $*" >&2
        if [ -f "$log_directory/$log_name.log" ]; then
            cat "$log_directory/$log_name.log" >&2
        else
            printf '%s\n' "scenario log was not created: $log_directory/$log_name.log" >&2
        fi
        exit 1
    fi
    capture_build_manifest "$log_directory/$log_name.manifest.after"
    printf '%s\n' "incremental-build: completed $log_name" >&2
    case "$log_name" in
        *_archive_members)
            before_manifest="$log_directory/$log_name.order.before"
            after_manifest="$log_directory/$log_name.order.after"
            capture_archive_order_manifest "$before_manifest"
            if ! make --no-print-directory "-j$make_jobs" "$@" global-all \
                >"$log_directory/$log_name.noop.log" 2>&1; then
                printf '%s\n' 'no-op build failed after archive membership scan' >&2
                cat "$log_directory/$log_name.noop.log" >&2
                exit 1
            fi
            capture_archive_order_manifest "$after_manifest"
            if ! diff -u "$before_manifest" "$after_manifest" \
                >"$log_directory/$log_name.order.diff" 2>&1; then
                printf '%s\n' 'no-op build changed archive membership or order' >&2
                cat "$log_directory/$log_name.order.diff" >&2
                exit 1
            fi
            ;;
        *)
            run_make "${log_name}_archive_members" "$@" archive-members-check
            ;;
    esac
}

run_optional_sanitizer_build()
{
    log_name=$1
    sanitizer_name=$2
    if make --no-print-directory -n "SANITIZERS=$sanitizer_name" \
        global-all >"$log_directory/$log_name.probe.log" 2>&1; then
        find build/libft -mindepth 1 -maxdepth 1 -type d -print \
            | LC_ALL=C sort >"$log_directory/$log_name.roots.before"
        run_make "$log_name" "SANITIZERS=$sanitizer_name" global-all
        find build/libft -mindepth 1 -maxdepth 1 -type d -print \
            | LC_ALL=C sort >"$log_directory/$log_name.roots.after"
        sanitizer_root=$(comm -13 \
            "$log_directory/$log_name.roots.before" \
            "$log_directory/$log_name.roots.after" | head -n 1)
        if [ -z "$sanitizer_root" ]; then
            printf '%s\n' \
                "SANITIZERS=$sanitizer_name reused an existing configuration root" \
                >&2
            exit 1
        fi
    else
        if [ "${LIBFT_INCREMENTAL_REQUIRE_SANITIZERS:-0}" = "1" ]; then
            printf '%s\n' \
                "required sanitizer configuration unavailable: $sanitizer_name" \
                >&2
            cat "$log_directory/$log_name.probe.log" >&2
            exit 1
        fi
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
    printf '%s\n' cpp c mm | while IFS= read -r extension; do
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
    baseline_plan_path=$3
    header_basename=${header_name##*/}
    expected_path="$plan_path.expected"
    actual_path="$plan_path.actual"
    baseline_sources_path="$plan_path.baseline_sources"
    dependency_path=''
    # Compare only sources that belong to the selected global-all build graph.
    # Some release objects, such as the optional Analytics archive, are omitted
    # from this target even when another build configuration has left matching
    # dependency files in the build tree.
    sed -n 's/.*"__BUILD_PLAN__|compile|libft|[^|]*|\([^" ]*\)".*/\1/p' \
        "$baseline_plan_path" \
        | sort -u >"$baseline_sources_path"
    dependent_sources "$header_name" | sort -u \
        | grep -Fxf "$baseline_sources_path" >"$expected_path" || true
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

capture_archive_order_manifest()
{
    manifest_path=$1
    archive_list_path="$manifest_path.archives"

    find Modules -type f -name '*.a' -print | LC_ALL=C sort \
        >"$archive_list_path"
    if [ -f Full_Libft.a ]; then
        printf '%s\n' 'Full_Libft.a' >>"$archive_list_path"
    fi
    : >"$manifest_path"
    while IFS= read -r archive_path; do
        archive_checksum=$(cksum "$archive_path" | awk '{print $1 ":" $2}')
        printf 'ARCHIVE_CHECKSUM|%s|%s\n' "$archive_path" \
            "$archive_checksum" >>"$manifest_path"
        printf 'ARCHIVE|%s\n' "$archive_path" >>"$manifest_path"
        ar t "$archive_path" | tr -d '\r' >>"$manifest_path"
    done <"$archive_list_path"
}

capture_interrupted_archive_members()
{
    manifest_path=$1
    find Modules -type f -name '*.a' -print | LC_ALL=C sort \
        >"$manifest_path.archives"
    if [ -f Full_Libft.a ]; then
        printf '%s\n' Full_Libft.a >>"$manifest_path.archives"
    fi
    : >"$manifest_path"
    while IFS= read -r archive_path; do
        printf 'ARCHIVE|%s\n' "$archive_path" >>"$manifest_path"
        if ar t "$archive_path" >>"$manifest_path" 2>&1; then
            printf '%s\n' 'AR_EXIT|0' >>"$manifest_path"
        else
            archive_status=$?
            printf 'AR_EXIT|%s\n' "$archive_status" >>"$manifest_path"
        fi
    done <"$manifest_path.archives"
}

copy_checkout_to "$checkout_directory"
cd "$checkout_directory"
printf '%s\n' "incremental-build temporary checkout: $temporary_root" >&2

run_make baseline global-all
release_root=$(find build/libft -type d -name release -print | head -n 1)
if [ -z "$release_root" ]; then
    printf '%s\n' 'could not locate release object root' >&2
    exit 1
fi
default_config_root=${release_root%/release}

# A module-private header must rebuild exactly its dependency closure.
private_header='Modules/Crypto/crypto_random.hpp'
touch "$private_header"
build_plan private_header_plan global-all
assert_plan_matches_dependencies "$private_header" \
    "$log_directory/private_header_plan.plan" \
    "$log_directory/baseline.plan"
run_make private_header_build global-all

# A common header must rebuild every dependent release object and nothing else.
common_header='Modules/Basic/basic.hpp'
sleep 1
touch "$common_header"
build_plan common_header_plan global-all
assert_plan_matches_dependencies "$common_header" \
    "$log_directory/common_header_plan.plan" \
    "$log_directory/baseline.plan"
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

# Removing an archive member must make ordinary Make detect and repair the
# archive even when every object file is older and unchanged.
ar d Modules/Crypto/crypto.a crypto_random.o >/dev/null
build_plan deleted_member_plan global-all
if ! grep -F '__BUILD_PLAN__|archive|libft|Crypto|' \
    "$log_directory/deleted_member_plan.plan" >/dev/null 2>&1 \
    || grep -F '__BUILD_PLAN__|compile|libft|' \
        "$log_directory/deleted_member_plan.plan" >/dev/null 2>&1; then
    printf '%s\n' 'archive-member corruption did not select archive-only repair' >&2
    cat "$log_directory/deleted_member_plan.plan" >&2
    exit 1
fi
run_make deleted_member_build global-all
if ! archive_has_member Modules/Crypto/crypto.a crypto_random.o; then
    printf '%s\n' 'deleted archive member was not restored' >&2
    exit 1
fi

# Removing a manifest source removes the stale member from module and aggregate
# archives.  Restore the manifest before continuing with the next scenario.
manifest='mk/modules/Crypto.mk'
cp "$manifest" "$manifest.incremental-backup"
awk '$0 !~ /crypto_random[.]cpp/ { print }' "$manifest" \
    >"$manifest.incremental-rewritten"
mv "$manifest.incremental-rewritten" "$manifest"
if grep -F 'crypto_random.cpp' "$manifest" >/dev/null 2>&1; then
    printf '%s\n' 'manifest rewrite did not remove crypto_random.cpp' >&2
    diff -u "$manifest.incremental-backup" "$manifest" >&2 || true
    exit 1
fi
run_make manifest_removal_build global-all
if archive_has_member Modules/Crypto/crypto.a crypto_random.o || \
    archive_has_member Full_Libft.a crypto_random.o; then
    printf '%s\n' 'stale member survived manifest removal' >&2
    printf '%s\n' 'Crypto archive members:' >&2
    ar t Modules/Crypto/crypto.a >&2 || true
    printf '%s\n' 'Full archive members matching crypto_random:' >&2
    ar t Full_Libft.a | grep -F 'crypto_random' >&2 || true
    printf '%s\n' 'Manifest-removal build log:' >&2
    cat "$log_directory/manifest_removal_build.log" >&2
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
    ! archive_has_member Modules/Crypto/crypto.a crypto_random_renamed.o || \
    archive_has_member Full_Libft.a crypto_random.o || \
    ! archive_has_member Full_Libft.a crypto_random_renamed.o; then
    printf '%s\n' 'source rename archive membership is incorrect' >&2
    exit 1
fi
mv Modules/Crypto/crypto_random_renamed.cpp Modules/Crypto/crypto_random.cpp
mv "$manifest.incremental-backup" "$manifest"
run_make source_restore_build global-all

# Root graph edits must regenerate archives in place without recompiling
# unrelated sources.  The graph itself is an archive prerequisite.
sleep 1
touch mk/global_graph.mk
build_plan root_graph_change_plan global-all
if ! grep -F '__BUILD_PLAN__|archive|libft|Crypto|' \
    "$log_directory/root_graph_change_plan.plan" >/dev/null 2>&1 \
    || ! grep -F '__BUILD_PLAN__|archive|libft|Full_Libft|' \
    "$log_directory/root_graph_change_plan.plan" >/dev/null 2>&1; then
    printf '%s\n' 'root graph change did not invalidate expected archives' >&2
    cat "$log_directory/root_graph_change_plan.plan" >&2
    exit 1
fi
if grep -F '__BUILD_PLAN__|compile|libft|' \
    "$log_directory/root_graph_change_plan.plan" >/dev/null 2>&1; then
    printf '%s\n' 'root graph-only change unexpectedly recompiled sources' >&2
    cat "$log_directory/root_graph_change_plan.plan" >&2
    exit 1
fi
run_make root_graph_change_build global-all

# Compiler configuration files invalidate archive rules, while a compiler
# command change selects a separate object root through the config fingerprint.
sleep 1
touch mk/build_config.mk
build_plan compiler_config_file_plan global-all
if ! grep -F '__BUILD_PLAN__|archive|libft|Crypto|' \
    "$log_directory/compiler_config_file_plan.plan" >/dev/null 2>&1; then
    printf '%s\n' 'compiler config file did not invalidate archives' >&2
    cat "$log_directory/compiler_config_file_plan.plan" >&2
    exit 1
fi
run_make compiler_config_file_build global-all
build_plan compiler_command_plan CXX=clang++ global-all
if ! grep -F '__BUILD_PLAN__|compile|libft|' \
    "$log_directory/compiler_command_plan.plan" >/dev/null 2>&1 \
    || grep -F "$release_root" "$log_directory/compiler_command_plan.plan" \
        >/dev/null 2>&1; then
    printf '%s\n' 'compiler command change reused the prior object root' >&2
    cat "$log_directory/compiler_command_plan.plan" >&2
    exit 1
fi

# A host/target change selects a separate object root and relinks archives
# even when the command name and flags match the previously recorded build.
build_plan platform_configuration_plan \
    LIBFT_TARGET_PLATFORM=incremental-platform-probe global-all
if ! grep -F '__BUILD_PLAN__|compile|libft|' \
    "$log_directory/platform_configuration_plan.plan" >/dev/null 2>&1 \
    || grep -F "$release_root" \
        "$log_directory/platform_configuration_plan.plan" >/dev/null 2>&1 \
    || ! grep -F '__BUILD_PLAN__|archive|libft|Crypto|' \
        "$log_directory/platform_configuration_plan.plan" >/dev/null 2>&1 \
    || ! grep -F '__BUILD_PLAN__|archive|libft|Full_Libft|' \
        "$log_directory/platform_configuration_plan.plan" >/dev/null 2>&1; then
    printf '%s\n' \
        'platform change reused objects or archives from another configuration' \
        >&2
    cat "$log_directory/platform_configuration_plan.plan" >&2
    exit 1
fi

# Configuration fingerprints must isolate compile-flag changes.
find build/libft -mindepth 1 -maxdepth 1 -type d -print \
    | LC_ALL=C sort >"$log_directory/config_roots.before_opt1"
run_make opt1_build OPT_LEVEL=1 global-all
find build/libft -mindepth 1 -maxdepth 1 -type d -print \
    | LC_ALL=C sort >"$log_directory/config_roots.after_opt1"
opt1_root=$(comm -13 "$log_directory/config_roots.before_opt1" \
    "$log_directory/config_roots.after_opt1" | head -n 1)
if [ -z "$opt1_root" ] || [ "$opt1_root" = "$default_config_root" ]; then
    printf '%s\n' 'OPT_LEVEL did not select a separate object root' >&2
    exit 1
fi
run_make debug_build global-debug
debug_object="$default_config_root/debug/Modules/Crypto/crypto_random.o"
if [ ! -f "$debug_object" ] || [ "$debug_object" = "$release_root/Modules/Crypto/crypto_random.o" ]; then
    printf '%s\n' 'debug build reused or omitted release objects' >&2
    exit 1
fi
run_make test_build tests
test_object="$default_config_root/test/Modules/Crypto/crypto_random.o"
if [ ! -f "$test_object" ] || [ "$test_object" = "$release_root/Modules/Crypto/crypto_random.o" ]; then
    printf '%s\n' 'test build reused or omitted release objects' >&2
    exit 1
fi
run_make test_debug_build debug-tests
test_debug_object="$default_config_root/test_debug/Modules/Crypto/crypto_random.o"
if [ ! -f "$test_debug_object" ] || [ "$test_debug_object" = "$test_object" ]; then
    printf '%s\n' 'test-debug build reused or omitted incompatible objects' >&2
    exit 1
fi
run_optional_sanitizer_build asan_build address
run_optional_sanitizer_build ubsan_build undefined

# Multiple dependency roots in a parallel build must leave unique archives.
touch Modules/Crypto/crypto_random.hpp Modules/Networking/message_transport.hpp
parallel_jobs=$make_jobs
make_jobs=32
run_make parallel32_build global-all
make_jobs=$parallel_jobs
printf '%s\n' Modules/Crypto/crypto.a Modules/Networking/networking.a \
    Full_Libft.a | while IFS= read -r archive_path; do
    archive_members_are_unique "$archive_path"
done

# Delay compiler startup in a disposable wrapper so interruption is guaranteed
# to happen while Make has active compile work.  Reuse the same compiler command
# for recovery so the interrupted configuration's partial objects are checked.
slow_compiler="$temporary_root/slow-cxx"
slow_compiler_marker="$temporary_root/slow-cxx-started"
slow_compiler_make_assignment="CXX=\"$slow_compiler\""
real_compiler=${CXX:-c++}
real_compiler=${real_compiler%% *}
real_compiler=$(command -v "$real_compiler")
cat >"$slow_compiler" <<'SLOW_COMPILER'
#!/usr/bin/env sh
printf '%s\n' started >>"$LIBFT_SLOW_COMPILER_MARKER"
if [ "${LIBFT_SLOW_COMPILER_DELAY:-0}" -gt 0 ]; then
    sleep "$LIBFT_SLOW_COMPILER_DELAY"
fi
exec "$LIBFT_SLOW_COMPILER_REAL" "$@"
SLOW_COMPILER
chmod +x "$slow_compiler"
touch Modules/Basic/basic.hpp
build_plan interrupted_build_plan "$slow_compiler_make_assignment" global-all
if ! grep -F '__BUILD_PLAN__|compile|libft|' \
    "$log_directory/interrupted_build_plan.plan" >/dev/null 2>&1; then
    printf '%s\n' 'interruption scenario did not schedule compilation' >&2
    cat "$log_directory/interrupted_build_plan.plan" >&2
    exit 1
fi
printf 'command=make --no-print-directory -j32 %s global-all\n' \
    "$slow_compiler_make_assignment" \
    >"$log_directory/interrupted_build.evidence"
printf 'LIBFT_SLOW_COMPILER_REAL=%s\nLIBFT_SLOW_COMPILER_DELAY=3\n' \
    "$real_compiler" >>"$log_directory/interrupted_build.evidence"
capture_build_manifest \
    "$log_directory/interrupted_build.manifest.before"
capture_interrupted_archive_members \
    "$log_directory/interrupted_build.archive_order.before"
rm -f "$slow_compiler_marker"
LIBFT_SLOW_COMPILER_MARKER="$slow_compiler_marker" \
    LIBFT_SLOW_COMPILER_REAL="$real_compiler" \
    LIBFT_SLOW_COMPILER_DELAY=3 \
    make --no-print-directory -j32 "$slow_compiler_make_assignment" \
    global-all \
    >"$log_directory/interrupted_build.log" 2>&1 &
build_pid=$!
poll_attempt=0
while [ ! -s "$slow_compiler_marker" ] && [ "$poll_attempt" -lt 100 ]; do
    sleep 0.1
    poll_attempt=$((poll_attempt + 1))
done
if [ ! -s "$slow_compiler_marker" ]; then
    kill -TERM "$build_pid" >/dev/null 2>&1 || true
    wait "$build_pid" >/dev/null 2>&1 || true
    printf '%s\n' 'compiler wrapper did not start; build was not interrupted' >&2
    cat "$log_directory/interrupted_build.log" >&2
    exit 1
fi
sleep 0.1
kill -TERM "$build_pid" >/dev/null 2>&1 || true
if wait "$build_pid" >/dev/null 2>&1; then
    printf '%s\n' 'interrupted build completed despite delayed active compiler' >&2
    exit 1
fi
capture_build_manifest \
    "$log_directory/interrupted_build.manifest.after_interrupt"
capture_interrupted_archive_members \
    "$log_directory/interrupted_build.archive_order.after_interrupt"
LIBFT_SLOW_COMPILER_MARKER="$slow_compiler_marker"
LIBFT_SLOW_COMPILER_REAL="$real_compiler"
LIBFT_SLOW_COMPILER_DELAY=0
export LIBFT_SLOW_COMPILER_MARKER LIBFT_SLOW_COMPILER_REAL \
    LIBFT_SLOW_COMPILER_DELAY
run_make interrupted_restart "$slow_compiler_make_assignment" global-all \
    archive-members-check
unset LIBFT_SLOW_COMPILER_MARKER LIBFT_SLOW_COMPILER_REAL \
    LIBFT_SLOW_COMPILER_DELAY

# Two parent-style consumers build concurrently against this same Libft source
# tree.  Their object roots and archive suffixes are separate, and both parent
# output paths contain spaces.
parent_one="$temporary_root/parent one"
parent_two="$temporary_root/parent two"
mkdir -p "$parent_one" "$parent_two"
(make --no-print-directory -C "$checkout_directory" \
    "-j$make_jobs" \
    "LIBFT_GLOBAL_ROOT=$parent_one/build/libft" \
    LIBFT_GLOBAL_ARCHIVE_SUFFIX=_parent_one \
    TARGET=Full_Libft_parent_one.a \
    global-all archive-members-check \
    >"$log_directory/parent_one.log" 2>&1) &
parent_one_pid=$!
(make --no-print-directory -C "$checkout_directory" \
    "-j$make_jobs" \
    "LIBFT_GLOBAL_ROOT=$parent_two/build/libft" \
    LIBFT_GLOBAL_ARCHIVE_SUFFIX=_parent_two \
    TARGET=Full_Libft_parent_two.a \
    global-all archive-members-check \
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
if [ ! -d "$parent_one/build/libft" ] || \
    [ ! -d "$parent_two/build/libft" ] || \
    [ "$parent_one/build/libft" = "$parent_two/build/libft" ]; then
    printf '%s\n' 'concurrent parent builds did not use separate object roots' >&2
    exit 1
fi
if ! find "$parent_one/build/libft" -type f \
    -path '*/release/Modules/Crypto/crypto_random.o' -print -quit \
    | grep . >/dev/null 2>&1 || \
    ! find "$parent_two/build/libft" -type f \
        -path '*/release/Modules/Crypto/crypto_random.o' -print -quit \
        | grep . >/dev/null 2>&1; then
    printf '%s\n' 'concurrent parent builds did not compile in both roots' >&2
    exit 1
fi
if [ ! -f Modules/Crypto/crypto_parent_one.a ] || \
    [ ! -f Modules/Crypto/crypto_parent_two.a ]; then
    printf '%s\n' 'concurrent parent builds did not create suffixed archives' >&2
    exit 1
fi
printf '%s\n' "$checkout_directory/Full_Libft_parent_one.a" \
    "$checkout_directory/Full_Libft_parent_two.a" \
    | while IFS= read -r parent_archive; do
    if [ ! -f "$parent_archive" ]; then
        printf '%s\n' "concurrent parent archive missing: $parent_archive" >&2
        exit 1
    fi
    archive_members_are_unique "$parent_archive"
done
if find Modules -type f -name '*.tmp.*' -print -quit | grep . \
    >/dev/null 2>&1 || find . -maxdepth 1 -type f \
        -name 'Full_Libft_parent*.tmp.*' -print -quit | grep . \
        >/dev/null 2>&1; then
    printf '%s\n' 'concurrent parent builds left temporary archive files' >&2
    find Modules -type f -name '*.tmp.*' -print >&2
    find . -maxdepth 1 -type f -name 'Full_Libft_parent*.tmp.*' -print >&2
    exit 1
fi

# Parse a checkout rooted beyond the traditional 260-character path limit.
# The dry-run checks that Make can expand the full graph and object paths
# without mutating the active tree or requiring a second full compilation.
long_path_root="$temporary_root/long path"
long_path_segment='segment0123456789abcdef'
while [ "${#long_path_root}" -lt 275 ]; do
    long_path_root="$long_path_root/$long_path_segment"
done
long_path_checkout="$long_path_root/checkout"
copy_checkout_to "$long_path_checkout"
if ! (
    cd "$long_path_checkout"
    make --no-print-directory -n BUILD_PLAN_MODE=1 global-all
) >"$log_directory/long_path_plan.log" 2>&1; then
    printf '%s\n' 'Make could not expand the graph from a long checkout path' >&2
    cat "$log_directory/long_path_plan.log" >&2
    exit 1
fi
if ! (
    cd "$long_path_checkout"
    LIBFT_CI_HEARTBEAT_SECONDS="$heartbeat_seconds" \
        sh "$script_directory/ci_run_with_timeout.sh" \
        "$make_timeout_seconds" make --no-print-directory \
        "-j$make_jobs" Modules/Crypto/crypto.a
) >"$log_directory/long_path_build.log" 2>&1; then
    printf '%s\n' 'Crypto archive build failed from a long checkout path' >&2
    cat "$log_directory/long_path_build.log" >&2
    exit 1
fi
if [ ! -f "$long_path_checkout/Modules/Crypto/crypto.a" ] || \
    ! archive_has_member "$long_path_checkout/Modules/Crypto/crypto.a" \
        crypto_random.o; then
    printf '%s\n' 'long-path build did not produce a complete Crypto archive' >&2
    exit 1
fi

# Two Make processes can also consume the exact same Libft checkout.  Keep
# objects and manifests warm so this scenario isolates concurrent archive
# staging rather than compiling the same source twice.
find Modules -type f -name '*.a' -delete
rm -f Full_Libft.a
make --no-print-directory -j"$make_jobs" global-all \
    >"$log_directory/shared_tree_one.log" 2>&1 &
shared_tree_one_pid=$!
make --no-print-directory -j"$make_jobs" global-all \
    >"$log_directory/shared_tree_two.log" 2>&1 &
shared_tree_two_pid=$!
if ! wait "$shared_tree_one_pid"; then
    cat "$log_directory/shared_tree_one.log" >&2
    exit 1
fi
if ! wait "$shared_tree_two_pid"; then
    cat "$log_directory/shared_tree_two.log" >&2
    exit 1
fi
run_make shared_tree_archive_members archive-members-check
if [ ! -f Full_Libft.a ]; then
    printf '%s\n' 'shared-tree concurrent aggregate archive missing' >&2
    exit 1
fi
archive_members_are_unique Full_Libft.a

# Progress accounting has its own adversarial runner because it watches the
# Make output while jobs complete and must reject missing or double-counted
# work.  Run it from this disposable checkout so it cannot clean the developer
# tree.
if ! sh mk/test_build_progress.sh make \
    >"$log_directory/build_progress.log" 2>&1; then
    printf '%s\n' 'build progress accounting regression failed' >&2
    cat "$log_directory/build_progress.log" >&2
    exit 1
fi

diff_check_directory="$checkout_directory"
if ! git -C "$diff_check_directory" rev-parse --is-inside-work-tree \
    >/dev/null 2>&1; then
    printf '%s\n' \
        'disposable checkout has no Git metadata; checking source repository' \
        >"$log_directory/diff_check.log"
    diff_check_directory="$source_directory"
fi
if ! git -C "$diff_check_directory" -c core.whitespace=cr-at-eol \
    diff --check -- Modules mk Test Makefile \
    >>"$log_directory/diff_check.log" 2>&1; then
    cat "$log_directory/diff_check.log" >&2
    exit 1
fi
git -C "$diff_check_directory" ls-files --others --exclude-standard \
    -- Modules mk Test Makefile >"$log_directory/untracked_files.txt"
while IFS= read -r untracked_file; do
    if [ -z "$untracked_file" ]; then
        continue
    fi
    set +e
    git -C "$diff_check_directory" -c core.whitespace=cr-at-eol \
        diff --no-index --check /dev/null \
        "$diff_check_directory/$untracked_file" \
        >>"$log_directory/diff_check.log" 2>&1
    diff_check_status=$?
    set -e
    if [ "$diff_check_status" -gt 1 ]; then
        cat "$log_directory/diff_check.log" >&2
        exit 1
    fi
done <"$log_directory/untracked_files.txt"

printf '%s\n' 'incremental build graph checks passed'
