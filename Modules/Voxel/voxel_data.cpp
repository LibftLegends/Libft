#include <stdint.h>
#include "../Basic/class_nullptr.hpp"
#include "voxel_api.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../RNG/rng.hpp"
#include "../Game/game_voxel_chunk.hpp"
#include "voxel_internal.hpp"
#include "voxel_block_registry.hpp"

static int32_t voxel_apply_default_generation_config(
    voxel_generation_config &config) noexcept;
static ft_bool voxel_template_is_valid(
    const voxel_tree_template *tree_template) noexcept;

static int32_t voxel_copy_tree_template(
    const voxel_tree_template &source,
    voxel_tree_template_block *destination_blocks,
    voxel_tree_template *destination) noexcept
{
    if (destination_blocks == ft_nullptr || destination == ft_nullptr
        || source.block_count > VOXEL_MAX_TREE_TEMPLATE_BLOCKS
        || (source.block_count != 0U && source.blocks == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (source.block_count != 0U)
        ft_memcpy(destination_blocks, source.blocks,
            sizeof(voxel_tree_template_block) * source.block_count);
    destination->blocks = destination_blocks;
    destination->block_count = source.block_count;
    return (FT_ERR_SUCCESS);
}

voxel_biome_definition::voxel_biome_definition() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), profile(),
      surface_block_id(0U), subsurface_block_id(0U), deep_block_id(0U),
      allow_shrubs(FT_FALSE), allow_trees(FT_FALSE),
      allow_snow_caps(FT_FALSE), allow_mountain_ridges(FT_FALSE),
      shrub_chance_percent(0U), tree_chance_percent(0U),
      tree_template_count(0U), tree_template_indices(), tree_template(ft_nullptr)
{
    return ;
}

voxel_biome_definition::~voxel_biome_definition() noexcept
{
    this->destroy();
    return ;
}

int32_t voxel_biome_definition::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    this->profile.surface_height = 0;
    this->profile.height_variation = 0;
    this->profile.topsoil_depth = 0;
    this->surface_block_id = 0U;
    this->subsurface_block_id = 0U;
    this->deep_block_id = 0U;
    this->allow_shrubs = FT_FALSE;
    this->allow_trees = FT_FALSE;
    this->allow_snow_caps = FT_FALSE;
    this->allow_mountain_ridges = FT_FALSE;
    this->shrub_chance_percent = 0U;
    this->tree_chance_percent = 0U;
    this->tree_template_count = 0U;
    ft_memset(this->tree_template_indices, 0,
        sizeof(this->tree_template_indices));
    this->tree_template = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_biome_definition::initialize(
    const voxel_biome_definition &other) noexcept
{
    uint32_t index;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    this->profile = other.profile;
    this->surface_block_id = other.surface_block_id;
    this->subsurface_block_id = other.subsurface_block_id;
    this->deep_block_id = other.deep_block_id;
    this->allow_shrubs = other.allow_shrubs;
    this->allow_trees = other.allow_trees;
    this->allow_snow_caps = other.allow_snow_caps;
    this->allow_mountain_ridges = other.allow_mountain_ridges;
    this->shrub_chance_percent = other.shrub_chance_percent;
    this->tree_chance_percent = other.tree_chance_percent;
    this->tree_template_count = other.tree_template_count;
    index = 0U;
    while (index < VOXEL_MAX_BIOME_TREE_TEMPLATES)
    {
        this->tree_template_indices[index]
            = other.tree_template_indices[index];
        index += 1U;
    }
    this->tree_template = other.tree_template;
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_biome_definition::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->profile.surface_height = 0;
    this->profile.height_variation = 0;
    this->profile.topsoil_depth = 0;
    this->surface_block_id = 0U;
    this->subsurface_block_id = 0U;
    this->deep_block_id = 0U;
    this->allow_shrubs = FT_FALSE;
    this->allow_trees = FT_FALSE;
    this->allow_snow_caps = FT_FALSE;
    this->allow_mountain_ridges = FT_FALSE;
    this->shrub_chance_percent = 0U;
    this->tree_chance_percent = 0U;
    this->tree_template_count = 0U;
    ft_memset(this->tree_template_indices, 0,
        sizeof(this->tree_template_indices));
    this->tree_template = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_biome_definition::move(
    voxel_biome_definition &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_biome_definition::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t voxel_biome_definition::set_profile(
    const voxel_biome_profile &value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (value.height_variation < 0 || value.topsoil_depth < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->profile = value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_biome_definition::set_block_palette(uint32_t surface_block,
    uint32_t subsurface_block, uint32_t deep_block) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->surface_block_id = surface_block;
    this->subsurface_block_id = subsurface_block;
    this->deep_block_id = deep_block;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_biome_definition::set_decoration_policy(ft_bool shrubs,
    ft_bool trees, uint32_t shrub_chance, uint32_t tree_chance) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (shrub_chance > 100U || tree_chance > 100U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->allow_shrubs = shrubs;
    this->allow_trees = trees;
    this->shrub_chance_percent = shrub_chance;
    this->tree_chance_percent = tree_chance;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_biome_definition::set_snow_cap_policy(
    ft_bool enabled) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->allow_snow_caps = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_biome_definition::set_mountain_ridge_policy(
    ft_bool enabled) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->allow_mountain_ridges = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_biome_definition::set_tree_template_override(
    const voxel_tree_template *value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->tree_template = value;
    return (FT_ERR_SUCCESS);
}

voxel_feature_rule::voxel_feature_rule() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      template_data(ft_nullptr), biome_index(-1), chance_percent(0U),
      minimum_height(0), maximum_height(0), requires_dry_land(FT_FALSE)
{
    return ;
}

voxel_feature_rule::~voxel_feature_rule() noexcept
{
    this->destroy();
    return ;
}

int32_t voxel_feature_rule::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    this->template_data = ft_nullptr;
    this->biome_index = -1;
    this->chance_percent = 0U;
    this->minimum_height = 0;
    this->maximum_height = 0;
    this->requires_dry_land = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_feature_rule::initialize(
    const voxel_feature_rule &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    this->template_data = other.template_data;
    this->biome_index = other.biome_index;
    this->chance_percent = other.chance_percent;
    this->minimum_height = other.minimum_height;
    this->maximum_height = other.maximum_height;
    this->requires_dry_land = other.requires_dry_land;
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_feature_rule::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->template_data = ft_nullptr;
    this->biome_index = -1;
    this->chance_percent = 0U;
    this->minimum_height = 0;
    this->maximum_height = 0;
    this->requires_dry_land = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_feature_rule::move(voxel_feature_rule &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_feature_rule::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t voxel_feature_rule::set_template(
    const voxel_tree_template *value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->template_data = value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_feature_rule::set_biome_range(int32_t biome,
    int32_t minimum, int32_t maximum) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (biome < -1 || minimum > maximum)
        return (FT_ERR_INVALID_ARGUMENT);
    this->biome_index = biome;
    this->minimum_height = minimum;
    this->maximum_height = maximum;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_feature_rule::set_chance(uint32_t value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (value > 100U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->chance_percent = value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_feature_rule::set_requires_dry_land(ft_bool value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->requires_dry_land = value;
    return (FT_ERR_SUCCESS);
}

voxel_ore_rule::voxel_ore_rule() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), block_id(0U),
      minimum_height(0), maximum_height(0), minimum_depth(0),
      maximum_depth(0), vein_size(0U), vein_size_min(0U), vein_size_max(0U),
      veins_per_chunk_min(0U), veins_per_chunk_max(0U),
      chance_percent(0U), allow_ore_replacement(FT_FALSE), enabled(FT_FALSE)
{
    return ;
}

voxel_ore_rule::~voxel_ore_rule() noexcept
{
    this->destroy();
    return ;
}

int32_t voxel_ore_rule::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    this->block_id = 0U;
    this->minimum_height = 0;
    this->maximum_height = 0;
    this->minimum_depth = 0;
    this->maximum_depth = 0;
    this->vein_size = 0U;
    this->vein_size_min = 0U;
    this->vein_size_max = 0U;
    this->veins_per_chunk_min = 0U;
    this->veins_per_chunk_max = 0U;
    this->chance_percent = 0U;
    this->allow_ore_replacement = FT_FALSE;
    this->enabled = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_ore_rule::initialize(const voxel_ore_rule &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    this->block_id = other.block_id;
    this->minimum_height = other.minimum_height;
    this->maximum_height = other.maximum_height;
    this->minimum_depth = other.minimum_depth;
    this->maximum_depth = other.maximum_depth;
    this->vein_size = other.vein_size;
    this->vein_size_min = other.vein_size_min;
    this->vein_size_max = other.vein_size_max;
    this->veins_per_chunk_min = other.veins_per_chunk_min;
    this->veins_per_chunk_max = other.veins_per_chunk_max;
    this->chance_percent = other.chance_percent;
    this->allow_ore_replacement = other.allow_ore_replacement;
    this->enabled = other.enabled;
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_ore_rule::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->block_id = 0U;
    this->minimum_height = 0;
    this->maximum_height = 0;
    this->minimum_depth = 0;
    this->maximum_depth = 0;
    this->vein_size = 0U;
    this->vein_size_min = 0U;
    this->vein_size_max = 0U;
    this->veins_per_chunk_min = 0U;
    this->veins_per_chunk_max = 0U;
    this->chance_percent = 0U;
    this->allow_ore_replacement = FT_FALSE;
    this->enabled = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_ore_rule::move(voxel_ore_rule &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_ore_rule::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t voxel_ore_rule::set_range(int32_t minimum,
    int32_t maximum) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (minimum > maximum)
        return (FT_ERR_INVALID_ARGUMENT);
    this->minimum_height = minimum;
    this->maximum_height = maximum;
    this->minimum_depth = minimum;
    this->maximum_depth = maximum;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_ore_rule::set_depth_range(int32_t minimum_depth_value,
    int32_t maximum_depth_value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (minimum_depth_value < 1
        || minimum_depth_value > maximum_depth_value)
        return (FT_ERR_INVALID_ARGUMENT);
    this->minimum_depth = minimum_depth_value;
    this->maximum_depth = maximum_depth_value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_ore_rule::set_vein(uint32_t size,
    uint32_t chance) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (chance > 100U || (size == 0U && chance != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    this->vein_size = size;
    this->vein_size_min = size;
    this->vein_size_max = size;
    this->chance_percent = chance;
    if (this->veins_per_chunk_min == 0U
        && this->veins_per_chunk_max == 0U)
    {
        this->veins_per_chunk_min = 1U;
        this->veins_per_chunk_max = 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t voxel_ore_rule::set_vein_size_range(uint32_t minimum_size,
    uint32_t maximum_size) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (minimum_size == 0U || minimum_size > maximum_size)
        return (FT_ERR_INVALID_ARGUMENT);
    this->vein_size_min = minimum_size;
    this->vein_size_max = maximum_size;
    this->vein_size = minimum_size;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_ore_rule::set_frequency_range(uint32_t minimum_veins,
    uint32_t maximum_veins) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (minimum_veins > maximum_veins)
        return (FT_ERR_INVALID_ARGUMENT);
    this->veins_per_chunk_min = minimum_veins;
    this->veins_per_chunk_max = maximum_veins;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_ore_rule::set_enabled(ft_bool value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->enabled = value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_ore_rule::set_ore_replacement(ft_bool value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (value > FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    this->allow_ore_replacement = value;
    return (FT_ERR_SUCCESS);
}

voxel_underground_structure_config::voxel_underground_structure_config()
    noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      enable_ravines(FT_FALSE), enable_cave_rooms(FT_FALSE),
      ravine_chance_percent(0U), cave_room_chance_percent(0U),
      minimum_height(0), maximum_height(0), ravine_width(0U), ravine_depth(0U),
      cave_small_radius(0U), cave_large_radius(0U),
      cave_large_chance_percent(0U), cave_entrance_chance_percent(0U),
      cave_entrance_radius(0U), enable_cavern_rooms(FT_FALSE),
      cavern_room_chance_percent(0U), cavern_room_radius(0U)
{
    return ;
}

voxel_underground_structure_config::~voxel_underground_structure_config()
    noexcept
{
    this->destroy();
    return ;
}

int32_t voxel_underground_structure_config::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    this->enable_ravines = FT_FALSE;
    this->enable_cave_rooms = FT_FALSE;
    this->ravine_chance_percent = 0U;
    this->cave_room_chance_percent = 0U;
    this->minimum_height = 0;
    this->maximum_height = 0;
    this->ravine_width = 0U;
    this->ravine_depth = 0U;
    this->cave_small_radius = 0U;
    this->cave_large_radius = 0U;
    this->cave_large_chance_percent = 0U;
    this->cave_entrance_chance_percent = 0U;
    this->cave_entrance_radius = 0U;
    this->enable_cavern_rooms = FT_FALSE;
    this->cavern_room_chance_percent = 0U;
    this->cavern_room_radius = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_underground_structure_config::initialize(
    const voxel_underground_structure_config &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    this->enable_ravines = other.enable_ravines;
    this->enable_cave_rooms = other.enable_cave_rooms;
    this->ravine_chance_percent = other.ravine_chance_percent;
    this->cave_room_chance_percent = other.cave_room_chance_percent;
    this->minimum_height = other.minimum_height;
    this->maximum_height = other.maximum_height;
    this->ravine_width = other.ravine_width;
    this->ravine_depth = other.ravine_depth;
    this->cave_small_radius = other.cave_small_radius;
    this->cave_large_radius = other.cave_large_radius;
    this->cave_large_chance_percent = other.cave_large_chance_percent;
    this->cave_entrance_chance_percent = other.cave_entrance_chance_percent;
    this->cave_entrance_radius = other.cave_entrance_radius;
    this->enable_cavern_rooms = other.enable_cavern_rooms;
    this->cavern_room_chance_percent = other.cavern_room_chance_percent;
    this->cavern_room_radius = other.cavern_room_radius;
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_underground_structure_config::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->enable_ravines = FT_FALSE;
    this->enable_cave_rooms = FT_FALSE;
    this->ravine_chance_percent = 0U;
    this->cave_room_chance_percent = 0U;
    this->minimum_height = 0;
    this->maximum_height = 0;
    this->ravine_width = 0U;
    this->ravine_depth = 0U;
    this->cave_small_radius = 0U;
    this->cave_large_radius = 0U;
    this->cave_large_chance_percent = 0U;
    this->cave_entrance_chance_percent = 0U;
    this->cave_entrance_radius = 0U;
    this->enable_cavern_rooms = FT_FALSE;
    this->cavern_room_chance_percent = 0U;
    this->cavern_room_radius = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_underground_structure_config::move(
    voxel_underground_structure_config &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_underground_structure_config::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t voxel_underground_structure_config::set_enabled(
    ft_bool ravines, ft_bool cave_rooms) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_ravines = ravines;
    this->enable_cave_rooms = cave_rooms;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_underground_structure_config::set_chances(uint32_t ravine,
    uint32_t cave_room) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (ravine > 100U || cave_room > 100U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->ravine_chance_percent = ravine;
    this->cave_room_chance_percent = cave_room;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_underground_structure_config::set_height_range(
    int32_t minimum, int32_t maximum) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (minimum > maximum)
        return (FT_ERR_INVALID_ARGUMENT);
    this->minimum_height = minimum;
    this->maximum_height = maximum;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_underground_structure_config::set_shape(uint32_t width,
    uint32_t depth) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->ravine_width = width;
    this->ravine_depth = depth;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_underground_structure_config::set_cave_shape(
    uint32_t small_radius, uint32_t large_radius,
    uint32_t large_chance) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (small_radius == 0U || large_radius < small_radius
        || large_radius > 16U || large_chance > 100U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->cave_small_radius = small_radius;
    this->cave_large_radius = large_radius;
    this->cave_large_chance_percent = large_chance;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_underground_structure_config::set_cave_entrances(
    uint32_t chance, uint32_t radius) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (chance > 100U || radius == 0U || radius > 8U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->cave_entrance_chance_percent = chance;
    this->cave_entrance_radius = radius;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_underground_structure_config::set_cavern_rooms(
    ft_bool enabled, uint32_t chance, uint32_t radius) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (enabled == FT_FALSE)
    {
        this->enable_cavern_rooms = FT_FALSE;
        this->cavern_room_chance_percent = 0U;
        this->cavern_room_radius = 0U;
        return (FT_ERR_SUCCESS);
    }
    if (chance > 100U || radius < 5U || radius > 32U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->enable_cavern_rooms = enabled;
    this->cavern_room_chance_percent = chance;
    this->cavern_room_radius = radius;
    return (FT_ERR_SUCCESS);
}

voxel_fluid_config::voxel_fluid_config() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      enable_rivers(FT_FALSE), enable_lakes(FT_FALSE),
      enable_underground_lakes(FT_FALSE), river_noise_scale(0),
      river_width(0), lake_noise_scale(0), lake_chance_percent(0U),
      underground_lake_chance_percent(0U), underground_lake_minimum_y(0),
      underground_lake_maximum_y(0), underground_lake_depth(0U),
      underground_lake_floor_thickness(0U), underground_lake_roof_thickness(0U)
{
    return ;
}

voxel_fluid_config::~voxel_fluid_config() noexcept
{
    this->destroy();
    return ;
}

int32_t voxel_fluid_config::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    this->enable_rivers = FT_FALSE;
    this->enable_lakes = FT_FALSE;
    this->enable_underground_lakes = FT_FALSE;
    this->river_noise_scale = 0;
    this->river_width = 0;
    this->lake_noise_scale = 0;
    this->lake_chance_percent = 0U;
    this->underground_lake_chance_percent = 0U;
    this->underground_lake_minimum_y = 0;
    this->underground_lake_maximum_y = 0;
    this->underground_lake_depth = 0U;
    this->underground_lake_floor_thickness = 0U;
    this->underground_lake_roof_thickness = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_fluid_config::initialize(
    const voxel_fluid_config &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    this->enable_rivers = other.enable_rivers;
    this->enable_lakes = other.enable_lakes;
    this->enable_underground_lakes = other.enable_underground_lakes;
    this->river_noise_scale = other.river_noise_scale;
    this->river_width = other.river_width;
    this->lake_noise_scale = other.lake_noise_scale;
    this->lake_chance_percent = other.lake_chance_percent;
    this->underground_lake_chance_percent = other.underground_lake_chance_percent;
    this->underground_lake_minimum_y = other.underground_lake_minimum_y;
    this->underground_lake_maximum_y = other.underground_lake_maximum_y;
    this->underground_lake_depth = other.underground_lake_depth;
    this->underground_lake_floor_thickness = other.underground_lake_floor_thickness;
    this->underground_lake_roof_thickness = other.underground_lake_roof_thickness;
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_fluid_config::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->enable_rivers = FT_FALSE;
    this->enable_lakes = FT_FALSE;
    this->enable_underground_lakes = FT_FALSE;
    this->river_noise_scale = 0;
    this->river_width = 0;
    this->lake_noise_scale = 0;
    this->lake_chance_percent = 0U;
    this->underground_lake_chance_percent = 0U;
    this->underground_lake_minimum_y = 0;
    this->underground_lake_maximum_y = 0;
    this->underground_lake_depth = 0U;
    this->underground_lake_floor_thickness = 0U;
    this->underground_lake_roof_thickness = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_fluid_config::move(voxel_fluid_config &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_fluid_config::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t voxel_fluid_config::set_enabled(ft_bool rivers,
    ft_bool lakes) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_rivers = rivers;
    this->enable_lakes = lakes;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_fluid_config::set_underground_lakes_enabled(ft_bool enabled) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_underground_lakes = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_fluid_config::set_river_settings(int32_t scale,
    int32_t width) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (scale <= 0 || width < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->river_noise_scale = scale;
    this->river_width = width;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_fluid_config::set_lake_settings(int32_t scale,
    uint32_t chance) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (scale <= 0 || chance > 100U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->lake_noise_scale = scale;
    this->lake_chance_percent = chance;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_fluid_config::set_underground_lake_settings(uint32_t chance,
    int32_t minimum_y, int32_t maximum_y, uint32_t depth,
    uint32_t floor_thickness, uint32_t roof_thickness) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (chance > 100U || minimum_y < 1 || maximum_y < minimum_y
        || maximum_y >= GAME_VOXEL_CHUNK_HEIGHT || depth == 0U
        || depth > 8U || floor_thickness == 0U || roof_thickness == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->underground_lake_chance_percent = chance;
    this->underground_lake_minimum_y = minimum_y;
    this->underground_lake_maximum_y = maximum_y;
    this->underground_lake_depth = depth;
    this->underground_lake_floor_thickness = floor_thickness;
    this->underground_lake_roof_thickness = roof_thickness;
    return (FT_ERR_SUCCESS);
}

voxel_layer_config::voxel_layer_config() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      enable_beaches(FT_FALSE), enable_snow_caps(FT_FALSE), beach_depth(0U),
      underwater_depth(0U), snow_cap_depth(0U), snow_cap_minimum_height(0),
      beach_block_id(0U), underwater_block_id(0U), snow_cap_block_id(0U)
{
    return ;
}

voxel_layer_config::~voxel_layer_config() noexcept
{
    this->destroy();
    return ;
}

int32_t voxel_layer_config::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    this->enable_beaches = FT_FALSE;
    this->enable_snow_caps = FT_FALSE;
    this->beach_depth = 0U;
    this->underwater_depth = 0U;
    this->snow_cap_depth = 0U;
    this->snow_cap_minimum_height = 0;
    this->beach_block_id = 0U;
    this->underwater_block_id = 0U;
    this->snow_cap_block_id = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_layer_config::initialize(
    const voxel_layer_config &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    this->enable_beaches = other.enable_beaches;
    this->enable_snow_caps = other.enable_snow_caps;
    this->beach_depth = other.beach_depth;
    this->underwater_depth = other.underwater_depth;
    this->snow_cap_depth = other.snow_cap_depth;
    this->snow_cap_minimum_height = other.snow_cap_minimum_height;
    this->beach_block_id = other.beach_block_id;
    this->underwater_block_id = other.underwater_block_id;
    this->snow_cap_block_id = other.snow_cap_block_id;
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_layer_config::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->enable_beaches = FT_FALSE;
    this->enable_snow_caps = FT_FALSE;
    this->beach_depth = 0U;
    this->underwater_depth = 0U;
    this->snow_cap_depth = 0U;
    this->snow_cap_minimum_height = 0;
    this->beach_block_id = 0U;
    this->underwater_block_id = 0U;
    this->snow_cap_block_id = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_layer_config::move(voxel_layer_config &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_layer_config::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t voxel_layer_config::set_enabled(ft_bool beaches,
    ft_bool snow_caps) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_beaches = beaches;
    this->enable_snow_caps = snow_caps;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_layer_config::set_depths(uint32_t beach,
    uint32_t underwater, uint32_t snow) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->beach_depth = beach;
    this->underwater_depth = underwater;
    this->snow_cap_depth = snow;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_layer_config::set_snowline(
    int32_t minimum_height_value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (minimum_height_value < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->snow_cap_minimum_height = minimum_height_value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_layer_config::set_block_palette(uint32_t beach,
    uint32_t underwater, uint32_t snow) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->beach_block_id = beach;
    this->underwater_block_id = underwater;
    this->snow_cap_block_id = snow;
    return (FT_ERR_SUCCESS);
}

static void voxel_abort_unknown_block_id(uint32_t block_id,
    const char *method_name) noexcept
{
    char decimal_buffer[10];
    ft_size_t digit_count;
    int32_t write_error;

    write_error = errno_write_stderr("voxel error: method=");
    if (write_error != FT_ERR_SUCCESS)
    {
        su_abort();
        return ;
    }
    write_error = errno_write_stderr(method_name);
    if (write_error != FT_ERR_SUCCESS)
    {
        su_abort();
        return ;
    }
    write_error = errno_write_stderr(" unknown block id=");
    if (write_error != FT_ERR_SUCCESS)
    {
        su_abort();
        return ;
    }
    if (block_id == 0U)
    {
        if (errno_write_stderr("0") != FT_ERR_SUCCESS)
        {
            su_abort();
            return ;
        }
    }
    else
    {
        digit_count = 0;
        while (block_id > 0U && digit_count < sizeof(decimal_buffer))
        {
            decimal_buffer[digit_count] = static_cast<char>('0'
                + (block_id % 10U));
            block_id /= 10U;
            digit_count++;
        }
        while (digit_count > 0U)
        {
            digit_count--;
            if (su_write(2, &decimal_buffer[digit_count], 1U) != 1)
            {
                su_abort();
                return ;
            }
        }
    }
    if (errno_write_stderr("\n") != FT_ERR_SUCCESS)
    {
        su_abort();
        return ;
    }
    su_abort();
    return ;
}

static const voxel_block_metadata VOXEL_BLOCK_REGISTRY[] =
{
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_FALSE, 0U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 2U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 4U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 3U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 2U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_TRUE, FT_TRUE, FT_FALSE, FT_FALSE, 0U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 255U, FT_FALSE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 3U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 3U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 3U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 3U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 5U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 6U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 8U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 4U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 4U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 4U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 2U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 2U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 50U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 4U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 3U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 4U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 5U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 8U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 8U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 6U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 3U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 3U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_TRUE, FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, 2U, FT_TRUE},
    {FT_TRUE, FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, 3U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 2U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 2U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 2U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 4U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 3U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 3U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 2U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 5U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 4U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 5U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 2U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 1U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, 4U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, FT_TRUE, 4U, FT_TRUE},
    {FT_TRUE, FT_FALSE, FT_FALSE, FT_FALSE, FT_TRUE, FT_TRUE, 4U, FT_TRUE}
};

static_assert(sizeof(VOXEL_BLOCK_REGISTRY)
        / sizeof(VOXEL_BLOCK_REGISTRY[0])
        == static_cast<uint32_t>(VOXEL_BUILTIN_BLOCK_COUNT),
    "voxel metadata must cover every built-in block ID");

static const voxel_tree_template_block VOXEL_SMALL_OAK_TREE_BLOCKS[] =
{
    {0, 0, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {-1, 2, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 2, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 2, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 3, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 3, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 3, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 3, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 3, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK}
};

static const voxel_tree_template_block VOXEL_SMALL_PINE_TREE_BLOCKS[] =
{
    {0, 0, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 4, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {-1, 3, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 3, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 3, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 3, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 3, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 3, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 3, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 3, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 6, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK}
};

static const voxel_tree_template_block VOXEL_SMALL_CACTUS_TREE_BLOCKS[] =
{
    {0, 0, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_CACTUS_BLOCK}
};

static const voxel_tree_template_block VOXEL_LARGE_OAK_TREE_BLOCKS[] =
{
    {0, 0, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 4, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {-2, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -2, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 2, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {2, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 6, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK}
};

static const voxel_tree_template_block VOXEL_LARGE_OAK_TREE_BLOCKS_VARIANT_1[] =
{
    {0, 0, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 4, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 5, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {-2, 5, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -2, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 2, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {2, 5, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 6, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK}
};

static const voxel_tree_template_block VOXEL_LARGE_PINE_TREE_BLOCKS[] =
{
    {0, 0, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 4, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 5, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 6, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {-1, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 5, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 5, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 6, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 6, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 6, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 6, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 7, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK}
};

static const voxel_tree_template_block VOXEL_LARGE_PINE_TREE_BLOCKS_VARIANT_1[] =
{
    {0, 0, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 4, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 5, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {-1, 4, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 4, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 6, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 5, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 5, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 7, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 8, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK}
};

static const voxel_tree_template VOXEL_SMALL_OAK_TREE_TEMPLATE =
{
    VOXEL_SMALL_OAK_TREE_BLOCKS,
    static_cast<uint32_t>(sizeof(VOXEL_SMALL_OAK_TREE_BLOCKS)
        / sizeof(VOXEL_SMALL_OAK_TREE_BLOCKS[0]))
};

static const voxel_tree_template VOXEL_SMALL_PINE_TREE_TEMPLATE =
{
    VOXEL_SMALL_PINE_TREE_BLOCKS,
    static_cast<uint32_t>(sizeof(VOXEL_SMALL_PINE_TREE_BLOCKS)
        / sizeof(VOXEL_SMALL_PINE_TREE_BLOCKS[0]))
};

static const voxel_tree_template VOXEL_SMALL_CACTUS_TREE_TEMPLATE =
{
    VOXEL_SMALL_CACTUS_TREE_BLOCKS,
    static_cast<uint32_t>(sizeof(VOXEL_SMALL_CACTUS_TREE_BLOCKS)
        / sizeof(VOXEL_SMALL_CACTUS_TREE_BLOCKS[0]))
};

static const voxel_tree_template VOXEL_LARGE_OAK_TREE_TEMPLATE =
{
    VOXEL_LARGE_OAK_TREE_BLOCKS,
    static_cast<uint32_t>(sizeof(VOXEL_LARGE_OAK_TREE_BLOCKS)
        / sizeof(VOXEL_LARGE_OAK_TREE_BLOCKS[0]))
};

static const voxel_tree_template VOXEL_LARGE_OAK_TREE_TEMPLATE_VARIANT_1 =
{
    VOXEL_LARGE_OAK_TREE_BLOCKS_VARIANT_1,
    static_cast<uint32_t>(sizeof(VOXEL_LARGE_OAK_TREE_BLOCKS_VARIANT_1)
        / sizeof(VOXEL_LARGE_OAK_TREE_BLOCKS_VARIANT_1[0]))
};

static const voxel_tree_template VOXEL_LARGE_PINE_TREE_TEMPLATE =
{
    VOXEL_LARGE_PINE_TREE_BLOCKS,
    static_cast<uint32_t>(sizeof(VOXEL_LARGE_PINE_TREE_BLOCKS)
        / sizeof(VOXEL_LARGE_PINE_TREE_BLOCKS[0]))
};

static const voxel_tree_template VOXEL_LARGE_PINE_TREE_TEMPLATE_VARIANT_1 =
{
    VOXEL_LARGE_PINE_TREE_BLOCKS_VARIANT_1,
    static_cast<uint32_t>(sizeof(VOXEL_LARGE_PINE_TREE_BLOCKS_VARIANT_1)
        / sizeof(VOXEL_LARGE_PINE_TREE_BLOCKS_VARIANT_1[0]))
};

static const voxel_tree_template_block VOXEL_SMALL_OAK_TREE_BLOCKS_VARIANT_1[] =
{
    {0, 0, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {-2, 2, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 2, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 2, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 2, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, -2, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, 2, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {2, 2, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK}
};

static const voxel_tree_template_block VOXEL_SMALL_OAK_TREE_BLOCKS_VARIANT_2[] =
{
    {0, 0, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {0, 4, 0, VOXEL_GENERATOR_OAK_LOG_BLOCK},
    {-1, 3, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 3, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 3, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 3, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 3, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 0, VOXEL_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 1, VOXEL_GENERATOR_OAK_LEAVES_BLOCK}
};

static const voxel_tree_template_block VOXEL_SMALL_PINE_TREE_BLOCKS_VARIANT_1[] =
{
    {0, 0, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {-1, 3, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 3, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 3, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 3, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK}
};

static const voxel_tree_template_block VOXEL_SMALL_PINE_TREE_BLOCKS_VARIANT_2[] =
{
    {0, 0, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 4, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {0, 5, 0, VOXEL_GENERATOR_PINE_LOG_BLOCK},
    {-1, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, -1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, 1, VOXEL_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 6, 0, VOXEL_GENERATOR_PINE_LEAVES_BLOCK}
};

static const voxel_tree_template_block VOXEL_SMALL_CACTUS_TREE_BLOCKS_VARIANT_1[] =
{
    {0, 0, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {1, 2, 0, VOXEL_GENERATOR_CACTUS_BLOCK}
};

static const voxel_tree_template_block VOXEL_SMALL_CACTUS_TREE_BLOCKS_VARIANT_2[] =
{
    {0, 0, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {0, 1, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {0, 2, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {0, 3, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {-1, 1, 0, VOXEL_GENERATOR_CACTUS_BLOCK},
    {1, 2, 0, VOXEL_GENERATOR_CACTUS_BLOCK}
};

static const voxel_tree_template VOXEL_SMALL_OAK_TREE_TEMPLATE_VARIANT_1 =
{
    VOXEL_SMALL_OAK_TREE_BLOCKS_VARIANT_1,
    static_cast<uint32_t>(sizeof(VOXEL_SMALL_OAK_TREE_BLOCKS_VARIANT_1)
        / sizeof(VOXEL_SMALL_OAK_TREE_BLOCKS_VARIANT_1[0]))
};

static const voxel_tree_template VOXEL_SMALL_OAK_TREE_TEMPLATE_VARIANT_2 =
{
    VOXEL_SMALL_OAK_TREE_BLOCKS_VARIANT_2,
    static_cast<uint32_t>(sizeof(VOXEL_SMALL_OAK_TREE_BLOCKS_VARIANT_2)
        / sizeof(VOXEL_SMALL_OAK_TREE_BLOCKS_VARIANT_2[0]))
};

static const voxel_tree_template VOXEL_SMALL_PINE_TREE_TEMPLATE_VARIANT_1 =
{
    VOXEL_SMALL_PINE_TREE_BLOCKS_VARIANT_1,
    static_cast<uint32_t>(sizeof(VOXEL_SMALL_PINE_TREE_BLOCKS_VARIANT_1)
        / sizeof(VOXEL_SMALL_PINE_TREE_BLOCKS_VARIANT_1[0]))
};

static const voxel_tree_template VOXEL_SMALL_PINE_TREE_TEMPLATE_VARIANT_2 =
{
    VOXEL_SMALL_PINE_TREE_BLOCKS_VARIANT_2,
    static_cast<uint32_t>(sizeof(VOXEL_SMALL_PINE_TREE_BLOCKS_VARIANT_2)
        / sizeof(VOXEL_SMALL_PINE_TREE_BLOCKS_VARIANT_2[0]))
};

static const voxel_tree_template VOXEL_SMALL_CACTUS_TREE_TEMPLATE_VARIANT_1 =
{
    VOXEL_SMALL_CACTUS_TREE_BLOCKS_VARIANT_1,
    static_cast<uint32_t>(sizeof(VOXEL_SMALL_CACTUS_TREE_BLOCKS_VARIANT_1)
        / sizeof(VOXEL_SMALL_CACTUS_TREE_BLOCKS_VARIANT_1[0]))
};

static const voxel_tree_template VOXEL_SMALL_CACTUS_TREE_TEMPLATE_VARIANT_2 =
{
    VOXEL_SMALL_CACTUS_TREE_BLOCKS_VARIANT_2,
    static_cast<uint32_t>(sizeof(VOXEL_SMALL_CACTUS_TREE_BLOCKS_VARIANT_2)
        / sizeof(VOXEL_SMALL_CACTUS_TREE_BLOCKS_VARIANT_2[0]))
};

int32_t voxel_floor_div(int32_t value, int32_t divisor) noexcept
{
    int32_t quotient;
    int32_t remainder;

    quotient = value / divisor;
    remainder = value % divisor;
    if (remainder < 0)
        quotient -= 1;
    return (quotient);
}

uint64_t voxel_mix_u64(uint64_t value) noexcept
{
    value ^= value >> 30;
    value *= UINT64_C(0xBF58476D1CE4E5B9);
    value ^= value >> 27;
    value *= UINT64_C(0x94D049BB133111EB);
    value ^= value >> 31;
    return (value);
}

double voxel_lerp(double left_value, double right_value,
    double factor) noexcept
{
    return (left_value + ((right_value - left_value) * factor));
}

double voxel_smooth_factor(double factor) noexcept
{
    return (factor * factor * (3.0 - (2.0 * factor)));
}

uint64_t voxel_seed_value(const char *seed_string) noexcept
{
    return (static_cast<uint64_t>(rng_seed_value(seed_string)));
}

uint64_t voxel_feature_seed(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, uint64_t salt) noexcept
{
    uint64_t feature_seed;

    feature_seed = voxel_mix_u64(seed_value ^ salt
        ^ (static_cast<uint64_t>(static_cast<int64_t>(world_block_x))
            * UINT64_C(0x9E3779B97F4A7C15))
        ^ (static_cast<uint64_t>(static_cast<int64_t>(world_block_z))
            * UINT64_C(0xBF58476D1CE4E5B9)));
    return (feature_seed);
}

double voxel_signed_unit_noise(uint64_t seed_value, int32_t grid_x,
    int32_t grid_z) noexcept
{
    uint64_t mixed_value;

    mixed_value = voxel_mix_u64(seed_value
        ^ (static_cast<uint64_t>(static_cast<int64_t>(grid_x))
            * UINT64_C(0x9E3779B97F4A7C15))
        ^ (static_cast<uint64_t>(static_cast<int64_t>(grid_z))
            * UINT64_C(0xBF58476D1CE4E5B9)));
    mixed_value = voxel_mix_u64(mixed_value);
    return (static_cast<double>(mixed_value >> 11)
        * (1.0 / 9007199254740992.0) * 2.0 - 1.0);
}

double voxel_value_noise(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, int32_t scale) noexcept
{
    int32_t grid_x0;
    int32_t grid_z0;
    int32_t grid_x1;
    int32_t grid_z1;
    int32_t local_x;
    int32_t local_z;
    double factor_x;
    double factor_z;
    double smooth_x;
    double smooth_z;
    double noise_top;
    double noise_bottom;
    double noise_left;
    double noise_right;
    double noise_value;

    grid_x0 = voxel_floor_div(world_block_x, scale);
    grid_z0 = voxel_floor_div(world_block_z, scale);
    grid_x1 = grid_x0 + 1;
    grid_z1 = grid_z0 + 1;
    local_x = world_block_x - (grid_x0 * scale);
    local_z = world_block_z - (grid_z0 * scale);
    factor_x = static_cast<double>(local_x) / static_cast<double>(scale);
    factor_z = static_cast<double>(local_z) / static_cast<double>(scale);
    smooth_x = voxel_smooth_factor(factor_x);
    smooth_z = voxel_smooth_factor(factor_z);
    noise_left = voxel_signed_unit_noise(seed_value, grid_x0, grid_z0);
    noise_right = voxel_signed_unit_noise(seed_value, grid_x1, grid_z0);
    noise_top = voxel_lerp(noise_left, noise_right, smooth_x);
    noise_left = voxel_signed_unit_noise(seed_value, grid_x0, grid_z1);
    noise_right = voxel_signed_unit_noise(seed_value, grid_x1, grid_z1);
    noise_bottom = voxel_lerp(noise_left, noise_right, smooth_x);
    noise_value = voxel_lerp(noise_top, noise_bottom, smooth_z);
    return (noise_value);
}

voxel_biome voxel_pick_biome(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    int32_t biome_zone_width) noexcept
{
    int32_t biome_zone_x;
    int32_t biome_zone_z;
    int64_t biome_selector;

    biome_zone_x = voxel_floor_div(world_block_x, biome_zone_width);
    biome_zone_z = voxel_floor_div(world_block_z, biome_zone_width);
    biome_selector = static_cast<int64_t>(seed_value % 5U)
        + static_cast<int64_t>(biome_zone_x)
        + static_cast<int64_t>(biome_zone_z);
    biome_selector %= 5;
    if (biome_selector < 0)
        biome_selector += 5;
    if (biome_selector == 0)
        return (VOXEL_BIOME_PLAINS);
    if (biome_selector == 1)
        return (VOXEL_BIOME_HILLS);
    if (biome_selector == 2)
        return (VOXEL_BIOME_DESERT);
    if (biome_selector == 3)
        return (VOXEL_BIOME_SNOW);
    return (VOXEL_BIOME_MOUNTAINS);
}

static uint32_t voxel_pick_biome_with_individual_sizes(
    const voxel_generation_config &config, uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z) noexcept
{
    uint32_t biome_index;
    uint32_t best_biome;
    double best_score;

    best_biome = 0U;
    best_score = 1.0e30;
    biome_index = 0U;
    while (biome_index < config.biome_count
        && biome_index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        const int32_t width = voxel_get_biome_zone_width_for_biome(
            config, seed_value, biome_index);
        const int32_t cell_x = voxel_floor_div(world_block_x, width);
        const int32_t cell_z = voxel_floor_div(world_block_z, width);
        int32_t neighbour_z = -1;
        while (neighbour_z <= 1)
        {
            int32_t neighbour_x = -1;
            while (neighbour_x <= 1)
            {
                const int32_t candidate_cell_x = cell_x + neighbour_x;
                const int32_t candidate_cell_z = cell_z + neighbour_z;
                const int32_t origin_x = candidate_cell_x * width;
                const int32_t origin_z = candidate_cell_z * width;
                const int32_t site_x = origin_x + width / 2
                    + static_cast<int32_t>(voxel_signed_unit_noise(
                        seed_value ^ UINT64_C(0xA24BAED4963EE407)
                            ^ (static_cast<uint64_t>(biome_index + 1U)
                                * UINT64_C(0xD6E8FEB86659FD93)),
                        candidate_cell_x, candidate_cell_z)
                        * static_cast<double>(width) * 0.35);
                const int32_t site_z = origin_z + width / 2
                    + static_cast<int32_t>(voxel_signed_unit_noise(
                        seed_value ^ UINT64_C(0x9FB21C651E98DF25)
                            ^ (static_cast<uint64_t>(biome_index + 1U)
                                * UINT64_C(0x94D049BB133111EB)),
                        candidate_cell_x, candidate_cell_z)
                        * static_cast<double>(width) * 0.35);
                const double distance_x = static_cast<double>(world_block_x
                    - site_x);
                const double distance_z = static_cast<double>(world_block_z
                    - site_z);
                const double normaliser = static_cast<double>(width)
                    * static_cast<double>(width);
                const double tie_break = (voxel_signed_unit_noise(
                    seed_value ^ UINT64_C(0xC6BC279692B5CC83),
                    candidate_cell_x + static_cast<int32_t>(biome_index),
                    candidate_cell_z - static_cast<int32_t>(biome_index))
                    + 1.0) * 1.0e-6;
                const double score = ((distance_x * distance_x)
                    + (distance_z * distance_z)) / normaliser + tie_break;
                if (score < best_score)
                {
                    best_score = score;
                    best_biome = biome_index;
                }
                neighbour_x += 1;
            }
            neighbour_z += 1;
        }
        biome_index += 1U;
    }
    return (best_biome);
}

uint32_t voxel_surface_block_for_biome(voxel_biome biome) noexcept
{
    if (biome == VOXEL_BIOME_DESERT)
        return (VOXEL_GENERATOR_SAND_BLOCK);
    if (biome == VOXEL_BIOME_SNOW)
        return (VOXEL_GENERATOR_SNOW_BLOCK);
    if (biome == VOXEL_BIOME_MOUNTAINS)
        return (VOXEL_GENERATOR_SLATE_BLOCK);
    return (VOXEL_GENERATOR_GRASS_BLOCK);
}

uint32_t voxel_subsurface_block_for_biome(voxel_biome biome) noexcept
{
    if (biome == VOXEL_BIOME_DESERT)
        return (VOXEL_GENERATOR_CANYON_ROCK_BLOCK);
    if (biome == VOXEL_BIOME_SNOW)
        return (VOXEL_GENERATOR_PERMAFROST_BLOCK);
    if (biome == VOXEL_BIOME_MOUNTAINS)
        return (VOXEL_GENERATOR_ANDESITE_BLOCK);
    return (VOXEL_GENERATOR_DIRT_BLOCK);
}

uint32_t voxel_deep_block_for_biome(voxel_biome biome) noexcept
{
    if (biome == VOXEL_BIOME_MOUNTAINS)
        return (VOXEL_GENERATOR_BASALT_BLOCK);
    if (biome == VOXEL_BIOME_DESERT)
        return (VOXEL_GENERATOR_LIMESTONE_BLOCK);
    if (biome == VOXEL_BIOME_SNOW)
        return (VOXEL_GENERATOR_FROZEN_STONE_BLOCK);
    if (biome == VOXEL_BIOME_HILLS)
        return (VOXEL_GENERATOR_GRANITE_BLOCK);
    return (VOXEL_GENERATOR_STONE_BLOCK);
}

ft_bool voxel_biome_has_shrubs(voxel_biome biome) noexcept
{
    if (biome == VOXEL_BIOME_PLAINS)
        return (FT_TRUE);
    if (biome == VOXEL_BIOME_HILLS)
        return (FT_TRUE);
    if (biome == VOXEL_BIOME_DESERT)
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool voxel_biome_has_trees(voxel_biome biome) noexcept
{
    if (biome == VOXEL_BIOME_PLAINS)
        return (FT_TRUE);
    if (biome == VOXEL_BIOME_HILLS)
        return (FT_TRUE);
    if (biome == VOXEL_BIOME_DESERT)
        return (FT_TRUE);
    if (biome == VOXEL_BIOME_SNOW)
        return (FT_TRUE);
    if (biome == VOXEL_BIOME_MOUNTAINS)
        return (FT_TRUE);
    return (FT_FALSE);
}

static uint32_t voxel_normalise_tree_variant(uint32_t variant_index,
    uint32_t variant_count) noexcept
{
    if (variant_count == 0U)
        return (0U);
    return (variant_index % variant_count);
}

const voxel_block_metadata &voxel_get_block_metadata(uint32_t block_id)
    noexcept
{
    const voxel_block_metadata *runtime_metadata;

    runtime_metadata = voxel_runtime_find_block_metadata(block_id);
    if (runtime_metadata != ft_nullptr)
        return (*runtime_metadata);
    if (block_id >= static_cast<uint32_t>(sizeof(VOXEL_BLOCK_REGISTRY)
            / sizeof(VOXEL_BLOCK_REGISTRY[0])))
    {
        voxel_abort_unknown_block_id(block_id,
            "voxel_get_block_metadata");
        return (VOXEL_BLOCK_REGISTRY[GAME_VOXEL_AIR_BLOCK]);
    }
    return (VOXEL_BLOCK_REGISTRY[block_id]);
}

ft_bool voxel_block_is_known(uint32_t block_id) noexcept
{
    if (voxel_runtime_block_is_known(block_id) == FT_TRUE)
        return (FT_TRUE);
    if (block_id >= static_cast<uint32_t>(sizeof(VOXEL_BLOCK_REGISTRY)
            / sizeof(VOXEL_BLOCK_REGISTRY[0])))
        return (FT_FALSE);
    return (FT_TRUE);
}

ft_bool voxel_block_is_solid(uint32_t block_id) noexcept
{
    return (voxel_get_block_metadata(block_id).solid);
}

ft_bool voxel_block_is_transparent(uint32_t block_id) noexcept
{
    return (voxel_get_block_metadata(block_id).transparent);
}

ft_bool voxel_block_is_liquid(uint32_t block_id) noexcept
{
    return (voxel_get_block_metadata(block_id).liquid);
}

ft_bool voxel_block_is_replaceable(uint32_t block_id) noexcept
{
    return (voxel_get_block_metadata(block_id).replaceable);
}

ft_bool voxel_block_can_host_ore(uint32_t block_id) noexcept
{
    if (voxel_get_block_metadata(block_id).can_host_ore == FT_TRUE)
        return (FT_TRUE);
    if (block_id == VOXEL_GENERATOR_STONE_BLOCK
        || block_id == VOXEL_GENERATOR_GRANITE_BLOCK
        || block_id == VOXEL_GENERATOR_ANDESITE_BLOCK
        || block_id == VOXEL_GENERATOR_DIORITE_BLOCK
        || block_id == VOXEL_GENERATOR_LIMESTONE_BLOCK
        || block_id == VOXEL_GENERATOR_BASALT_BLOCK)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t voxel_get_biome_zone_width(
    const voxel_generation_config &config, uint64_t seed_value) noexcept
{
    int64_t range;

    if (config.enable_biome_size_control == FT_FALSE
        || config.biome_size_min < VOXEL_BIOME_SIZE_MINIMUM
        || config.biome_size_max < config.biome_size_min)
        return (VOXEL_BIOME_ZONE_WIDTH);
    range = static_cast<int64_t>(config.biome_size_max)
        - static_cast<int64_t>(config.biome_size_min) + 1;
    if (range <= 1)
        return (config.biome_size_min);
    return (config.biome_size_min + static_cast<int32_t>(seed_value
        % static_cast<uint64_t>(range)));
}

int32_t voxel_get_biome_zone_width_for_biome(
    const voxel_generation_config &config, uint64_t seed_value,
    uint32_t biome_index) noexcept
{
    int32_t minimum_size;
    int32_t maximum_size;
    int64_t range;

    if (config.enable_biome_size_control == FT_FALSE
        || biome_index >= VOXEL_MAX_CUSTOM_BIOMES)
        return (VOXEL_BIOME_ZONE_WIDTH);
    minimum_size = config.biome_size_min;
    maximum_size = config.biome_size_max;
    if (config.biome_size_override_enabled[biome_index] == FT_TRUE)
    {
        minimum_size = config.biome_size_min_by_biome[biome_index];
        maximum_size = config.biome_size_max_by_biome[biome_index];
    }
    if (minimum_size < VOXEL_BIOME_SIZE_MINIMUM
        || maximum_size < minimum_size)
        return (VOXEL_BIOME_ZONE_WIDTH);
    range = static_cast<int64_t>(maximum_size)
        - static_cast<int64_t>(minimum_size) + 1;
    if (range <= 1)
        return (minimum_size);
    return (minimum_size + static_cast<int32_t>((seed_value
        ^ (UINT64_C(0x9E3779B97F4A7C15)
            * static_cast<uint64_t>(biome_index + 1U)))
        % static_cast<uint64_t>(range)));
}

ft_bool voxel_block_is_ore(uint32_t block_id) noexcept
{
    if (voxel_get_block_metadata(block_id).is_ore == FT_TRUE)
        return (FT_TRUE);
    if (block_id == VOXEL_GENERATOR_COAL_ORE_BLOCK
        || block_id == VOXEL_GENERATOR_IRON_ORE_BLOCK
        || block_id == VOXEL_GENERATOR_GOLD_ORE_BLOCK
        || block_id == VOXEL_GENERATOR_DIAMOND_ORE_BLOCK
        || block_id == VOXEL_GENERATOR_EMERALD_ORE_BLOCK
        || block_id == VOXEL_GENERATOR_COPPER_ORE_BLOCK)
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool voxel_block_emits_light(uint32_t block_id) noexcept
{
    return (voxel_get_block_metadata(block_id).light_emitting);
}

uint8_t voxel_block_emitted_light_level(uint32_t block_id) noexcept
{
    const voxel_block_metadata &metadata = voxel_get_block_metadata(block_id);
    if (metadata.emitted_light_level > 15U)
        return (15U);
    if (metadata.light_emitting == FT_TRUE && metadata.emitted_light_level == 0U)
        return (15U);
    return (metadata.emitted_light_level);
}

uint8_t voxel_block_light_attenuation(uint32_t block_id) noexcept
{
    return (voxel_get_block_metadata(block_id).light_attenuation);
}

ft_bool voxel_block_occludes_faces(uint32_t block_id) noexcept
{
    return (voxel_get_block_metadata(block_id).occludes_faces);
}

uint32_t voxel_block_hardness(uint32_t block_id) noexcept
{
    return (voxel_get_block_metadata(block_id).hardness);
}

ft_bool voxel_block_is_breakable(uint32_t block_id) noexcept
{
    return (voxel_get_block_metadata(block_id).breakable);
}

const voxel_tree_template &voxel_small_oak_tree_template_variant(
    uint32_t variant_index) noexcept
{
    variant_index = voxel_normalise_tree_variant(variant_index, 3U);
    if (variant_index == 1U)
        return (VOXEL_SMALL_OAK_TREE_TEMPLATE_VARIANT_1);
    if (variant_index == 2U)
        return (VOXEL_SMALL_OAK_TREE_TEMPLATE_VARIANT_2);
    return (VOXEL_SMALL_OAK_TREE_TEMPLATE);
}

const voxel_tree_template &voxel_small_pine_tree_template_variant(
    uint32_t variant_index) noexcept
{
    variant_index = voxel_normalise_tree_variant(variant_index, 3U);
    if (variant_index == 1U)
        return (VOXEL_SMALL_PINE_TREE_TEMPLATE_VARIANT_1);
    if (variant_index == 2U)
        return (VOXEL_SMALL_PINE_TREE_TEMPLATE_VARIANT_2);
    return (VOXEL_SMALL_PINE_TREE_TEMPLATE);
}

const voxel_tree_template &voxel_small_cactus_tree_template_variant(
    uint32_t variant_index) noexcept
{
    variant_index = voxel_normalise_tree_variant(variant_index, 3U);
    if (variant_index == 1U)
        return (VOXEL_SMALL_CACTUS_TREE_TEMPLATE_VARIANT_1);
    if (variant_index == 2U)
        return (VOXEL_SMALL_CACTUS_TREE_TEMPLATE_VARIANT_2);
    return (VOXEL_SMALL_CACTUS_TREE_TEMPLATE);
}

const voxel_tree_template &voxel_small_oak_tree_template(
    uint32_t variant_index) noexcept
{
    return (voxel_small_oak_tree_template_variant(variant_index));
}

const voxel_tree_template &voxel_small_pine_tree_template(
    uint32_t variant_index) noexcept
{
    return (voxel_small_pine_tree_template_variant(variant_index));
}

const voxel_tree_template &voxel_small_cactus_tree_template(
    uint32_t variant_index) noexcept
{
    return (voxel_small_cactus_tree_template_variant(variant_index));
}

const voxel_tree_template &voxel_large_oak_tree_template(
    uint32_t variant_index) noexcept
{
    return (voxel_large_oak_tree_template_variant(variant_index));
}

const voxel_tree_template &voxel_large_pine_tree_template(
    uint32_t variant_index) noexcept
{
    return (voxel_large_pine_tree_template_variant(variant_index));
}

const voxel_tree_template &voxel_large_oak_tree_template_variant(
    uint32_t variant_index) noexcept
{
    variant_index = voxel_normalise_tree_variant(variant_index, 2U);
    if (variant_index == 1U)
        return (VOXEL_LARGE_OAK_TREE_TEMPLATE_VARIANT_1);
    return (VOXEL_LARGE_OAK_TREE_TEMPLATE);
}

const voxel_tree_template &voxel_large_pine_tree_template_variant(
    uint32_t variant_index) noexcept
{
    variant_index = voxel_normalise_tree_variant(variant_index, 2U);
    if (variant_index == 1U)
        return (VOXEL_LARGE_PINE_TREE_TEMPLATE_VARIANT_1);
    return (VOXEL_LARGE_PINE_TREE_TEMPLATE);
}

const voxel_tree_template &voxel_tree_template_for_biome(
    voxel_biome biome) noexcept
{
    return (voxel_tree_template_for_biome(biome, 0U));
}

const voxel_tree_template &voxel_tree_template_for_biome(
    voxel_biome biome, uint64_t seed_value) noexcept
{
    uint32_t variant_index;

    variant_index = static_cast<uint32_t>(seed_value % 5U);
    if (biome == VOXEL_BIOME_DESERT)
        return (voxel_small_cactus_tree_template(variant_index));
    if (biome == VOXEL_BIOME_SNOW)
    {
        if (variant_index < 3U)
            return (voxel_small_pine_tree_template(variant_index));
        return (voxel_large_pine_tree_template(variant_index - 3U));
    }
    if (biome == VOXEL_BIOME_MOUNTAINS)
    {
        if (variant_index < 3U)
            return (voxel_small_pine_tree_template(variant_index));
        return (voxel_large_pine_tree_template(variant_index - 3U));
    }
    if (biome == VOXEL_BIOME_HILLS)
    {
        if (variant_index < 3U)
            return (voxel_small_oak_tree_template(variant_index));
        return (voxel_large_oak_tree_template(variant_index - 3U));
    }
    if (variant_index < 3U)
        return (voxel_small_oak_tree_template(variant_index));
    return (voxel_large_oak_tree_template(variant_index - 3U));
}

voxel_biome voxel_get_biome(int32_t world_block_x, int32_t world_block_z,
    const char *seed_string) noexcept
{
    return (voxel_pick_biome(voxel_seed_value(seed_string), world_block_x,
        world_block_z, VOXEL_BIOME_ZONE_WIDTH));
}

uint32_t voxel_select_biome(const voxel_generation_config &config,
    uint64_t seed_value, int32_t world_block_x, int32_t world_block_z) noexcept
{
    uint32_t selected;

    if (config.biome_count == 0U)
        return (0U);
    if (config.biome_selector != ft_nullptr)
        selected = config.biome_selector(seed_value, world_block_x,
            world_block_z, config.biome_count, config.biome_selector_user_data);
    else if (config.enable_biome_size_control == FT_TRUE)
        selected = voxel_pick_biome_with_individual_sizes(config, seed_value,
            world_block_x, world_block_z);
    else
        selected = static_cast<uint32_t>(voxel_pick_biome(seed_value,
            world_block_x, world_block_z,
            voxel_get_biome_zone_width(config, seed_value)));
    return (selected % config.biome_count);
}

uint32_t voxel_get_biome_index(const voxel_generation_config &config,
    int32_t world_block_x, int32_t world_block_z,
    const char *seed_string) noexcept
{
    return (voxel_select_biome(config, voxel_seed_value(seed_string),
        world_block_x, world_block_z));
}

voxel_generation_config::voxel_generation_config() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), sea_level(0),
      large_noise_scale(0), detail_noise_scale(0), detail_noise_percent(0),
      enable_biome_size_control(FT_FALSE),
      biome_size_min(0), biome_size_max(0),
      biome_size_min_by_biome(), biome_size_max_by_biome(),
      biome_size_override_enabled(),
      water_chance_percent(0U), biome_count(0U), biomes(),
      tree_template_count(0U), tree_templates(), tree_template_blocks(),
      biome_selector(ft_nullptr),
      biome_selector_user_data(ft_nullptr), feature_count(0U), features(),
      ore_rule_count(0U), ores(), underground_structures(), fluids(), layers(),
      enable_biome_transitions(FT_FALSE), biome_transition_noise_scale(0),
      biome_transition_noise_strength(0U), enable_mountain_ridges(FT_FALSE),
      enable_erosion(FT_FALSE), mountain_ridge_scale(0),
      mountain_ridge_strength(0U), erosion_noise_scale(0), erosion_strength(0U),
      allow_cross_chunk_features(FT_FALSE), cross_chunk_block_writer(ft_nullptr),
      cross_chunk_block_writer_user_data(ft_nullptr),
      cross_chunk_block_reader(ft_nullptr),
      cross_chunk_block_reader_user_data(ft_nullptr)
{
    return ;
}

voxel_generation_config::~voxel_generation_config() noexcept
{
    this->destroy();
    return ;
}

int32_t voxel_generation_config::initialize() noexcept
{
    uint32_t index;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        this->biomes[index].initialize();
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_FEATURE_RULES)
    {
        this->features[index].initialize();
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_ORE_RULES)
    {
        this->ores[index].initialize();
        index += 1U;
    }
    this->underground_structures.initialize();
    this->fluids.initialize();
    this->layers.initialize();
    if (voxel_apply_default_generation_config(*this)
        != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::initialize(
    const voxel_generation_config &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    this->sea_level = other.sea_level;
    this->large_noise_scale = other.large_noise_scale;
    this->detail_noise_scale = other.detail_noise_scale;
    this->detail_noise_percent = other.detail_noise_percent;
    this->enable_biome_size_control = other.enable_biome_size_control;
    this->biome_size_min = other.biome_size_min;
    this->biome_size_max = other.biome_size_max;
    ft_memcpy(this->biome_size_min_by_biome,
        other.biome_size_min_by_biome,
        sizeof(this->biome_size_min_by_biome));
    ft_memcpy(this->biome_size_max_by_biome,
        other.biome_size_max_by_biome,
        sizeof(this->biome_size_max_by_biome));
    ft_memcpy(this->biome_size_override_enabled,
        other.biome_size_override_enabled,
        sizeof(this->biome_size_override_enabled));
    this->water_chance_percent = other.water_chance_percent;
    this->biome_count = other.biome_count;
    this->tree_template_count = other.tree_template_count;
    this->biome_selector = other.biome_selector;
    this->biome_selector_user_data = other.biome_selector_user_data;
    this->feature_count = other.feature_count;
    this->ore_rule_count = other.ore_rule_count;
    this->underground_structures.initialize(other.underground_structures);
    this->fluids.initialize(other.fluids);
    this->layers.initialize(other.layers);
    this->enable_biome_transitions = other.enable_biome_transitions;
    this->biome_transition_noise_scale = other.biome_transition_noise_scale;
    this->biome_transition_noise_strength =
        other.biome_transition_noise_strength;
    this->enable_mountain_ridges = other.enable_mountain_ridges;
    this->enable_erosion = other.enable_erosion;
    this->mountain_ridge_scale = other.mountain_ridge_scale;
    this->mountain_ridge_strength = other.mountain_ridge_strength;
    this->erosion_noise_scale = other.erosion_noise_scale;
    this->erosion_strength = other.erosion_strength;
    this->allow_cross_chunk_features = other.allow_cross_chunk_features;
    this->cross_chunk_block_writer = other.cross_chunk_block_writer;
    this->cross_chunk_block_writer_user_data = other.cross_chunk_block_writer_user_data;
    this->cross_chunk_block_reader = other.cross_chunk_block_reader;
    this->cross_chunk_block_reader_user_data = other.cross_chunk_block_reader_user_data;
    uint32_t index;

    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        this->biomes[index].initialize(other.biomes[index]);
        index += 1U;
    }
    ft_memcpy(this->tree_templates, other.tree_templates,
        sizeof(this->tree_templates));
    ft_memcpy(this->tree_template_blocks, other.tree_template_blocks,
        sizeof(this->tree_template_blocks));
    index = 0U;
    while (index < this->tree_template_count)
    {
        this->tree_templates[index].blocks =
            this->tree_template_blocks[index];
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        uint32_t template_index;

        template_index = 0U;
        while (template_index < this->tree_template_count)
        {
            if (other.biomes[index].tree_template
                == &other.tree_templates[template_index])
            {
                this->biomes[index].tree_template =
                    &this->tree_templates[template_index];
                break ;
            }
            template_index += 1U;
        }
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_FEATURE_RULES)
    {
        this->features[index].initialize(other.features[index]);
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_ORE_RULES)
    {
        this->ores[index].initialize(other.ores[index]);
        index += 1U;
    }
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_generation_config::destroy() noexcept
{
    uint32_t index;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        this->biomes[index].destroy();
        index += 1U;
    }
    ft_memset(this->tree_templates, 0, sizeof(this->tree_templates));
    ft_memset(this->tree_template_blocks, 0,
        sizeof(this->tree_template_blocks));
    index = 0U;
    while (index < VOXEL_MAX_FEATURE_RULES)
    {
        this->features[index].destroy();
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_ORE_RULES)
    {
        this->ores[index].destroy();
        index += 1U;
    }
    this->sea_level = 0;
    this->large_noise_scale = 0;
    this->detail_noise_scale = 0;
    this->detail_noise_percent = 0;
    this->enable_biome_size_control = FT_FALSE;
    this->biome_size_min = 0;
    this->biome_size_max = 0;
    ft_memset(this->biome_size_min_by_biome, 0,
        sizeof(this->biome_size_min_by_biome));
    ft_memset(this->biome_size_max_by_biome, 0,
        sizeof(this->biome_size_max_by_biome));
    ft_memset(this->biome_size_override_enabled, 0,
        sizeof(this->biome_size_override_enabled));
    this->water_chance_percent = 0U;
    this->biome_count = 0U;
    this->tree_template_count = 0U;
    this->biome_selector = ft_nullptr;
    this->biome_selector_user_data = ft_nullptr;
    this->feature_count = 0U;
    this->ore_rule_count = 0U;
    this->underground_structures.destroy();
    this->fluids.destroy();
    this->layers.destroy();
    this->enable_biome_transitions = FT_FALSE;
    this->biome_transition_noise_scale = 0;
    this->biome_transition_noise_strength = 0U;
    this->enable_mountain_ridges = FT_FALSE;
    this->enable_erosion = FT_FALSE;
    this->mountain_ridge_scale = 0;
    this->mountain_ridge_strength = 0U;
    this->erosion_noise_scale = 0;
    this->erosion_strength = 0U;
    this->allow_cross_chunk_features = FT_FALSE;
    this->cross_chunk_block_writer = ft_nullptr;
    this->cross_chunk_block_writer_user_data = ft_nullptr;
    this->cross_chunk_block_reader = ft_nullptr;
    this->cross_chunk_block_reader_user_data = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_generation_config::move(
    voxel_generation_config &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_generation_config::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

static int32_t voxel_apply_default_generation_config(
    voxel_generation_config &config) noexcept
{
    uint32_t index;

    config.sea_level = VOXEL_GENERATOR_SEA_LEVEL;
    config.large_noise_scale = 32;
    config.detail_noise_scale = 8;
    config.detail_noise_percent = 50;
    config.enable_biome_size_control = FT_TRUE;
    config.biome_size_min = VOXEL_BIOME_ZONE_WIDTH;
    config.biome_size_max = VOXEL_BIOME_ZONE_WIDTH;
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        config.biome_size_min_by_biome[index] = config.biome_size_min;
        config.biome_size_max_by_biome[index] = config.biome_size_max;
        config.biome_size_override_enabled[index] = FT_FALSE;
        index += 1U;
    }
    config.water_chance_percent = 0U;
    config.biome_count = 5U;
    config.tree_template_count = 13U;
    voxel_copy_tree_template(voxel_small_oak_tree_template_variant(0U),
        config.tree_template_blocks[0], &config.tree_templates[0]);
    voxel_copy_tree_template(voxel_small_oak_tree_template_variant(1U),
        config.tree_template_blocks[1], &config.tree_templates[1]);
    voxel_copy_tree_template(voxel_small_oak_tree_template_variant(2U),
        config.tree_template_blocks[2], &config.tree_templates[2]);
    voxel_copy_tree_template(voxel_small_pine_tree_template_variant(0U),
        config.tree_template_blocks[3], &config.tree_templates[3]);
    voxel_copy_tree_template(voxel_small_pine_tree_template_variant(1U),
        config.tree_template_blocks[4], &config.tree_templates[4]);
    voxel_copy_tree_template(voxel_small_pine_tree_template_variant(2U),
        config.tree_template_blocks[5], &config.tree_templates[5]);
    voxel_copy_tree_template(voxel_small_cactus_tree_template_variant(0U),
        config.tree_template_blocks[6], &config.tree_templates[6]);
    voxel_copy_tree_template(voxel_small_cactus_tree_template_variant(1U),
        config.tree_template_blocks[7], &config.tree_templates[7]);
    voxel_copy_tree_template(voxel_small_cactus_tree_template_variant(2U),
        config.tree_template_blocks[8], &config.tree_templates[8]);
    voxel_copy_tree_template(voxel_large_oak_tree_template_variant(0U),
        config.tree_template_blocks[9], &config.tree_templates[9]);
    voxel_copy_tree_template(voxel_large_oak_tree_template_variant(1U),
        config.tree_template_blocks[10], &config.tree_templates[10]);
    voxel_copy_tree_template(voxel_large_pine_tree_template_variant(0U),
        config.tree_template_blocks[11], &config.tree_templates[11]);
    voxel_copy_tree_template(voxel_large_pine_tree_template_variant(1U),
        config.tree_template_blocks[12], &config.tree_templates[12]);
    config.biome_selector = ft_nullptr;
    config.biome_selector_user_data = ft_nullptr;
    config.ore_rule_count = 18U;
    config.ores[0].block_id = VOXEL_GENERATOR_COAL_ORE_BLOCK;
    config.ores[0].set_range(8, 120);
    config.ores[0].set_vein(8U, 12U);
    config.ores[0].set_enabled(FT_TRUE);
    config.ores[1].block_id = VOXEL_GENERATOR_IRON_ORE_BLOCK;
    config.ores[1].set_range(4, 80);
    config.ores[1].set_vein(6U, 8U);
    config.ores[1].set_enabled(FT_TRUE);
    config.ores[2].block_id = VOXEL_GENERATOR_GOLD_ORE_BLOCK;
    config.ores[2].set_range(4, 48);
    config.ores[2].set_vein(4U, 4U);
    config.ores[2].set_enabled(FT_TRUE);
    config.ores[3].block_id = VOXEL_GENERATOR_COPPER_ORE_BLOCK;
    config.ores[3].set_range(16, 90);
    config.ores[3].set_vein(6U, 10U);
    config.ores[3].set_enabled(FT_TRUE);
    config.ores[4].block_id = VOXEL_GENERATOR_DIAMOND_ORE_BLOCK;
    config.ores[4].set_range(4, 24);
    config.ores[4].set_vein(3U, 2U);
    config.ores[4].set_enabled(FT_TRUE);
    config.ores[5].block_id = VOXEL_GENERATOR_EMERALD_ORE_BLOCK;
    config.ores[5].set_range(8, 36);
    config.ores[5].set_vein(2U, 2U);
    config.ores[5].set_enabled(FT_TRUE);
    config.ores[6].block_id = VOXEL_GENERATOR_DIORITE_BLOCK;
    config.ores[6].set_range(4, 55);
    config.ores[6].set_vein(10U, 14U);
    config.ores[6].set_enabled(FT_TRUE);
    config.ores[7].block_id = VOXEL_GENERATOR_GRAVEL_BLOCK;
    config.ores[7].set_range(20, 55);
    config.ores[7].set_vein(10U, 14U);
    config.ores[7].set_enabled(FT_TRUE);
    config.ores[8].block_id = VOXEL_GENERATOR_CLAY_BLOCK;
    config.ores[8].set_range(40, 60);
    config.ores[8].set_vein(6U, 10U);
    config.ores[8].set_enabled(FT_TRUE);
    config.ores[9].block_id = VOXEL_GENERATOR_MOSSY_STONE_BLOCK;
    config.ores[9].set_range(8, 55);
    config.ores[9].set_vein(5U, 6U);
    config.ores[9].set_enabled(FT_TRUE);
    config.ores[10].block_id = VOXEL_GENERATOR_CRACKED_STONE_BLOCK;
    config.ores[10].set_range(4, 40);
    config.ores[10].set_vein(5U, 6U);
    config.ores[10].set_enabled(FT_TRUE);
    config.ores[11].block_id = VOXEL_GENERATOR_OBSIDIAN_BLOCK;
    config.ores[11].set_range(4, 16);
    config.ores[11].set_vein(2U, 2U);
    config.ores[11].set_enabled(FT_TRUE);
    config.ores[12].block_id = VOXEL_GENERATOR_QUARTZ_BLOCK;
    config.ores[12].set_range(4, 48);
    config.ores[12].set_vein(4U, 5U);
    config.ores[12].set_enabled(FT_TRUE);
    config.ores[13].block_id = VOXEL_GENERATOR_AMETHYST_BLOCK;
    config.ores[13].set_range(4, 32);
    config.ores[13].set_vein(3U, 2U);
    config.ores[13].set_enabled(FT_TRUE);
    config.ores[14].block_id = VOXEL_GENERATOR_VOLCANIC_ROCK_BLOCK;
    config.ores[14].set_range(4, 24);
    config.ores[14].set_vein(4U, 3U);
    config.ores[14].set_enabled(FT_TRUE);
    config.ores[15].block_id = VOXEL_GENERATOR_FROST_CRYSTAL_BLOCK;
    config.ores[15].set_range(4, 32);
    config.ores[15].set_vein(3U, 2U);
    config.ores[15].set_enabled(FT_TRUE);
    config.ores[16].block_id = VOXEL_GENERATOR_SHIMMER_STONE_BLOCK;
    config.ores[16].set_range(4, 40);
    config.ores[16].set_vein(4U, 3U);
    config.ores[16].set_enabled(FT_TRUE);
    config.ores[17].block_id = VOXEL_GENERATOR_AMBER_BLOCK;
    config.ores[17].set_range(4, 36);
    config.ores[17].set_vein(3U, 2U);
    config.ores[17].set_enabled(FT_TRUE);
    config.underground_structures.set_enabled(FT_TRUE, FT_TRUE);
    config.underground_structures.set_chances(4U, 3U);
    config.underground_structures.set_height_range(8, 120);
    config.underground_structures.set_shape(2U, 20U);
    config.underground_structures.set_cave_shape(2U, 3U, 20U);
    config.underground_structures.set_cave_entrances(8U, 1U);
    config.underground_structures.set_cavern_rooms(FT_FALSE, 0U, 0U);
    config.fluids.set_enabled(FT_TRUE, FT_TRUE);
    config.fluids.set_underground_lakes_enabled(FT_TRUE);
    config.fluids.set_river_settings(96, 3);
    config.fluids.set_lake_settings(48, 4U);
    config.fluids.set_underground_lake_settings(4U, 8, 96, 1U, 1U, 1U);
    config.layers.set_enabled(FT_TRUE, FT_TRUE);
    config.layers.set_depths(3U, 2U, 2U);
    config.layers.set_snowline(84);
    config.layers.set_block_palette(VOXEL_GENERATOR_SAND_BLOCK,
        VOXEL_GENERATOR_SAND_BLOCK, VOXEL_GENERATOR_SNOW_BLOCK);
    config.enable_biome_transitions = FT_TRUE;
    config.biome_transition_noise_scale = 8;
    config.biome_transition_noise_strength = 35U;
    config.enable_mountain_ridges = FT_TRUE;
    config.enable_erosion = FT_TRUE;
    config.mountain_ridge_scale = 48;
    config.mountain_ridge_strength = 8U;
    config.erosion_noise_scale = 24;
    config.erosion_strength = 3U;
    config.allow_cross_chunk_features = FT_TRUE;
    config.cross_chunk_block_writer = ft_nullptr;
    config.cross_chunk_block_writer_user_data = ft_nullptr;
    config.cross_chunk_block_reader = ft_nullptr;
    config.cross_chunk_block_reader_user_data = ft_nullptr;
    index = 0U;
    while (index < config.biome_count)
    {
        voxel_biome biome = static_cast<voxel_biome>(index);
        config.biomes[index].profile = voxel_get_biome_profile(biome);
        config.biomes[index].surface_block_id = voxel_surface_block_for_biome(biome);
        config.biomes[index].subsurface_block_id = voxel_subsurface_block_for_biome(biome);
        config.biomes[index].deep_block_id = voxel_deep_block_for_biome(biome);
        config.biomes[index].allow_shrubs = voxel_biome_has_shrubs(biome);
        config.biomes[index].allow_trees = voxel_biome_has_trees(biome);
        config.biomes[index].allow_snow_caps = FT_TRUE;
        config.biomes[index].allow_mountain_ridges = FT_TRUE;
        config.biomes[index].shrub_chance_percent = 6U;
        config.biomes[index].tree_chance_percent = 18U;
        config.biomes[index].tree_template_count = 0U;
        while (config.biomes[index].tree_template_count
            < VOXEL_MAX_BIOME_TREE_TEMPLATES)
        {
            config.biomes[index].tree_template_indices[
                config.biomes[index].tree_template_count] = 0U;
            config.biomes[index].tree_template_count += 1U;
        }
        config.biomes[index].tree_template = ft_nullptr;
        config.biome_size_min_by_biome[index] = config.biome_size_min;
        config.biome_size_max_by_biome[index] = config.biome_size_max;
        config.biome_size_override_enabled[index] = FT_FALSE;
        index += 1U;
    }
    config.biomes[VOXEL_BIOME_PLAINS].tree_template_count = 5U;
    config.biomes[VOXEL_BIOME_HILLS].tree_template_count = 5U;
    config.biomes[VOXEL_BIOME_DESERT].tree_template_count = 3U;
    config.biomes[VOXEL_BIOME_SNOW].tree_template_count = 5U;
    config.biomes[VOXEL_BIOME_MOUNTAINS].tree_template_count = 5U;
    index = 0U;
    while (index < 5U)
    {
        config.biomes[VOXEL_BIOME_PLAINS].tree_template_indices[index] = index;
        config.biomes[VOXEL_BIOME_HILLS].tree_template_indices[index] = index;
        index += 1U;
    }
    config.biomes[VOXEL_BIOME_SNOW].tree_template_indices[0] = 3U;
    config.biomes[VOXEL_BIOME_SNOW].tree_template_indices[1] = 4U;
    config.biomes[VOXEL_BIOME_SNOW].tree_template_indices[2] = 5U;
    config.biomes[VOXEL_BIOME_SNOW].tree_template_indices[3] = 11U;
    config.biomes[VOXEL_BIOME_SNOW].tree_template_indices[4] = 12U;
    config.biomes[VOXEL_BIOME_MOUNTAINS].tree_template_indices[0] = 3U;
    config.biomes[VOXEL_BIOME_MOUNTAINS].tree_template_indices[1] = 4U;
    config.biomes[VOXEL_BIOME_MOUNTAINS].tree_template_indices[2] = 5U;
    config.biomes[VOXEL_BIOME_MOUNTAINS].tree_template_indices[3] = 11U;
    config.biomes[VOXEL_BIOME_MOUNTAINS].tree_template_indices[4] = 12U;
    index = 0U;
    while (index < 3U)
    {
        config.biomes[VOXEL_BIOME_DESERT].tree_template_indices[index]
            = index + 6U;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t voxel_default_generation_config(
    voxel_generation_config &config) noexcept
{
    return (config.initialize());
}

static int32_t voxel_config_require_initialised(
    const voxel_generation_config &config) noexcept
{
    if (config.is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    return (FT_ERR_SUCCESS);
}

static uint32_t voxel_generation_config_count_template_references(
    const voxel_generation_config &config,
    const voxel_tree_template *tree_template) noexcept
{
    uint32_t reference_count;
    uint32_t index;
    uint32_t template_index;
    uint32_t biome_template_index;

    reference_count = 0U;
    template_index = 0U;
    while (template_index < config.tree_template_count)
    {
        if (&config.tree_templates[template_index] == tree_template)
            break ;
        template_index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        if (config.biomes[index].tree_template == tree_template)
            reference_count += 1U;
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_FEATURE_RULES)
    {
        if (config.features[index].template_data == tree_template)
            reference_count += 1U;
        index += 1U;
    }
    if (template_index < config.tree_template_count)
    {
        index = 0U;
        while (index < VOXEL_MAX_CUSTOM_BIOMES)
        {
            biome_template_index = 0U;
            while (biome_template_index
                < config.biomes[index].tree_template_count)
            {
                if (config.biomes[index].tree_template_indices[
                        biome_template_index] == template_index)
                    reference_count += 1U;
                biome_template_index += 1U;
            }
            index += 1U;
        }
    }
    return (reference_count);
}

int32_t voxel_generation_config::set_sea_level(int32_t value) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->sea_level = value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_noise_scales(int32_t large_scale,
    int32_t detail_scale, int32_t detail_percent) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (large_scale <= 0 || detail_scale <= 0 || detail_percent < 0
        || detail_percent > 100)
        return (FT_ERR_INVALID_ARGUMENT);
    this->large_noise_scale = large_scale;
    this->detail_noise_scale = detail_scale;
    this->detail_noise_percent = detail_percent;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_biome_size_range(
    int32_t minimum_size, int32_t maximum_size) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (minimum_size < VOXEL_BIOME_SIZE_MINIMUM
        || maximum_size < minimum_size
        || maximum_size > VOXEL_BIOME_SIZE_MAXIMUM)
        return (FT_ERR_INVALID_ARGUMENT);
    this->biome_size_min = minimum_size;
    this->biome_size_max = maximum_size;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_biome_size_control_enabled(
    ft_bool enabled) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (enabled != FT_FALSE && enabled != FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    this->enable_biome_size_control = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_biome_size_range_for_biome(
    uint32_t biome_index, int32_t minimum_size,
    int32_t maximum_size) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES
        || minimum_size < VOXEL_BIOME_SIZE_MINIMUM
        || maximum_size < minimum_size
        || maximum_size > VOXEL_BIOME_SIZE_MAXIMUM)
        return (FT_ERR_INVALID_ARGUMENT);
    this->biome_size_min_by_biome[biome_index] = minimum_size;
    this->biome_size_max_by_biome[biome_index] = maximum_size;
    this->biome_size_override_enabled[biome_index] = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_biome_size_override_enabled(
    uint32_t biome_index, ft_bool enabled) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES
        || (enabled != FT_FALSE && enabled != FT_TRUE))
        return (FT_ERR_INVALID_ARGUMENT);
    this->biome_size_override_enabled[biome_index] = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_water_chance_percent(
    uint32_t value) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (value > 100U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->water_chance_percent = value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_biome_count(uint32_t value) noexcept
{
    uint32_t index;

    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (value == 0U || value > VOXEL_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    index = this->biome_count;
    while (index < value)
    {
        if (this->biome_size_min_by_biome[index] == 0
            && this->biome_size_max_by_biome[index] == 0)
        {
            this->biome_size_min_by_biome[index] = this->biome_size_min;
            this->biome_size_max_by_biome[index] = this->biome_size_max;
            this->biome_size_override_enabled[index] = FT_FALSE;
        }
        index += 1U;
    }
    this->biome_count = value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_biome_selector(
    voxel_biome_selector selector, void *user_data) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->biome_selector = selector;
    this->biome_selector_user_data = user_data;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_cross_chunk_writer(
    voxel_cross_chunk_block_writer writer, void *user_data) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->cross_chunk_block_writer = writer;
    this->cross_chunk_block_writer_user_data = user_data;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_cross_chunk_reader(
    voxel_cross_chunk_block_reader reader, void *user_data) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->cross_chunk_block_reader = reader;
    this->cross_chunk_block_reader_user_data = user_data;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_biome(uint32_t biome_index,
    const voxel_biome_definition &biome) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].initialize(biome));
}

int32_t voxel_generation_config::set_biome_profile(uint32_t biome_index,
    const voxel_biome_profile &profile) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].set_profile(profile));
}

int32_t voxel_generation_config::set_biome_height_profile(
    uint32_t biome_index, int32_t surface_height, int32_t height_variation,
    int32_t topsoil_depth) noexcept
{
    voxel_biome_profile profile;

    profile.surface_height = surface_height;
    profile.height_variation = height_variation;
    profile.topsoil_depth = topsoil_depth;
    return (this->set_biome_profile(biome_index, profile));
}

int32_t voxel_generation_config::set_biome_block_palette(
    uint32_t biome_index, uint32_t surface_block, uint32_t subsurface_block,
    uint32_t deep_block) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].set_block_palette(surface_block,
        subsurface_block, deep_block));
}

int32_t voxel_generation_config::set_biome_decoration_policy(
    uint32_t biome_index, ft_bool shrubs, ft_bool trees,
    uint32_t shrub_chance, uint32_t tree_chance) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].set_decoration_policy(shrubs, trees,
        shrub_chance, tree_chance));
}

int32_t voxel_generation_config::set_biome_snow_caps_enabled(
    uint32_t biome_index, ft_bool enabled) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].set_snow_cap_policy(enabled));
}

int32_t voxel_generation_config::set_biome_mountain_ridges_enabled(
    uint32_t biome_index, ft_bool enabled) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].set_mountain_ridge_policy(enabled));
}

int32_t voxel_generation_config::set_biome_tree_template_override(
    uint32_t biome_index, const voxel_tree_template *value) noexcept
{
    int32_t error_code;
    uint32_t template_index;

    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    if (value == ft_nullptr)
    {
        template_index = 0U;
        while (template_index < this->tree_template_count)
        {
            if (this->biomes[biome_index].tree_template
                == &this->tree_templates[template_index])
            {
                if (voxel_generation_config_count_template_references(*this,
                        &this->tree_templates[template_index]) <= 1U)
                    return (voxel_generation_config_remove_tree_template(
                        *this, template_index));
                return (this->biomes[biome_index]
                    .set_tree_template_override(value));
            }
            template_index += 1U;
        }
        return (this->biomes[biome_index].set_tree_template_override(value));
    }
    if (voxel_template_is_valid(value) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    template_index = 0U;
    while (template_index < this->tree_template_count)
    {
        if (this->biomes[biome_index].tree_template
            == &this->tree_templates[template_index])
        {
            error_code = voxel_copy_tree_template(*value,
                    this->tree_template_blocks[template_index],
                    &this->tree_templates[template_index]);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            this->biomes[biome_index].tree_template =
                &this->tree_templates[template_index];
            return (FT_ERR_SUCCESS);
        }
        template_index += 1U;
    }
    if (this->tree_template_count >= VOXEL_MAX_TREE_TEMPLATES)
        return (FT_ERR_FULL);
    error_code = voxel_copy_tree_template(*value,
            this->tree_template_blocks[this->tree_template_count],
            &this->tree_templates[this->tree_template_count]);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->biomes[biome_index].tree_template =
        &this->tree_templates[this->tree_template_count];
    this->tree_template_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_feature(uint32_t feature_index,
    const voxel_feature_rule &feature) noexcept
{
    int32_t error_code;
    uint32_t template_index;
    const voxel_tree_template *old_template;

    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (feature_index >= VOXEL_MAX_FEATURE_RULES)
        return (FT_ERR_OUT_OF_RANGE);
    if (feature.is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    old_template = this->features[feature_index].template_data;
    if (feature.template_data == ft_nullptr && old_template != ft_nullptr)
    {
        template_index = 0U;
        while (template_index < this->tree_template_count)
        {
            if (old_template == &this->tree_templates[template_index])
                break ;
            template_index += 1U;
        }
        if (template_index < this->tree_template_count)
        {
            this->features[feature_index].template_data = ft_nullptr;
            if (voxel_generation_config_count_template_references(*this,
                    old_template) == 0U)
            {
                error_code = voxel_generation_config_remove_tree_template(
                    *this, template_index);
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
            }
        }
    }
    if (feature.template_data != ft_nullptr)
    {
        if (voxel_template_is_valid(feature.template_data) == FT_FALSE)
            return (FT_ERR_INVALID_ARGUMENT);
        template_index = 0U;
        while (template_index < this->tree_template_count)
        {
            if (this->features[feature_index].template_data
                == &this->tree_templates[template_index])
                break ;
            template_index += 1U;
        }
        if (template_index >= this->tree_template_count)
        {
            if (this->tree_template_count >= VOXEL_MAX_TREE_TEMPLATES)
                return (FT_ERR_FULL);
            template_index = this->tree_template_count;
        }
    }
    else
        template_index = this->tree_template_count;
    if (feature.template_data != ft_nullptr
        && template_index < this->tree_template_count)
    {
        error_code = voxel_copy_tree_template(*feature.template_data,
            this->tree_template_blocks[template_index],
            &this->tree_templates[template_index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    error_code = this->features[feature_index].initialize(feature);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (feature.template_data != ft_nullptr)
    {
        this->features[feature_index].template_data =
            &this->tree_templates[template_index];
        if (template_index == this->tree_template_count)
            this->tree_template_count += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_ore_rule(uint32_t ore_index,
    const voxel_ore_rule &ore) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (ore_index >= VOXEL_MAX_ORE_RULES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->ores[ore_index].initialize(ore));
}

int32_t voxel_generation_config::set_underground_structures(
    const voxel_underground_structure_config &value) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    return (this->underground_structures.initialize(value));
}

int32_t voxel_generation_config::set_fluids(
    const voxel_fluid_config &value) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    return (this->fluids.initialize(value));
}

int32_t voxel_generation_config::set_layers(
    const voxel_layer_config &value) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    return (this->layers.initialize(value));
}

int32_t voxel_generation_config::set_feature_count(uint32_t value) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (value > VOXEL_MAX_FEATURE_RULES)
        return (FT_ERR_OUT_OF_RANGE);
    this->feature_count = value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_ore_rule_count(uint32_t value) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (value > VOXEL_MAX_ORE_RULES)
        return (FT_ERR_OUT_OF_RANGE);
    this->ore_rule_count = value;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_biome_transitions_enabled(
    ft_bool enabled) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_biome_transitions = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_biome_transition_settings(
    int32_t noise_scale, uint32_t noise_strength) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (noise_scale <= 0 || noise_strength > 100U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->biome_transition_noise_scale = noise_scale;
    this->biome_transition_noise_strength = noise_strength;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_mountain_ridges_enabled(
    ft_bool enabled) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_mountain_ridges = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_erosion_enabled(
    ft_bool enabled) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_erosion = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_mountain_ridge_settings(
    int32_t scale, uint32_t strength) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (scale <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->mountain_ridge_scale = scale;
    this->mountain_ridge_strength = strength;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_erosion_settings(
    int32_t scale, uint32_t strength) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (scale <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->erosion_noise_scale = scale;
    this->erosion_strength = strength;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config::set_cross_chunk_features_enabled(
    ft_bool enabled) noexcept
{
    if (voxel_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->allow_cross_chunk_features = enabled;
    return (FT_ERR_SUCCESS);
}

static void voxel_signature_add(uint64_t &signature,
    uint64_t value) noexcept
{
    signature ^= value;
    signature = voxel_mix_u64(signature);
    return ;
}

static void voxel_signature_add_template(uint64_t &signature,
    const voxel_tree_template *tree_template) noexcept
{
    uint32_t index;

    if (tree_template == ft_nullptr)
    {
        voxel_signature_add(signature, 0U);
        return ;
    }
    if (tree_template->blocks == ft_nullptr && tree_template->block_count != 0U)
    {
        voxel_signature_add(signature, UINT64_MAX);
        return ;
    }
    voxel_signature_add(signature, 1U);
    voxel_signature_add(signature, tree_template->block_count);
    index = 0U;
    while (index < tree_template->block_count)
    {
        voxel_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(tree_template->blocks[index].offset_x)));
        voxel_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(tree_template->blocks[index].offset_y)));
        voxel_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(tree_template->blocks[index].offset_z)));
        voxel_signature_add(signature,
            tree_template->blocks[index].block_id);
        index += 1U;
    }
    return ;
}

uint32_t voxel_generation_config_signature(
    const voxel_generation_config &config) noexcept
{
    uint64_t signature;
    uint32_t index;
    uint32_t biome_template_index;

    signature = UINT64_C(0x5445525241494E31);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(config.sea_level));
    signature = voxel_mix_u64(signature);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.large_noise_scale));
    signature = voxel_mix_u64(signature);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.detail_noise_scale));
    signature = voxel_mix_u64(signature);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.detail_noise_percent));
    signature = voxel_mix_u64(signature);
    signature ^= static_cast<uint64_t>(config.enable_biome_size_control);
    signature = voxel_mix_u64(signature);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.biome_size_min));
    signature = voxel_mix_u64(signature);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.biome_size_max));
    signature = voxel_mix_u64(signature);
    signature ^= static_cast<uint64_t>(config.water_chance_percent);
    signature = voxel_mix_u64(signature);
    signature ^= static_cast<uint64_t>(config.biome_count);
    index = 0U;
    while (index < config.biome_count && index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
            config.biome_size_min_by_biome[index]));
        signature = voxel_mix_u64(signature);
        signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
            config.biome_size_max_by_biome[index]));
        signature = voxel_mix_u64(signature);
        signature ^= static_cast<uint64_t>(
            config.biome_size_override_enabled[index]);
        signature = voxel_mix_u64(signature);
        signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
            config.biomes[index].profile.surface_height));
        signature = voxel_mix_u64(signature);
        signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
            config.biomes[index].profile.height_variation));
        signature = voxel_mix_u64(signature);
        signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
            config.biomes[index].profile.topsoil_depth));
        signature = voxel_mix_u64(signature);
        signature ^= static_cast<uint64_t>(config.biomes[index].surface_block_id);
        signature ^= static_cast<uint64_t>(config.biomes[index].subsurface_block_id)
            << 8;
        signature ^= static_cast<uint64_t>(config.biomes[index].deep_block_id)
            << 16;
        signature ^= static_cast<uint64_t>(config.biomes[index].shrub_chance_percent)
            << 24;
        signature ^= static_cast<uint64_t>(config.biomes[index].tree_chance_percent)
            << 32;
        signature = voxel_mix_u64(signature);
        index += 1U;
    }
    signature ^= static_cast<uint64_t>(config.ore_rule_count) << 7;
    index = 0U;
    while (index < config.ore_rule_count && index < VOXEL_MAX_ORE_RULES)
    {
        signature ^= static_cast<uint64_t>(config.ores[index].block_id)
            + static_cast<uint64_t>(config.ores[index].enabled) * 17U
            + static_cast<uint64_t>(config.ores[index].chance_percent) * 31U
            + static_cast<uint64_t>(config.ores[index]
                .allow_ore_replacement) * 43U;
        signature = voxel_mix_u64(signature);
        index += 1U;
    }
    signature ^= static_cast<uint64_t>(config.enable_biome_transitions);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.biome_transition_noise_scale)) << 12;
    signature ^= static_cast<uint64_t>(config.biome_transition_noise_strength)
        << 20;
    signature ^= static_cast<uint64_t>(config.enable_mountain_ridges) << 1;
    signature ^= static_cast<uint64_t>(config.enable_erosion) << 2;
    signature ^= static_cast<uint64_t>(config.fluids.enable_rivers) << 3;
    signature ^= static_cast<uint64_t>(config.fluids.enable_lakes) << 4;
    signature ^= static_cast<uint64_t>(config.fluids.enable_underground_lakes) << 5;
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.fluids.river_noise_scale)) << 9;
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.fluids.river_width)) << 13;
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.fluids.lake_noise_scale)) << 17;
    signature ^= static_cast<uint64_t>(config.fluids.lake_chance_percent)
        << 21;
    signature ^= static_cast<uint64_t>(config.fluids.underground_lake_chance_percent)
        << 27;
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.fluids.underground_lake_minimum_y)) << 31;
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.fluids.underground_lake_maximum_y)) << 37;
    signature ^= static_cast<uint64_t>(config.fluids.underground_lake_depth) << 43;
    signature ^= static_cast<uint64_t>(config.fluids.underground_lake_floor_thickness) << 47;
    signature ^= static_cast<uint64_t>(config.fluids.underground_lake_roof_thickness) << 51;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .enable_ravines) << 5;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .enable_cave_rooms) << 6;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .ravine_chance_percent) << 22;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .cave_room_chance_percent) << 26;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .ravine_width) << 30;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .ravine_depth) << 34;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .cave_small_radius) << 38;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .cave_large_radius) << 42;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .cave_large_chance_percent) << 46;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .cave_entrance_chance_percent) << 50;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .cave_entrance_radius) << 54;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .enable_cavern_rooms) << 5;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .cavern_room_chance_percent) << 11;
    signature ^= static_cast<uint64_t>(config.underground_structures
        .cavern_room_radius) << 18;
    signature ^= static_cast<uint64_t>(config.layers.enable_beaches) << 38;
    signature ^= static_cast<uint64_t>(config.layers.enable_snow_caps) << 39;
    signature ^= static_cast<uint64_t>(config.layers.beach_depth) << 40;
    signature ^= static_cast<uint64_t>(config.layers.underwater_depth) << 44;
    signature ^= static_cast<uint64_t>(config.layers.snow_cap_depth) << 48;
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.layers.snow_cap_minimum_height)) << 52;
    signature ^= static_cast<uint64_t>(config.allow_cross_chunk_features)
        << 7;
    signature ^= static_cast<uint64_t>(config.cross_chunk_block_writer
        != ft_nullptr) << 8;
    voxel_signature_add(signature, static_cast<uint64_t>(
        config.biome_selector != ft_nullptr));
    index = 0U;
    while (index < config.biome_count && index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        voxel_signature_add(signature, static_cast<uint64_t>(
            config.biomes[index].allow_shrubs));
        voxel_signature_add(signature, static_cast<uint64_t>(
            config.biomes[index].allow_trees));
        voxel_signature_add(signature, static_cast<uint64_t>(
            config.biomes[index].allow_snow_caps));
        voxel_signature_add(signature, static_cast<uint64_t>(
            config.biomes[index].allow_mountain_ridges));
        voxel_signature_add(signature, static_cast<uint64_t>(
            config.biomes[index].tree_template_count));
        biome_template_index = 0U;
        while (biome_template_index
            < config.biomes[index].tree_template_count)
        {
            voxel_signature_add(signature, static_cast<uint64_t>(
                config.biomes[index].tree_template_indices[
                    biome_template_index]));
            biome_template_index += 1U;
        }
        voxel_signature_add_template(signature,
            config.biomes[index].tree_template);
        index += 1U;
    }
    voxel_signature_add(signature, static_cast<uint64_t>(
        config.tree_template_count));
    index = 0U;
    while (index < config.tree_template_count)
    {
        voxel_signature_add(signature, static_cast<uint64_t>(
            config.tree_templates[index].block_count));
        voxel_signature_add_template(signature,
            &config.tree_templates[index]);
        index += 1U;
    }
    index = 0U;
    while (index < config.ore_rule_count && index < VOXEL_MAX_ORE_RULES)
    {
        voxel_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(config.ores[index].minimum_height)));
        voxel_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(config.ores[index].maximum_height)));
        voxel_signature_add(signature, static_cast<uint64_t>(
            config.ores[index].vein_size));
        index += 1U;
    }
    voxel_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.underground_structures.minimum_height)));
    voxel_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.underground_structures.maximum_height)));
    voxel_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.layers.beach_block_id)));
    voxel_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.layers.underwater_block_id)));
    voxel_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.layers.snow_cap_block_id)));
    voxel_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.mountain_ridge_scale)));
    voxel_signature_add(signature, static_cast<uint64_t>(
        config.mountain_ridge_strength));
    voxel_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.erosion_noise_scale)));
    voxel_signature_add(signature, static_cast<uint64_t>(
        config.erosion_strength));
    voxel_signature_add(signature, static_cast<uint64_t>(config.feature_count));
    index = 0U;
    while (index < config.feature_count && index < VOXEL_MAX_FEATURE_RULES)
    {
        voxel_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(config.features[index].biome_index)));
        voxel_signature_add(signature, static_cast<uint64_t>(
            config.features[index].chance_percent));
        voxel_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(config.features[index].minimum_height)));
        voxel_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(config.features[index].maximum_height)));
        voxel_signature_add(signature, static_cast<uint64_t>(
            config.features[index].requires_dry_land));
        voxel_signature_add_template(signature,
            config.features[index].template_data);
        index += 1U;
    }
    voxel_signature_add(signature, static_cast<uint64_t>(
        config.cross_chunk_block_writer != ft_nullptr));
    signature = voxel_mix_u64(signature);
    return (static_cast<uint32_t>(signature ^ (signature >> 32)));
}

