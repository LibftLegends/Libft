#!/usr/bin/env sh

set -eu

if ! command -v ip >/dev/null 2>&1 || ! command -v tc >/dev/null 2>&1; then
    printf '%s\n' 'iproute2 is required for namespace/netem tests' >&2
    exit 1
fi
if ! command -v iptables >/dev/null 2>&1; then
    printf '%s\n' 'iptables is required for restricted-NAT namespace tests' >&2
    exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
    printf '%s\n' 'python3 is required for symmetric-NAT namespace tests' >&2
    exit 1
fi
if [ "$(id -u)" -eq 0 ]; then
    privilege=""
else
    privilege="sudo -n"
fi

namespace_one="libft-netem-a-$$"
namespace_two="libft-netem-b-$$"
namespace_client="libft-netem-client-$$"
namespace_nat="libft-netem-nat-$$"
namespace_server="libft-netem-server-$$"
namespace_symmetric_client="libft-netem-sym-client-$$"
namespace_symmetric_nat="libft-netem-sym-nat-$$"
namespace_symmetric_server="libft-netem-sym-server-$$"
interface_one="lfa$$"
interface_two="lfb$$"
interface_client="lfc$$"
interface_nat_inside="lfni$$"
interface_nat_outside="lfno$$"
interface_server="lfs$$"
interface_symmetric_client="lfsc$$"
interface_symmetric_nat_inside="lfsni$$"
interface_symmetric_nat_outside="lfsno$$"
interface_symmetric_server="lfss$$"
server_log="${TMPDIR:-/tmp}/libft-netem-server-$$.log"
client_log="${TMPDIR:-/tmp}/libft-netem-client-$$.log"
server_pid=""
cleanup()
{
    if [ -n "$server_pid" ]; then
        kill "$server_pid" >/dev/null 2>&1 || true
        wait "$server_pid" >/dev/null 2>&1 || true
    fi
    $privilege ip netns delete "$namespace_one" >/dev/null 2>&1 || true
    $privilege ip netns delete "$namespace_two" >/dev/null 2>&1 || true
    $privilege ip netns delete "$namespace_client" >/dev/null 2>&1 || true
    $privilege ip netns delete "$namespace_nat" >/dev/null 2>&1 || true
    $privilege ip netns delete "$namespace_server" >/dev/null 2>&1 || true
    $privilege ip netns delete "$namespace_symmetric_client" >/dev/null 2>&1 || true
    $privilege ip netns delete "$namespace_symmetric_nat" >/dev/null 2>&1 || true
    $privilege ip netns delete "$namespace_symmetric_server" >/dev/null 2>&1 || true
    rm -f "$server_log" "$client_log"
}
trap cleanup EXIT INT TERM HUP

make --no-print-directory -j4 tests
test_executable="./Test/libft_tests"
if [ -f "${test_executable}.exe" ]; then
    test_executable="${test_executable}.exe"
fi
$privilege ip netns add "$namespace_one"
$privilege ip netns add "$namespace_two"
$privilege ip link add "$interface_one" type veth peer name "$interface_two"
$privilege ip link set "$interface_one" netns "$namespace_one"
$privilege ip link set "$interface_two" netns "$namespace_two"
$privilege ip -n "$namespace_one" addr add 10.89.1.1/24 dev "$interface_one"
$privilege ip -n "$namespace_two" addr add 10.89.1.2/24 dev "$interface_two"
$privilege ip -n "$namespace_one" link set lo up
$privilege ip -n "$namespace_two" link set lo up
$privilege ip -n "$namespace_one" link set "$interface_one" up
$privilege ip -n "$namespace_two" link set "$interface_two" up
$privilege ip -n "$namespace_one" tc qdisc replace dev "$interface_one" root \
    netem delay 5ms 2ms
$privilege ip -n "$namespace_two" tc qdisc replace dev "$interface_two" root \
    netem delay 5ms 2ms

$privilege ip netns exec "$namespace_one" env \
    LIBFT_NETWORK_NAMESPACE_TESTS=1 \
    FT_TEST_NAME_FILTER="test_networking_message_namespace_server_process" \
    timeout 30s "$test_executable" >"$server_log" 2>&1 &
server_pid=$!
client_status=0
$privilege ip netns exec "$namespace_two" env \
    LIBFT_NETWORK_NAMESPACE_TESTS=1 \
    FT_TEST_NAME_FILTER="test_networking_message_namespace_client_process" \
    timeout 30s "$test_executable" >"$client_log" 2>&1 || client_status=$?
server_status=0
wait "$server_pid" || server_status=$?
server_pid=""
if [ "$client_status" -ne 0 ] || [ "$server_status" -ne 0 ]; then
    cat "$server_log" "$client_log" >&2
    exit 1
