#ifndef NETWORKING_NAT_TRAVERSAL_HPP
#define NETWORKING_NAT_TRAVERSAL_HPP

#include "message_transport.hpp"
#include "../Template/vector.hpp"
#include <cstdint>

enum class networking_nat_candidate_type : uint8_t
{
    HOST = 0U,
    SERVER_REFLEXIVE = 1U,
    RELAY = 2U
};

struct networking_nat_candidate
{
    networking_message_endpoint endpoint;
    networking_nat_candidate_type type;
    uint32_t priority;
    uint32_t component;
    uint64_t foundation;
    uint32_t measured_rtt_milliseconds;

    networking_nat_candidate() noexcept;
    ~networking_nat_candidate() noexcept;
};

struct networking_nat_peer_ticket
{
private:
    uint8_t _initialised_state;

public:
    uint64_t peer_id;
    uint64_t attempt_id;
    uint64_t expires_at;
    uint64_t tie_breaker;
    ft_vector<networking_nat_candidate> candidates;
    ft_vector<uint8_t> signature;

    networking_nat_peer_ticket() noexcept;
    networking_nat_peer_ticket(const networking_nat_peer_ticket &other) noexcept = delete;
    networking_nat_peer_ticket(networking_nat_peer_ticket &&other) noexcept = delete;
    ~networking_nat_peer_ticket() noexcept;

    networking_nat_peer_ticket &operator=(const networking_nat_peer_ticket &other) noexcept = delete;
    networking_nat_peer_ticket &operator=(networking_nat_peer_ticket &&other) noexcept = delete;

    int32_t initialize() noexcept;
    int32_t destroy() noexcept;
    int32_t move(networking_nat_peer_ticket &other) noexcept;
    ft_bool is_initialised() const noexcept;
};

class networking_nat_probe_io
{
    public:
        networking_nat_probe_io() noexcept;
        networking_nat_probe_io(const networking_nat_probe_io &other) noexcept = delete;
        networking_nat_probe_io(networking_nat_probe_io &&other) noexcept = delete;
        virtual ~networking_nat_probe_io() noexcept;

        networking_nat_probe_io &operator=(const networking_nat_probe_io &other) noexcept = delete;
        networking_nat_probe_io &operator=(networking_nat_probe_io &&other) noexcept = delete;

        virtual int32_t send_probe(const networking_message_endpoint &local,
            const networking_message_endpoint &remote, uint64_t attempt_id) noexcept = 0;
};

class networking_nat_candidate_provider
{
    public:
        networking_nat_candidate_provider() noexcept;
        networking_nat_candidate_provider(
            const networking_nat_candidate_provider &other) noexcept = delete;
        networking_nat_candidate_provider(networking_nat_candidate_provider &&other)
            noexcept = delete;
        virtual ~networking_nat_candidate_provider() noexcept;

        networking_nat_candidate_provider &operator=(
            const networking_nat_candidate_provider &other) noexcept = delete;
        networking_nat_candidate_provider &operator=(
            networking_nat_candidate_provider &&other) noexcept = delete;

        virtual int32_t gather(
            ft_vector<networking_nat_candidate> &candidates) noexcept = 0;
};

class networking_nat_relay_io
{
    public:
        networking_nat_relay_io() noexcept;
        networking_nat_relay_io(const networking_nat_relay_io &other) noexcept = delete;
        networking_nat_relay_io(networking_nat_relay_io &&other) noexcept = delete;
        virtual ~networking_nat_relay_io() noexcept;

        networking_nat_relay_io &operator=(
            const networking_nat_relay_io &other) noexcept = delete;
        networking_nat_relay_io &operator=(
            networking_nat_relay_io &&other) noexcept = delete;

        virtual int32_t open_relay(uint64_t local_peer_id,
            uint64_t remote_peer_id, uint64_t attempt_id,
            networking_message_endpoint &relay_endpoint) noexcept = 0;
        virtual int32_t send_relay(const networking_message_endpoint &relay_endpoint,
            const uint8_t *data, ft_size_t size) noexcept = 0;
        virtual int32_t receive_relay(
            const networking_message_endpoint &relay_endpoint, uint8_t *data,
            ft_size_t capacity, ft_size_t *received_size) noexcept;
        virtual uint64_t now_milliseconds() const noexcept;
        virtual int32_t close_relay(
            const networking_message_endpoint &relay_endpoint) noexcept = 0;
};

class networking_nat_relay_datagram_io : public networking_datagram_io
{
    private:
        uint8_t _initialised_state;
        networking_nat_relay_io *_relay_io;
        networking_message_endpoint _relay_endpoint;

