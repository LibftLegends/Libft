#include "../../../Modules/Networking/networking_handshake.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_hello_copy(ft_bool with_retry_cookie)
{
    constexpr uint32_t ITERATIONS = 120000U;
    networking_handshake handshake;
    ft_vector<uint8_t> hello;
    const uint8_t cookie[40] = {0x81U};
    ft_size_t expected_length = 80U;
    if (handshake.initialize(networking_handshake_role::CLIENT, 1U) != FT_ERR_SUCCESS)
        return (1);
    if (with_retry_cookie != FT_FALSE)
    {
        if (handshake.set_retry_cookie(cookie) != FT_ERR_SUCCESS)
            return (1);
        expected_length += sizeof(cookie);
    }
    ft_size_t starting_allocations = 0U;
    if (cma_get_stats(&starting_allocations, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (handshake.get_local_hello(hello) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    ft_size_t ending_allocations = 0U;
    if (cma_get_stats(&ending_allocations, ft_nullptr) != FT_ERR_SUCCESS
        || hello.size() != expected_length)
        return (1);
    std::printf("%u-byte hello: %.3f us and %.2f CMA allocations per copy\n",
        static_cast<uint32_t>(expected_length), elapsed_us / ITERATIONS,
        static_cast<double>(ending_allocations - starting_allocations) / ITERATIONS);
    return (0);
}

static int32_t measure_move()
{
    constexpr uint32_t ITERATIONS = 100000U;
    networking_handshake source;
    networking_handshake peer;
    networking_handshake destination;
    ft_vector<uint8_t> peer_hello;
    if (source.initialize(networking_handshake_role::CLIENT, 1U) != FT_ERR_SUCCESS
        || peer.initialize(networking_handshake_role::SERVER, 2U) != FT_ERR_SUCCESS
        || peer.get_local_hello(peer_hello) != FT_ERR_SUCCESS
        || source.accept_peer_hello(peer_hello.begin(), peer_hello.size()) != FT_ERR_SUCCESS
        || source.derive_keys() != FT_ERR_SUCCESS)
        return (1);
    ft_size_t starting_allocations = 0U;
    if (cma_get_stats(&starting_allocations, ft_nullptr) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (destination.move(source) != FT_ERR_SUCCESS
            || source.move(destination) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    ft_size_t ending_allocations = 0U;
    if (cma_get_stats(&ending_allocations, ft_nullptr) != FT_ERR_SUCCESS
        || source.get_state() != networking_handshake_state::KEYS_DERIVED)
        return (1);
    std::printf("Handshake move: %.3f us and %.2f CMA allocations per move\n",
        elapsed_us / (2U * ITERATIONS),
        static_cast<double>(ending_allocations - starting_allocations) / (2U * ITERATIONS));
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--move") == 0)
        return (measure_move());
    if (argument_count != 1 || measure_hello_copy(FT_FALSE) != 0
        || measure_hello_copy(FT_TRUE) != 0 || measure_move() != 0)
        return (1);
    return (0);
}