fi

$privilege ip netns add "$namespace_symmetric_client"
$privilege ip netns add "$namespace_symmetric_nat"
$privilege ip netns add "$namespace_symmetric_server"
$privilege ip link add "$interface_symmetric_client" type veth peer name "$interface_symmetric_nat_inside"
$privilege ip link set "$interface_symmetric_client" netns "$namespace_symmetric_client"
$privilege ip link set "$interface_symmetric_nat_inside" netns "$namespace_symmetric_nat"
$privilege ip link add "$interface_symmetric_nat_outside" type veth peer name "$interface_symmetric_server"
$privilege ip link set "$interface_symmetric_nat_outside" netns "$namespace_symmetric_nat"
$privilege ip link set "$interface_symmetric_server" netns "$namespace_symmetric_server"
$privilege ip -n "$namespace_symmetric_client" addr add 10.89.3.2/24 dev "$interface_symmetric_client"
$privilege ip -n "$namespace_symmetric_nat" addr add 10.89.3.1/24 dev "$interface_symmetric_nat_inside"
$privilege ip -n "$namespace_symmetric_nat" addr add 10.90.3.1/24 dev "$interface_symmetric_nat_outside"
$privilege ip -n "$namespace_symmetric_server" addr add 10.90.3.2/24 dev "$interface_symmetric_server"
$privilege ip -n "$namespace_symmetric_server" addr add 10.90.3.3/24 dev "$interface_symmetric_server"
$privilege ip -n "$namespace_symmetric_client" link set lo up
$privilege ip -n "$namespace_symmetric_nat" link set lo up
$privilege ip -n "$namespace_symmetric_server" link set lo up
$privilege ip -n "$namespace_symmetric_client" link set "$interface_symmetric_client" up
$privilege ip -n "$namespace_symmetric_nat" link set "$interface_symmetric_nat_inside" up
$privilege ip -n "$namespace_symmetric_nat" link set "$interface_symmetric_nat_outside" up
$privilege ip -n "$namespace_symmetric_server" link set "$interface_symmetric_server" up
$privilege ip -n "$namespace_symmetric_client" route add default via 10.89.3.1
$privilege ip netns exec "$namespace_symmetric_nat" sysctl -q -w net.ipv4.ip_forward=1
$privilege ip netns exec "$namespace_symmetric_nat" iptables -P FORWARD DROP
$privilege ip netns exec "$namespace_symmetric_nat" iptables -A FORWARD \
    -i "$interface_symmetric_nat_inside" -o "$interface_symmetric_nat_outside" \
    -s 10.89.3.0/24 -d 10.90.3.0/24 -p udp --dport 49322 -j ACCEPT
$privilege ip netns exec "$namespace_symmetric_nat" iptables -A FORWARD \
    -i "$interface_symmetric_nat_outside" -o "$interface_symmetric_nat_inside" \
    -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
$privilege ip netns exec "$namespace_symmetric_nat" iptables -t nat -A POSTROUTING \
    -o "$interface_symmetric_nat_outside" -d 10.90.3.2 -p udp --dport 49322 \
    -j SNAT --to-source 10.90.3.1:42001
$privilege ip netns exec "$namespace_symmetric_nat" iptables -t nat -A POSTROUTING \
    -o "$interface_symmetric_nat_outside" -d 10.90.3.3 -p udp --dport 49322 \
    -j SNAT --to-source 10.90.3.1:42002
$privilege ip netns exec "$namespace_symmetric_server" python3 \
    mk/networking_symmetric_nat_probe.py server >"$server_log" 2>&1 &
server_pid=$!
client_status=0
$privilege ip netns exec "$namespace_symmetric_client" python3 \
    mk/networking_symmetric_nat_probe.py client >"$client_log" 2>&1 || client_status=$?
server_status=0
wait "$server_pid" || server_status=$?
server_pid=""
if [ "$client_status" -ne 0 ] || [ "$server_status" -ne 0 ]; then
    cat "$server_log" "$client_log" >&2
    exit 1
fi

