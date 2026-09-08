#include "voxel_mesh.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "voxel_api.hpp"
#include "../Errno/errno.hpp"
#include "../Game/game_voxel_chunk.hpp"
#include "../Template/vector.hpp"

static void chunk_mesh_reset_bounds(chunk_mesh &mesh) noexcept
{
    mesh.bounds.minimum_x = 0;
    mesh.bounds.minimum_y = 0;
    mesh.bounds.minimum_z = 0;
    mesh.bounds.maximum_x = GAME_VOXEL_CHUNK_WIDTH;
    mesh.bounds.maximum_y = GAME_VOXEL_CHUNK_HEIGHT;
    mesh.bounds.maximum_z = GAME_VOXEL_CHUNK_DEPTH;
    return ;
}

ft_bool chunk_mesh_bounds_is_valid(const chunk_mesh_bounds &bounds) noexcept
{
    if (bounds.minimum_x < 0 || bounds.minimum_y < 0
        || bounds.minimum_z < 0)
        return (FT_FALSE);
    if (bounds.maximum_x > GAME_VOXEL_CHUNK_WIDTH
        || bounds.maximum_y > GAME_VOXEL_CHUNK_HEIGHT
        || bounds.maximum_z > GAME_VOXEL_CHUNK_DEPTH)
        return (FT_FALSE);
    if (bounds.minimum_x >= bounds.maximum_x
        || bounds.minimum_y >= bounds.maximum_y
        || bounds.minimum_z >= bounds.maximum_z)
        return (FT_FALSE);
    return (FT_TRUE);
}

void chunk_mesh_bounds_set_full(chunk_mesh_bounds &bounds) noexcept
{
    bounds.minimum_x = 0;
    bounds.minimum_y = 0;
    bounds.minimum_z = 0;
    bounds.maximum_x = GAME_VOXEL_CHUNK_WIDTH;
    bounds.maximum_y = GAME_VOXEL_CHUNK_HEIGHT;
    bounds.maximum_z = GAME_VOXEL_CHUNK_DEPTH;
    return ;
}

static void chunk_mesh_reset_occupied_bounds(chunk_mesh &mesh) noexcept
{
    mesh.occupied_bounds.minimum_x = 0;
    mesh.occupied_bounds.minimum_y = 0;
    mesh.occupied_bounds.minimum_z = 0;
    mesh.occupied_bounds.maximum_x = 0;
    mesh.occupied_bounds.maximum_y = 0;
    mesh.occupied_bounds.maximum_z = 0;
    mesh.has_occupied_bounds = FT_FALSE;
    return ;
}

static void chunk_mesh_update_occupied_bounds(chunk_mesh &mesh,
    const chunk_mesh_vertex vertices[4]) noexcept
{
    int32_t vertex_index;
    int32_t coordinate_x;
    int32_t coordinate_y;
    int32_t coordinate_z;

    vertex_index = 0;
    while (vertex_index < 4)
    {
        coordinate_x = static_cast<int32_t>(
            vertices[vertex_index].coordinate_x);
        coordinate_y = static_cast<int32_t>(
            vertices[vertex_index].coordinate_y);
        coordinate_z = static_cast<int32_t>(
            vertices[vertex_index].coordinate_z);
        if (mesh.has_occupied_bounds == FT_FALSE)
        {
            mesh.occupied_bounds.minimum_x = coordinate_x;
            mesh.occupied_bounds.minimum_y = coordinate_y;
            mesh.occupied_bounds.minimum_z = coordinate_z;
            mesh.occupied_bounds.maximum_x = coordinate_x;
            mesh.occupied_bounds.maximum_y = coordinate_y;
            mesh.occupied_bounds.maximum_z = coordinate_z;
            mesh.has_occupied_bounds = FT_TRUE;
        }
        else
        {
            if (coordinate_x < mesh.occupied_bounds.minimum_x)
                mesh.occupied_bounds.minimum_x = coordinate_x;
            if (coordinate_y < mesh.occupied_bounds.minimum_y)
                mesh.occupied_bounds.minimum_y = coordinate_y;
            if (coordinate_z < mesh.occupied_bounds.minimum_z)
                mesh.occupied_bounds.minimum_z = coordinate_z;
            if (coordinate_x > mesh.occupied_bounds.maximum_x)
                mesh.occupied_bounds.maximum_x = coordinate_x;
            if (coordinate_y > mesh.occupied_bounds.maximum_y)
                mesh.occupied_bounds.maximum_y = coordinate_y;
            if (coordinate_z > mesh.occupied_bounds.maximum_z)
                mesh.occupied_bounds.maximum_z = coordinate_z;
        }
        vertex_index += 1;
    }
    return ;
}

int32_t chunk_mesh_initialize(chunk_mesh &mesh) noexcept
{
    int32_t error_code;

    error_code = mesh.vertices.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = mesh.indices.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)mesh.vertices.destroy();
        return (error_code);
    }
    error_code = mesh.solid_indices.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)mesh.indices.destroy();
        (void)mesh.vertices.destroy();
        return (error_code);
    }
    error_code = mesh.water_indices.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)mesh.solid_indices.destroy();
        (void)mesh.indices.destroy();
        (void)mesh.vertices.destroy();
        return (error_code);
    }
    chunk_mesh_reset_bounds(mesh);
    chunk_mesh_reset_occupied_bounds(mesh);
    return (FT_ERR_SUCCESS);
}

int32_t chunk_mesh_destroy(chunk_mesh &mesh) noexcept
{
    int32_t first_error;
    int32_t error_code;

    first_error = mesh.vertices.destroy();
    error_code = mesh.indices.destroy();
    if (first_error == FT_ERR_SUCCESS)
        first_error = mesh.solid_indices.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = mesh.water_indices.destroy();
    if (first_error != FT_ERR_SUCCESS)
        return (first_error);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    chunk_mesh_reset_bounds(mesh);
    chunk_mesh_reset_occupied_bounds(mesh);
    return (FT_ERR_SUCCESS);
}

