#ifndef CARD_GAME_REPLAY_HPP
# define CARD_GAME_REPLAY_HPP

# include <cstdint>
# include "card_game.hpp"
# include "card_game_limits.hpp"
# include "../Basic/basic.hpp"
# include "../Errno/errno.hpp"

static const uint32_t FT_CARD_GAME_MAX_REPLAY_EVENTS =
    FT_CARD_GAME_MAX_EVENT_RECORDS;
static const uint32_t FT_CARD_GAME_REPLAY_INITIAL_CAPACITY = 256U;
static const uint32_t FT_CARD_GAME_REPLAY_PRIVATE_BYTES = 64U;

enum card_game_replay_visibility_mode : uint8_t
{
    CARD_GAME_REPLAY_FULL_INFORMATION = 1U,
    CARD_GAME_REPLAY_PLAYER_VIEW = 2U
};

struct card_game_replay_header
{
    uint32_t profile_id;
    uint32_t format_id;
    uint32_t corpus_version;
    uint64_t rules_hash;
    uint64_t corpus_hash;
    uint64_t source_replay_hash;
    card_game_replay_visibility_mode visibility_mode;
    uint32_t viewer_player_id;
};

struct card_game_replay_event
{
    uint64_t sequence;
    uint64_t expected_state_sequence;
    uint32_t player_id;
    card_game_command_type command_type;
    uint32_t card_id;
    uint32_t target_instance;
    uint64_t state_hash_before;
    uint64_t state_hash_after;
    uint32_t private_owner_id;
    uint32_t private_size;
    uint8_t private_data[FT_CARD_GAME_REPLAY_PRIVATE_BYTES];
};

struct card_game_replay_result
{
    uint32_t outcome;
    uint32_t winner_player_id;
    uint64_t final_state_hash;
    uint64_t duration_epoch;
};

class card_game_replay
{
    private:
        uint8_t _initialised_state;
        card_game_replay_header _header;
        uint32_t _event_count;
        uint32_t _event_capacity;
        card_game_replay_event *_events;
        ft_bool _has_result;
        card_game_replay_result _result;

        card_game_replay(const card_game_replay &other) = delete;
        card_game_replay(card_game_replay &&other) = delete;
        card_game_replay &operator=(const card_game_replay &other) = delete;
        card_game_replay &operator=(card_game_replay &&other) = delete;

        int32_t validate_event(const card_game_replay_event &event) const noexcept;
        int32_t grow_events() noexcept;

    public:
        card_game_replay() noexcept;
        ~card_game_replay() noexcept;

        int32_t initialize(const card_game_replay_header &header) noexcept;
        int32_t destroy() noexcept;
        int32_t append(const card_game_replay_event &event) noexcept;
        int32_t append_command_record(
            const card_game_command_record &record) noexcept;
        int32_t set_result(const card_game_replay_result &result) noexcept;
        int32_t get_result(card_game_replay_result *result) const noexcept;
        int32_t get_header(card_game_replay_header *header) const noexcept;
        int32_t get_event(uint32_t index,
            card_game_replay_event *event) const noexcept;
        int32_t project_player_view(uint32_t viewer_player_id,
            card_game_replay &output) const noexcept;
        int32_t serialize(uint8_t *output, uint32_t output_capacity,
            uint32_t *output_size) const noexcept;
        int32_t serialized_size(uint32_t *size) const noexcept;
        int32_t save_file(const char *path) const noexcept;
        int32_t load_file(const char *path) noexcept;
        int32_t deserialize(const uint8_t *input, uint32_t input_size) noexcept;
        int32_t replay_into(card_game_engine &engine) const noexcept;
        uint32_t event_count() const noexcept;
};

#endif