    public:
        networking_nat_relay_datagram_io() noexcept;
        networking_nat_relay_datagram_io(
            const networking_nat_relay_datagram_io &other) noexcept = delete;
        networking_nat_relay_datagram_io(
            networking_nat_relay_datagram_io &&other) noexcept = delete;
        ~networking_nat_relay_datagram_io() noexcept;

        networking_nat_relay_datagram_io &operator=(
            const networking_nat_relay_datagram_io &other) noexcept = delete;
        networking_nat_relay_datagram_io &operator=(
            networking_nat_relay_datagram_io &&other) noexcept = delete;

        int32_t initialize(networking_nat_relay_io &relay_io,
            const networking_message_endpoint &relay_endpoint) noexcept;
        int32_t destroy() noexcept;
        int32_t move(networking_nat_relay_datagram_io &other) noexcept;
        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override;
        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity,
            ft_size_t *received_size) noexcept override;
        uint64_t now_milliseconds() const noexcept override;
};

class networking_nat_ticket_verifier
{
    public:
        networking_nat_ticket_verifier() noexcept;
        networking_nat_ticket_verifier(const networking_nat_ticket_verifier &other) noexcept = delete;
        networking_nat_ticket_verifier(networking_nat_ticket_verifier &&other) noexcept = delete;
        virtual ~networking_nat_ticket_verifier() noexcept;

        networking_nat_ticket_verifier &operator=(const networking_nat_ticket_verifier &other) noexcept = delete;
        networking_nat_ticket_verifier &operator=(networking_nat_ticket_verifier &&other) noexcept = delete;

        virtual ft_bool verify(const networking_nat_peer_ticket &ticket) noexcept = 0;
};

class networking_nat_traversal
{
    private:
        uint8_t _initialised_state;
        uint64_t _local_peer_id;
        uint64_t _remote_peer_id;
        uint64_t _attempt_id;
        uint64_t _started_at;
        uint32_t _deadline_milliseconds;
        ft_bool _direct_path_valid;
        ft_bool _relay_available;
        ft_bool _using_relay;
        ft_bool _probing;
        ft_bool _containers_initialised;
        ft_size_t _next_local_probe;
        ft_size_t _next_remote_probe;
        uint64_t _probe_count;
        uint64_t _last_probe_at;
        ft_vector<networking_nat_candidate> _local_candidates;
        ft_vector<networking_nat_candidate> _remote_candidates;
        networking_nat_candidate _selected_local;
        networking_nat_candidate _selected_remote;

        int32_t select_pair() noexcept;
        static int32_t candidate_score(const networking_nat_candidate &local,
            const networking_nat_candidate &remote) noexcept;

    public:
        networking_nat_traversal() noexcept;
        networking_nat_traversal(const networking_nat_traversal &other) noexcept = delete;
        networking_nat_traversal(networking_nat_traversal &&other) noexcept = delete;
        ~networking_nat_traversal() noexcept;

        networking_nat_traversal &operator=(const networking_nat_traversal &other) noexcept = delete;
        networking_nat_traversal &operator=(networking_nat_traversal &&other) noexcept = delete;

        int32_t initialize(uint64_t local_peer_id, uint32_t deadline_milliseconds) noexcept;
        int32_t destroy() noexcept;
        int32_t move(networking_nat_traversal &other) noexcept;
        int32_t add_local_candidate(const networking_nat_candidate &candidate) noexcept;
        int32_t gather_candidates(networking_nat_candidate_provider &provider) noexcept;
        int32_t set_peer_ticket(const networking_nat_peer_ticket &ticket,
            uint64_t now_milliseconds) noexcept;
        int32_t set_peer_ticket(const networking_nat_peer_ticket &ticket,
            uint64_t now_milliseconds, networking_nat_ticket_verifier &verifier) noexcept;
        int32_t begin(uint64_t now_milliseconds, networking_nat_probe_io &probe_io) noexcept;
        int32_t probe_next(uint64_t now_milliseconds,
            uint32_t minimum_interval_milliseconds,
            networking_nat_probe_io &probe_io) noexcept;
        int32_t probe_batch(uint64_t now_milliseconds,
            uint32_t minimum_interval_milliseconds,
            uint32_t maximum_probes, networking_nat_probe_io &probe_io,
            uint32_t &sent_probes) noexcept;
        int32_t fallback_to_relay(uint64_t now_milliseconds,
            networking_nat_relay_io &relay_io) noexcept;
        int32_t mark_probe_success(const networking_message_endpoint &local,
            const networking_message_endpoint &remote,
            uint64_t attempt_id) noexcept;
        int32_t get_selected_pair(networking_nat_candidate &local,
            networking_nat_candidate &remote) const noexcept;
        ft_bool needs_relay(uint64_t now_milliseconds) const noexcept;
        ft_bool has_direct_path() const noexcept;
        ft_bool using_relay() const noexcept;
};

#endif
