#include "../../../Modules/Game/game_voxel_chunk.hpp"
#include "../../../Modules/Geometry/geometry_3d.hpp"
#include "../../../Modules/Voxel/voxel_api.hpp"
#include "../../../Modules/Voxel/voxel_mesh.hpp"
#include <chrono>
#include <cstdio>

using benchmark_clock = std::chrono::steady_clock;

static void print_average(const char *name, benchmark_clock::time_point started,
    uint32_t iterations)
{
    const double elapsed = std::chrono::duration<double, std::nano>(
        benchmark_clock::now() - started).count();
    std::printf("%s ns/op=%.2f\n", name, elapsed / iterations);
    return ;
}

static int32_t benchmark_chunk_copy(game_voxel_chunk &chunk)
{
    constexpr ft_size_t BLOCK_COUNT = 65536U;
    constexpr uint32_t ITERATIONS = 3000U;
    uint32_t blocks[BLOCK_COUNT];
    if (chunk.write_block(2, 17, 3, 41U))
        return (1);
    uint32_t iteration = 0U;
    const auto started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        if (chunk.copy_blocks(blocks, BLOCK_COUNT))
            return (1);
        ++iteration;
    }
    print_average("voxel_copy_chunk", started, ITERATIONS);
    if (blocks[(3U * 256U + 17U) * 16U + 2U] != 41U)
        return (1);
    return (chunk.write_block(2, 17, 3, 0U));
}

static int32_t benchmark_frustum(chunk_mesh &mesh, ft_bool thread_safe)
{
    uint32_t iterations = 1000000U;
    if (thread_safe != FT_FALSE)
        iterations = 60000U;
    geometry_frustum frustum;
    if (frustum.planes[0].normal.initialize(1, 0, 0)
        || frustum.planes[1].normal.initialize(-1, 0, 0)
        || frustum.planes[2].normal.initialize(0, 1, 0)
        || frustum.planes[3].normal.initialize(0, -1, 0)
        || frustum.planes[4].normal.initialize(0, 0, 1)
        || frustum.planes[5].normal.initialize(0, 0, -1))
        return (1);
    uint32_t index = 0U;
    while (index < 6U)
    {
        frustum.planes[index].distance = 1.0;
        if (thread_safe != FT_FALSE && frustum.planes[index].normal.enable_thread_safety())
            return (1);
        ++index;
    }
    mesh.has_occupied_bounds = FT_TRUE;
    mesh.occupied_bounds = {0, 0, 0, 1, 1, 1};
    uint32_t visible = 0U;
    index = 0U;
    const auto started = benchmark_clock::now();
    while (index < iterations)
    {
        visible += chunk_mesh_intersects_frustum(frustum, mesh,
            -static_cast<int32_t>(index % 4U), 0, 0);
        ++index;
    }
    if (thread_safe == FT_FALSE)
        print_average("voxel_frustum_cull", started, iterations);
    else
        print_average("voxel_frustum_cull_locked", started, iterations);
    return (visible != iterations / 4U * 3U);
}

static int32_t benchmark_checkerboard(game_voxel_chunk &chunk, chunk_mesh &mesh)
{
    constexpr uint32_t ITERATIONS = 16U;
    const chunk_mesh_bounds bounds = {0, 0, 0, 16, 32, 16};
    uint32_t position = 0U;
    while (position < 16U * 32U * 16U)
    {
        const int32_t x = static_cast<int32_t>(position % 16U);
        const int32_t z = static_cast<int32_t>((position / 16U) % 16U);
        const int32_t y = static_cast<int32_t>(position / 256U);
        if ((x + y + z) % 2 == 0
            && chunk.write_block(x, y, z, VOXEL_GENERATOR_STONE_BLOCK))
            return (1);
        ++position;
    }
    if (chunk_mesh_generate_from_chunk_in_bounds(mesh, chunk, bounds))
        return (1);
    uint32_t iteration = 0U;
    const auto started = benchmark_clock::now();
    while (iteration < ITERATIONS)
    {
        if (chunk_mesh_generate_from_chunk_in_bounds(mesh, chunk, bounds))
            return (1);
        ++iteration;
    }
    print_average("voxel_mesh_checkerboard", started, ITERATIONS);
    const ft_size_t index_count = mesh.indices.size();
    if (index_count == 0U || index_count != mesh.solid_indices.size()
        || !mesh.water_indices.empty())
        return (1);
    ft_size_t index = 0U;
    while (index < index_count)
    {
        if (mesh.indices[index] != mesh.solid_indices[index])
            return (1);
        ++index;
    }
    return (0);
}

