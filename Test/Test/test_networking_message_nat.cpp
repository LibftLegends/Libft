#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Networking/networking_test_hooks.hpp"
#include "networking_nat_test_support.hpp"

class networking_nat_test_verifier : public networking_nat_ticket_verifier
{
    private:
        ft_bool _accept;
        uint32_t _calls;

    public:
        networking_nat_test_verifier() noexcept : _accept(FT_FALSE),
            _calls(0U)
        {
            return ;
        }

        ~networking_nat_test_verifier() noexcept override
        {
            return ;
        }

        ft_bool verify(const networking_nat_peer_ticket &ticket) noexcept override
        {
            (void)ticket;
            this->_calls += 1U;
            return (this->_accept);
        }

        void set_accept(ft_bool accept) noexcept
        {
            this->_accept = accept;
            return ;
        }

        uint32_t get_calls() const noexcept
        {
            return (this->_calls);
        }
};

class networking_nat_duplicate_candidate_provider
    : public networking_nat_candidate_provider
{
    public:
        networking_message_endpoint endpoint;

        networking_nat_duplicate_candidate_provider() noexcept : endpoint()
        {
            ft_memset(&this->endpoint, 0, sizeof(this->endpoint));
            return ;
        }

        ~networking_nat_duplicate_candidate_provider() noexcept override
        {
            return ;
        }

        int32_t gather(
            ft_vector<networking_nat_candidate> &candidates) noexcept override
        {
            networking_nat_candidate candidate;

            candidate.endpoint = this->endpoint;
            candidate.type = networking_nat_candidate_type::HOST;
            candidate.priority = 100U;
            if (candidates.push_back(candidate) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            candidate.priority = 200U;
            return (candidates.push_back(candidate));
        }
};

class networking_nat_test_probe : public networking_nat_probe_io
{
    public:
        uint32_t calls;

        networking_nat_test_probe() noexcept : calls(0U)
        {
            return ;
        }

        ~networking_nat_test_probe() noexcept override
        {
            return ;
        }

        int32_t send_probe(const networking_message_endpoint &local,
            const networking_message_endpoint &remote, uint64_t attempt_id)
            noexcept override
        {
            (void)local;
            (void)remote;
            (void)attempt_id;
            this->calls += 1U;
            return (FT_ERR_SUCCESS);
        }
};

static void networking_nat_prepare_ipv6_endpoint(
    networking_message_endpoint &endpoint, uint8_t host, uint16_t port)
    noexcept;

static void networking_nat_prepare_ipv4_endpoint(
    networking_message_endpoint &endpoint, uint8_t host, uint16_t port)
    noexcept
{
    struct sockaddr_in *address;

    ft_memset(&endpoint, 0, sizeof(endpoint));
    address = reinterpret_cast<struct sockaddr_in *>(&endpoint.address);
    address->sin_family = AF_INET;
    address->sin_port = htons(port);
    address->sin_addr.s_addr = htonl(0xc0000200U | host);
    endpoint.length = sizeof(struct sockaddr_in);
    return ;
}

class networking_nat_test_relay : public networking_nat_relay_io
{
    public:
        uint32_t opens;
        uint32_t closes;
        int32_t open_result;
        ft_bool malformed_endpoint;

        networking_nat_test_relay() noexcept : opens(0U), closes(0U),
            open_result(FT_ERR_SUCCESS), malformed_endpoint(FT_FALSE)
        {
            return ;
        }

        ~networking_nat_test_relay() noexcept override
        {
            return ;
        }

        int32_t open_relay(uint64_t local_peer_id, uint64_t remote_peer_id,
            uint64_t attempt_id,
            networking_message_endpoint &relay_endpoint) noexcept override
        {
            (void)local_peer_id;
            (void)remote_peer_id;
            (void)attempt_id;
            this->opens += 1U;
            if (this->open_result != FT_ERR_SUCCESS)
                return (this->open_result);
            if (this->malformed_endpoint != FT_FALSE)
            {
                ft_memset(&relay_endpoint, 0, sizeof(relay_endpoint));
                relay_endpoint.length = sizeof(sockaddr_in6);
                return (FT_ERR_SUCCESS);
            }
            networking_nat_prepare_ipv6_endpoint(relay_endpoint, 9U, 9000U);
            return (FT_ERR_SUCCESS);
        }

        int32_t send_relay(
            const networking_message_endpoint &relay_endpoint,
            const uint8_t *data, ft_size_t size) noexcept override
        {
            (void)relay_endpoint;
            (void)data;
            (void)size;
            return (FT_ERR_SUCCESS);
        }

        int32_t receive_relay(
            const networking_message_endpoint &relay_endpoint,
            uint8_t *data, ft_size_t capacity,
            ft_size_t *received_size) noexcept override
        {
            (void)relay_endpoint;
            (void)data;
            (void)capacity;
            if (received_size != ft_nullptr)
                *received_size = 0U;
            return (FT_ERR_EMPTY);
        }

        int32_t close_relay(
            const networking_message_endpoint &relay_endpoint) noexcept override
        {
            (void)relay_endpoint;
            this->closes += 1U;
            return (FT_ERR_SUCCESS);
        }
};

static void networking_nat_prepare_ipv6_endpoint(
    networking_message_endpoint &endpoint, uint8_t host, uint16_t port)
    noexcept
{
    ft_memset(&endpoint, 0, sizeof(endpoint));
    reinterpret_cast<sockaddr_in6 *>(&endpoint.address)->sin6_family = AF_INET6;
    reinterpret_cast<sockaddr_in6 *>(&endpoint.address)->sin6_port = htons(port);
    reinterpret_cast<sockaddr_in6 *>(&endpoint.address)->sin6_addr.s6_addr[15]
        = host;
    endpoint.length = sizeof(sockaddr_in6);
    return ;
}

FT_TEST(test_networking_nat_model_ipv6_restriction_matrix)
{
    const networking_nat_model_type model_types[8] = {
        networking_nat_model_type::FULL_CONE,
        networking_nat_model_type::ADDRESS_RESTRICTED,
        networking_nat_model_type::PORT_RESTRICTED,
        networking_nat_model_type::SYMMETRIC,
        networking_nat_model_type::DOUBLE_NAT,
        networking_nat_model_type::UDP_BLOCKED,
        networking_nat_model_type::HAIRPIN_SUPPORTED,
        networking_nat_model_type::HAIRPIN_UNSUPPORTED};
    networking_nat_test_model model;
    networking_message_endpoint local;
    networking_message_endpoint remote;
    networking_message_endpoint same_host_other_port;
    networking_message_endpoint other_host_same_port;
    ft_size_t index;
    int32_t send_result;

    networking_nat_prepare_ipv6_endpoint(local, 1U, 3000U);
    networking_nat_prepare_ipv6_endpoint(remote, 1U, 4000U);
    networking_nat_prepare_ipv6_endpoint(same_host_other_port, 1U, 4001U);
    networking_nat_prepare_ipv6_endpoint(other_host_same_port, 2U, 4000U);
    index = 0U;
    while (index < 8U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, model.initialize(model_types[index]));
        send_result = model.send_probe(local, remote, 9U);
        if (model_types[index] == networking_nat_model_type::UDP_BLOCKED)
            FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, send_result);
        else
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, send_result);
            if (model_types[index] == networking_nat_model_type::FULL_CONE)
                FT_ASSERT(model.accepts_response(other_host_same_port));
            if (model_types[index] == networking_nat_model_type::ADDRESS_RESTRICTED)
            {
                FT_ASSERT(model.accepts_response(same_host_other_port));
                FT_ASSERT(!model.accepts_response(other_host_same_port));
            }
            if (model_types[index] == networking_nat_model_type::PORT_RESTRICTED)
                FT_ASSERT(!model.accepts_response(same_host_other_port));
            if (model_types[index] == networking_nat_model_type::SYMMETRIC)
            {
                FT_ASSERT(model.accepts_response(remote));
                FT_ASSERT(!model.accepts_response(other_host_same_port));
            }
            if (model_types[index] == networking_nat_model_type::DOUBLE_NAT)
                FT_ASSERT(!model.accepts_response(remote));
            if (model_types[index]
                == networking_nat_model_type::HAIRPIN_SUPPORTED)
                FT_ASSERT(model.accepts_response(local));
            if (model_types[index]
                == networking_nat_model_type::HAIRPIN_UNSUPPORTED)
                FT_ASSERT(!model.accepts_response(local));
        }
        index += 1U;
    }
    return (1);
}

