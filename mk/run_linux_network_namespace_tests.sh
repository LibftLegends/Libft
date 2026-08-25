#!/usr/bin/env sh

set -eu

if ! command -v ip >/dev/null 2>&1 || ! command -v tc >/dev/null 2>&1; then
    printf '%s\n' 'iproute2 is required for namespace/netem tests' >&2
    exit 1
fi
if [ "$(id -u)" -eq 0 ]; then
    privilege=""
else
    privilege="sudo -n"
fi

namespace="libft-netem-$$"
cleanup()
{
    $privilege ip netns delete "$namespace" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM HUP

make --no-print-directory -j4 tests
test_executable="./Test/libft_tests"
if [ -f "${test_executable}.exe" ]; then
    test_executable="${test_executable}.exe"
fi
$privilege ip netns add "$namespace"
$privilege ip -n "$namespace" link set lo up
$privilege ip netns exec "$namespace" tc qdisc replace dev lo root \
    netem delay 5ms 2ms
$privilege ip netns exec "$namespace" env \
    FT_TEST_NAME_FILTER="test_networking_udp_datagram_adapter_loopback" \
    "$test_executable"

printf '%s\n' 'Linux namespace/netem networking smoke test passed'
