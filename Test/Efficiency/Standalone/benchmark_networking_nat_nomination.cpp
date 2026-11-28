#include "../../Test/networking_test_support.hpp"
#include "../../../Modules/Networking/networking_nat_traversal.hpp"
#include <chrono>
#include <cstdio>

class networking_nat_benchmark_verifier : public networking_nat_ticket_verifier
{
    public:
        networking_nat_benchmark_verifier() noexcept;
        ~networking_nat_benchmark_verifier() noexcept override;
        ft_bool verify(const networking_nat_peer_ticket &ticket) noexcept override;
};

networking_nat_benchmark_verifier::networking_nat_benchmark_verifier() noexcept
{
    return ;
}

networking_nat_benchmark_verifier::~networking_nat_benchmark_verifier() noexcept
{
    return ;
}

ft_bool networking_nat_benchmark_verifier::verify(
    const networking_nat_peer_ticket &ticket) noexcept
{
    return (ticket.signature.size() == 1U);
}

static int32_t measure_nomination(uint32_t candidate_count)
{
    constexpr uint32_t ITERATIONS = 250000U;
    networking_nat_traversal traversal;
    networking_nat_peer_ticket ticket;
    networking_nat_benchmark_verifier verifier;
    networking_nat_candidate local_candidate;
    networking_nat_candidate remote_candidate;

    if (traversal.initialize(1U, 50U) != FT_ERR_SUCCESS
        || ticket.initialize() != FT_ERR_SUCCESS
        || ticket.signature.push_back(1U) != FT_ERR_SUCCESS)
        return (1);
    ticket.peer_id = 2U;
    ticket.attempt_id = 21U;
    ticket.expires_at = 1000U;
    networking_test_prepare_endpoint(local_candidate.endpoint);
    networking_test_prepare_endpoint(remote_candidate.endpoint);
    sockaddr_in *local_address = reinterpret_cast<sockaddr_in *>(
        &local_candidate.endpoint.address);
    sockaddr_in *remote_address = reinterpret_cast<sockaddr_in *>(
        &remote_candidate.endpoint.address);
    uint32_t index = 0U;
    while (index < candidate_count)
    {
        local_address->sin_port = htons(static_cast<uint16_t>(3000U + index));
        remote_address->sin_port = htons(static_cast<uint16_t>(4000U + index));
        if (traversal.add_local_candidate(local_candidate) != FT_ERR_SUCCESS
            || ticket.candidates.push_back(remote_candidate) != FT_ERR_SUCCESS)
            return (1);
        index += 1U;
    }
    if (traversal.set_peer_ticket(ticket, 0U, verifier) != FT_ERR_SUCCESS)
        return (1);
    const auto ticket_start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        if (traversal.set_peer_ticket(ticket, 0U, verifier) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double ticket_elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - ticket_start).count();
    const auto start = std::chrono::steady_clock::now();
    iteration = 0U;
    while (iteration < ITERATIONS)
    {
        if (traversal.mark_probe_success(local_candidate.endpoint,
                remote_candidate.endpoint, ticket.attempt_id) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%u candidates per side: %.2f ns per ticket, %.2f ns per nomination\n",
        candidate_count, ticket_elapsed_ns / ITERATIONS, elapsed_ns / ITERATIONS);
    return (0);
}

int32_t main()
{
    if (measure_nomination(1U) != 0
        || measure_nomination(NETWORKING_NAT_MAX_CANDIDATES) != 0)
        return (1);
    return (0);
}