FT_TEST(test_networking_nat_model_ipv4_restriction_matrix)
{
    const networking_nat_model_type model_types[8] = {
        networking_nat_model_type::FULL_CONE,
        networking_nat_model_type::ADDRESS_RESTRICTED,
        networking_nat_model_type::PORT_RESTRICTED,
        networking_nat_model_type::SYMMETRIC,
        networking_nat_model_type::DOUBLE_NAT,
        networking_nat_model_type::UDP_BLOCKED,
        networking_nat_model_type::HAIRPIN_SUPPORTED,
        networking_nat_model_type::HAIRPIN_UNSUPPORTED};
    networking_nat_test_model model;
    networking_message_endpoint local;
    networking_message_endpoint remote;
    networking_message_endpoint same_host_other_port;
    networking_message_endpoint other_host_same_port;
    ft_size_t index;
    int32_t send_result;

    networking_nat_prepare_ipv4_endpoint(local, 1U, 3000U);
    networking_nat_prepare_ipv4_endpoint(remote, 1U, 4000U);
    networking_nat_prepare_ipv4_endpoint(same_host_other_port, 1U, 4001U);
    networking_nat_prepare_ipv4_endpoint(other_host_same_port, 2U, 4000U);
    index = 0U;
    while (index < 8U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, model.initialize(model_types[index]));
        send_result = model.send_probe(local, remote, 9U);
        if (model_types[index] == networking_nat_model_type::UDP_BLOCKED)
            FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, send_result);
        else
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, send_result);
            if (model_types[index] == networking_nat_model_type::FULL_CONE)
                FT_ASSERT(model.accepts_response(other_host_same_port));
            if (model_types[index] == networking_nat_model_type::ADDRESS_RESTRICTED)
            {
                FT_ASSERT(model.accepts_response(same_host_other_port));
                FT_ASSERT(!model.accepts_response(other_host_same_port));
            }
            if (model_types[index] == networking_nat_model_type::PORT_RESTRICTED)
                FT_ASSERT(!model.accepts_response(same_host_other_port));
            if (model_types[index] == networking_nat_model_type::SYMMETRIC)
            {
                FT_ASSERT(model.accepts_response(remote));
                FT_ASSERT(!model.accepts_response(other_host_same_port));
            }
            if (model_types[index] == networking_nat_model_type::DOUBLE_NAT)
                FT_ASSERT(!model.accepts_response(remote));
            if (model_types[index]
                == networking_nat_model_type::HAIRPIN_SUPPORTED)
                FT_ASSERT(model.accepts_response(local));
            if (model_types[index]
                == networking_nat_model_type::HAIRPIN_UNSUPPORTED)
                FT_ASSERT(!model.accepts_response(local));
        }
        index += 1U;
    }
    return (1);
}

