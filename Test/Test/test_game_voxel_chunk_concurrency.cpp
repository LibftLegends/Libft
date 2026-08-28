#include "../../Modules/Game/game_voxel_chunk.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../test_internal.hpp"
#include <atomic>

struct s_game_chunk_concurrency_state
{
    game_voxel_chunk *chunk;
    std::atomic<int> *ready;
    std::atomic<int> *go;
    std::atomic<int> *failures;
};

struct s_game_chunk_snapshot_state
{
    game_voxel_chunk *chunk;
    std::atomic<int> *ready;
    std::atomic<int> *go;
    std::atomic<int> *failures;
};

static void game_chunk_wait_for_start(
    s_game_chunk_concurrency_state *state)
{
    state->ready->fetch_add(1);
    while (state->go->load() == 0)
        pt_thread_yield();
    return ;
}

static void *game_chunk_reader(void *argument)
{
    s_game_chunk_concurrency_state *state;
    uint32_t block_id;
    int32_t index;

    state = static_cast<s_game_chunk_concurrency_state *>(argument);
    game_chunk_wait_for_start(state);
    index = 0;
    while (index < 2000)
    {
        if (state->chunk->read_block(0, 0, 0, &block_id) != FT_ERR_SUCCESS)
            state->failures->fetch_add(1);
        index += 1;
    }
    return (ft_nullptr);
}

static void *game_chunk_writer(void *argument)
{
    s_game_chunk_concurrency_state *state;
    int32_t index;

    state = static_cast<s_game_chunk_concurrency_state *>(argument);
    game_chunk_wait_for_start(state);
    index = 0;
    while (index < 1000)
    {
        if (state->chunk->write_block(0, 0, 0,
                static_cast<uint32_t>(index & 1) + 1U) != FT_ERR_SUCCESS)
            state->failures->fetch_add(1);
        index += 1;
    }
    return (ft_nullptr);
}

static void *game_chunk_snapshot_reader(void *argument)
{
    s_game_chunk_snapshot_state *state;
    game_voxel_chunk snapshot;
    ft_byte_buffer encoded;
    uint32_t block_id;
    uint64_t revision;
    int32_t index;

    state = static_cast<s_game_chunk_snapshot_state *>(argument);
    state->ready->fetch_add(1);
    while (state->go->load() == 0)
        pt_thread_yield();
    if (snapshot.initialize() != FT_ERR_SUCCESS
        || encoded.initialize() != FT_ERR_SUCCESS)
    {
        state->failures->fetch_add(1);
        return (ft_nullptr);
    }
    index = 0;
    while (index < 250)
    {
        if (state->chunk->serialize(encoded) != FT_ERR_SUCCESS
            || encoded.reset_read_position() != FT_ERR_SUCCESS
            || snapshot.deserialize(encoded) != FT_ERR_SUCCESS
            || snapshot.read_block(0, 0, 0, &block_id) != FT_ERR_SUCCESS)
            state->failures->fetch_add(1);
        else
        {
            revision = snapshot.get_revision();
            if (revision == 0U || (block_id != 1U && block_id != 2U))
                state->failures->fetch_add(1);
        }
        index += 1;
    }
    (void)encoded.destroy();
    (void)snapshot.destroy();
    return (ft_nullptr);
}

FT_TEST(test_game_voxel_chunk_serializes_concurrent_readers_and_writes)
{
    game_voxel_chunk chunk;
    std::atomic<int> ready(0);
    std::atomic<int> go(0);
    std::atomic<int> failures(0);
    s_game_chunk_concurrency_state state = {&chunk, &ready, &go, &failures};
    pthread_t readers[2];
    pthread_t writer;
    uint64_t revision;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(0, pt_thread_create(&readers[0], ft_nullptr,
        game_chunk_reader, &state));
    FT_ASSERT_EQ(0, pt_thread_create(&readers[1], ft_nullptr,
        game_chunk_reader, &state));
    FT_ASSERT_EQ(0, pt_thread_create(&writer, ft_nullptr,
        game_chunk_writer, &state));
    while (ready.load() != 3)
        pt_thread_yield();
    go.store(1);
    FT_ASSERT_EQ(0, pt_thread_join(readers[0], ft_nullptr));
    FT_ASSERT_EQ(0, pt_thread_join(readers[1], ft_nullptr));
    FT_ASSERT_EQ(0, pt_thread_join(writer, ft_nullptr));
    FT_ASSERT_EQ(0, failures.load());
    revision = chunk.get_revision();
    FT_ASSERT_EQ(static_cast<uint64_t>(1000U), revision);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_move_uses_ordered_write_locks)
{
    game_voxel_chunk source;
    game_voxel_chunk destination;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.write_block(1, 2, 3, 77U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.write_block(1, 2, 3, 88U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.read_block(1, 2, 3,
        &block_id));
    FT_ASSERT_EQ(77U, block_id);
    FT_ASSERT_EQ(static_cast<uint64_t>(1U), destination.get_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_snapshots_are_detached_and_consistent)
{
    game_voxel_chunk chunk;
    std::atomic<int> ready(0);
    std::atomic<int> go(0);
    std::atomic<int> failures(0);
    s_game_chunk_concurrency_state writer_state = {&chunk, &ready, &go,
        &failures};
    s_game_chunk_snapshot_state reader_state = {&chunk, &ready, &go,
        &failures};
    pthread_t writer;
    pthread_t readers[2];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(0, pt_thread_create(&writer, ft_nullptr,
        game_chunk_writer, &writer_state));
    FT_ASSERT_EQ(0, pt_thread_create(&readers[0], ft_nullptr,
        game_chunk_snapshot_reader, &reader_state));
    FT_ASSERT_EQ(0, pt_thread_create(&readers[1], ft_nullptr,
        game_chunk_snapshot_reader, &reader_state));
    while (ready.load() != 3)
        pt_thread_yield();
    go.store(1);
    FT_ASSERT_EQ(0, pt_thread_join(writer, ft_nullptr));
    FT_ASSERT_EQ(0, pt_thread_join(readers[0], ft_nullptr));
    FT_ASSERT_EQ(0, pt_thread_join(readers[1], ft_nullptr));
    FT_ASSERT_EQ(0, failures.load());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}
