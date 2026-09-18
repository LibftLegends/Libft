#ifndef NETWORKING_REPLICATION_REVISION_TRACKER_HPP
#define NETWORKING_REPLICATION_REVISION_TRACKER_HPP

#include "../Errno/errno.hpp"
#include <cstdint>

/*
 * Caller-owned gate for one authoritative replication stream. The consuming
 * application owns the blocks and lights; this class only validates revision
 * dependencies before the application commits decoded data.
 */
class networking_replication_revision_tracker
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        uint64_t _block_revision;
        uint64_t _light_revision;
        uint64_t _snapshot_generation;
        ft_bool _snapshot_ready;

    public:
        networking_replication_revision_tracker() noexcept;
        networking_replication_revision_tracker(
            const networking_replication_revision_tracker &other) noexcept = delete;
        networking_replication_revision_tracker(
            networking_replication_revision_tracker &&other) noexcept = delete;
        ~networking_replication_revision_tracker() noexcept;

        networking_replication_revision_tracker &operator=(
            const networking_replication_revision_tracker &other) noexcept = delete;
        networking_replication_revision_tracker &operator=(
            networking_replication_revision_tracker &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(networking_replication_revision_tracker &other) noexcept;
        int32_t accept_snapshot(uint64_t block_revision,
            uint64_t light_revision, uint64_t snapshot_generation) noexcept;
        ft_bool can_accept_snapshot(uint64_t block_revision,
            uint64_t light_revision, uint64_t snapshot_generation) const noexcept;
        int32_t accept_block_delta(uint64_t base_revision,
            uint64_t final_revision) noexcept;
        ft_bool can_accept_block_delta(uint64_t base_revision,
            uint64_t final_revision) const noexcept;
        int32_t accept_light_delta(uint64_t base_revision,
            uint64_t final_revision, uint64_t source_block_revision) noexcept;
        ft_bool can_accept_light_delta(uint64_t base_revision,
            uint64_t final_revision, uint64_t source_block_revision) const noexcept;
        uint64_t get_block_revision() const noexcept;
        uint64_t get_light_revision() const noexcept;
        uint64_t get_snapshot_generation() const noexcept;
        ft_bool is_snapshot_ready() const noexcept;
};

#endif