FT_TEST(test_networking_nat_ticket_verifier_and_expiry_are_transactional)
{
    networking_nat_traversal traversal;
    networking_nat_candidate local_candidate;
    networking_nat_candidate remote_candidate;
    networking_nat_candidate selected_local;
    networking_nat_candidate selected_remote;
    networking_nat_candidate prior_selected_local;
    networking_nat_candidate prior_selected_remote;
    networking_nat_peer_ticket ticket;
    networking_nat_test_verifier verifier;
    networking_message_endpoint endpoint;

    networking_nat_prepare_ipv6_endpoint(endpoint, 1U, 4000U);
    local_candidate.endpoint = endpoint;
    local_candidate.type = networking_nat_candidate_type::HOST;
    local_candidate.priority = 1000U;
    remote_candidate.endpoint = endpoint;
    remote_candidate.type = networking_nat_candidate_type::SERVER_REFLEXIVE;
    remote_candidate.priority = 900U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.peer_id = 2U;
    ticket.attempt_id = 3U;
    ticket.expires_at = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.signature.push_back(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(remote_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 100U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.add_local_candidate(local_candidate));
    FT_ASSERT_EQ(FT_ERR_UNSUPPORTED_TYPE,
        traversal.set_peer_ticket(ticket, 0U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND,
        traversal.get_selected_pair(selected_local, selected_remote));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
        traversal.set_peer_ticket(ticket, 0U, verifier));
    FT_ASSERT_EQ(1U, verifier.get_calls());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND,
        traversal.get_selected_pair(selected_local, selected_remote));
    verifier.set_accept(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
        traversal.set_peer_ticket(ticket, 10U, verifier));
    FT_ASSERT_EQ(1U, verifier.get_calls());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND,
        traversal.get_selected_pair(selected_local, selected_remote));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.set_peer_ticket(ticket, 9U, verifier));
    FT_ASSERT_EQ(2U, verifier.get_calls());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.get_selected_pair(selected_local, selected_remote));
    prior_selected_local = selected_local;
    prior_selected_remote = selected_remote;
    ticket.candidates[0].endpoint.length = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        traversal.set_peer_ticket(ticket, 9U, verifier));
    FT_ASSERT_EQ(2U, verifier.get_calls());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.get_selected_pair(selected_local, selected_remote));
    FT_ASSERT_EQ(prior_selected_local.endpoint.length,
        selected_local.endpoint.length);
    FT_ASSERT_EQ(0, ft_memcmp(&prior_selected_local.endpoint.address,
        &selected_local.endpoint.address, selected_local.endpoint.length));
    FT_ASSERT_EQ(prior_selected_remote.endpoint.length,
        selected_remote.endpoint.length);
    FT_ASSERT_EQ(0, ft_memcmp(&prior_selected_remote.endpoint.address,
        &selected_remote.endpoint.address, selected_remote.endpoint.length));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.destroy());
    return (1);
}

