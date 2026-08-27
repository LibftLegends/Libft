#ifndef GAME_WORLD_DELTA_HPP
# define GAME_WORLD_DELTA_HPP

#include "../Buffer/byte_buffer.hpp"
#include "../Template/vector.hpp"
#include <stdint.h>

#define GAME_WORLD_DELTA_PROTOCOL_VERSION 1U
#define GAME_WORLD_DELTA_DEFAULT_HISTORY_CAPACITY 256U

class game_voxel_chunk;

struct game_block_change_request
{
    uint16_t protocol_version;
    uint64_t session_id;
    uint64_t request_id;
    uint64_t world_id;
    int32_t chunk_x;
    int32_t chunk_z;
    uint64_t expected_revision;
    uint32_t expected_block_id;
    uint32_t requested_block_id;
    uint8_t local_x;
    uint16_t local_y;
    uint8_t local_z;
};

struct game_block_delta
{
    uint16_t protocol_version;
    uint64_t session_id;
    uint64_t request_id;
    uint64_t world_id;
    int32_t chunk_x;
    int32_t chunk_z;
    uint64_t previous_revision;
    uint64_t revision;
    uint32_t current_block_id;
    uint8_t player_modified;
    uint64_t server_tick;
    uint8_t local_x;
    uint16_t local_y;
    uint8_t local_z;
};

int32_t game_block_change_request_serialize(
    const game_block_change_request &request, ft_byte_buffer &buffer) noexcept;
int32_t game_block_change_request_deserialize(
    game_block_change_request &request, ft_byte_buffer &buffer) noexcept;
int32_t game_block_delta_serialize(const game_block_delta &delta,
    ft_byte_buffer &buffer) noexcept;
int32_t game_block_delta_deserialize(game_block_delta &delta,
    ft_byte_buffer &buffer) noexcept;
int32_t game_world_delta_snapshot_serialize(const game_voxel_chunk &chunk,
    ft_byte_buffer &buffer) noexcept;
int32_t game_world_delta_snapshot_deserialize(game_voxel_chunk &chunk,
    ft_byte_buffer &buffer) noexcept;

class game_world_delta_history
{
    private:
        ft_vector<game_block_delta> _entries;
        uint32_t _capacity;
        uint8_t _initialised_state;

    public:
        game_world_delta_history() noexcept;
        game_world_delta_history(const game_world_delta_history &other)
            noexcept = delete;
        game_world_delta_history(game_world_delta_history &&other)
            noexcept = delete;
        ~game_world_delta_history() noexcept;

        game_world_delta_history &operator=(
            const game_world_delta_history &other) noexcept = delete;
        game_world_delta_history &operator=(
            game_world_delta_history &&other) noexcept = delete;

        int32_t initialize(uint32_t capacity =
            GAME_WORLD_DELTA_DEFAULT_HISTORY_CAPACITY) noexcept;
        int32_t destroy() noexcept;
        int32_t append(const game_block_delta &delta) noexcept;
        int32_t get_since(uint64_t revision,
            ft_vector<game_block_delta> &deltas) const noexcept;
        uint64_t get_oldest_revision() const noexcept;
        uint64_t get_latest_revision() const noexcept;
        uint32_t size() const noexcept;
};

struct game_world_delta_interest
{
    uint64_t client_id;
    uint64_t world_id;
    int32_t chunk_x;
    int32_t chunk_z;
    uint64_t snapshot_revision;
    uint64_t acknowledged_revision;
    ft_bool snapshot_pending;
};

class game_world_delta_interest_set
{
    private:
        ft_vector<game_world_delta_interest> _entries;
        uint8_t _initialised_state;

        int32_t find(uint64_t client_id, uint64_t world_id,
            int32_t chunk_x, int32_t chunk_z,
            ft_size_t *index_out) const noexcept;

    public:
        game_world_delta_interest_set() noexcept;
        game_world_delta_interest_set(
            const game_world_delta_interest_set &other) noexcept = delete;
        game_world_delta_interest_set(
            game_world_delta_interest_set &&other) noexcept = delete;
        ~game_world_delta_interest_set() noexcept;

        game_world_delta_interest_set &operator=(
            const game_world_delta_interest_set &other) noexcept = delete;
        game_world_delta_interest_set &operator=(
            game_world_delta_interest_set &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t subscribe(uint64_t client_id, uint64_t world_id,
            int32_t chunk_x, int32_t chunk_z,
            uint64_t snapshot_revision) noexcept;
        int32_t unsubscribe(uint64_t client_id, uint64_t world_id,
            int32_t chunk_x, int32_t chunk_z) noexcept;
        int32_t acknowledge_snapshot(uint64_t client_id, uint64_t world_id,
            int32_t chunk_x, int32_t chunk_z,
            uint64_t snapshot_revision) noexcept;
        int32_t acknowledge_revision(uint64_t client_id, uint64_t world_id,
            int32_t chunk_x, int32_t chunk_z,
            uint64_t revision) noexcept;
        int32_t collect_live_clients(uint64_t world_id, int32_t chunk_x,
            int32_t chunk_z, ft_vector<uint64_t> &client_ids) const noexcept;
        ft_bool is_snapshot_pending(uint64_t client_id, uint64_t world_id,
            int32_t chunk_x, int32_t chunk_z) const noexcept;
        uint32_t size() const noexcept;
};

class game_world_delta_channel
{
    private:
        game_voxel_chunk *_chunk;
        game_world_delta_history _history;
        game_world_delta_interest_set _interests;
        uint64_t _world_id;
        int32_t _chunk_x;
        int32_t _chunk_z;
        uint64_t _server_tick;
        uint8_t _initialised_state;

    public:
        game_world_delta_channel() noexcept;
        game_world_delta_channel(const game_world_delta_channel &other)
            noexcept = delete;
        game_world_delta_channel(game_world_delta_channel &&other)
            noexcept = delete;
        ~game_world_delta_channel() noexcept;

        game_world_delta_channel &operator=(
            const game_world_delta_channel &other) noexcept = delete;
        game_world_delta_channel &operator=(
            game_world_delta_channel &&other) noexcept = delete;

        int32_t initialize(game_voxel_chunk &chunk, uint64_t world_id,
            int32_t chunk_x, int32_t chunk_z,
            uint32_t history_capacity =
                GAME_WORLD_DELTA_DEFAULT_HISTORY_CAPACITY) noexcept;
        int32_t destroy() noexcept;
        int32_t apply_request(const game_block_change_request &request,
            game_block_delta &delta) noexcept;
        int32_t subscribe(uint64_t client_id,
            uint64_t snapshot_revision) noexcept;
        int32_t unsubscribe(uint64_t client_id) noexcept;
        int32_t acknowledge_snapshot(uint64_t client_id,
            uint64_t snapshot_revision) noexcept;
        int32_t acknowledge_revision(uint64_t client_id,
            uint64_t revision) noexcept;
        int32_t collect_live_clients(ft_vector<uint64_t> &client_ids) const
            noexcept;
        int32_t recover_from(uint64_t revision,
            ft_vector<game_block_delta> &deltas) const noexcept;
        int32_t serialize_snapshot(ft_byte_buffer &buffer) const noexcept;
        uint64_t get_revision() const noexcept;
};

#endif
