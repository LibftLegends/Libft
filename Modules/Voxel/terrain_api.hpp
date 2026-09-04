#ifndef TERRAIN_API_HPP
# define TERRAIN_API_HPP

#ifdef GAME_USE_VOXEL_REGION_BACKEND

# include "terrain_generation.hpp"
# include "../Buffer/byte_buffer.hpp"

class game_voxel_region;

int32_t terrain_default_generation_config(
    terrain_generation_config &config) noexcept;
int32_t terrain_generation_config_add_tree_template(
    terrain_generation_config &config,
    const terrain_tree_template &tree_template,
    uint32_t *template_index_out) noexcept;
int32_t terrain_generation_config_remove_tree_template(
    terrain_generation_config &config, uint32_t template_index) noexcept;
int32_t terrain_generation_config_clear_tree_templates(
    terrain_generation_config &config) noexcept;
int32_t terrain_generation_config_assign_tree_template_to_biome(
    terrain_generation_config &config, uint32_t biome_index,
    uint32_t template_index) noexcept;
int32_t terrain_generation_config_remove_tree_template_from_biome(
    terrain_generation_config &config, uint32_t biome_index,
    uint32_t template_index) noexcept;
const terrain_tree_template *terrain_generation_config_get_tree_template(
    const terrain_generation_config &config, uint32_t template_index) noexcept;
uint32_t terrain_generation_config_signature(
    const terrain_generation_config &config) noexcept;
ft_bool terrain_generation_config_is_valid(
    const terrain_generation_config &config) noexcept;
int32_t terrain_generation_context_initialize(
    terrain_generation_context &context,
    const terrain_generation_config &config) noexcept;
ft_bool terrain_generation_context_is_initialised(
    const terrain_generation_context &context) noexcept;
int32_t terrain_generation_config_serialize(
    const terrain_generation_config &config, ft_byte_buffer &buffer) noexcept;
int32_t terrain_generation_config_deserialize(
    terrain_generation_config &config, ft_byte_buffer &buffer) noexcept;
int32_t terrain_generation_config_save_file(const char *file_path,
    const terrain_generation_config &config) noexcept;
int32_t terrain_generation_config_load_file(const char *file_path,
    terrain_generation_config &config) noexcept;
uint32_t terrain_select_biome(const terrain_generation_config &config,
    uint64_t seed_value, int32_t world_block_x, int32_t world_block_z) noexcept;
int32_t terrain_get_biome_zone_width(
    const terrain_generation_config &config, uint64_t seed_value) noexcept;
int32_t terrain_get_biome_zone_width_for_biome(
    const terrain_generation_config &config, uint64_t seed_value,
    uint32_t biome_index) noexcept;
uint32_t terrain_get_biome_index(const terrain_generation_config &config,
    int32_t world_block_x, int32_t world_block_z,
    const char *seed_string = ft_nullptr) noexcept;

terrain_biome terrain_get_biome(int32_t world_block_x, int32_t world_block_z,
    const char *seed_string = ft_nullptr) noexcept;
terrain_biome_profile terrain_get_biome_profile(terrain_biome biome) noexcept;

const terrain_block_metadata &terrain_get_block_metadata(
    uint32_t block_id) noexcept;
ft_bool terrain_block_is_known(uint32_t block_id) noexcept;
ft_bool terrain_block_is_solid(uint32_t block_id) noexcept;
ft_bool terrain_block_is_transparent(uint32_t block_id) noexcept;
ft_bool terrain_block_is_liquid(uint32_t block_id) noexcept;
ft_bool terrain_block_is_replaceable(uint32_t block_id) noexcept;
ft_bool terrain_block_can_host_ore(uint32_t block_id) noexcept;
ft_bool terrain_block_is_ore(uint32_t block_id) noexcept;
ft_bool terrain_block_emits_light(uint32_t block_id) noexcept;
uint8_t terrain_block_emitted_light_level(uint32_t block_id) noexcept;
uint8_t terrain_block_light_attenuation(uint32_t block_id) noexcept;
ft_bool terrain_block_occludes_faces(uint32_t block_id) noexcept;
uint32_t terrain_block_hardness(uint32_t block_id) noexcept;
ft_bool terrain_block_is_breakable(uint32_t block_id) noexcept;
int32_t terrain_register_block(const terrain_block_registration &registration,
    uint32_t *block_id_out) noexcept;