FT_TEST(test_networking_nat_deduplicates_candidates_and_enforces_limit)
{
    networking_nat_traversal traversal;
    networking_nat_duplicate_candidate_provider provider;
    networking_nat_test_probe probe;
    networking_nat_candidate remote_candidate;
    networking_nat_candidate selected_local;
    networking_nat_candidate selected_remote;
    networking_nat_peer_ticket ticket;
    networking_nat_test_verifier verifier;
    networking_message_endpoint endpoint;
    uint32_t candidate_index;

    networking_nat_prepare_ipv6_endpoint(endpoint, 1U, 3000U);
    provider.endpoint = endpoint;
    networking_nat_prepare_ipv6_endpoint(remote_candidate.endpoint, 2U,
        4000U);
    remote_candidate.type = networking_nat_candidate_type::SERVER_REFLEXIVE;
    remote_candidate.priority = 900U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.peer_id = 2U;
    ticket.attempt_id = 7U;
    ticket.expires_at = 100U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.signature.push_back(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(remote_candidate));
    verifier.set_accept(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.gather_candidates(provider));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.set_peer_ticket(ticket, 0U,
        verifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.get_selected_pair(selected_local,
        selected_remote));
    FT_ASSERT_EQ(200U, selected_local.priority);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.begin(1U, probe));
    FT_ASSERT_EQ(1U, probe.calls);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, traversal.probe_next(2U, 0U, probe));
    FT_ASSERT_EQ(1U, probe.calls);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.destroy());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.peer_id = 2U;
    ticket.attempt_id = 8U;
    ticket.expires_at = 100U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.signature.push_back(1U));
    verifier.set_accept(FT_TRUE);
    candidate_index = 0U;
    while (candidate_index <= NETWORKING_NAT_MAX_CANDIDATES)
    {
        networking_nat_candidate candidate;

        networking_nat_prepare_ipv6_endpoint(candidate.endpoint,
            static_cast<uint8_t>(candidate_index + 1U),
            static_cast<uint16_t>(4001U + candidate_index));
        candidate.type = networking_nat_candidate_type::SERVER_REFLEXIVE;
        candidate.priority = 800U;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(candidate));
        candidate_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.add_local_candidate(
        selected_local));
    FT_ASSERT_EQ(FT_ERR_FULL, traversal.set_peer_ticket(ticket, 0U,
        verifier));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, traversal.get_selected_pair(selected_local,
        selected_remote));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.destroy());
    return (1);
}