int32_t chunk_mesh_clear(chunk_mesh &mesh) noexcept
{
    mesh.vertices.clear();
    if (mesh.vertices.get_error() != FT_ERR_SUCCESS)
        return (mesh.vertices.get_error());
    mesh.indices.clear();
    if (mesh.indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.indices.get_error());
    mesh.solid_indices.clear();
    if (mesh.solid_indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.solid_indices.get_error());
    mesh.water_indices.clear();
    if (mesh.water_indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.water_indices.get_error());
    chunk_mesh_reset_bounds(mesh);
    chunk_mesh_reset_occupied_bounds(mesh);
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_read_or_air(const game_voxel_chunk &chunk,
    int32_t local_x, int32_t local_y, int32_t local_z,
    uint32_t *block_id) noexcept
{
    const game_voxel_chunk_section *section;
    uint16_t section_local_index;

    if (local_x < 0 || local_x >= GAME_VOXEL_CHUNK_WIDTH || local_y < 0
        || local_y >= GAME_VOXEL_CHUNK_HEIGHT || local_z < 0
        || local_z >= GAME_VOXEL_CHUNK_DEPTH)
    {
        *block_id = GAME_VOXEL_AIR_BLOCK;
        return (FT_ERR_SUCCESS);
    }
    section = &chunk.get_section(static_cast<uint8_t>(local_y >> 4));
    section_local_index = static_cast<uint16_t>(local_x + (local_z << 4)
        + ((local_y & 15) << 8));
    *block_id = section->get_block(section_local_index);
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_face_is_visible(const game_voxel_chunk &chunk,
    int32_t local_x, int32_t local_y, int32_t local_z,
    chunk_mesh_face face, ft_bool *visible) noexcept
{
    uint32_t neighbor_block_id;
    int32_t neighbor_x;
    int32_t neighbor_y;
    int32_t neighbor_z;
    int32_t error_code;

    neighbor_x = local_x;
    neighbor_y = local_y;
    neighbor_z = local_z;
    if (face == CHUNK_MESH_FACE_WEST)
        neighbor_x -= 1;
    if (face == CHUNK_MESH_FACE_EAST)
        neighbor_x += 1;
    if (face == CHUNK_MESH_FACE_DOWN)
        neighbor_y -= 1;
    if (face == CHUNK_MESH_FACE_UP)
        neighbor_y += 1;
    if (face == CHUNK_MESH_FACE_NORTH)
        neighbor_z -= 1;
    if (face == CHUNK_MESH_FACE_SOUTH)
        neighbor_z += 1;
    error_code = chunk_mesh_read_or_air(chunk, neighbor_x, neighbor_y,
        neighbor_z, &neighbor_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (voxel_block_occludes_faces(neighbor_block_id) == FT_FALSE)
        *visible = FT_TRUE;
    else
        *visible = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

static void chunk_mesh_fill_vertex(chunk_mesh_vertex *vertex,
    int32_t coordinate_x, int32_t coordinate_y, int32_t coordinate_z,
    uint16_t texture_u, uint16_t texture_v, uint32_t block_id,
    chunk_mesh_face face) noexcept
{
    vertex->coordinate_x = static_cast<uint16_t>(coordinate_x);
    vertex->coordinate_y = static_cast<uint16_t>(coordinate_y);
    vertex->coordinate_z = static_cast<uint16_t>(coordinate_z);
    vertex->texture_u = texture_u;
    vertex->texture_v = texture_v;
    vertex->block_id = block_id;
    vertex->face = static_cast<uint8_t>(face);
    vertex->packed_light = 0U;
    return ;
}

struct chunk_mesh_mask_cell
{
    uint32_t block_id;
    uint8_t packed_light;
};

static void chunk_mesh_make_west_rectangle(chunk_mesh_vertex vertices[4],
    int32_t coordinate_x, int32_t minimum_y, int32_t minimum_z,
    int32_t maximum_y, int32_t maximum_z, uint32_t block_id) noexcept
{
    chunk_mesh_fill_vertex(&vertices[0], coordinate_x, minimum_y, minimum_z,
        0U, 0U, block_id, CHUNK_MESH_FACE_WEST);
    chunk_mesh_fill_vertex(&vertices[1], coordinate_x, maximum_y, minimum_z,
        0U, static_cast<uint16_t>(maximum_y - minimum_y), block_id,
        CHUNK_MESH_FACE_WEST);
    chunk_mesh_fill_vertex(&vertices[2], coordinate_x, maximum_y, maximum_z,
        static_cast<uint16_t>(maximum_z - minimum_z),
        static_cast<uint16_t>(maximum_y - minimum_y), block_id,
        CHUNK_MESH_FACE_WEST);
    chunk_mesh_fill_vertex(&vertices[3], coordinate_x, minimum_y, maximum_z,
        static_cast<uint16_t>(maximum_z - minimum_z), 0U, block_id,
        CHUNK_MESH_FACE_WEST);
    return ;
}

static void chunk_mesh_make_east_rectangle(chunk_mesh_vertex vertices[4],
    int32_t coordinate_x, int32_t minimum_y, int32_t minimum_z,
    int32_t maximum_y, int32_t maximum_z, uint32_t block_id) noexcept
{
    chunk_mesh_fill_vertex(&vertices[0], coordinate_x, minimum_y, maximum_z,
        0U, 0U, block_id, CHUNK_MESH_FACE_EAST);
    chunk_mesh_fill_vertex(&vertices[1], coordinate_x, maximum_y, maximum_z,
        0U, static_cast<uint16_t>(maximum_y - minimum_y), block_id,
        CHUNK_MESH_FACE_EAST);
    chunk_mesh_fill_vertex(&vertices[2], coordinate_x, maximum_y, minimum_z,
        static_cast<uint16_t>(maximum_z - minimum_z),
        static_cast<uint16_t>(maximum_y - minimum_y), block_id,
        CHUNK_MESH_FACE_EAST);
    chunk_mesh_fill_vertex(&vertices[3], coordinate_x, minimum_y, minimum_z,
        static_cast<uint16_t>(maximum_z - minimum_z), 0U, block_id,
        CHUNK_MESH_FACE_EAST);
    return ;
}

static void chunk_mesh_make_down_rectangle(chunk_mesh_vertex vertices[4],
    int32_t minimum_x, int32_t coordinate_y, int32_t minimum_z,
    int32_t maximum_x, int32_t maximum_z, uint32_t block_id) noexcept
{
    chunk_mesh_fill_vertex(&vertices[0], minimum_x, coordinate_y, maximum_z,
        0U, 0U, block_id, CHUNK_MESH_FACE_DOWN);
    chunk_mesh_fill_vertex(&vertices[1], maximum_x, coordinate_y, maximum_z,
        0U, static_cast<uint16_t>(maximum_x - minimum_x), block_id,
        CHUNK_MESH_FACE_DOWN);
    chunk_mesh_fill_vertex(&vertices[2], maximum_x, coordinate_y, minimum_z,
        static_cast<uint16_t>(maximum_z - minimum_z),
        static_cast<uint16_t>(maximum_x - minimum_x), block_id,
        CHUNK_MESH_FACE_DOWN);
    chunk_mesh_fill_vertex(&vertices[3], minimum_x, coordinate_y, minimum_z,
        static_cast<uint16_t>(maximum_z - minimum_z), 0U, block_id,
        CHUNK_MESH_FACE_DOWN);
    return ;
}

static void chunk_mesh_make_up_rectangle(chunk_mesh_vertex vertices[4],
    int32_t minimum_x, int32_t coordinate_y, int32_t minimum_z,
    int32_t maximum_x, int32_t maximum_z, uint32_t block_id) noexcept
{
    chunk_mesh_fill_vertex(&vertices[0], minimum_x, coordinate_y, minimum_z,
        0U, 0U, block_id, CHUNK_MESH_FACE_UP);
    chunk_mesh_fill_vertex(&vertices[1], maximum_x, coordinate_y, minimum_z,
        0U, static_cast<uint16_t>(maximum_x - minimum_x), block_id,
        CHUNK_MESH_FACE_UP);
    chunk_mesh_fill_vertex(&vertices[2], maximum_x, coordinate_y, maximum_z,
        static_cast<uint16_t>(maximum_z - minimum_z),
        static_cast<uint16_t>(maximum_x - minimum_x), block_id,
        CHUNK_MESH_FACE_UP);
    chunk_mesh_fill_vertex(&vertices[3], minimum_x, coordinate_y, maximum_z,
        static_cast<uint16_t>(maximum_z - minimum_z), 0U, block_id,
        CHUNK_MESH_FACE_UP);
    return ;
}

static void chunk_mesh_make_north_rectangle(chunk_mesh_vertex vertices[4],
    int32_t minimum_x, int32_t minimum_y, int32_t coordinate_z,
    int32_t maximum_x, int32_t maximum_y, uint32_t block_id) noexcept
{
    chunk_mesh_fill_vertex(&vertices[0], maximum_x, minimum_y, coordinate_z,
        0U, 0U, block_id, CHUNK_MESH_FACE_NORTH);
    chunk_mesh_fill_vertex(&vertices[1], maximum_x, maximum_y, coordinate_z,
        0U, static_cast<uint16_t>(maximum_y - minimum_y), block_id,
        CHUNK_MESH_FACE_NORTH);
    chunk_mesh_fill_vertex(&vertices[2], minimum_x, maximum_y, coordinate_z,
        static_cast<uint16_t>(maximum_x - minimum_x),
        static_cast<uint16_t>(maximum_y - minimum_y), block_id,
        CHUNK_MESH_FACE_NORTH);
    chunk_mesh_fill_vertex(&vertices[3], minimum_x, minimum_y, coordinate_z,
        static_cast<uint16_t>(maximum_x - minimum_x), 0U, block_id,
        CHUNK_MESH_FACE_NORTH);
    return ;
}

static void chunk_mesh_make_south_rectangle(chunk_mesh_vertex vertices[4],
    int32_t minimum_x, int32_t minimum_y, int32_t coordinate_z,
    int32_t maximum_x, int32_t maximum_y, uint32_t block_id) noexcept
{
    chunk_mesh_fill_vertex(&vertices[0], minimum_x, minimum_y, coordinate_z,
        0U, 0U, block_id, CHUNK_MESH_FACE_SOUTH);
    chunk_mesh_fill_vertex(&vertices[1], minimum_x, maximum_y, coordinate_z,
        0U, static_cast<uint16_t>(maximum_y - minimum_y), block_id,
        CHUNK_MESH_FACE_SOUTH);
    chunk_mesh_fill_vertex(&vertices[2], maximum_x, maximum_y, coordinate_z,
        static_cast<uint16_t>(maximum_x - minimum_x),
        static_cast<uint16_t>(maximum_y - minimum_y), block_id,
        CHUNK_MESH_FACE_SOUTH);
    chunk_mesh_fill_vertex(&vertices[3], maximum_x, minimum_y, coordinate_z,
        static_cast<uint16_t>(maximum_x - minimum_x), 0U, block_id,
        CHUNK_MESH_FACE_SOUTH);
    return ;
}

static void chunk_mesh_make_rectangle_vertices(chunk_mesh_vertex vertices[4],
    int32_t axis_value, int32_t minimum_column, int32_t minimum_row,
    int32_t maximum_column, int32_t maximum_row, uint32_t block_id,
    chunk_mesh_face face) noexcept
{
    if (face == CHUNK_MESH_FACE_WEST)
        chunk_mesh_make_west_rectangle(vertices, axis_value, minimum_row,
            minimum_column, maximum_row, maximum_column, block_id);
    if (face == CHUNK_MESH_FACE_EAST)
        chunk_mesh_make_east_rectangle(vertices, axis_value + 1, minimum_row,
            minimum_column, maximum_row, maximum_column, block_id);
    if (face == CHUNK_MESH_FACE_DOWN)
        chunk_mesh_make_down_rectangle(vertices, minimum_column, axis_value,
            minimum_row, maximum_column, maximum_row, block_id);
    if (face == CHUNK_MESH_FACE_UP)
        chunk_mesh_make_up_rectangle(vertices, minimum_column, axis_value + 1,
            minimum_row, maximum_column, maximum_row, block_id);
    if (face == CHUNK_MESH_FACE_NORTH)
        chunk_mesh_make_north_rectangle(vertices, minimum_column, minimum_row,
            axis_value, maximum_column, maximum_row, block_id);
    if (face == CHUNK_MESH_FACE_SOUTH)
        chunk_mesh_make_south_rectangle(vertices, minimum_column, minimum_row,
            axis_value + 1, maximum_column, maximum_row, block_id);
    return ;
}

static int32_t chunk_mesh_push_index(chunk_mesh &mesh,
    uint32_t index_value) noexcept
{
    return (mesh.indices.push_back(index_value));
}

static int32_t chunk_mesh_emit_rectangle(chunk_mesh &mesh, int32_t axis_value,
    int32_t minimum_column, int32_t minimum_row, int32_t maximum_column,
    int32_t maximum_row, uint32_t block_id, chunk_mesh_face face,
    uint8_t packed_light = 0U) noexcept
{
    chunk_mesh_vertex vertices[4];
    uint32_t base_vertex;
    int32_t error_code;

    base_vertex = static_cast<uint32_t>(mesh.vertices.size());
    chunk_mesh_make_rectangle_vertices(vertices, axis_value, minimum_column,
        minimum_row, maximum_column, maximum_row, block_id, face);
    vertices[0].packed_light = packed_light;
    vertices[1].packed_light = packed_light;
    vertices[2].packed_light = packed_light;
    vertices[3].packed_light = packed_light;
    chunk_mesh_update_occupied_bounds(mesh, vertices);
    error_code = mesh.vertices.push_back(vertices[0]);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = mesh.vertices.push_back(vertices[1]);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = mesh.vertices.push_back(vertices[2]);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = mesh.vertices.push_back(vertices[3]);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = chunk_mesh_push_index(mesh, base_vertex);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = chunk_mesh_push_index(mesh, base_vertex + 1U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = chunk_mesh_push_index(mesh, base_vertex + 2U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = chunk_mesh_push_index(mesh, base_vertex);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = chunk_mesh_push_index(mesh, base_vertex + 2U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (chunk_mesh_push_index(mesh, base_vertex + 3U));
}

static void chunk_mesh_plane_dimensions(chunk_mesh_face face,
    int32_t *column_count, int32_t *row_count) noexcept
{
    if (face == CHUNK_MESH_FACE_WEST || face == CHUNK_MESH_FACE_EAST)
    {
        *column_count = GAME_VOXEL_CHUNK_DEPTH;
        *row_count = GAME_VOXEL_CHUNK_HEIGHT;
        return ;
    }
    if (face == CHUNK_MESH_FACE_DOWN || face == CHUNK_MESH_FACE_UP)
    {
        *column_count = GAME_VOXEL_CHUNK_WIDTH;
        *row_count = GAME_VOXEL_CHUNK_DEPTH;
        return ;
    }
    *column_count = GAME_VOXEL_CHUNK_WIDTH;
    *row_count = GAME_VOXEL_CHUNK_HEIGHT;
    return ;
}

static void chunk_mesh_block_coordinates_for_plane(chunk_mesh_face face,
    int32_t axis_value, int32_t column_value, int32_t row_value,
    int32_t *local_x, int32_t *local_y, int32_t *local_z) noexcept
{
    if (face == CHUNK_MESH_FACE_WEST || face == CHUNK_MESH_FACE_EAST)
    {
        *local_x = axis_value;
        *local_y = row_value;
        *local_z = column_value;
        return ;
    }
    if (face == CHUNK_MESH_FACE_DOWN || face == CHUNK_MESH_FACE_UP)
    {
        *local_x = column_value;
        *local_y = axis_value;
        *local_z = row_value;
        return ;
    }
    *local_x = column_value;
    *local_y = row_value;
    *local_z = axis_value;
    return ;
}

static void chunk_mesh_plane_ranges(chunk_mesh_face face,
    const chunk_mesh_bounds *bounds, int32_t *axis_start, int32_t *axis_end,
    int32_t *column_start, int32_t *column_end, int32_t *row_start,
    int32_t *row_end) noexcept
{
    *axis_start = 0;
    *column_start = 0;
    *row_start = 0;
    if (face == CHUNK_MESH_FACE_WEST || face == CHUNK_MESH_FACE_EAST)
    {
        *axis_end = GAME_VOXEL_CHUNK_WIDTH;
        *column_end = GAME_VOXEL_CHUNK_DEPTH;
        *row_end = GAME_VOXEL_CHUNK_HEIGHT;
        if (bounds != nullptr)
        {
            *axis_start = bounds->minimum_x;
            *axis_end = bounds->maximum_x;
            *column_start = bounds->minimum_z;
            *column_end = bounds->maximum_z;
            *row_start = bounds->minimum_y;
            *row_end = bounds->maximum_y;
        }
        return ;
    }
    if (face == CHUNK_MESH_FACE_DOWN || face == CHUNK_MESH_FACE_UP)
    {
        *axis_end = GAME_VOXEL_CHUNK_HEIGHT;
        *column_end = GAME_VOXEL_CHUNK_WIDTH;
        *row_end = GAME_VOXEL_CHUNK_DEPTH;
        if (bounds != nullptr)
        {
            *axis_start = bounds->minimum_y;
            *axis_end = bounds->maximum_y;
            *column_start = bounds->minimum_x;
            *column_end = bounds->maximum_x;
            *row_start = bounds->minimum_z;
            *row_end = bounds->maximum_z;
        }
        return ;
    }
    *axis_end = GAME_VOXEL_CHUNK_DEPTH;
    *column_end = GAME_VOXEL_CHUNK_WIDTH;
    *row_end = GAME_VOXEL_CHUNK_HEIGHT;
    if (bounds != nullptr)
    {
        *axis_start = bounds->minimum_z;
        *axis_end = bounds->maximum_z;
        *column_start = bounds->minimum_x;
        *column_end = bounds->maximum_x;
        *row_start = bounds->minimum_y;
        *row_end = bounds->maximum_y;
    }
    return ;
}

static int32_t chunk_mesh_face_light(const game_voxel_chunk &chunk,
    const voxel_light_chunk *light, voxel_light_packed_lookup_fn light_lookup,
    void *light_user_data, int32_t chunk_x, int32_t chunk_z,
    int32_t local_x, int32_t local_y, int32_t local_z, chunk_mesh_face face,
    uint8_t *packed_light) noexcept
{
    int32_t sample_x;
    int32_t sample_y;
    int32_t sample_z;
    ft_bool outside;
    int32_t error_code;
    uint32_t block_id;
    uint8_t light_value;
    uint8_t emitted_level;

    if (packed_light == nullptr)
        return (FT_ERR_INVALID_POINTER);
    sample_x = local_x;
    sample_y = local_y;
    sample_z = local_z;
    if (face == CHUNK_MESH_FACE_WEST)
        sample_x -= 1;
    else if (face == CHUNK_MESH_FACE_EAST)
        sample_x += 1;
    else if (face == CHUNK_MESH_FACE_DOWN)
        sample_y -= 1;
    else if (face == CHUNK_MESH_FACE_UP)
        sample_y += 1;
    else if (face == CHUNK_MESH_FACE_NORTH)
        sample_z -= 1;
    else if (face == CHUNK_MESH_FACE_SOUTH)
        sample_z += 1;
    outside = FT_FALSE;
    if (sample_x < 0 || sample_x >= GAME_VOXEL_CHUNK_WIDTH
        || sample_y < 0 || sample_y >= GAME_VOXEL_CHUNK_HEIGHT
        || sample_z < 0 || sample_z >= GAME_VOXEL_CHUNK_DEPTH)
        outside = FT_TRUE;
    if (light_lookup != nullptr && sample_y >= 0
        && sample_y < GAME_VOXEL_CHUNK_HEIGHT
        && (sample_x < 0 || sample_x >= GAME_VOXEL_CHUNK_WIDTH
            || sample_z < 0 || sample_z >= GAME_VOXEL_CHUNK_DEPTH))
    {
        error_code = light_lookup(light_user_data,
            chunk_x * GAME_VOXEL_CHUNK_WIDTH + sample_x, sample_y,
            chunk_z * GAME_VOXEL_CHUNK_DEPTH + sample_z, packed_light);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    else if (light == nullptr)
        *packed_light = 0U;
    else if (outside == FT_TRUE)
    {
        /* Preserve the old fallback when no world-light callback exists. */
        ft_bool open_to_sky = FT_TRUE;
        int32_t sky_y = local_y + 1;

        while (sky_y < GAME_VOXEL_CHUNK_HEIGHT)
        {
            block_id = GAME_VOXEL_AIR_BLOCK;
            if (chunk.read_block(local_x, sky_y, local_z, &block_id)
                != FT_ERR_SUCCESS
                || voxel_get_block_metadata(block_id).transparent == FT_FALSE)
            {
                open_to_sky = FT_FALSE;
                break ;
            }
            sky_y += 1;
        }
        if (open_to_sky == FT_TRUE)
            *packed_light = voxel_light_pack(15U,
                voxel_light_block(light->get(local_x, local_y, local_z)));
        else
        {
            if (sample_x < 0)
                sample_x = 0;
            else if (sample_x >= GAME_VOXEL_CHUNK_WIDTH)
                sample_x = GAME_VOXEL_CHUNK_WIDTH - 1;
            if (sample_y < 0)
                sample_y = 0;
            else if (sample_y >= GAME_VOXEL_CHUNK_HEIGHT)
                sample_y = GAME_VOXEL_CHUNK_HEIGHT - 1;
            if (sample_z < 0)
                sample_z = 0;
            else if (sample_z >= GAME_VOXEL_CHUNK_DEPTH)
                sample_z = GAME_VOXEL_CHUNK_DEPTH - 1;
            *packed_light = light->get(sample_x, sample_y, sample_z);
        }
    }
    else
        *packed_light = light->get(sample_x, sample_y, sample_z);
    error_code = chunk.read_block(local_x, local_y, local_z, &block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    emitted_level = voxel_block_emitted_light_level(block_id);
    light_value = voxel_light_block(*packed_light);
    if (emitted_level > light_value)
        *packed_light = voxel_light_pack(voxel_light_sky(*packed_light),
            emitted_level);
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_fill_visible_face_mask(const game_voxel_chunk &chunk,
    chunk_mesh_face face, int32_t axis_value, chunk_mesh_mask_cell mask[4096],
    const voxel_light_chunk *light, voxel_light_packed_lookup_fn light_lookup,
    void *light_user_data, int32_t chunk_x, int32_t chunk_z,
    const chunk_mesh_bounds *bounds) noexcept
{
    int32_t column_count;
    int32_t row_count;
    int32_t column_value;
    int32_t row_value;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;
    uint32_t block_id;
    ft_bool visible;
    int32_t error_code;
    int32_t axis_start;
    int32_t axis_end;
    int32_t column_start;
    int32_t column_end;
    int32_t row_start;
    int32_t row_end;

    chunk_mesh_plane_dimensions(face, &column_count, &row_count);
    chunk_mesh_plane_ranges(face, bounds, &axis_start, &axis_end,
        &column_start, &column_end, &row_start, &row_end);
    if (axis_value < axis_start || axis_value >= axis_end)
        return (FT_ERR_SUCCESS);
    row_value = row_start;
    while (row_value < row_end)
    {
        column_value = column_start;
        while (column_value < column_end)
        {
            chunk_mesh_block_coordinates_for_plane(face, axis_value,
                column_value, row_value, &local_x, &local_y, &local_z);
            error_code = chunk_mesh_read_or_air(chunk, local_x, local_y,
                local_z, &block_id);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            mask[(row_value * column_count) + column_value].block_id = 0U;
            mask[(row_value * column_count) + column_value].packed_light = 0U;
            if (block_id != GAME_VOXEL_AIR_BLOCK)
            {
                if (voxel_block_is_liquid(block_id) == FT_FALSE
                    || face == CHUNK_MESH_FACE_UP)
                {
                    error_code = chunk_mesh_face_is_visible(chunk, local_x,
                        local_y, local_z, face, &visible);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    if (visible == FT_TRUE
                        && voxel_block_is_liquid(block_id) == FT_TRUE)
                    {
                        uint32_t above_id = 0U;
                        (void)chunk_mesh_read_or_air(chunk, local_x,
                            local_y + 1, local_z, &above_id);
                        if (voxel_block_is_liquid(above_id) == FT_TRUE)
                            visible = FT_FALSE;
                    }
                    if (visible == FT_TRUE)
                    {
                        mask[(row_value * column_count) + column_value].block_id = block_id;
                        error_code = chunk_mesh_face_light(chunk, light,
                            light_lookup, light_user_data, chunk_x, chunk_z,
                            local_x, local_y, local_z, face,
                            &mask[(row_value * column_count)
                                + column_value].packed_light);
                        if (error_code != FT_ERR_SUCCESS)
                            return (error_code);
                    }
                }
            }
            column_value += 1;
        }
        row_value += 1;
    }
    return (FT_ERR_SUCCESS);
}

static ft_bool chunk_mesh_mask_cell_matches(chunk_mesh_mask_cell mask[4096],
    ft_bool consumed[4096], int32_t column_count, int32_t column_value,
    int32_t row_value, uint32_t block_id, uint8_t packed_light) noexcept
{
    int32_t mask_index;

    mask_index = (row_value * column_count) + column_value;
    if (consumed[mask_index] == FT_TRUE)
        return (FT_FALSE);
    if (mask[mask_index].block_id != block_id
        || mask[mask_index].packed_light != packed_light)
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t chunk_mesh_greedy_width(chunk_mesh_mask_cell mask[4096],
    ft_bool consumed[4096], int32_t column_count, int32_t column_end,
    int32_t row_value, int32_t start_column, uint32_t block_id,
    uint8_t packed_light) noexcept
{
    int32_t width_count;

    width_count = 0;
    while (start_column + width_count < column_end
        && chunk_mesh_mask_cell_matches(mask, consumed, column_count,
            start_column + width_count, row_value, block_id,
            packed_light) == FT_TRUE)
        width_count += 1;
    return (width_count);
}

static ft_bool chunk_mesh_greedy_row_matches(chunk_mesh_mask_cell mask[4096],
    ft_bool consumed[4096], int32_t column_count, int32_t row_value,
    int32_t start_column, int32_t width_count, uint32_t block_id,
    uint8_t packed_light) noexcept
{
    int32_t width_index;

    width_index = 0;
    while (width_index < width_count)
    {
        if (chunk_mesh_mask_cell_matches(mask, consumed, column_count,
                start_column + width_index, row_value, block_id,
                packed_light) == FT_FALSE)
            return (FT_FALSE);
        width_index += 1;
    }
    return (FT_TRUE);
}

static int32_t chunk_mesh_greedy_height(chunk_mesh_mask_cell mask[4096],
    ft_bool consumed[4096], int32_t column_count, int32_t row_end,
    int32_t start_column, int32_t start_row, int32_t width_count,
    uint32_t block_id, uint8_t packed_light) noexcept
{
    int32_t height_count;

    height_count = 0;
    while (start_row + height_count < row_end
        && chunk_mesh_greedy_row_matches(mask, consumed, column_count,
            start_row + height_count, start_column, width_count,
            block_id, packed_light) == FT_TRUE)
        height_count += 1;
    return (height_count);
}

static void chunk_mesh_mark_rectangle_consumed(ft_bool consumed[4096],
    int32_t column_count, int32_t start_column, int32_t start_row,
    int32_t width_count, int32_t height_count) noexcept
{
    int32_t width_index;
    int32_t height_index;

    height_index = 0;
    while (height_index < height_count)
    {
        width_index = 0;
        while (width_index < width_count)
        {
            consumed[((start_row + height_index) * column_count)
                + start_column + width_index] = FT_TRUE;
            width_index += 1;
        }
        height_index += 1;
    }
    return ;
}

static void chunk_mesh_clear_consumed_mask(ft_bool consumed[4096],
    int32_t cell_count) noexcept
{
    int32_t cell_index;

    cell_index = 0;
    while (cell_index < cell_count)
    {
        consumed[cell_index] = FT_FALSE;
        cell_index += 1;
    }
    return ;
}

static int32_t chunk_mesh_emit_greedy_mask(chunk_mesh &mesh,
    chunk_mesh_mask_cell mask[4096], ft_bool consumed[4096], chunk_mesh_face face,
    int32_t axis_value, const chunk_mesh_bounds *bounds) noexcept
{
    int32_t column_count;
    int32_t row_count;
    int32_t column_value;
    int32_t row_value;
    int32_t width_count;
    int32_t height_count;
    uint32_t block_id;
    int32_t error_code;
    int32_t axis_start;
    int32_t axis_end;
    int32_t column_start;
    int32_t column_end;
    int32_t row_start;
    int32_t row_end;

    chunk_mesh_plane_dimensions(face, &column_count, &row_count);
    chunk_mesh_plane_ranges(face, bounds, &axis_start, &axis_end,
        &column_start, &column_end, &row_start, &row_end);
    chunk_mesh_clear_consumed_mask(consumed, column_count * row_count);
    if (axis_value < axis_start || axis_value >= axis_end)
        return (FT_ERR_SUCCESS);
    row_value = row_start;
    while (row_value < row_end)
    {
        column_value = column_start;
        while (column_value < column_end)
        {
            block_id = mask[(row_value * column_count) + column_value].block_id;
            if (block_id != GAME_VOXEL_AIR_BLOCK
                && consumed[(row_value * column_count)
                    + column_value] == FT_FALSE)
            {
                width_count = chunk_mesh_greedy_width(mask, consumed,
                    column_count, column_end, row_value, column_value,
                    block_id,
                    mask[(row_value * column_count) + column_value].packed_light);
                height_count = chunk_mesh_greedy_height(mask, consumed,
                    column_count, row_end, column_value, row_value,
                    width_count, block_id,
                    mask[(row_value * column_count) + column_value].packed_light);
                error_code = chunk_mesh_emit_rectangle(mesh, axis_value,
                    column_value, row_value, column_value + width_count,
                    row_value + height_count, block_id, face,
                    mask[(row_value * column_count) + column_value].packed_light);
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
                chunk_mesh_mark_rectangle_consumed(consumed, column_count,
                    column_value, row_value, width_count, height_count);
            }
            column_value += 1;
        }
        row_value += 1;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_emit_greedy_faces_for_direction(chunk_mesh &mesh,
    const game_voxel_chunk &chunk, chunk_mesh_face face,
    const voxel_light_chunk *light, voxel_light_packed_lookup_fn light_lookup,
    void *light_user_data, int32_t chunk_x, int32_t chunk_z,
    const chunk_mesh_bounds *bounds) noexcept
{
    chunk_mesh_mask_cell mask[4096];
    ft_bool consumed[4096];
    int32_t axis_value;
    int32_t error_code;
    int32_t axis_start;
    int32_t axis_end;
    int32_t column_start;
    int32_t column_end;
    int32_t row_start;
    int32_t row_end;

    chunk_mesh_plane_ranges(face, bounds, &axis_start, &axis_end,
        &column_start, &column_end, &row_start, &row_end);
    (void)column_start;
    (void)column_end;
    (void)row_start;
    (void)row_end;
    axis_value = axis_start;
    while (axis_value < axis_end)
    {
        error_code = chunk_mesh_fill_visible_face_mask(chunk, face,
            axis_value, mask, light, light_lookup, light_user_data, chunk_x,
            chunk_z, bounds);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = chunk_mesh_emit_greedy_mask(mesh, mask, consumed, face,
            axis_value, bounds);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        axis_value += 1;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_emit_visible_faces(chunk_mesh &mesh,
    const game_voxel_chunk &chunk, const voxel_light_chunk *light,
    voxel_light_packed_lookup_fn light_lookup, void *light_user_data,
    int32_t chunk_x, int32_t chunk_z, const chunk_mesh_bounds *bounds) noexcept
{
    chunk_mesh_face face;
    int32_t error_code;

    face = CHUNK_MESH_FACE_WEST;
    while (face <= CHUNK_MESH_FACE_SOUTH)
    {
        error_code = chunk_mesh_emit_greedy_faces_for_direction(mesh, chunk,
            face, light, light_lookup, light_user_data, chunk_x, chunk_z,
            bounds);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        face = static_cast<chunk_mesh_face>(static_cast<int32_t>(face) + 1);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_partition_indices(chunk_mesh &mesh) noexcept
{
    ft_vector<uint32_t> *destination;
    uint32_t vertex_index;
    ft_size_t index;
    int32_t error_code;

    mesh.solid_indices.clear();
    if (mesh.solid_indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.solid_indices.get_error());
    mesh.water_indices.clear();
    if (mesh.water_indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.water_indices.get_error());
    mesh.solid_indices.reserve(mesh.indices.size());
    if (mesh.solid_indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.solid_indices.get_error());
    mesh.water_indices.reserve(mesh.indices.size());
    if (mesh.water_indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.water_indices.get_error());
    index = 0U;
    while (index < mesh.indices.size())
    {
        vertex_index = mesh.indices[index];
        if (vertex_index >= mesh.vertices.size())
            return (FT_ERR_INVALID_ARGUMENT);
        if (mesh.vertices[vertex_index].block_id
            == VOXEL_GENERATOR_WATER_BLOCK)
            destination = &mesh.water_indices;
        else
            destination = &mesh.solid_indices;
        error_code = destination->push_back(vertex_index);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

struct chunk_mesh_primitive
{
    uint32_t base_vertex;
    chunk_mesh_face face;
    uint32_t block_id;
    int32_t anchor_x;
    int32_t anchor_y;
    int32_t anchor_z;
    chunk_mesh_bounds geometry_bounds;
};

static ft_bool chunk_mesh_vectors_ready(const chunk_mesh &mesh) noexcept
{
    if (mesh.vertices.is_initialised() != FT_CLASS_STATE_INITIALISED
        || mesh.indices.is_initialised() != FT_CLASS_STATE_INITIALISED
        || mesh.solid_indices.is_initialised() != FT_CLASS_STATE_INITIALISED
        || mesh.water_indices.is_initialised() != FT_CLASS_STATE_INITIALISED)
        return (FT_FALSE);
    if (mesh.vertices.is_thread_safe() == FT_TRUE
        || mesh.indices.is_thread_safe() == FT_TRUE
        || mesh.solid_indices.is_thread_safe() == FT_TRUE
        || mesh.water_indices.is_thread_safe() == FT_TRUE)
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t chunk_mesh_read_primitive(const chunk_mesh &mesh,
    ft_size_t primitive_offset, chunk_mesh_primitive *primitive) noexcept
{
    ft_size_t vertex_offset;
    ft_size_t vertex_index;
    chunk_mesh_face face;
    uint32_t block_id;
    int32_t coordinate_x;
    int32_t coordinate_y;
    int32_t coordinate_z;

    if (primitive == nullptr || primitive_offset + 5U >= mesh.indices.size())
        return (FT_ERR_INVALID_ARGUMENT);
    primitive->base_vertex = mesh.indices[primitive_offset];
    if ((primitive->base_vertex % 4U) != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (mesh.indices[primitive_offset + 1U] != primitive->base_vertex + 1U
        || mesh.indices[primitive_offset + 2U] != primitive->base_vertex + 2U
        || mesh.indices[primitive_offset + 3U] != primitive->base_vertex
        || mesh.indices[primitive_offset + 4U] != primitive->base_vertex + 2U
        || mesh.indices[primitive_offset + 5U] != primitive->base_vertex + 3U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (static_cast<ft_size_t>(primitive->base_vertex) + 3U
        >= mesh.vertices.size())
        return (FT_ERR_INVALID_ARGUMENT);
    face = static_cast<chunk_mesh_face>(mesh.vertices[primitive->base_vertex]
        .face);
    if (face < CHUNK_MESH_FACE_WEST || face > CHUNK_MESH_FACE_SOUTH)
        return (FT_ERR_INVALID_ARGUMENT);
    block_id = mesh.vertices[primitive->base_vertex].block_id;
    primitive->geometry_bounds.minimum_x = GAME_VOXEL_CHUNK_WIDTH;
    primitive->geometry_bounds.minimum_y = GAME_VOXEL_CHUNK_HEIGHT;
    primitive->geometry_bounds.minimum_z = GAME_VOXEL_CHUNK_DEPTH;
    primitive->geometry_bounds.maximum_x = 0;
    primitive->geometry_bounds.maximum_y = 0;
    primitive->geometry_bounds.maximum_z = 0;
    vertex_offset = 0U;
    while (vertex_offset < 4U)
    {
        vertex_index = static_cast<ft_size_t>(primitive->base_vertex)
            + vertex_offset;
        if (mesh.vertices[vertex_index].face != static_cast<uint8_t>(face)
            || mesh.vertices[vertex_index].block_id != block_id)
            return (FT_ERR_INVALID_ARGUMENT);
        coordinate_x = static_cast<int32_t>(
            mesh.vertices[vertex_index].coordinate_x);
        coordinate_y = static_cast<int32_t>(
            mesh.vertices[vertex_index].coordinate_y);
        coordinate_z = static_cast<int32_t>(
            mesh.vertices[vertex_index].coordinate_z);
        if (coordinate_x < 0 || coordinate_x > GAME_VOXEL_CHUNK_WIDTH
            || coordinate_y < 0 || coordinate_y > GAME_VOXEL_CHUNK_HEIGHT
            || coordinate_z < 0 || coordinate_z > GAME_VOXEL_CHUNK_DEPTH)
            return (FT_ERR_INVALID_ARGUMENT);
        if (coordinate_x < primitive->geometry_bounds.minimum_x)
            primitive->geometry_bounds.minimum_x = coordinate_x;
        if (coordinate_y < primitive->geometry_bounds.minimum_y)
            primitive->geometry_bounds.minimum_y = coordinate_y;
        if (coordinate_z < primitive->geometry_bounds.minimum_z)
            primitive->geometry_bounds.minimum_z = coordinate_z;
        if (coordinate_x > primitive->geometry_bounds.maximum_x)
            primitive->geometry_bounds.maximum_x = coordinate_x;
        if (coordinate_y > primitive->geometry_bounds.maximum_y)
            primitive->geometry_bounds.maximum_y = coordinate_y;
        if (coordinate_z > primitive->geometry_bounds.maximum_z)
            primitive->geometry_bounds.maximum_z = coordinate_z;
        vertex_offset += 1U;
    }
    if (face == CHUNK_MESH_FACE_WEST || face == CHUNK_MESH_FACE_EAST)
    {
        if (primitive->geometry_bounds.minimum_x
            != primitive->geometry_bounds.maximum_x)
            return (FT_ERR_INVALID_ARGUMENT);
    }
    else if (face == CHUNK_MESH_FACE_DOWN || face == CHUNK_MESH_FACE_UP)
    {
        if (primitive->geometry_bounds.minimum_y
            != primitive->geometry_bounds.maximum_y)
            return (FT_ERR_INVALID_ARGUMENT);
    }
    else if (primitive->geometry_bounds.minimum_z
        != primitive->geometry_bounds.maximum_z)
        return (FT_ERR_INVALID_ARGUMENT);
    primitive->face = face;
    primitive->block_id = block_id;
    primitive->anchor_x = primitive->geometry_bounds.minimum_x;
    primitive->anchor_y = primitive->geometry_bounds.minimum_y;
    primitive->anchor_z = primitive->geometry_bounds.minimum_z;
    if (face == CHUNK_MESH_FACE_EAST)
        primitive->anchor_x -= 1;
    else if (face == CHUNK_MESH_FACE_UP)
        primitive->anchor_y -= 1;
    else if (face == CHUNK_MESH_FACE_SOUTH)
        primitive->anchor_z -= 1;
    if (primitive->anchor_x < 0
        || primitive->anchor_x >= GAME_VOXEL_CHUNK_WIDTH
        || primitive->anchor_y < 0
        || primitive->anchor_y >= GAME_VOXEL_CHUNK_HEIGHT
        || primitive->anchor_z < 0
        || primitive->anchor_z >= GAME_VOXEL_CHUNK_DEPTH)
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

static ft_bool chunk_mesh_anchor_in_bounds(
    const chunk_mesh_primitive &primitive,
    const chunk_mesh_bounds &bounds) noexcept
{
    if (primitive.anchor_x < bounds.minimum_x
        || primitive.anchor_x >= bounds.maximum_x
        || primitive.anchor_y < bounds.minimum_y
        || primitive.anchor_y >= bounds.maximum_y
        || primitive.anchor_z < bounds.minimum_z
        || primitive.anchor_z >= bounds.maximum_z)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool chunk_mesh_geometry_intersects_bounds(
    const chunk_mesh_primitive &primitive,
    const chunk_mesh_bounds &bounds) noexcept
{
    if (primitive.geometry_bounds.maximum_x < bounds.minimum_x
        || primitive.geometry_bounds.minimum_x > bounds.maximum_x
        || primitive.geometry_bounds.maximum_y < bounds.minimum_y
        || primitive.geometry_bounds.minimum_y > bounds.maximum_y
        || primitive.geometry_bounds.maximum_z < bounds.minimum_z
        || primitive.geometry_bounds.minimum_z > bounds.maximum_z)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool chunk_mesh_geometry_is_inside_bounds(
    const chunk_mesh_primitive &primitive,
    const chunk_mesh_bounds &bounds) noexcept
{
    if (primitive.geometry_bounds.minimum_x < bounds.minimum_x
        || primitive.geometry_bounds.maximum_x > bounds.maximum_x
        || primitive.geometry_bounds.minimum_y < bounds.minimum_y
        || primitive.geometry_bounds.maximum_y > bounds.maximum_y
        || primitive.geometry_bounds.minimum_z < bounds.minimum_z
        || primitive.geometry_bounds.maximum_z > bounds.maximum_z)
        return (FT_FALSE);
    return (FT_TRUE);
}

static int32_t chunk_mesh_validate_canonical(const chunk_mesh &mesh) noexcept
{
    ft_size_t primitive_offset;
    ft_size_t index_offset;
    ft_size_t solid_index;
    ft_size_t water_index;
    chunk_mesh_primitive primitive;
    uint32_t vertex_index;
    int32_t error_code;

    if ((mesh.indices.size() % 6U) != 0U
        || (mesh.vertices.size() % 4U) != 0U
        || mesh.vertices.size() != (mesh.indices.size() / 6U) * 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    primitive_offset = 0U;
    solid_index = 0U;
    water_index = 0U;
    while (primitive_offset < mesh.indices.size())
    {
        error_code = chunk_mesh_read_primitive(mesh, primitive_offset,
            &primitive);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index_offset = 0U;
        while (index_offset < 6U)
        {
            vertex_index = mesh.indices[primitive_offset + index_offset];
            if (primitive.block_id == VOXEL_GENERATOR_WATER_BLOCK)
            {
                if (water_index >= mesh.water_indices.size()
                    || mesh.water_indices[water_index] != vertex_index)
                    return (FT_ERR_INVALID_ARGUMENT);
                water_index += 1U;
            }
            else
            {
                if (solid_index >= mesh.solid_indices.size()
                    || mesh.solid_indices[solid_index] != vertex_index)
                    return (FT_ERR_INVALID_ARGUMENT);
                solid_index += 1U;
            }
            index_offset += 1U;
        }
        primitive_offset += 6U;
    }
    if (solid_index != mesh.solid_indices.size()
        || water_index != mesh.water_indices.size())
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_append_primitive(chunk_mesh &destination,
    const chunk_mesh &source, const chunk_mesh_primitive &primitive) noexcept
{
    chunk_mesh_vertex vertices[4];
    uint32_t destination_base;
    ft_size_t vertex_offset;
    int32_t error_code;

    destination_base = static_cast<uint32_t>(destination.vertices.size());
    vertex_offset = 0U;
    while (vertex_offset < 4U)
    {
        vertices[vertex_offset] = source.vertices[
            static_cast<ft_size_t>(primitive.base_vertex) + vertex_offset];
        error_code = destination.vertices.push_back(vertices[vertex_offset]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        vertex_offset += 1U;
    }
    chunk_mesh_update_occupied_bounds(destination, vertices);
    error_code = destination.indices.push_back(destination_base);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = destination.indices.push_back(destination_base + 1U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = destination.indices.push_back(destination_base + 2U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = destination.indices.push_back(destination_base);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = destination.indices.push_back(destination_base + 2U);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (destination.indices.push_back(destination_base + 3U));
}

int32_t chunk_mesh_replace_in_bounds(chunk_mesh &mesh,
    const chunk_mesh &replacement, const chunk_mesh_bounds &bounds) noexcept
{
    chunk_mesh merged;
    chunk_mesh_primitive primitive;
    ft_size_t primitive_offset;
    int32_t error_code;

    if (&mesh == &replacement
        || chunk_mesh_bounds_is_valid(bounds) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (chunk_mesh_vectors_ready(mesh) == FT_FALSE
        || chunk_mesh_vectors_ready(replacement) == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    error_code = chunk_mesh_validate_canonical(mesh);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = chunk_mesh_validate_canonical(replacement);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    primitive_offset = 0U;
    while (primitive_offset < mesh.indices.size())
    {
        error_code = chunk_mesh_read_primitive(mesh, primitive_offset,
            &primitive);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (chunk_mesh_anchor_in_bounds(primitive, bounds) == FT_TRUE)
        {
            if (chunk_mesh_geometry_is_inside_bounds(primitive, bounds)
                == FT_FALSE)
                return (FT_ERR_INVALID_ARGUMENT);
        }
        else if (chunk_mesh_geometry_intersects_bounds(primitive, bounds)
            == FT_TRUE)
            return (FT_ERR_INVALID_ARGUMENT);
        primitive_offset += 6U;
    }
    primitive_offset = 0U;
    while (primitive_offset < replacement.indices.size())
    {
        error_code = chunk_mesh_read_primitive(replacement, primitive_offset,
            &primitive);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (chunk_mesh_anchor_in_bounds(primitive, bounds) == FT_FALSE
            || chunk_mesh_geometry_is_inside_bounds(primitive, bounds)
                == FT_FALSE)
            return (FT_ERR_INVALID_ARGUMENT);
        primitive_offset += 6U;
    }
    error_code = chunk_mesh_initialize(merged);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    merged.bounds = mesh.bounds;
    primitive_offset = 0U;
    while (primitive_offset < mesh.indices.size())
    {
        error_code = chunk_mesh_read_primitive(mesh, primitive_offset,
            &primitive);
        if (error_code != FT_ERR_SUCCESS)
            break ;
        if (chunk_mesh_anchor_in_bounds(primitive, bounds) == FT_FALSE)
        {
            error_code = chunk_mesh_append_primitive(merged, mesh, primitive);
            if (error_code != FT_ERR_SUCCESS)
                break ;
        }
        primitive_offset += 6U;
    }
    primitive_offset = 0U;
    while (error_code == FT_ERR_SUCCESS
        && primitive_offset < replacement.indices.size())
    {
        error_code = chunk_mesh_read_primitive(replacement, primitive_offset,
            &primitive);
        if (error_code == FT_ERR_SUCCESS)
            error_code = chunk_mesh_append_primitive(merged, replacement,
                primitive);
        primitive_offset += 6U;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = chunk_mesh_partition_indices(merged);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)chunk_mesh_destroy(merged);
        return (error_code);
    }
    /*
     * The commit is non-allocating under the validated preconditions: the
     * vector destinations are initialised with thread safety disabled, the
     * source vectors are initialised, and both element types are trivially
     * movable. Heap-backed storage transfers ownership; inline storage copies
     * four-byte/vertex values without allocation. All fallible work completed
     * before this ownership commit.
     */
    error_code = mesh.vertices.move(merged.vertices);
    if (error_code == FT_ERR_SUCCESS)
        error_code = mesh.indices.move(merged.indices);
    if (error_code == FT_ERR_SUCCESS)
        error_code = mesh.solid_indices.move(merged.solid_indices);
    if (error_code == FT_ERR_SUCCESS)
        error_code = mesh.water_indices.move(merged.water_indices);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)chunk_mesh_destroy(merged);
        return (error_code);
    }
    mesh.occupied_bounds = merged.occupied_bounds;
    mesh.has_occupied_bounds = merged.has_occupied_bounds;
    (void)chunk_mesh_destroy(merged);
    return (FT_ERR_SUCCESS);
}

int32_t chunk_mesh_generate_from_chunk(chunk_mesh &mesh,
    const game_voxel_chunk &chunk) noexcept
{
    int32_t error_code;

    error_code = chunk_mesh_clear(mesh);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    mesh.vertices.reserve(4096U);
    if (mesh.vertices.get_error() != FT_ERR_SUCCESS)
        return (mesh.vertices.get_error());
    mesh.indices.reserve(6144U);
    if (mesh.indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.indices.get_error());
    chunk_mesh_reset_occupied_bounds(mesh);
    error_code = chunk_mesh_emit_visible_faces(mesh, chunk, nullptr, nullptr,
        nullptr, 0, 0, nullptr);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (chunk_mesh_partition_indices(mesh));
}

int32_t chunk_mesh_generate_from_chunk_with_light(chunk_mesh &mesh,
    const game_voxel_chunk &chunk, const voxel_light_chunk &light) noexcept
{
    int32_t error_code;
    error_code = chunk_mesh_clear(mesh);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    mesh.vertices.reserve(4096U);
    if (mesh.vertices.get_error() != FT_ERR_SUCCESS)
        return (mesh.vertices.get_error());
    mesh.indices.reserve(6144U);
    if (mesh.indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.indices.get_error());
    chunk_mesh_reset_occupied_bounds(mesh);
    error_code = chunk_mesh_emit_visible_faces(mesh, chunk, &light, nullptr,
        nullptr, 0, 0, nullptr);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (chunk_mesh_partition_indices(mesh));
}

int32_t chunk_mesh_generate_from_chunk_in_bounds(chunk_mesh &mesh,
    const game_voxel_chunk &chunk, const chunk_mesh_bounds &bounds) noexcept
{
    int32_t error_code;

    if (chunk_mesh_bounds_is_valid(bounds) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = chunk_mesh_clear(mesh);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    mesh.vertices.reserve(4096U);
    if (mesh.vertices.get_error() != FT_ERR_SUCCESS)
        return (mesh.vertices.get_error());
    mesh.indices.reserve(6144U);
    if (mesh.indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.indices.get_error());
    chunk_mesh_reset_occupied_bounds(mesh);
    error_code = chunk_mesh_emit_visible_faces(mesh, chunk, nullptr, nullptr,
        nullptr, 0, 0, &bounds);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (chunk_mesh_partition_indices(mesh));
}

int32_t chunk_mesh_generate_from_chunk_with_light_in_bounds(
    chunk_mesh &mesh, const game_voxel_chunk &chunk,
    const voxel_light_chunk &light, const chunk_mesh_bounds &bounds) noexcept
{
    int32_t error_code;

    if (chunk_mesh_bounds_is_valid(bounds) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = chunk_mesh_clear(mesh);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    mesh.vertices.reserve(4096U);
    if (mesh.vertices.get_error() != FT_ERR_SUCCESS)
        return (mesh.vertices.get_error());
    mesh.indices.reserve(6144U);
    if (mesh.indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.indices.get_error());
    chunk_mesh_reset_occupied_bounds(mesh);
    error_code = chunk_mesh_emit_visible_faces(mesh, chunk, &light, nullptr,
        nullptr, 0, 0, &bounds);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (chunk_mesh_partition_indices(mesh));
}

int32_t chunk_mesh_apply_light(chunk_mesh &mesh,
    const voxel_light_chunk &light) noexcept
{
    for (size_t index = 0U; index < mesh.vertices.size(); ++index)
    {
        chunk_mesh_vertex &vertex = mesh.vertices[index];
        int32_t x = static_cast<int32_t>(vertex.coordinate_x);
        int32_t y = static_cast<int32_t>(vertex.coordinate_y);
        int32_t z = static_cast<int32_t>(vertex.coordinate_z);
        if (vertex.face == CHUNK_MESH_FACE_WEST)
            --x;
        else if (vertex.face == CHUNK_MESH_FACE_EAST)
            x += 1;
        else if (vertex.face == CHUNK_MESH_FACE_DOWN)
            --y;
        else if (vertex.face == CHUNK_MESH_FACE_UP)
            y += 1;
        else if (vertex.face == CHUNK_MESH_FACE_NORTH)
            --z;
        else if (vertex.face == CHUNK_MESH_FACE_SOUTH)
            z += 1;
        if (x < 0)
            x = 0;
        else if (x >= GAME_VOXEL_CHUNK_WIDTH)
            x = GAME_VOXEL_CHUNK_WIDTH - 1;
        if (y < 0)
            y = 0;
        else if (y >= GAME_VOXEL_CHUNK_HEIGHT)
            y = GAME_VOXEL_CHUNK_HEIGHT - 1;
        if (z < 0)
            z = 0;
        else if (z >= GAME_VOXEL_CHUNK_DEPTH)
            z = GAME_VOXEL_CHUNK_DEPTH - 1;
        vertex.packed_light = light.get(x, y, z);
    }
    return (FT_ERR_SUCCESS);
}

namespace
{
    struct chunk_neighbor_ctx
    {
        const game_voxel_chunk *chunk;
        int32_t chunk_x;
        int32_t chunk_z;
        int32_t (*lookup_block)(void *user_data, int32_t world_x,
            int32_t world_y, int32_t world_z, uint32_t *block_id);
        void *user_data;
        const voxel_light_chunk *light;
        voxel_light_packed_lookup_fn light_lookup;
        void *light_user_data;
        const chunk_mesh_bounds *bounds;
    };
}

static int32_t chunk_mesh_read_or_air_nb(const chunk_neighbor_ctx &ctx,
    int32_t local_x, int32_t local_y, int32_t local_z,
    uint32_t *block_id) noexcept
{
    int32_t world_x;
    int32_t world_z;

    if (local_y < 0 || local_y >= GAME_VOXEL_CHUNK_HEIGHT)
    {
        *block_id = GAME_VOXEL_AIR_BLOCK;
        return (FT_ERR_SUCCESS);
    }
    if (local_x < 0 || local_x >= GAME_VOXEL_CHUNK_WIDTH
        || local_z < 0 || local_z >= GAME_VOXEL_CHUNK_DEPTH)
    {
        world_x = ctx.chunk_x * GAME_VOXEL_CHUNK_WIDTH + local_x;
        world_z = ctx.chunk_z * GAME_VOXEL_CHUNK_DEPTH + local_z;
        return (ctx.lookup_block(ctx.user_data, world_x, local_y,
            world_z, block_id));
    }
    return (chunk_mesh_read_or_air(*ctx.chunk, local_x, local_y,
        local_z, block_id));
}

static int32_t chunk_mesh_face_is_visible_nb(const chunk_neighbor_ctx &ctx,
    int32_t local_x, int32_t local_y, int32_t local_z,
    chunk_mesh_face face, ft_bool *visible) noexcept
{
    uint32_t neighbor_block_id;
    int32_t neighbor_x;
    int32_t neighbor_y;
    int32_t neighbor_z;
    int32_t error_code;

    neighbor_x = local_x;
    neighbor_y = local_y;
    neighbor_z = local_z;
    if (face == CHUNK_MESH_FACE_WEST)
        neighbor_x -= 1;
    if (face == CHUNK_MESH_FACE_EAST)
        neighbor_x += 1;
    if (face == CHUNK_MESH_FACE_DOWN)
        neighbor_y -= 1;
    if (face == CHUNK_MESH_FACE_UP)
        neighbor_y += 1;
    if (face == CHUNK_MESH_FACE_NORTH)
        neighbor_z -= 1;
    if (face == CHUNK_MESH_FACE_SOUTH)
        neighbor_z += 1;
    error_code = chunk_mesh_read_or_air_nb(ctx, neighbor_x, neighbor_y,
        neighbor_z, &neighbor_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (voxel_block_occludes_faces(neighbor_block_id) == FT_FALSE)
        *visible = FT_TRUE;
    else
        *visible = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_fill_visible_face_mask_nb(
    const chunk_neighbor_ctx &ctx, chunk_mesh_face face,
    int32_t axis_value, chunk_mesh_mask_cell mask[4096]) noexcept
{
    int32_t column_count;
    int32_t row_count;
    int32_t column_value;
    int32_t row_value;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;
    uint32_t block_id;
    ft_bool visible;
    int32_t error_code;
    int32_t axis_start;
    int32_t axis_end;
    int32_t column_start;
    int32_t column_end;
    int32_t row_start;
    int32_t row_end;

    chunk_mesh_plane_dimensions(face, &column_count, &row_count);
    chunk_mesh_plane_ranges(face, ctx.bounds, &axis_start, &axis_end,
        &column_start, &column_end, &row_start, &row_end);
    if (axis_value < axis_start || axis_value >= axis_end)
        return (FT_ERR_SUCCESS);
    row_value = row_start;
    while (row_value < row_end)
    {
        column_value = column_start;
        while (column_value < column_end)
        {
            chunk_mesh_block_coordinates_for_plane(face, axis_value,
                column_value, row_value, &local_x, &local_y, &local_z);
            error_code = chunk_mesh_read_or_air(*ctx.chunk, local_x, local_y,
                local_z, &block_id);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            mask[(row_value * column_count) + column_value].block_id = 0U;
            mask[(row_value * column_count) + column_value].packed_light = 0U;
            if (block_id != GAME_VOXEL_AIR_BLOCK)
            {
                if (voxel_block_is_liquid(block_id) == FT_FALSE
                    || face == CHUNK_MESH_FACE_UP)
                {
                    error_code = chunk_mesh_face_is_visible_nb(ctx, local_x,
                        local_y, local_z, face, &visible);
                    if (error_code != FT_ERR_SUCCESS)
                        return (error_code);
                    if (visible == FT_TRUE
                        && voxel_block_is_liquid(block_id) == FT_TRUE)
                    {
                        uint32_t above_id = 0U;
                        (void)chunk_mesh_read_or_air(*ctx.chunk, local_x,
                            local_y + 1, local_z, &above_id);
                        if (voxel_block_is_liquid(above_id) == FT_TRUE)
                            visible = FT_FALSE;
                    }
                    if (visible == FT_TRUE)
                    {
                        mask[(row_value * column_count) + column_value].block_id = block_id;
                        error_code = chunk_mesh_face_light(*ctx.chunk,
                            ctx.light, ctx.light_lookup, ctx.light_user_data,
                            ctx.chunk_x, ctx.chunk_z, local_x, local_y,
                            local_z, face,
                            &mask[(row_value * column_count)
                                + column_value].packed_light);
                        if (error_code != FT_ERR_SUCCESS)
                            return (error_code);
                    }
                }
            }
            column_value += 1;
        }
        row_value += 1;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_emit_greedy_faces_nb(chunk_mesh &mesh,
    const chunk_neighbor_ctx &ctx, chunk_mesh_face face) noexcept
{
    chunk_mesh_mask_cell mask[4096];
    ft_bool consumed[4096];
    int32_t axis_value;
    int32_t error_code;
    int32_t axis_start;
    int32_t axis_end;
    int32_t column_start;
    int32_t column_end;
    int32_t row_start;
    int32_t row_end;

    chunk_mesh_plane_ranges(face, ctx.bounds, &axis_start, &axis_end,
        &column_start, &column_end, &row_start, &row_end);
    (void)column_start;
    (void)column_end;
    (void)row_start;
    (void)row_end;
    axis_value = axis_start;
    while (axis_value < axis_end)
    {
        error_code = chunk_mesh_fill_visible_face_mask_nb(ctx, face,
            axis_value, mask);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = chunk_mesh_emit_greedy_mask(mesh, mask, consumed, face,
            axis_value, ctx.bounds);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        axis_value += 1;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_emit_visible_faces_nb(chunk_mesh &mesh,
    const chunk_neighbor_ctx &ctx) noexcept
{
    chunk_mesh_face face;
    int32_t error_code;

    face = CHUNK_MESH_FACE_WEST;
    while (face <= CHUNK_MESH_FACE_SOUTH)
    {
        error_code = chunk_mesh_emit_greedy_faces_nb(mesh, ctx, face);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        face = static_cast<chunk_mesh_face>(static_cast<int32_t>(face) + 1);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t chunk_mesh_generate_from_chunk_with_neighbors_and_light_lookup_internal(
    chunk_mesh &mesh,
    const game_voxel_chunk &chunk, int32_t chunk_x, int32_t chunk_z,
    int32_t (*lookup_block)(void *user_data, int32_t world_x, int32_t world_y,
        int32_t world_z, uint32_t *block_id),
    void *user_data, const voxel_light_chunk *light,
    voxel_light_packed_lookup_fn light_lookup, void *light_user_data,
    const chunk_mesh_bounds *bounds) noexcept
{
    chunk_neighbor_ctx ctx;
    int32_t error_code;

    error_code = chunk_mesh_clear(mesh);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    mesh.vertices.reserve(4096U);
    if (mesh.vertices.get_error() != FT_ERR_SUCCESS)
        return (mesh.vertices.get_error());
    mesh.indices.reserve(6144U);
    if (mesh.indices.get_error() != FT_ERR_SUCCESS)
        return (mesh.indices.get_error());
    chunk_mesh_reset_occupied_bounds(mesh);
    ctx.chunk = &chunk;
    ctx.chunk_x = chunk_x;
    ctx.chunk_z = chunk_z;
    ctx.lookup_block = lookup_block;
    ctx.user_data = user_data;
    ctx.light = light;
    ctx.light_lookup = light_lookup;
    ctx.light_user_data = light_user_data;
    ctx.bounds = bounds;
    error_code = chunk_mesh_emit_visible_faces_nb(mesh, ctx);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (chunk_mesh_partition_indices(mesh));
}

int32_t chunk_mesh_generate_from_chunk_with_neighbors_and_light_lookup(
    chunk_mesh &mesh, const game_voxel_chunk &chunk, int32_t chunk_x,
    int32_t chunk_z,
    int32_t (*lookup_block)(void *user_data, int32_t world_x, int32_t world_y,
        int32_t world_z, uint32_t *block_id),
    void *user_data, const voxel_light_chunk *light,
    voxel_light_packed_lookup_fn light_lookup, void *light_user_data) noexcept
{
    return (chunk_mesh_generate_from_chunk_with_neighbors_and_light_lookup_internal(
        mesh, chunk, chunk_x, chunk_z, lookup_block, user_data, light,
        light_lookup, light_user_data, nullptr));
}

int32_t chunk_mesh_generate_from_chunk_with_neighbors_and_light_lookup_in_bounds(
    chunk_mesh &mesh, const game_voxel_chunk &chunk, int32_t chunk_x,
    int32_t chunk_z,
    int32_t (*lookup_block)(void *user_data, int32_t world_x, int32_t world_y,
        int32_t world_z, uint32_t *block_id),
    void *user_data, const voxel_light_chunk *light,
    voxel_light_packed_lookup_fn light_lookup, void *light_user_data,
    const chunk_mesh_bounds &bounds) noexcept
{
    if (chunk_mesh_bounds_is_valid(bounds) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    return (chunk_mesh_generate_from_chunk_with_neighbors_and_light_lookup_internal(
        mesh, chunk, chunk_x, chunk_z, lookup_block, user_data, light,
        light_lookup, light_user_data, &bounds));
}

int32_t chunk_mesh_generate_from_chunk_with_neighbors(chunk_mesh &mesh,
    const game_voxel_chunk &chunk, int32_t chunk_x, int32_t chunk_z,
    int32_t (*lookup_block)(void *user_data, int32_t world_x, int32_t world_y,
        int32_t world_z, uint32_t *block_id),
    void *user_data, const voxel_light_chunk *light) noexcept
{
    return (chunk_mesh_generate_from_chunk_with_neighbors_and_light_lookup(
        mesh, chunk, chunk_x, chunk_z, lookup_block, user_data, light,
        nullptr, nullptr));
}

int32_t chunk_mesh_generate_from_chunk_with_neighbors_in_bounds(
    chunk_mesh &mesh, const game_voxel_chunk &chunk, int32_t chunk_x,
    int32_t chunk_z,
    int32_t (*lookup_block)(void *user_data, int32_t world_x, int32_t world_y,
        int32_t world_z, uint32_t *block_id),
    void *user_data, const voxel_light_chunk *light,
    const chunk_mesh_bounds &bounds) noexcept
{
    return (chunk_mesh_generate_from_chunk_with_neighbors_and_light_lookup_in_bounds(
        mesh, chunk, chunk_x, chunk_z, lookup_block, user_data, light,
        nullptr, nullptr, bounds));
}

#endif
