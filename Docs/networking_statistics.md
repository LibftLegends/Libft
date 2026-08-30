# Libft networking statistics and health thresholds

`networking_message_statistics` is a bounded snapshot for one logical
connection. It is safe to sample directly and should be exported to aggregate
observability separately from the packet path.

## Field groups

| Group | Fields represented by the transport | Interpretation |
| --- | --- | --- |
| Delivery | sent/received/acknowledged/lost/retransmitted packets and message/byte counters | Reliability and application throughput. |
| Timing | latest, minimum, smoothed RTT, RTT variance, jitter, and progress timestamps | Path quality and timer health. |
| Congestion | congestion window, bytes in flight, pacing rate, queue depth | Sender pressure and available service rate. |
| Reassembly | active bytes/messages, completed fragments, expiry/drop counts | Receiver memory pressure and fragmentation behavior. |
| Security | authentication failures, replay rejections, malformed packets | Attack or interoperability signals; never include secret data. |
| Path | migrations, NAT attempts, relay fallback, active direct/relay path | Connectivity quality and migration behavior. |
| Lanes | per-lane messages/bytes, queued bytes, configured weight/reservation, current send window/rate | Scheduler fairness, lane pressure, and reserved-bandwidth service. |

Counters use fixed-width saturating arithmetic where overflow is possible.
Snapshots are samples, not wire-protocol state, and must not be used to infer a
peer identity or authorization decision.

## Suggested health classification

Applications may tune thresholds for their game, but the initial dashboard
classification can use these bounded defaults:

- `HEALTHY`: smoothed RTT below 100 ms, loss below 2%, queue depth below 50%,
  no authentication/replay failures, and progress within 3 idle intervals;
- `DEGRADED`: RTT 100–250 ms, loss 2–10%, queue depth 50–80%, or a relay path
  selected while traffic still progresses;
- `UNSTABLE`: RTT above 250 ms, loss above 10%, repeated probe timeouts, queue
  depth above 80%, or a path migration in progress;
- `FAILED`: terminal state, expired handshake/idle deadline, resource limit
  close, or no progress after the configured retry budget.

Threshold transitions should be hysteretic so one delayed packet does not
flap the user-facing status. Peer-controlled strings must never become metric
labels. Export rates and percentiles from an aggregate sampler rather than
calling expensive instrumentation while holding transport or connection locks.
