#ifndef NETWORKING_REPLICATION_RETENTION_WINDOW_HPP
#define NETWORKING_REPLICATION_RETENTION_WINDOW_HPP

#include "../Errno/errno.hpp"
#include <cstdint>

/*
 * Caller-owned revision window for one server stream or client. It tracks
 * which contiguous revisions can still be replayed; payload bytes and their
 * ownership remain with the application.
 */
class networking_replication_retention_window
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        uint32_t _maximum_revisions;
        uint32_t _retained_count;
        uint64_t _oldest_revision;
        uint64_t _latest_revision;
        uint64_t _acknowledged_revision;

    public:
        networking_replication_retention_window() noexcept;
        networking_replication_retention_window(
            const networking_replication_retention_window &other) noexcept = delete;
        networking_replication_retention_window(
            networking_replication_retention_window &&other) noexcept = delete;
        ~networking_replication_retention_window() noexcept;

        networking_replication_retention_window &operator=(
            const networking_replication_retention_window &other) noexcept = delete;
        networking_replication_retention_window &operator=(
            networking_replication_retention_window &&other) noexcept = delete;

        int32_t initialize(uint32_t maximum_revisions) noexcept;
        int32_t destroy() noexcept;
        int32_t move(networking_replication_retention_window &other) noexcept;
        int32_t append_revision(uint64_t revision) noexcept;
        int32_t acknowledge(uint64_t revision) noexcept;
        ft_bool can_replay_from(uint64_t base_revision) const noexcept;
        ft_bool needs_snapshot(uint64_t base_revision) const noexcept;
        uint64_t get_oldest_revision() const noexcept;
        uint64_t get_latest_revision() const noexcept;
        uint64_t get_acknowledged_revision() const noexcept;
        uint32_t get_retained_count() const noexcept;
};

#endif