FT_TEST(test_networking_nat_selects_highest_priority_with_stable_ties)
{
    networking_nat_traversal traversal;
    networking_nat_candidate local_first;
    networking_nat_candidate local_high_priority;
    networking_nat_candidate remote_first;
    networking_nat_candidate remote_equal_priority;
    networking_nat_candidate selected_local;
    networking_nat_candidate selected_remote;
    networking_nat_peer_ticket ticket;
    networking_nat_test_verifier verifier;

    networking_nat_prepare_ipv6_endpoint(local_first.endpoint, 1U, 3000U);
    local_first.type = networking_nat_candidate_type::HOST;
    local_first.priority = 200U;
    networking_nat_prepare_ipv6_endpoint(local_high_priority.endpoint, 2U,
        3001U);
    local_high_priority.type = networking_nat_candidate_type::HOST;
    local_high_priority.priority = 800U;
    networking_nat_prepare_ipv6_endpoint(remote_first.endpoint, 3U, 4000U);
    remote_first.type = networking_nat_candidate_type::SERVER_REFLEXIVE;
    remote_first.priority = 900U;
    networking_nat_prepare_ipv6_endpoint(remote_equal_priority.endpoint, 4U,
        4001U);
    remote_equal_priority.type = networking_nat_candidate_type::SERVER_REFLEXIVE;
    remote_equal_priority.priority = 900U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.peer_id = 2U;
    ticket.attempt_id = 19U;
    ticket.expires_at = 100U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.signature.push_back(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(remote_first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        ticket.candidates.push_back(remote_equal_priority));
    verifier.set_accept(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.add_local_candidate(local_first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.add_local_candidate(local_high_priority));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.set_peer_ticket(ticket, 0U,
        verifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.get_selected_pair(selected_local,
        selected_remote));
    FT_ASSERT_EQ(800U, selected_local.priority);
    FT_ASSERT_EQ(900U, selected_remote.priority);
    FT_ASSERT_EQ(0, ft_memcmp(&remote_first.endpoint.address,
        &selected_remote.endpoint.address,
        remote_first.endpoint.length));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.destroy());
    return (1);
}

FT_TEST(test_networking_nat_relay_fallback_after_direct_path_loss)
{
    networking_nat_traversal traversal;
    networking_nat_candidate local_candidate;
    networking_nat_candidate remote_direct_candidate;
    networking_nat_candidate remote_relay_candidate;
    networking_nat_peer_ticket ticket;
    networking_nat_test_verifier verifier;
    networking_nat_test_probe probe;
    networking_nat_test_relay relay;
    networking_message_endpoint local_endpoint;
    networking_message_endpoint remote_endpoint;

    networking_nat_prepare_ipv6_endpoint(local_endpoint, 1U, 3000U);
    networking_nat_prepare_ipv6_endpoint(remote_endpoint, 2U, 4000U);
    local_candidate.endpoint = local_endpoint;
    local_candidate.type = networking_nat_candidate_type::HOST;
    local_candidate.priority = 1000U;
    remote_direct_candidate.endpoint = remote_endpoint;
    remote_direct_candidate.type = networking_nat_candidate_type::HOST;
    remote_direct_candidate.priority = 1000U;
    remote_relay_candidate.endpoint = remote_endpoint;
    remote_relay_candidate.type = networking_nat_candidate_type::RELAY;
    remote_relay_candidate.priority = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.peer_id = 2U;
    ticket.attempt_id = 21U;
    ticket.expires_at = 1000U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.signature.push_back(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(
        remote_direct_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(
        remote_relay_candidate));
    verifier.set_accept(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.add_local_candidate(local_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.set_peer_ticket(ticket, 0U,
        verifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.begin(1U, probe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.mark_probe_success(local_endpoint,
        remote_endpoint, 21U));
    FT_ASSERT(traversal.has_direct_path());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.send_keepalive(probe));
    FT_ASSERT_EQ(2U, probe.calls);
    FT_ASSERT_EQ(FT_ERR_TIMEOUT, traversal.mark_direct_path_lost(0U));
    FT_ASSERT(traversal.has_direct_path());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.mark_direct_path_lost(20U));
    FT_ASSERT(!traversal.has_direct_path());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, traversal.get_selected_pair(
        local_candidate, remote_direct_candidate));
    FT_ASSERT(!traversal.needs_relay(69U));
    FT_ASSERT(traversal.needs_relay(70U));
    relay.open_result = FT_ERR_SOCKET_CONNECT_FAILED;
    FT_ASSERT_EQ(FT_ERR_SOCKET_CONNECT_FAILED,
        traversal.fallback_to_relay(70U, relay));
    FT_ASSERT(!traversal.using_relay());
    relay.open_result = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.fallback_to_relay(70U, relay));
    FT_ASSERT_EQ(2U, relay.opens);
    FT_ASSERT(traversal.using_relay());
    FT_ASSERT(!traversal.needs_relay(70U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.mark_relay_path_lost(75U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, traversal.get_selected_pair(
        local_candidate, remote_direct_candidate));
    FT_ASSERT(traversal.needs_relay(125U));
    relay.malformed_endpoint = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        traversal.fallback_to_relay(125U, relay));
    FT_ASSERT_EQ(1U, relay.closes);
    FT_ASSERT(!traversal.using_relay());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.destroy());
    return (1);
}

FT_TEST(test_networking_nat_upgrades_relay_to_recovered_direct_path)
{
    networking_nat_traversal traversal;
    networking_nat_candidate local_candidate;
    networking_nat_candidate remote_direct_candidate;
    networking_nat_candidate remote_relay_candidate;
    networking_nat_candidate selected_local;
    networking_nat_candidate selected_remote;
    networking_nat_peer_ticket ticket;
    networking_nat_test_verifier verifier;
    networking_nat_test_probe probe;
    networking_nat_test_relay relay;
    networking_message_endpoint local_endpoint;
    networking_message_endpoint remote_endpoint;

    networking_nat_prepare_ipv6_endpoint(local_endpoint, 1U, 3000U);
    networking_nat_prepare_ipv6_endpoint(remote_endpoint, 2U, 4000U);
    local_candidate.endpoint = local_endpoint;
    local_candidate.type = networking_nat_candidate_type::HOST;
    local_candidate.priority = 1000U;
    remote_direct_candidate.endpoint = remote_endpoint;
    remote_direct_candidate.type = networking_nat_candidate_type::HOST;
    remote_direct_candidate.priority = 1000U;
    remote_relay_candidate.endpoint = remote_endpoint;
    remote_relay_candidate.type = networking_nat_candidate_type::RELAY;
    remote_relay_candidate.priority = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.peer_id = 2U;
    ticket.attempt_id = 41U;
    ticket.expires_at = 1000U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.signature.push_back(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(
        remote_direct_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(
        remote_relay_candidate));
    verifier.set_accept(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.add_local_candidate(local_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.set_peer_ticket(ticket, 0U, verifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.begin(1U, probe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.mark_probe_success(local_endpoint,
        remote_endpoint, 41U));
    FT_ASSERT(traversal.has_direct_path());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.mark_direct_path_lost(20U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.fallback_to_relay(70U, relay));
    FT_ASSERT(traversal.using_relay());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.begin(71U, probe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.mark_probe_success(local_endpoint,
        remote_endpoint, 41U));
    FT_ASSERT(traversal.has_direct_path());
    FT_ASSERT(!traversal.using_relay());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.get_selected_pair(selected_local,
        selected_remote));
    FT_ASSERT_EQ(networking_nat_candidate_type::HOST, selected_local.type);
    FT_ASSERT_EQ(networking_nat_candidate_type::HOST, selected_remote.type);
    FT_ASSERT_EQ(1U, relay.opens);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.destroy());
    return (1);
}

FT_TEST(test_networking_nat_probe_failure_is_transactional)
{
    networking_nat_traversal traversal;
    networking_nat_candidate local_candidate;
    networking_nat_candidate remote_candidate;
    networking_nat_peer_ticket ticket;
    networking_nat_test_verifier verifier;
    networking_nat_test_probe probe;
    networking_message_endpoint local_endpoint;
    networking_message_endpoint remote_endpoint;

    networking_nat_prepare_ipv6_endpoint(local_endpoint, 1U, 3000U);
    networking_nat_prepare_ipv6_endpoint(remote_endpoint, 2U, 4000U);
    local_candidate.endpoint = local_endpoint;
    local_candidate.type = networking_nat_candidate_type::HOST;
    local_candidate.priority = 1000U;
    remote_candidate.endpoint = remote_endpoint;
    remote_candidate.type = networking_nat_candidate_type::HOST;
    remote_candidate.priority = 1000U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.peer_id = 2U;
    ticket.attempt_id = 31U;
    ticket.expires_at = 100U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.signature.push_back(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(remote_candidate));
    verifier.set_accept(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.add_local_candidate(local_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.set_peer_ticket(ticket, 0U, verifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_after(
        NETWORKING_TEST_NAT_PROBE, 0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, traversal.begin(1U, probe));
    FT_ASSERT_EQ(1U, networking_test_failure_attempt_count(
        NETWORKING_TEST_NAT_PROBE));
    FT_ASSERT_EQ(1U, networking_test_failure_count(
        NETWORKING_TEST_NAT_PROBE));
    FT_ASSERT_EQ(0U, probe.calls);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_test_failure_reset(NETWORKING_TEST_NAT_PROBE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.begin(1U, probe));
    FT_ASSERT_EQ(1U, probe.calls);
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, traversal.mark_probe_success(
        local_endpoint, remote_endpoint, 30U));
    FT_ASSERT(!traversal.has_direct_path());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.mark_probe_success(
        local_endpoint, remote_endpoint, ticket.attempt_id));
    FT_ASSERT(traversal.has_direct_path());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.destroy());
    return (1);
}

FT_TEST(test_networking_nat_relay_record_failure_does_not_select_relay)
{
    networking_nat_traversal traversal;
    networking_nat_candidate local_candidate;
    networking_nat_candidate remote_direct_candidate;
    networking_nat_candidate remote_relay_candidate;
    networking_nat_peer_ticket ticket;
    networking_nat_test_verifier verifier;
    networking_nat_test_probe probe;
    networking_nat_test_relay relay;
    networking_message_endpoint local_endpoint;
    networking_message_endpoint remote_endpoint;
    networking_nat_candidate selected_local;
    networking_nat_candidate selected_remote;

    networking_nat_prepare_ipv6_endpoint(local_endpoint, 1U, 3000U);
    networking_nat_prepare_ipv6_endpoint(remote_endpoint, 2U, 4000U);
    local_candidate.endpoint = local_endpoint;
    local_candidate.type = networking_nat_candidate_type::HOST;
    local_candidate.priority = 1000U;
    remote_direct_candidate.endpoint = remote_endpoint;
    remote_direct_candidate.type = networking_nat_candidate_type::HOST;
    remote_direct_candidate.priority = 1000U;
    remote_relay_candidate.endpoint = remote_endpoint;
    remote_relay_candidate.type = networking_nat_candidate_type::RELAY;
    remote_relay_candidate.priority = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.peer_id = 2U;
    ticket.attempt_id = 32U;
    ticket.expires_at = 1000U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.signature.push_back(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(
        remote_direct_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(
        remote_relay_candidate));
    verifier.set_accept(FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 50U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.add_local_candidate(local_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.set_peer_ticket(ticket, 0U, verifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.begin(1U, probe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.mark_probe_success(local_endpoint,
        remote_endpoint, 32U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.mark_direct_path_lost(20U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_after(
        NETWORKING_TEST_RELAY_RECORD, 0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, traversal.fallback_to_relay(70U, relay));
    FT_ASSERT(!traversal.using_relay());
    FT_ASSERT_EQ(0U, relay.opens);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, traversal.get_selected_pair(selected_local,
        selected_remote));
    FT_ASSERT_EQ(1U, networking_test_failure_attempt_count(
        NETWORKING_TEST_RELAY_RECORD));
    FT_ASSERT_EQ(1U, networking_test_failure_count(
        NETWORKING_TEST_RELAY_RECORD));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_test_failure_reset(NETWORKING_TEST_RELAY_RECORD));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.fallback_to_relay(70U, relay));
    FT_ASSERT(traversal.using_relay());
    FT_ASSERT_EQ(1U, relay.opens);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.destroy());
    return (1);
}