static int32_t benchmark_sparse_mesh(game_voxel_chunk &chunk, chunk_mesh &mesh)
{
    constexpr uint32_t ITERATIONS = 100U;
    if (chunk.write_block(1, 15, 1, VOXEL_GENERATOR_STONE_BLOCK)
        || chunk.write_block(1, 48, 1, VOXEL_GENERATOR_STONE_BLOCK)
        || chunk.write_block(1, 200, 1, VOXEL_GENERATOR_LADDER_BLOCK))
        return (1);
    const auto started = benchmark_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        if (chunk_mesh_generate_from_chunk(mesh, chunk))
            return (1);
        iteration += 1U;
    }
    print_average("voxel_mesh_sparse", started, ITERATIONS);
    if (mesh.vertices.size() != 56U || mesh.indices.size() != 84U
        || mesh.vertices[48].block_id != VOXEL_GENERATOR_LADDER_BLOCK)
        return (1);
    if (chunk.write_block(1, 15, 1, GAME_VOXEL_AIR_BLOCK)
        || chunk.write_block(1, 48, 1, GAME_VOXEL_AIR_BLOCK)
        || chunk.write_block(1, 200, 1, GAME_VOXEL_AIR_BLOCK))
        return (1);
    return (0);
}

static int32_t benchmark_local_mesh_replacement()
{
    constexpr uint32_t ITERATIONS = 65U;
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    chunk_mesh replacement;
    const chunk_mesh_bounds bounds = {0, 0, 0, 3, 3, 3};
    if (chunk.initialize() || chunk_mesh_initialize(mesh) || chunk_mesh_initialize(replacement)
        || chunk.write_block(1, 1, 1, VOXEL_GENERATOR_STONE_BLOCK))
        return (1);
    uint32_t index = 0U;
    while (index < 280U)
    {
        const int32_t x = 5 + static_cast<int32_t>(index % 5U) * 2;
        const int32_t y = 5 + static_cast<int32_t>((index / 5U) % 8U) * 2;
        const int32_t z = 1 + static_cast<int32_t>(index / 40U) * 2;
        if (chunk.write_block(x, y, z, VOXEL_GENERATOR_STONE_BLOCK))
            return (1);
        index += 1U;
    }
    if (chunk_mesh_generate_from_chunk(mesh, chunk)
        || chunk.write_block(1, 1, 1, VOXEL_GENERATOR_WATER_BLOCK)
        || chunk_mesh_generate_from_chunk_in_bounds(replacement, chunk, bounds))
        return (1);
    const auto started = benchmark_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        if (chunk_mesh_replace_in_bounds(mesh, replacement, bounds))
            return (1);
        iteration += 1U;
    }
    print_average("voxel_mesh_replace", started, ITERATIONS);
    if (mesh.solid_indices.size() != 280U * 36U || mesh.water_indices.size() != 6U)
        return (1);
    if (chunk_mesh_destroy(replacement) || chunk_mesh_destroy(mesh) || chunk.destroy())
        return (1);
    return (0);
}

int main()
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    if (chunk.initialize() || chunk_mesh_initialize(mesh))
        return (1);
    const ft_bool failed = benchmark_chunk_copy(chunk)
        || benchmark_frustum(mesh, FT_FALSE) || benchmark_frustum(mesh, FT_TRUE)
        || benchmark_sparse_mesh(chunk, mesh) || benchmark_checkerboard(chunk, mesh)
        || benchmark_local_mesh_replacement();
    const int32_t mesh_error = chunk_mesh_destroy(mesh);
    const int32_t chunk_error = chunk.destroy();
    return (failed || mesh_error || chunk_error);
}
