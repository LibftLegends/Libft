#include "networking_nat_traversal.hpp"
#ifdef LIBFT_TEST_BUILD
# include "../../Test/Test/networking_test_hooks.hpp"
#else
# define NETWORKING_TEST_SHOULD_FAIL(point) FT_FALSE
#endif
networking_nat_candidate::networking_nat_candidate() noexcept
    : endpoint(), type(networking_nat_candidate_type::HOST), priority(0U),
      component(1U), foundation(0U), measured_rtt_milliseconds(0U)
{
    ft_memset(&this->endpoint, 0, sizeof(this->endpoint));
    return ;
}

networking_nat_candidate::~networking_nat_candidate() noexcept
{
    return ;
}

networking_nat_peer_ticket::networking_nat_peer_ticket() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), peer_id(0U),
      attempt_id(0U), expires_at(0U), tie_breaker(0U), candidates(), signature()
{
    return ;
}

networking_nat_peer_ticket::~networking_nat_peer_ticket() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_nat_peer_ticket::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (this->candidates.initialize() != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INITIALIZATION_FAILED);
    }
    if (this->signature.initialize() != FT_ERR_SUCCESS)
    {
        (void)this->candidates.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INITIALIZATION_FAILED);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_peer_ticket::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    (void)this->candidates.destroy();
    (void)this->signature.destroy();
    this->peer_id = 0U;
    this->attempt_id = 0U;
    this->expires_at = 0U;
    this->tie_breaker = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_peer_ticket::move(networking_nat_peer_ticket &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    if (this->candidates.initialize(other.candidates) != FT_ERR_SUCCESS)
    {
        (void)this->candidates.destroy();
        (void)this->signature.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    if (this->signature.initialize(other.signature) != FT_ERR_SUCCESS)
    {
        (void)this->candidates.destroy();
        (void)this->signature.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    this->peer_id = other.peer_id;
    this->attempt_id = other.attempt_id;
    this->expires_at = other.expires_at;
    this->tie_breaker = other.tie_breaker;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool networking_nat_peer_ticket::is_initialised() const noexcept
{
    return (this->_initialised_state == FT_CLASS_STATE_INITIALISED);
}

networking_nat_probe_io::networking_nat_probe_io() noexcept
{
    return ;
}

networking_nat_probe_io::~networking_nat_probe_io() noexcept
{
    return ;
}

networking_nat_candidate_provider::networking_nat_candidate_provider() noexcept
{
    return ;
}

networking_nat_candidate_provider::~networking_nat_candidate_provider() noexcept
{
    return ;
}

networking_nat_relay_io::networking_nat_relay_io() noexcept
{
    return ;
}

networking_nat_relay_io::~networking_nat_relay_io() noexcept
{
    return ;
}

int32_t networking_nat_relay_io::receive_relay(
    const networking_message_endpoint &relay_endpoint, uint8_t *data,
    ft_size_t capacity, ft_size_t *received_size) noexcept
{
    (void)relay_endpoint;
    (void)data;
    (void)capacity;
    (void)received_size;
    return (FT_ERR_UNSUPPORTED_TYPE);
}

uint64_t networking_nat_relay_io::now_milliseconds() const noexcept
{
    return (0U);
}

networking_nat_relay_datagram_io::networking_nat_relay_datagram_io() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _relay_io(ft_nullptr), _relay_endpoint()
{
    ft_memset(&this->_relay_endpoint, 0, sizeof(this->_relay_endpoint));
    return ;
}

networking_nat_relay_datagram_io::~networking_nat_relay_datagram_io() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_nat_relay_datagram_io::initialize(
    networking_nat_relay_io &relay_io,
    const networking_message_endpoint &relay_endpoint) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (relay_endpoint.length == 0U
        || relay_endpoint.length > static_cast<socklen_t>(
            sizeof(relay_endpoint.address)))
        return (FT_ERR_INVALID_ARGUMENT);
    this->_relay_io = &relay_io;
    this->_relay_endpoint = relay_endpoint;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_relay_datagram_io::destroy() noexcept
{
    int32_t result;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    result = FT_ERR_SUCCESS;
    if (this->_relay_io != ft_nullptr)
        result = this->_relay_io->close_relay(this->_relay_endpoint);
    this->_relay_io = ft_nullptr;
    ft_memset(&this->_relay_endpoint, 0, sizeof(this->_relay_endpoint));
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (result);
}

int32_t networking_nat_relay_datagram_io::move(
    networking_nat_relay_datagram_io &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    this->_relay_io = other._relay_io;
    this->_relay_endpoint = other._relay_endpoint;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._relay_io = ft_nullptr;
    ft_memset(&other._relay_endpoint, 0, sizeof(other._relay_endpoint));
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_relay_datagram_io::send_datagram(
    const networking_message_endpoint &destination, const uint8_t *data,
    ft_size_t size) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_relay_io == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    if (destination.length == 0U
        || destination.length > static_cast<socklen_t>(
            sizeof(destination.address))
        || destination.length != this->_relay_endpoint.length
        || ft_memcmp(&destination.address, &this->_relay_endpoint.address,
            destination.length) != 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (data == ft_nullptr && size != 0U)
        return (FT_ERR_INVALID_POINTER);
    return (this->_relay_io->send_relay(this->_relay_endpoint, data, size));
}

int32_t networking_nat_relay_datagram_io::receive_datagram(
    networking_message_endpoint &source, uint8_t *data, ft_size_t capacity,
    ft_size_t *received_size) noexcept
{
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_relay_io == ft_nullptr)
        return (FT_ERR_NOT_INITIALISED);
    if (data == ft_nullptr || received_size == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    result = this->_relay_io->receive_relay(this->_relay_endpoint, data,
        capacity, received_size);
    if (result == FT_ERR_SUCCESS)
        source = this->_relay_endpoint;
    return (result);
}

uint64_t networking_nat_relay_datagram_io::now_milliseconds() const noexcept
{
    if (this->_relay_io == ft_nullptr)
        return (0U);
    return (this->_relay_io->now_milliseconds());
}

networking_nat_ticket_verifier::networking_nat_ticket_verifier() noexcept
{
    return ;
}

networking_nat_ticket_verifier::~networking_nat_ticket_verifier() noexcept
{
    return ;
}

networking_nat_traversal::networking_nat_traversal() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _local_peer_id(0U),
      _remote_peer_id(0U), _attempt_id(0U),
      _started_at(0U), _deadline_milliseconds(0U), _direct_path_valid(FT_FALSE),
      _relay_available(FT_FALSE), _using_relay(FT_FALSE),
      _probing(FT_FALSE),
      _containers_initialised(FT_FALSE), _next_local_probe(0U),
      _next_remote_probe(0U), _probe_count(0U), _last_probe_at(0U),
      _local_candidates(), _remote_candidates(),
      _selected_local(), _selected_remote()
{
    return ;
}

networking_nat_traversal::~networking_nat_traversal() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t networking_nat_traversal::initialize(uint64_t local_peer_id,
    uint32_t deadline_milliseconds) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    if (local_peer_id == 0U || deadline_milliseconds == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_local_peer_id = local_peer_id;
    this->_remote_peer_id = 0U;
    this->_attempt_id = 0U;
    this->_started_at = 0U;
    this->_deadline_milliseconds = deadline_milliseconds;
    this->_direct_path_valid = FT_FALSE;
    this->_relay_available = FT_FALSE;
    this->_using_relay = FT_FALSE;
    this->_probing = FT_FALSE;
    this->_next_local_probe = 0U;
    this->_next_remote_probe = 0U;
    this->_probe_count = 0U;
    this->_last_probe_at = 0U;
    if (this->_containers_initialised == FT_FALSE)
    {
        if (this->_local_candidates.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_INITIALIZATION_FAILED);
        if (this->_remote_candidates.initialize() != FT_ERR_SUCCESS)
        {
            (void)this->_local_candidates.destroy();
            return (FT_ERR_INITIALIZATION_FAILED);
        }
        this->_containers_initialised = FT_TRUE;
    }
    else
    {
        this->_local_candidates.clear();
        this->_remote_candidates.clear();
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_traversal::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    if (this->_containers_initialised != FT_FALSE)
    {
        this->_local_candidates.clear();
        this->_remote_candidates.clear();
        (void)this->_local_candidates.destroy();
        (void)this->_remote_candidates.destroy();
        this->_containers_initialised = FT_FALSE;
    }
    this->_direct_path_valid = FT_FALSE;
    this->_remote_peer_id = 0U;
    this->_relay_available = FT_FALSE;
    this->_using_relay = FT_FALSE;
    this->_probing = FT_FALSE;
    this->_next_local_probe = 0U;
    this->_next_remote_probe = 0U;
    this->_probe_count = 0U;
    this->_last_probe_at = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_traversal::move(networking_nat_traversal &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    if (this->_local_candidates.initialize(other._local_candidates)
        != FT_ERR_SUCCESS)
    {
        this->_containers_initialised = FT_FALSE;
        return (FT_ERR_NO_MEMORY);
    }
    if (this->_remote_candidates.initialize(other._remote_candidates)
        != FT_ERR_SUCCESS)
    {
        (void)this->_local_candidates.destroy();
        this->_containers_initialised = FT_FALSE;
        return (FT_ERR_NO_MEMORY);
    }
    this->_containers_initialised = FT_TRUE;
    this->_local_peer_id = other._local_peer_id;
    this->_remote_peer_id = other._remote_peer_id;
    this->_attempt_id = other._attempt_id;
    this->_started_at = other._started_at;
    this->_deadline_milliseconds = other._deadline_milliseconds;
    this->_direct_path_valid = other._direct_path_valid;
    this->_relay_available = other._relay_available;
    this->_using_relay = other._using_relay;
    this->_probing = other._probing;
    this->_next_local_probe = other._next_local_probe;
    this->_next_remote_probe = other._next_remote_probe;
    this->_probe_count = other._probe_count;
    this->_last_probe_at = other._last_probe_at;
    this->_selected_local = other._selected_local;
    this->_selected_remote = other._selected_remote;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._containers_initialised = FT_FALSE;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_traversal::add_local_candidate(
    const networking_nat_candidate &candidate) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (candidate.endpoint.length == 0U || candidate.component == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_NAT_CANDIDATE)
        != FT_FALSE)
        return (FT_ERR_NO_MEMORY);
    if (this->_local_candidates.push_back(candidate) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_traversal::gather_candidates(
    networking_nat_candidate_provider &provider) noexcept
{
    ft_size_t index;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    this->_local_candidates.clear();
    result = provider.gather(this->_local_candidates);
    if (result != FT_ERR_SUCCESS)
        return (result);
    index = 0U;
    while (index < this->_local_candidates.size())
    {
        if (this->_local_candidates[index].endpoint.length == 0U
            || this->_local_candidates[index].endpoint.length
                > static_cast<socklen_t>(sizeof(
                    this->_local_candidates[index].endpoint.address))
            || this->_local_candidates[index].component == 0U)
        {
            this->_local_candidates.clear();
            return (FT_ERR_INVALID_ARGUMENT);
        }
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_traversal::candidate_score(
    const networking_nat_candidate &local, const networking_nat_candidate &remote) noexcept
{
    int32_t score;

    score = static_cast<int32_t>(local.priority / 100U + remote.priority / 100U);
    if (local.type == networking_nat_candidate_type::RELAY
        || remote.type == networking_nat_candidate_type::RELAY)
        score -= 1000;
    if (local.type == networking_nat_candidate_type::HOST
        && remote.type == networking_nat_candidate_type::HOST)
        score += 100;
    if (local.endpoint.address.ss_family == AF_INET6
        && remote.endpoint.address.ss_family == AF_INET6)
        score += 50;
    else if (local.endpoint.address.ss_family == AF_INET
        && remote.endpoint.address.ss_family == AF_INET)
        score += 20;
    if (local.measured_rtt_milliseconds != 0U
        && remote.measured_rtt_milliseconds != 0U)
    {
        uint32_t combined_rtt = local.measured_rtt_milliseconds
            + remote.measured_rtt_milliseconds;
        if (combined_rtt < local.measured_rtt_milliseconds)
            combined_rtt = UINT32_MAX;
        if (combined_rtt < 1000U)
            score += static_cast<int32_t>(1000U - combined_rtt);
    }
    return (score);
}

int32_t networking_nat_traversal::select_pair() noexcept
{
    int32_t best_score;
    ft_bool found;
    ft_size_t local_index;
    ft_size_t remote_index;

    found = FT_FALSE;
    best_score = -2147483647;
    local_index = 0U;
    while (local_index < this->_local_candidates.size())
    {
        remote_index = 0U;
        while (remote_index < this->_remote_candidates.size())
        {
            int32_t score = networking_nat_traversal::candidate_score(
                this->_local_candidates[local_index], this->_remote_candidates[remote_index]);
            if (found == FT_FALSE || score > best_score)
            {
                best_score = score;
                this->_selected_local = this->_local_candidates[local_index];
                this->_selected_remote = this->_remote_candidates[remote_index];
                found = FT_TRUE;
            }
            remote_index += 1U;
        }
        local_index += 1U;
    }
    if (found == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_traversal::set_peer_ticket(const networking_nat_peer_ticket &ticket,
    uint64_t now_milliseconds) noexcept
{
    (void)ticket;
    (void)now_milliseconds;
    return (FT_ERR_UNSUPPORTED_TYPE);
}

int32_t networking_nat_traversal::set_peer_ticket(const networking_nat_peer_ticket &ticket,
    uint64_t now_milliseconds, networking_nat_ticket_verifier &verifier) noexcept
{
    ft_vector<networking_nat_candidate> new_remote_candidates;
    ft_bool new_relay_available;
    ft_size_t index;
    int32_t result;

    if (verifier.verify(ticket) == FT_FALSE)
        return (FT_ERR_PERMISSION_DENIED);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (ticket.is_initialised() == FT_FALSE
        || ticket.peer_id == 0U || ticket.attempt_id == 0U
        || ticket.expires_at <= now_milliseconds || ticket.signature.empty())
        return (FT_ERR_PERMISSION_DENIED);
    result = new_remote_candidates.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    new_relay_available = FT_FALSE;
    index = 0U;
    while (index < ticket.candidates.size())
    {
        if (ticket.candidates[index].endpoint.length == 0U
            || ticket.candidates[index].endpoint.length
                > static_cast<socklen_t>(sizeof(
                    ticket.candidates[index].endpoint.address))
            || ticket.candidates[index].component == 0U)
        {
            (void)new_remote_candidates.destroy();
            return (FT_ERR_INVALID_ARGUMENT);
        }
        if (new_remote_candidates.push_back(ticket.candidates[index])
            != FT_ERR_SUCCESS)
        {
            (void)new_remote_candidates.destroy();
            return (FT_ERR_NO_MEMORY);
        }
        if (ticket.candidates[index].type == networking_nat_candidate_type::RELAY)
            new_relay_available = FT_TRUE;
        index += 1U;
    }
    result = this->_remote_candidates.move(new_remote_candidates);
    if (result != FT_ERR_SUCCESS)
    {
        (void)new_remote_candidates.destroy();
        return (result);
    }
    this->_attempt_id = ticket.attempt_id;
    this->_remote_peer_id = ticket.peer_id;
    this->_relay_available = new_relay_available;
    this->_using_relay = FT_FALSE;
    return (this->select_pair());
}

int32_t networking_nat_traversal::begin(uint64_t now_milliseconds,
    networking_nat_probe_io &probe_io) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_attempt_id == 0U || this->select_pair() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_NAT_PROBE)
        != FT_FALSE)
        return (FT_ERR_NO_MEMORY);
    this->_started_at = now_milliseconds;
    this->_probing = FT_TRUE;
    this->_next_local_probe = 0U;
    this->_next_remote_probe = 0U;
    this->_probe_count = 0U;
    return (this->probe_next(now_milliseconds, 0U, probe_io));
}

int32_t networking_nat_traversal::probe_next(uint64_t now_milliseconds,
    uint32_t minimum_interval_milliseconds,
    networking_nat_probe_io &probe_io) noexcept
{
    uint32_t sent_probes;
    int32_t result;

    sent_probes = 0U;
    result = this->probe_batch(now_milliseconds,
        minimum_interval_milliseconds, 1U, probe_io, sent_probes);
    return (result);
}

int32_t networking_nat_traversal::probe_batch(uint64_t now_milliseconds,
    uint32_t minimum_interval_milliseconds, uint32_t maximum_probes,
    networking_nat_probe_io &probe_io, uint32_t &sent_probes) noexcept
{
    ft_size_t local_index;
    ft_size_t remote_index;
    int32_t result;

    sent_probes = 0U;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_attempt_id == 0U || this->_probing == FT_FALSE
        || maximum_probes == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_probe_count != 0U
        && now_milliseconds < this->_last_probe_at)
        return (FT_ERR_TIMEOUT);
    if (this->_probe_count != 0U
        && now_milliseconds - this->_last_probe_at
            < minimum_interval_milliseconds)
        return (FT_ERR_TIMEOUT);
    while (sent_probes < maximum_probes)
    {
        if (this->_next_local_probe >= this->_local_candidates.size())
        {
            this->_next_local_probe = 0U;
            this->_next_remote_probe += 1U;
        }
        if (this->_next_remote_probe >= this->_remote_candidates.size())
            break ;
        local_index = this->_next_local_probe;
        remote_index = this->_next_remote_probe;
        result = probe_io.send_probe(
            this->_local_candidates[local_index].endpoint,
            this->_remote_candidates[remote_index].endpoint,
            this->_attempt_id);
        if (result != FT_ERR_SUCCESS)
            return (FT_ERR_SOCKET_SEND_FAILED);
        this->_next_local_probe += 1U;
        this->_probe_count += 1U;
        this->_last_probe_at = now_milliseconds;
        sent_probes += 1U;
    }
    if (sent_probes == 0U)
        return (FT_ERR_NOT_FOUND);
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_traversal::fallback_to_relay(
    uint64_t now_milliseconds, networking_nat_relay_io &relay_io) noexcept
{
    networking_message_endpoint relay_endpoint;
    int32_t result;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_direct_path_valid != FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    if (this->needs_relay(now_milliseconds) == FT_FALSE
        || this->_remote_peer_id == 0U)
        return (FT_ERR_NOT_FOUND);
    if (NETWORKING_TEST_SHOULD_FAIL(NETWORKING_TEST_RELAY_RECORD)
        != FT_FALSE)
        return (FT_ERR_NO_MEMORY);
    ft_memset(&relay_endpoint, 0, sizeof(relay_endpoint));
    result = relay_io.open_relay(this->_local_peer_id, this->_remote_peer_id,
        this->_attempt_id, relay_endpoint);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (relay_endpoint.length == 0U
        || relay_endpoint.length > static_cast<socklen_t>(
            sizeof(relay_endpoint.address)))
    {
        (void)relay_io.close_relay(relay_endpoint);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_selected_remote.endpoint = relay_endpoint;
    this->_selected_remote.type = networking_nat_candidate_type::RELAY;
    this->_using_relay = FT_TRUE;
    this->_relay_available = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_traversal::mark_probe_success(
    const networking_message_endpoint &local,
    const networking_message_endpoint &remote, uint64_t attempt_id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (local.length == 0U || remote.length == 0U || attempt_id == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (attempt_id != this->_attempt_id)
        return (FT_ERR_PERMISSION_DENIED);
    ft_size_t local_index = 0U;
    ft_bool found = FT_FALSE;
    while (local_index < this->_local_candidates.size())
    {
        ft_size_t remote_index = 0U;
        while (remote_index < this->_remote_candidates.size())
        {
            if (this->_local_candidates[local_index].endpoint.length == local.length
                && this->_remote_candidates[remote_index].endpoint.length == remote.length
                && ft_memcmp(&this->_local_candidates[local_index].endpoint.address,
                    &local.address, local.length) == 0
                && ft_memcmp(&this->_remote_candidates[remote_index].endpoint.address,
                    &remote.address, remote.length) == 0)
            {
                this->_selected_local = this->_local_candidates[local_index];
                this->_selected_remote = this->_remote_candidates[remote_index];
                found = FT_TRUE;
            }
            remote_index += 1U;
        }
        local_index += 1U;
    }
    if (found == FT_FALSE)
        return (FT_ERR_NOT_FOUND);
    this->_direct_path_valid = FT_TRUE;
    this->_probing = FT_FALSE;
    this->_using_relay = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t networking_nat_traversal::get_selected_pair(networking_nat_candidate &local,
    networking_nat_candidate &remote) const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this->_selected_local.endpoint.length == 0U
        || this->_selected_remote.endpoint.length == 0U)
        return (FT_ERR_NOT_FOUND);
    local = this->_selected_local;
    remote = this->_selected_remote;
    return (FT_ERR_SUCCESS);
}

ft_bool networking_nat_traversal::needs_relay(uint64_t now_milliseconds) const noexcept
{
    if (this->_direct_path_valid != FT_FALSE
        || this->_using_relay != FT_FALSE)
        return (FT_FALSE);
    if (this->_probing == FT_FALSE || now_milliseconds < this->_started_at
        || now_milliseconds - this->_started_at < this->_deadline_milliseconds)
        return (FT_FALSE);
    return (this->_relay_available);
}

ft_bool networking_nat_traversal::has_direct_path() const noexcept
{
    return (this->_direct_path_valid);
}

ft_bool networking_nat_traversal::using_relay() const noexcept
{
    return (this->_using_relay);
}
