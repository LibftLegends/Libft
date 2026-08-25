#include "../../Modules/Networking/networking_nat_traversal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <cstddef>
#include <cstdint>

class networking_nat_fuzz_probe : public networking_nat_probe_io
{
    public:
        networking_nat_fuzz_probe() noexcept
        {
            return ;
        }

        ~networking_nat_fuzz_probe() noexcept override
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
            return (FT_ERR_SUCCESS);
        }
};

class networking_nat_fuzz_provider : public networking_nat_candidate_provider
{
    private:
        const uint8_t *_data;
        ft_size_t _size;

    public:
        networking_nat_fuzz_provider(const uint8_t *data, ft_size_t size) noexcept
            : networking_nat_candidate_provider(), _data(data), _size(size)
        {
            return ;
        }

        ~networking_nat_fuzz_provider() noexcept override
        {
            return ;
        }

        int32_t gather(ft_vector<networking_nat_candidate> &candidates)
            noexcept override
        {
            networking_nat_candidate candidate;

            ft_memset(&candidate.endpoint, 0, sizeof(candidate.endpoint));
            candidate.endpoint.address.ss_family = AF_INET;
            candidate.endpoint.length = sizeof(struct sockaddr_in);
            candidate.type = networking_nat_candidate_type::HOST;
            candidate.priority = 1000U;
            candidate.component = 1U;
            if (this->_size > 0U)
                candidate.priority += static_cast<uint32_t>(this->_data[0U]);
            if (candidates.push_back(candidate) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            if (this->_size > 1U && (this->_data[1U] & 1U) != 0U)
            {
                candidate.type = networking_nat_candidate_type::SERVER_REFLEXIVE;
                candidate.priority = 900U;
                if (candidates.push_back(candidate) != FT_ERR_SUCCESS)
                    return (FT_ERR_NO_MEMORY);
            }
            return (FT_ERR_SUCCESS);
        }
};

class networking_nat_fuzz_verifier : public networking_nat_ticket_verifier
{
    private:
        ft_bool _accept;

    public:
        explicit networking_nat_fuzz_verifier(ft_bool accept) noexcept
            : networking_nat_ticket_verifier(), _accept(accept)
        {
            return ;
        }

        ~networking_nat_fuzz_verifier() noexcept override
        {
            return ;
        }

        ft_bool verify(const networking_nat_peer_ticket &ticket) noexcept override
        {
            (void)ticket;
            return (this->_accept);
        }

        void set_accept(ft_bool accept) noexcept
        {
            this->_accept = accept;
            return ;
        }
};

static void networking_nat_fuzz_run(const uint8_t *data, ft_size_t size) noexcept
{
    networking_nat_traversal traversal;
    networking_nat_peer_ticket ticket;
    networking_nat_fuzz_provider provider(data, size);
    ft_bool accept;
    networking_nat_fuzz_verifier verifier(FT_TRUE);
    networking_nat_fuzz_probe probe;
    networking_nat_candidate remote_candidate;
    networking_message_endpoint endpoint;
    uint32_t sent_probes;
    int32_t result;
    int32_t cleanup_result;

    accept = FT_TRUE;
    if (size > 0U && (data[0U] & 2U) != 0U)
        accept = FT_FALSE;
    verifier.set_accept(accept);
    ft_memset(&endpoint, 0, sizeof(endpoint));
    endpoint.address.ss_family = AF_INET;
    endpoint.length = sizeof(struct sockaddr_in);
    remote_candidate.endpoint = endpoint;
    remote_candidate.type = networking_nat_candidate_type::RELAY;
    remote_candidate.priority = 800U;
    remote_candidate.component = 1U;
    if (size > 2U)
        remote_candidate.priority += static_cast<uint32_t>(data[2U]);
    if (ticket.initialize() != FT_ERR_SUCCESS)
        return ;
    ticket.peer_id = 2U;
    ticket.attempt_id = 1U;
    ticket.expires_at = 100000U;
    if (ticket.signature.push_back(1U) != FT_ERR_SUCCESS
        || ticket.candidates.push_back(remote_candidate) != FT_ERR_SUCCESS)
    {
        result = ticket.destroy();
        if (result == FT_ERR_NO_MEMORY)
            return ;
        return ;
    }
    if (traversal.initialize(1U, 10U) == FT_ERR_SUCCESS)
    {
        result = traversal.gather_candidates(provider);
        result = traversal.set_peer_ticket(ticket, 0U, verifier);
        result = traversal.begin(1U, probe);
        sent_probes = 0U;
        result = traversal.probe_batch(21U, 1U, 8U, probe, sent_probes);
        result = traversal.mark_probe_success(endpoint, endpoint,
            ticket.attempt_id);
        if (traversal.needs_relay(1001U) == FT_TRUE && sent_probes == 0U)
            result = FT_ERR_INVALID_STATE;
        cleanup_result = traversal.destroy();
        if (result == FT_ERR_NO_MEMORY && cleanup_result == FT_ERR_NO_MEMORY)
            result = cleanup_result;
    }
    cleanup_result = ticket.destroy();
    if (result == FT_ERR_NO_MEMORY || cleanup_result == FT_ERR_NO_MEMORY)
        return ;
    return ;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ft_size_t bounded_size;

    if (data == ft_nullptr && size != 0U)
        return (0);
    bounded_size = static_cast<ft_size_t>(size);
    if (bounded_size > 256U)
        bounded_size = 256U;
    networking_nat_fuzz_run(data, bounded_size);
    return (0);
}