$privilege ip netns add "$namespace_client"
$privilege ip netns add "$namespace_nat"
$privilege ip netns add "$namespace_server"
$privilege ip link add "$interface_client" type veth peer name "$interface_nat_inside"
$privilege ip link set "$interface_client" netns "$namespace_client"
$privilege ip link set "$interface_nat_inside" netns "$namespace_nat"
$privilege ip link add "$interface_nat_outside" type veth peer name "$interface_server"
$privilege ip link set "$interface_nat_outside" netns "$namespace_nat"
$privilege ip link set "$interface_server" netns "$namespace_server"
$privilege ip -n "$namespace_client" addr add 10.89.2.2/24 dev "$interface_client"
$privilege ip -n "$namespace_nat" addr add 10.89.2.1/24 dev "$interface_nat_inside"
$privilege ip -n "$namespace_nat" addr add 10.90.2.1/24 dev "$interface_nat_outside"
$privilege ip -n "$namespace_server" addr add 10.90.2.2/24 dev "$interface_server"
$privilege ip -n "$namespace_client" link set lo up
$privilege ip -n "$namespace_nat" link set lo up
$privilege ip -n "$namespace_server" link set lo up
$privilege ip -n "$namespace_client" link set "$interface_client" up
$privilege ip -n "$namespace_nat" link set "$interface_nat_inside" up
$privilege ip -n "$namespace_nat" link set "$interface_nat_outside" up
$privilege ip -n "$namespace_server" link set "$interface_server" up
$privilege ip -n "$namespace_client" route add default via 10.89.2.1
$privilege ip -n "$namespace_server" route add default via 10.90.2.1
$privilege ip netns exec "$namespace_nat" sysctl -q -w net.ipv4.ip_forward=1
$privilege ip netns exec "$namespace_nat" iptables -P FORWARD DROP
$privilege ip netns exec "$namespace_nat" iptables -A FORWARD -i "$interface_nat_inside" \
    -o "$interface_nat_outside" -s 10.89.2.0/24 -d 10.90.2.0/24 -j ACCEPT
$privilege ip netns exec "$namespace_nat" iptables -A FORWARD -i "$interface_nat_outside" \
    -o "$interface_nat_inside" -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT
$privilege ip netns exec "$namespace_nat" iptables -t nat -A POSTROUTING \
    -o "$interface_nat_outside" -j MASQUERADE
$privilege ip netns exec "$namespace_nat" iptables -A INPUT -i lo -j ACCEPT
$privilege ip netns exec "$namespace_nat" iptables -A INPUT -m conntrack \
    --ctstate ESTABLISHED,RELATED -j ACCEPT
$privilege ip netns exec "$namespace_nat" iptables -A INPUT -i "$interface_nat_inside" \
    -s 10.89.2.0/24 -j ACCEPT
$privilege ip netns exec "$namespace_nat" iptables -P INPUT DROP
$privilege ip netns exec "$namespace_server" env \
    LIBFT_NETWORK_NAMESPACE_TESTS=1 LIBFT_NETWORK_NAMESPACE_SERVER_IP=10.90.2.2 \
    FT_TEST_NAME_FILTER="test_networking_message_namespace_server_process" \
    timeout 30s "$test_executable" >"$server_log" 2>&1 &
server_pid=$!
client_status=0
$privilege ip netns exec "$namespace_client" env \
    LIBFT_NETWORK_NAMESPACE_TESTS=1 LIBFT_NETWORK_NAMESPACE_SERVER_IP=10.90.2.2 \
    FT_TEST_NAME_FILTER="test_networking_message_namespace_client_process" \
    timeout 30s "$test_executable" >"$client_log" 2>&1 || client_status=$?
server_status=0
wait "$server_pid" || server_status=$?
server_pid=""
if [ "$client_status" -ne 0 ] || [ "$server_status" -ne 0 ]; then
    cat "$server_log" "$client_log" >&2
    exit 1
fi

$privilege ip -n "$namespace_two" tc qdisc replace dev "$interface_two" root \
    netem loss 100%
$privilege ip -n "$namespace_one" tc qdisc replace dev "$interface_one" root \
    netem loss 100%
$privilege ip netns exec "$namespace_one" env \
    LIBFT_NETWORK_NAMESPACE_TESTS=1 LIBFT_NETWORK_NAMESPACE_BLOCK_UDP=1 \
    FT_TEST_NAME_FILTER="test_networking_message_namespace_server_process" \
    timeout 15s "$test_executable" >"$server_log" 2>&1 &
server_pid=$!
client_status=0
$privilege ip netns exec "$namespace_two" env \
    LIBFT_NETWORK_NAMESPACE_TESTS=1 LIBFT_NETWORK_NAMESPACE_BLOCK_UDP=1 \
    FT_TEST_NAME_FILTER="test_networking_message_namespace_client_process" \
    timeout 15s "$test_executable" >"$client_log" 2>&1 || client_status=$?
server_status=0
wait "$server_pid" || server_status=$?
server_pid=""
if [ "$client_status" -ne 0 ] || [ "$server_status" -ne 0 ]; then
    cat "$server_log" "$client_log" >&2
    exit 1
fi

printf '%s\n' 'Linux namespace direct-path, restricted-NAT, symmetric-NAT, and blocked-UDP tests passed'