const char *terrain_get_block_name(uint32_t block_id) noexcept;
int32_t terrain_find_block_id_by_name(const char *name,
    uint32_t *block_id_out) noexcept;
const char *terrain_get_block_asset_path(uint32_t block_id,
    terrain_block_asset_face face) noexcept;
const uint8_t *terrain_get_block_asset_data(uint32_t block_id,
    terrain_block_asset_face face, ft_size_t *size_out) noexcept;

uint32_t terrain_surface_block_for_biome(terrain_biome biome) noexcept;
uint32_t terrain_subsurface_block_for_biome(terrain_biome biome) noexcept;
uint32_t terrain_deep_block_for_biome(terrain_biome biome) noexcept;
ft_bool terrain_biome_has_shrubs(terrain_biome biome) noexcept;
ft_bool terrain_biome_has_trees(terrain_biome biome) noexcept;
const terrain_tree_template &terrain_small_oak_tree_template(
    uint32_t variant_index = 0U) noexcept;
const terrain_tree_template &terrain_small_pine_tree_template(
    uint32_t variant_index = 0U) noexcept;
const terrain_tree_template &terrain_small_cactus_tree_template(
    uint32_t variant_index = 0U) noexcept;
const terrain_tree_template &terrain_large_oak_tree_template(
    uint32_t variant_index = 0U) noexcept;
const terrain_tree_template &terrain_large_pine_tree_template(
    uint32_t variant_index = 0U) noexcept;
const terrain_tree_template &terrain_small_oak_tree_template_variant(
    uint32_t variant_index) noexcept;
const terrain_tree_template &terrain_small_pine_tree_template_variant(
    uint32_t variant_index) noexcept;
const terrain_tree_template &terrain_small_cactus_tree_template_variant(
    uint32_t variant_index) noexcept;
const terrain_tree_template &terrain_large_oak_tree_template_variant(
    uint32_t variant_index) noexcept;
const terrain_tree_template &terrain_large_pine_tree_template_variant(
    uint32_t variant_index) noexcept;
const terrain_tree_template &terrain_tree_template_for_biome(
    terrain_biome biome) noexcept;
const terrain_tree_template &terrain_tree_template_for_biome(
    terrain_biome biome, uint64_t seed_value) noexcept;
ft_bool terrain_can_place_tree_template(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    const terrain_tree_template &tree_template) noexcept;
int32_t terrain_place_tree_template(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    const terrain_tree_template &tree_template) noexcept;

int32_t terrain_generate_chunk(game_voxel_chunk &chunk,
    const char *seed_string = ft_nullptr) noexcept;
int32_t terrain_generate_chunk(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string = ft_nullptr) noexcept;
int32_t terrain_generate_chunk(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const terrain_generation_config &config) noexcept;
int32_t terrain_generate_chunk_with_context(game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string,
    const terrain_generation_context &context) noexcept;
int32_t terrain_generate_chunk_with_stage_mask(
    game_voxel_chunk &chunk, int32_t world_block_origin_x,
    int32_t world_block_origin_z, const char *seed_string,
    const terrain_generation_config &config, uint32_t stage_mask) noexcept;
int32_t terrain_generate_chunk_at_world_coordinate(game_voxel_chunk &chunk,
    const terrain_world_chunk_coordinate &coordinate,
    const char *seed_string, const terrain_generation_config &config) noexcept;
int32_t terrain_generate_chunk_in_region(game_voxel_region &region,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, const terrain_generation_config &config) noexcept;
int32_t terrain_generate_chunk_in_region_with_context(
    game_voxel_region &region, int32_t world_block_origin_x,
    int32_t world_block_origin_z, const char *seed_string,
    const terrain_generation_context &context) noexcept;

#endif

#endif