static ft_bool voxel_template_is_valid(
    const voxel_tree_template *tree_template) noexcept
{
    uint32_t index;

    if (tree_template == ft_nullptr)
        return (FT_TRUE);
    if (tree_template->blocks == ft_nullptr && tree_template->block_count != 0U)
        return (FT_FALSE);
    index = 0U;
    while (index < tree_template->block_count)
    {
        if (voxel_block_is_known(tree_template->blocks[index].block_id)
            == FT_FALSE)
            return (FT_FALSE);
        index += 1U;
    }
    return (FT_TRUE);
}

int32_t voxel_generation_config_add_tree_template(
    voxel_generation_config &config,
    const voxel_tree_template &tree_template,
    uint32_t *template_index_out) noexcept
{
    uint32_t template_index;

    if (voxel_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (template_index_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (voxel_template_is_valid(&tree_template) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (config.tree_template_count >= VOXEL_MAX_TREE_TEMPLATES)
        return (FT_ERR_FULL);
    template_index = config.tree_template_count;
    if (voxel_copy_tree_template(tree_template,
            config.tree_template_blocks[template_index],
            &config.tree_templates[template_index]) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    config.tree_template_count += 1U;
    *template_index_out = template_index;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config_remove_tree_template(
    voxel_generation_config &config, uint32_t template_index) noexcept
{
    uint32_t index;
    uint32_t biome_index;
    uint32_t biome_template_index;
    uint32_t kept_count;
    uint32_t old_template_index;
    uint32_t feature_index;

    if (voxel_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (template_index >= config.tree_template_count)
        return (FT_ERR_OUT_OF_RANGE);
    feature_index = 0U;
    while (feature_index < VOXEL_MAX_FEATURE_RULES)
    {
        if (config.features[feature_index].template_data
            == &config.tree_templates[template_index])
            return (FT_ERR_INVALID_OPERATION);
        feature_index += 1U;
    }
    biome_index = 0U;
    while (biome_index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        if (config.biomes[biome_index].tree_template
            == &config.tree_templates[template_index])
            config.biomes[biome_index].tree_template = ft_nullptr;
        kept_count = 0U;
        biome_template_index = 0U;
        while (biome_template_index
            < config.biomes[biome_index].tree_template_count)
        {
            old_template_index = config.biomes[biome_index]
                .tree_template_indices[biome_template_index];
            if (old_template_index != template_index)
            {
                if (old_template_index > template_index)
                    old_template_index -= 1U;
                config.biomes[biome_index]
                    .tree_template_indices[kept_count] = old_template_index;
                kept_count += 1U;
            }
            biome_template_index += 1U;
        }
        config.biomes[biome_index].tree_template_count = kept_count;
        biome_index += 1U;
    }
    index = template_index;
    while (index + 1U < config.tree_template_count)
    {
        biome_index = 0U;
        while (biome_index < VOXEL_MAX_CUSTOM_BIOMES)
        {
            if (config.biomes[biome_index].tree_template
                == &config.tree_templates[index + 1U])
                config.biomes[biome_index].tree_template =
                    &config.tree_templates[index];
            biome_index += 1U;
        }
        feature_index = 0U;
        while (feature_index < VOXEL_MAX_FEATURE_RULES)
        {
            if (config.features[feature_index].template_data
                == &config.tree_templates[index + 1U])
                config.features[feature_index].template_data =
                    &config.tree_templates[index];
            feature_index += 1U;
        }
        config.tree_templates[index] = config.tree_templates[index + 1U];
        ft_memcpy(config.tree_template_blocks[index],
            config.tree_template_blocks[index + 1U],
            sizeof(config.tree_template_blocks[index]));
        config.tree_templates[index].blocks = config.tree_template_blocks[index];
        index += 1U;
    }
    config.tree_template_count -= 1U;
    config.tree_templates[config.tree_template_count].blocks = ft_nullptr;
    config.tree_templates[config.tree_template_count].block_count = 0U;
    ft_memset(config.tree_template_blocks[config.tree_template_count], 0,
        sizeof(config.tree_template_blocks[config.tree_template_count]));
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config_clear_tree_templates(
    voxel_generation_config &config) noexcept
{
    uint32_t index;

    if (voxel_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    index = 0U;
    while (index < VOXEL_MAX_FEATURE_RULES)
    {
        if (config.features[index].template_data != ft_nullptr)
            return (FT_ERR_INVALID_OPERATION);
        index += 1U;
    }
    config.tree_template_count = 0U;
    index = 0U;
    while (index < VOXEL_MAX_TREE_TEMPLATES)
    {
        config.tree_templates[index].blocks = ft_nullptr;
        config.tree_templates[index].block_count = 0U;
        ft_memset(config.tree_template_blocks[index], 0,
            sizeof(config.tree_template_blocks[index]));
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        config.biomes[index].tree_template_count = 0U;
        config.biomes[index].tree_template = ft_nullptr;
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_FEATURE_RULES)
    {
        config.features[index].template_data = ft_nullptr;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config_assign_tree_template_to_biome(
    voxel_generation_config &config, uint32_t biome_index,
    uint32_t template_index) noexcept
{
    uint32_t index;

    if (voxel_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES
        || template_index >= config.tree_template_count)
        return (FT_ERR_OUT_OF_RANGE);
    index = 0U;
    while (index < config.biomes[biome_index].tree_template_count)
    {
        if (config.biomes[biome_index].tree_template_indices[index]
            == template_index)
            return (FT_ERR_ALREADY_EXISTS);
        index += 1U;
    }
    if (index >= VOXEL_MAX_BIOME_TREE_TEMPLATES)
        return (FT_ERR_FULL);
    config.biomes[biome_index].tree_template_indices[index] = template_index;
    config.biomes[biome_index].tree_template_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config_remove_tree_template_from_biome(
    voxel_generation_config &config, uint32_t biome_index,
    uint32_t template_index) noexcept
{
    uint32_t index;
    uint32_t kept_count;

    if (voxel_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= VOXEL_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    kept_count = 0U;
    index = 0U;
    while (index < config.biomes[biome_index].tree_template_count)
    {
        if (config.biomes[biome_index].tree_template_indices[index]
            != template_index)
        {
            config.biomes[biome_index].tree_template_indices[kept_count]
                = config.biomes[biome_index].tree_template_indices[index];
            kept_count += 1U;
        }
        index += 1U;
    }
    if (kept_count == config.biomes[biome_index].tree_template_count)
        return (FT_ERR_NOT_FOUND);
    config.biomes[biome_index].tree_template_count = kept_count;
    return (FT_ERR_SUCCESS);
}

const voxel_tree_template *voxel_generation_config_get_tree_template(
    const voxel_generation_config &config, uint32_t template_index) noexcept
{
    if (voxel_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (template_index >= config.tree_template_count)
        return (ft_nullptr);
    return (&config.tree_templates[template_index]);
}

ft_bool voxel_generation_config_is_valid(
    const voxel_generation_config &config) noexcept
{
    uint32_t index;
    uint32_t biome_template_index;

    if (config.is_initialised() == FT_FALSE)
        return (FT_FALSE);
    if (config.biome_count == 0U || config.biome_count > VOXEL_MAX_CUSTOM_BIOMES
        || (config.enable_biome_size_control != FT_FALSE
            && config.enable_biome_size_control != FT_TRUE)
        || config.tree_template_count > VOXEL_MAX_TREE_TEMPLATES
        || config.feature_count > VOXEL_MAX_FEATURE_RULES
        || config.ore_rule_count > VOXEL_MAX_ORE_RULES
        || config.large_noise_scale <= 0 || config.detail_noise_scale <= 0
        || config.detail_noise_percent < 0 || config.detail_noise_percent > 100
        || config.biome_size_min < VOXEL_BIOME_SIZE_MINIMUM
        || config.biome_size_max < config.biome_size_min
        || config.biome_size_max > VOXEL_BIOME_SIZE_MAXIMUM
        || config.water_chance_percent > 100
        || config.biome_transition_noise_scale <= 0
        || config.biome_transition_noise_strength > 100U
        || config.fluids.river_noise_scale <= 0
        || config.fluids.lake_noise_scale <= 0
        || config.fluids.river_width < 0
        || config.fluids.lake_chance_percent > 100
        || (config.fluids.enable_underground_lakes != FT_FALSE
            && config.fluids.enable_underground_lakes != FT_TRUE)
        || config.fluids.underground_lake_chance_percent > 100U
        || (config.fluids.enable_underground_lakes == FT_TRUE
            && (config.fluids.underground_lake_minimum_y < 1
                || config.fluids.underground_lake_maximum_y
                    < config.fluids.underground_lake_minimum_y
                || config.fluids.underground_lake_maximum_y
                    >= GAME_VOXEL_CHUNK_HEIGHT
                || config.fluids.underground_lake_depth == 0U
                || config.fluids.underground_lake_depth > 8U
                || config.fluids.underground_lake_floor_thickness == 0U
                || config.fluids.underground_lake_roof_thickness == 0U))
        || config.underground_structures.ravine_chance_percent > 100
        || config.underground_structures.cave_room_chance_percent > 100
        || config.underground_structures.minimum_height
            > config.underground_structures.maximum_height
        || config.underground_structures.cave_small_radius == 0U
        || config.underground_structures.cave_large_radius
            < config.underground_structures.cave_small_radius
        || config.underground_structures.cave_large_radius > 16U
        || config.underground_structures.cave_large_chance_percent > 100U
        || config.underground_structures.cave_entrance_chance_percent > 100U
        || config.underground_structures.cave_entrance_radius == 0U
        || config.underground_structures.cave_entrance_radius > 8U
        || config.underground_structures.cavern_room_chance_percent > 100U
        || (config.underground_structures.enable_cavern_rooms == FT_TRUE
            && (config.underground_structures.cavern_room_radius < 5U
                || config.underground_structures.cavern_room_radius > 32U))
        || (config.underground_structures.enable_cavern_rooms == FT_FALSE
            && (config.underground_structures.cavern_room_chance_percent != 0U
                || config.underground_structures.cavern_room_radius != 0U))
        || config.mountain_ridge_scale <= 0
        || config.erosion_noise_scale <= 0)
        return (FT_FALSE);
    index = 0U;
    while (index < config.tree_template_count)
    {
        if (voxel_template_is_valid(&config.tree_templates[index])
            == FT_FALSE)
            return (FT_FALSE);
        index += 1U;
    }
    if (config.layers.snow_cap_minimum_height < 0)
        return (FT_FALSE);
    index = 0U;
    while (index < config.biome_count)
    {
        if (config.biome_size_min_by_biome[index]
                < VOXEL_BIOME_SIZE_MINIMUM
            || config.biome_size_max_by_biome[index]
                < config.biome_size_min_by_biome[index]
            || config.biome_size_max_by_biome[index]
                > VOXEL_BIOME_SIZE_MAXIMUM
            || (config.biome_size_override_enabled[index] != FT_FALSE
                && config.biome_size_override_enabled[index] != FT_TRUE))
            return (FT_FALSE);
        if (config.biomes[index].is_initialised() == FT_FALSE
            || config.biomes[index].profile.height_variation < 0
            || config.biomes[index].profile.topsoil_depth < 0
            || config.biomes[index].shrub_chance_percent > 100
            || config.biomes[index].tree_chance_percent > 100
            || config.biomes[index].tree_template_count
                > VOXEL_MAX_BIOME_TREE_TEMPLATES
            || voxel_block_is_known(config.biomes[index].surface_block_id)
                == FT_FALSE
            || voxel_block_is_known(config.biomes[index].subsurface_block_id)
                == FT_FALSE
            || voxel_block_is_known(config.biomes[index].deep_block_id)
                == FT_FALSE
            || voxel_template_is_valid(config.biomes[index].tree_template)
                == FT_FALSE)
            return (FT_FALSE);
        biome_template_index = 0U;
        while (biome_template_index
            < config.biomes[index].tree_template_count)
        {
            if (config.biomes[index].tree_template_indices[
                    biome_template_index] >= config.tree_template_count)
                return (FT_FALSE);
            biome_template_index += 1U;
        }
        index += 1U;
    }
    index = 0U;
    while (index < config.ore_rule_count)
    {
        if (config.ores[index].is_initialised() == FT_FALSE
            || voxel_block_is_known(config.ores[index].block_id) == FT_FALSE
            || config.ores[index].minimum_height
                > config.ores[index].maximum_height
            || config.ores[index].minimum_height < 0
            || config.ores[index].maximum_height >= GAME_VOXEL_CHUNK_HEIGHT
            || config.ores[index].vein_size == 0U
            || config.ores[index].chance_percent > 100U)
            return (FT_FALSE);
        index += 1U;
    }
    if (voxel_block_is_known(config.layers.beach_block_id) == FT_FALSE
        || voxel_block_is_known(config.layers.underwater_block_id)
            == FT_FALSE
        || voxel_block_is_known(config.layers.snow_cap_block_id)
            == FT_FALSE)
        return (FT_FALSE);
    index = 0U;
    while (index < config.feature_count)
    {
        if (config.features[index].is_initialised() == FT_FALSE
            || config.features[index].chance_percent > 100
            || config.features[index].biome_index < -1
            || config.features[index].biome_index >=
                static_cast<int32_t>(config.biome_count)
            || config.features[index].minimum_height
                > config.features[index].maximum_height
            || voxel_template_is_valid(config.features[index].template_data)
                == FT_FALSE)
            return (FT_FALSE);
        index += 1U;
    }
    if (config.underground_structures.is_initialised() == FT_FALSE
        || config.fluids.is_initialised() == FT_FALSE
        || config.layers.is_initialised() == FT_FALSE)
        return (FT_FALSE);
    return (FT_TRUE);
}

voxel_generation_context::voxel_generation_context() noexcept
    : _config(), _configuration_signature(0U),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

voxel_generation_context::~voxel_generation_context() noexcept
{
    this->destroy();
    return ;
}

int32_t voxel_generation_context::initialize(
    const voxel_generation_config &config) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    if (voxel_generation_config_is_valid(config) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_config.initialize(config) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_configuration_signature = voxel_generation_config_signature(
        this->_config);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_context::destroy() noexcept
{
    this->_config.destroy();
    this->_configuration_signature = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t voxel_generation_context::move(
    voxel_generation_context &other) noexcept
{
    int32_t error_code;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    error_code = this->initialize(other._config);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool voxel_generation_context::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

const voxel_generation_config &voxel_generation_context::config() const noexcept
{
    return (this->_config);
}

uint32_t voxel_generation_context::configuration_signature() const noexcept
{
    return (this->_configuration_signature);
}

int32_t voxel_generation_context_initialize(
    voxel_generation_context &context,
    const voxel_generation_config &config) noexcept
{
    return (context.initialize(config));
}

ft_bool voxel_generation_context_is_initialised(
    const voxel_generation_context &context) noexcept
{
    return (context.is_initialised());
}

voxel_biome_profile voxel_get_biome_profile(voxel_biome biome) noexcept
{
    voxel_biome_profile biome_profile;

    if (biome == VOXEL_BIOME_HILLS)
    {
        biome_profile.surface_height = 80;
        biome_profile.height_variation = 8;
        biome_profile.topsoil_depth = 4;
        return (biome_profile);
    }
    if (biome == VOXEL_BIOME_DESERT)
    {
        biome_profile.surface_height = 70;
        biome_profile.height_variation = 3;
        biome_profile.topsoil_depth = 5;
        return (biome_profile);
    }
    if (biome == VOXEL_BIOME_SNOW)
    {
        biome_profile.surface_height = 84;
        biome_profile.height_variation = 6;
        biome_profile.topsoil_depth = 4;
        return (biome_profile);
    }
    if (biome == VOXEL_BIOME_MOUNTAINS)
    {
        biome_profile.surface_height = 100;
        biome_profile.height_variation = 14;
        biome_profile.topsoil_depth = 2;
        return (biome_profile);
    }
    biome_profile.surface_height = VOXEL_GENERATOR_SURFACE_HEIGHT;
    biome_profile.height_variation = 2;
    biome_profile.topsoil_depth = 3;
    return (biome_profile);
}

ft_bool voxel_can_place_tree_template(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    const voxel_tree_template &tree_template) noexcept
{
    uint32_t block_index;
    int32_t target_x;
    int32_t target_y;
    int32_t target_z;
    uint32_t block_id;

    if (tree_template.blocks == ft_nullptr)
        return (FT_FALSE);
    block_index = 0;
    while (block_index < tree_template.block_count)
    {
        target_x = local_origin_x + tree_template.blocks[block_index].offset_x;
        target_y = local_origin_y + tree_template.blocks[block_index].offset_y;
        target_z = local_origin_z + tree_template.blocks[block_index].offset_z;
        if (target_x < 0 || target_x >= GAME_VOXEL_CHUNK_WIDTH
            || target_y < 0 || target_y >= GAME_VOXEL_CHUNK_HEIGHT
            || target_z < 0 || target_z >= GAME_VOXEL_CHUNK_DEPTH)
            return (FT_FALSE);
        if (chunk.read_block(target_x, target_y, target_z, &block_id)
            != FT_ERR_SUCCESS)
            return (FT_FALSE);
        if (voxel_block_is_replaceable(block_id) == FT_FALSE)
            return (FT_FALSE);
        block_index += 1;
    }
    return (FT_TRUE);
}

int32_t voxel_place_tree_template(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    const voxel_tree_template &tree_template) noexcept
{
    if (voxel_can_place_tree_template(chunk, local_origin_x, local_origin_y,
            local_origin_z, tree_template) == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    uint32_t block_index;
    int32_t target_x;
    int32_t target_y;
    int32_t target_z;
    int32_t error_code;

    if (tree_template.blocks == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    block_index = 0;
    while (block_index < tree_template.block_count)
    {
        target_x = local_origin_x + tree_template.blocks[block_index].offset_x;
        target_y = local_origin_y + tree_template.blocks[block_index].offset_y;
        target_z = local_origin_z + tree_template.blocks[block_index].offset_z;
        if (target_x >= 0 && target_x < GAME_VOXEL_CHUNK_WIDTH
            && target_y >= 0 && target_y < GAME_VOXEL_CHUNK_HEIGHT
            && target_z >= 0 && target_z < GAME_VOXEL_CHUNK_DEPTH)
        {
            error_code = chunk.write_block(target_x, target_y, target_z,
                tree_template.blocks[block_index].block_id);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        block_index += 1;
    }
    return (FT_ERR_SUCCESS);
}

#endif
