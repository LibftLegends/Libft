#include <stdint.h>
#include "../Basic/class_nullptr.hpp"
#include "voxel.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../RNG/rng.hpp"
#include "../Game/game_voxel_chunk.hpp"
#include "voxel_internal.hpp"
#include "voxel_block_registry.hpp"

static int32_t terrain_apply_default_generation_config(
    terrain_generation_config &config) noexcept;
static ft_bool terrain_template_is_valid(
    const terrain_tree_template *tree_template) noexcept;

static int32_t terrain_copy_tree_template(
    const terrain_tree_template &source,
    terrain_tree_template_block *destination_blocks,
    terrain_tree_template *destination) noexcept
{
    if (destination_blocks == ft_nullptr || destination == ft_nullptr
        || source.block_count > TERRAIN_MAX_TREE_TEMPLATE_BLOCKS
        || (source.block_count != 0U && source.blocks == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    if (source.block_count != 0U)
        ft_memcpy(destination_blocks, source.blocks,
            sizeof(terrain_tree_template_block) * source.block_count);
    destination->blocks = destination_blocks;
    destination->block_count = source.block_count;
    return (FT_ERR_SUCCESS);
}

terrain_biome_definition::terrain_biome_definition() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), profile(),
      surface_block_id(0U), subsurface_block_id(0U), deep_block_id(0U),
      allow_shrubs(FT_FALSE), allow_trees(FT_FALSE),
      allow_snow_caps(FT_FALSE), allow_mountain_ridges(FT_FALSE),
      shrub_chance_percent(0U), tree_chance_percent(0U),
      tree_template_count(0U), tree_template_indices(), tree_template(ft_nullptr)
{
    return ;
}

terrain_biome_definition::~terrain_biome_definition() noexcept
{
    this->destroy();
    return ;
}

int32_t terrain_biome_definition::initialize() noexcept
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

int32_t terrain_biome_definition::initialize(
    const terrain_biome_definition &other) noexcept
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
    while (index < TERRAIN_MAX_BIOME_TREE_TEMPLATES)
    {
        this->tree_template_indices[index]
            = other.tree_template_indices[index];
        index += 1U;
    }
    this->tree_template = other.tree_template;
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t terrain_biome_definition::destroy() noexcept
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

uint32_t terrain_biome_definition::move(
    terrain_biome_definition &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool terrain_biome_definition::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t terrain_biome_definition::set_profile(
    const terrain_biome_profile &value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (value.height_variation < 0 || value.topsoil_depth < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->profile = value;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_biome_definition::set_block_palette(uint32_t surface_block,
    uint32_t subsurface_block, uint32_t deep_block) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->surface_block_id = surface_block;
    this->subsurface_block_id = subsurface_block;
    this->deep_block_id = deep_block;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_biome_definition::set_decoration_policy(ft_bool shrubs,
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

int32_t terrain_biome_definition::set_snow_cap_policy(
    ft_bool enabled) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->allow_snow_caps = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_biome_definition::set_mountain_ridge_policy(
    ft_bool enabled) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->allow_mountain_ridges = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_biome_definition::set_tree_template_override(
    const terrain_tree_template *value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->tree_template = value;
    return (FT_ERR_SUCCESS);
}

terrain_feature_rule::terrain_feature_rule() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      template_data(ft_nullptr), biome_index(-1), chance_percent(0U),
      minimum_height(0), maximum_height(0), requires_dry_land(FT_FALSE)
{
    return ;
}

terrain_feature_rule::~terrain_feature_rule() noexcept
{
    this->destroy();
    return ;
}

int32_t terrain_feature_rule::initialize() noexcept
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

int32_t terrain_feature_rule::initialize(
    const terrain_feature_rule &other) noexcept
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

uint32_t terrain_feature_rule::destroy() noexcept
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

uint32_t terrain_feature_rule::move(terrain_feature_rule &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool terrain_feature_rule::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t terrain_feature_rule::set_template(
    const terrain_tree_template *value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->template_data = value;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_feature_rule::set_biome_range(int32_t biome,
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

int32_t terrain_feature_rule::set_chance(uint32_t value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (value > 100U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->chance_percent = value;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_feature_rule::set_requires_dry_land(ft_bool value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->requires_dry_land = value;
    return (FT_ERR_SUCCESS);
}

terrain_ore_rule::terrain_ore_rule() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), block_id(0U),
      minimum_height(0), maximum_height(0), vein_size(0U),
      chance_percent(0U), allow_ore_replacement(FT_FALSE), enabled(FT_FALSE)
{
    return ;
}

terrain_ore_rule::~terrain_ore_rule() noexcept
{
    this->destroy();
    return ;
}

int32_t terrain_ore_rule::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    this->block_id = 0U;
    this->minimum_height = 0;
    this->maximum_height = 0;
    this->vein_size = 0U;
    this->chance_percent = 0U;
    this->allow_ore_replacement = FT_FALSE;
    this->enabled = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_ore_rule::initialize(const terrain_ore_rule &other) noexcept
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
    this->vein_size = other.vein_size;
    this->chance_percent = other.chance_percent;
    this->allow_ore_replacement = other.allow_ore_replacement;
    this->enabled = other.enabled;
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t terrain_ore_rule::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->block_id = 0U;
    this->minimum_height = 0;
    this->maximum_height = 0;
    this->vein_size = 0U;
    this->chance_percent = 0U;
    this->allow_ore_replacement = FT_FALSE;
    this->enabled = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t terrain_ore_rule::move(terrain_ore_rule &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool terrain_ore_rule::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t terrain_ore_rule::set_range(int32_t minimum,
    int32_t maximum) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (minimum > maximum)
        return (FT_ERR_INVALID_ARGUMENT);
    this->minimum_height = minimum;
    this->maximum_height = maximum;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_ore_rule::set_vein(uint32_t size,
    uint32_t chance) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (chance > 100U || (size == 0U && chance != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    this->vein_size = size;
    this->chance_percent = chance;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_ore_rule::set_enabled(ft_bool value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->enabled = value;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_ore_rule::set_ore_replacement(ft_bool value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (value > FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    this->allow_ore_replacement = value;
    return (FT_ERR_SUCCESS);
}

terrain_underground_structure_config::terrain_underground_structure_config()
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

terrain_underground_structure_config::~terrain_underground_structure_config()
    noexcept
{
    this->destroy();
    return ;
}

int32_t terrain_underground_structure_config::initialize() noexcept
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

int32_t terrain_underground_structure_config::initialize(
    const terrain_underground_structure_config &other) noexcept
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

uint32_t terrain_underground_structure_config::destroy() noexcept
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

uint32_t terrain_underground_structure_config::move(
    terrain_underground_structure_config &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool terrain_underground_structure_config::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t terrain_underground_structure_config::set_enabled(
    ft_bool ravines, ft_bool cave_rooms) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_ravines = ravines;
    this->enable_cave_rooms = cave_rooms;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_underground_structure_config::set_chances(uint32_t ravine,
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

int32_t terrain_underground_structure_config::set_height_range(
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

int32_t terrain_underground_structure_config::set_shape(uint32_t width,
    uint32_t depth) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->ravine_width = width;
    this->ravine_depth = depth;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_underground_structure_config::set_cave_shape(
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

int32_t terrain_underground_structure_config::set_cave_entrances(
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

int32_t terrain_underground_structure_config::set_cavern_rooms(
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

terrain_fluid_config::terrain_fluid_config() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      enable_rivers(FT_FALSE), enable_lakes(FT_FALSE), river_noise_scale(0),
      river_width(0), lake_noise_scale(0), lake_chance_percent(0U)
{
    return ;
}

terrain_fluid_config::~terrain_fluid_config() noexcept
{
    this->destroy();
    return ;
}

int32_t terrain_fluid_config::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    this->enable_rivers = FT_FALSE;
    this->enable_lakes = FT_FALSE;
    this->river_noise_scale = 0;
    this->river_width = 0;
    this->lake_noise_scale = 0;
    this->lake_chance_percent = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_fluid_config::initialize(
    const terrain_fluid_config &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->destroy();
    this->enable_rivers = other.enable_rivers;
    this->enable_lakes = other.enable_lakes;
    this->river_noise_scale = other.river_noise_scale;
    this->river_width = other.river_width;
    this->lake_noise_scale = other.lake_noise_scale;
    this->lake_chance_percent = other.lake_chance_percent;
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t terrain_fluid_config::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    this->enable_rivers = FT_FALSE;
    this->enable_lakes = FT_FALSE;
    this->river_noise_scale = 0;
    this->river_width = 0;
    this->lake_noise_scale = 0;
    this->lake_chance_percent = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t terrain_fluid_config::move(terrain_fluid_config &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool terrain_fluid_config::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t terrain_fluid_config::set_enabled(ft_bool rivers,
    ft_bool lakes) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_rivers = rivers;
    this->enable_lakes = lakes;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_fluid_config::set_river_settings(int32_t scale,
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

int32_t terrain_fluid_config::set_lake_settings(int32_t scale,
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

terrain_layer_config::terrain_layer_config() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      enable_beaches(FT_FALSE), enable_snow_caps(FT_FALSE), beach_depth(0U),
      underwater_depth(0U), snow_cap_depth(0U), snow_cap_minimum_height(0),
      beach_block_id(0U), underwater_block_id(0U), snow_cap_block_id(0U)
{
    return ;
}

terrain_layer_config::~terrain_layer_config() noexcept
{
    this->destroy();
    return ;
}

int32_t terrain_layer_config::initialize() noexcept
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

int32_t terrain_layer_config::initialize(
    const terrain_layer_config &other) noexcept
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

uint32_t terrain_layer_config::destroy() noexcept
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

uint32_t terrain_layer_config::move(terrain_layer_config &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool terrain_layer_config::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t terrain_layer_config::set_enabled(ft_bool beaches,
    ft_bool snow_caps) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_beaches = beaches;
    this->enable_snow_caps = snow_caps;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_layer_config::set_depths(uint32_t beach,
    uint32_t underwater, uint32_t snow) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->beach_depth = beach;
    this->underwater_depth = underwater;
    this->snow_cap_depth = snow;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_layer_config::set_snowline(
    int32_t minimum_height_value) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (minimum_height_value < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->snow_cap_minimum_height = minimum_height_value;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_layer_config::set_block_palette(uint32_t beach,
    uint32_t underwater, uint32_t snow) noexcept
{
    if (this->is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    this->beach_block_id = beach;
    this->underwater_block_id = underwater;
    this->snow_cap_block_id = snow;
    return (FT_ERR_SUCCESS);
}

static void terrain_abort_unknown_block_id(uint32_t block_id,
    const char *method_name) noexcept
{
    char decimal_buffer[10];
    ft_size_t digit_count;
    int32_t write_error;

    write_error = errno_write_stderr("terrain error: method=");
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

static const terrain_block_metadata TERRAIN_BLOCK_REGISTRY[] =
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

static_assert(sizeof(TERRAIN_BLOCK_REGISTRY)
        / sizeof(TERRAIN_BLOCK_REGISTRY[0])
        == static_cast<uint32_t>(TERRAIN_BUILTIN_BLOCK_COUNT),
    "terrain metadata must cover every built-in block ID");

static const terrain_tree_template_block TERRAIN_SMALL_OAK_TREE_BLOCKS[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {-1, 2, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 2, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 2, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 3, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 3, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 3, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 3, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 3, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK}
};

static const terrain_tree_template_block TERRAIN_SMALL_PINE_TREE_BLOCKS[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 4, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {-1, 3, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 3, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 3, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 3, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 3, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 3, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 3, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 3, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 6, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK}
};

static const terrain_tree_template_block TERRAIN_SMALL_CACTUS_TREE_BLOCKS[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_CACTUS_BLOCK}
};

static const terrain_tree_template_block TERRAIN_LARGE_OAK_TREE_BLOCKS[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 4, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {-2, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -2, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 2, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {2, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 6, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK}
};

static const terrain_tree_template_block TERRAIN_LARGE_OAK_TREE_BLOCKS_VARIANT_1[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 4, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 5, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {-2, 5, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -2, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 2, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {2, 5, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 5, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 5, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 6, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK}
};

static const terrain_tree_template_block TERRAIN_LARGE_PINE_TREE_BLOCKS[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 4, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 5, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 6, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {-1, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 5, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 5, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 6, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 6, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 6, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 6, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 7, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK}
};

static const terrain_tree_template_block TERRAIN_LARGE_PINE_TREE_BLOCKS_VARIANT_1[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 4, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 5, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {-1, 4, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 4, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 6, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 5, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 5, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 7, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 8, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK}
};

static const terrain_tree_template TERRAIN_SMALL_OAK_TREE_TEMPLATE =
{
    TERRAIN_SMALL_OAK_TREE_BLOCKS,
    static_cast<uint32_t>(sizeof(TERRAIN_SMALL_OAK_TREE_BLOCKS)
        / sizeof(TERRAIN_SMALL_OAK_TREE_BLOCKS[0]))
};

static const terrain_tree_template TERRAIN_SMALL_PINE_TREE_TEMPLATE =
{
    TERRAIN_SMALL_PINE_TREE_BLOCKS,
    static_cast<uint32_t>(sizeof(TERRAIN_SMALL_PINE_TREE_BLOCKS)
        / sizeof(TERRAIN_SMALL_PINE_TREE_BLOCKS[0]))
};

static const terrain_tree_template TERRAIN_SMALL_CACTUS_TREE_TEMPLATE =
{
    TERRAIN_SMALL_CACTUS_TREE_BLOCKS,
    static_cast<uint32_t>(sizeof(TERRAIN_SMALL_CACTUS_TREE_BLOCKS)
        / sizeof(TERRAIN_SMALL_CACTUS_TREE_BLOCKS[0]))
};

static const terrain_tree_template TERRAIN_LARGE_OAK_TREE_TEMPLATE =
{
    TERRAIN_LARGE_OAK_TREE_BLOCKS,
    static_cast<uint32_t>(sizeof(TERRAIN_LARGE_OAK_TREE_BLOCKS)
        / sizeof(TERRAIN_LARGE_OAK_TREE_BLOCKS[0]))
};

static const terrain_tree_template TERRAIN_LARGE_OAK_TREE_TEMPLATE_VARIANT_1 =
{
    TERRAIN_LARGE_OAK_TREE_BLOCKS_VARIANT_1,
    static_cast<uint32_t>(sizeof(TERRAIN_LARGE_OAK_TREE_BLOCKS_VARIANT_1)
        / sizeof(TERRAIN_LARGE_OAK_TREE_BLOCKS_VARIANT_1[0]))
};

static const terrain_tree_template TERRAIN_LARGE_PINE_TREE_TEMPLATE =
{
    TERRAIN_LARGE_PINE_TREE_BLOCKS,
    static_cast<uint32_t>(sizeof(TERRAIN_LARGE_PINE_TREE_BLOCKS)
        / sizeof(TERRAIN_LARGE_PINE_TREE_BLOCKS[0]))
};

static const terrain_tree_template TERRAIN_LARGE_PINE_TREE_TEMPLATE_VARIANT_1 =
{
    TERRAIN_LARGE_PINE_TREE_BLOCKS_VARIANT_1,
    static_cast<uint32_t>(sizeof(TERRAIN_LARGE_PINE_TREE_BLOCKS_VARIANT_1)
        / sizeof(TERRAIN_LARGE_PINE_TREE_BLOCKS_VARIANT_1[0]))
};

static const terrain_tree_template_block TERRAIN_SMALL_OAK_TREE_BLOCKS_VARIANT_1[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {-2, 2, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 2, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 2, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 2, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, -2, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 2, 2, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 2, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {2, 2, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK}
};

static const terrain_tree_template_block TERRAIN_SMALL_OAK_TREE_BLOCKS_VARIANT_2[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {0, 4, 0, TERRAIN_GENERATOR_OAK_LOG_BLOCK},
    {-1, 3, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 3, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 3, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 3, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 3, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 3, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 5, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, -1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {1, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {-1, 4, 0, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK},
    {0, 4, 1, TERRAIN_GENERATOR_OAK_LEAVES_BLOCK}
};

static const terrain_tree_template_block TERRAIN_SMALL_PINE_TREE_BLOCKS_VARIANT_1[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {-1, 3, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 3, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 3, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 3, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {-1, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 5, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK}
};

static const terrain_tree_template_block TERRAIN_SMALL_PINE_TREE_BLOCKS_VARIANT_2[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 4, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {0, 5, 0, TERRAIN_GENERATOR_PINE_LOG_BLOCK},
    {-1, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {1, 4, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, -1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 4, 1, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK},
    {0, 6, 0, TERRAIN_GENERATOR_PINE_LEAVES_BLOCK}
};

static const terrain_tree_template_block TERRAIN_SMALL_CACTUS_TREE_BLOCKS_VARIANT_1[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {1, 2, 0, TERRAIN_GENERATOR_CACTUS_BLOCK}
};

static const terrain_tree_template_block TERRAIN_SMALL_CACTUS_TREE_BLOCKS_VARIANT_2[] =
{
    {0, 0, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {0, 1, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {0, 2, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {0, 3, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {-1, 1, 0, TERRAIN_GENERATOR_CACTUS_BLOCK},
    {1, 2, 0, TERRAIN_GENERATOR_CACTUS_BLOCK}
};

static const terrain_tree_template TERRAIN_SMALL_OAK_TREE_TEMPLATE_VARIANT_1 =
{
    TERRAIN_SMALL_OAK_TREE_BLOCKS_VARIANT_1,
    static_cast<uint32_t>(sizeof(TERRAIN_SMALL_OAK_TREE_BLOCKS_VARIANT_1)
        / sizeof(TERRAIN_SMALL_OAK_TREE_BLOCKS_VARIANT_1[0]))
};

static const terrain_tree_template TERRAIN_SMALL_OAK_TREE_TEMPLATE_VARIANT_2 =
{
    TERRAIN_SMALL_OAK_TREE_BLOCKS_VARIANT_2,
    static_cast<uint32_t>(sizeof(TERRAIN_SMALL_OAK_TREE_BLOCKS_VARIANT_2)
        / sizeof(TERRAIN_SMALL_OAK_TREE_BLOCKS_VARIANT_2[0]))
};

static const terrain_tree_template TERRAIN_SMALL_PINE_TREE_TEMPLATE_VARIANT_1 =
{
    TERRAIN_SMALL_PINE_TREE_BLOCKS_VARIANT_1,
    static_cast<uint32_t>(sizeof(TERRAIN_SMALL_PINE_TREE_BLOCKS_VARIANT_1)
        / sizeof(TERRAIN_SMALL_PINE_TREE_BLOCKS_VARIANT_1[0]))
};

static const terrain_tree_template TERRAIN_SMALL_PINE_TREE_TEMPLATE_VARIANT_2 =
{
    TERRAIN_SMALL_PINE_TREE_BLOCKS_VARIANT_2,
    static_cast<uint32_t>(sizeof(TERRAIN_SMALL_PINE_TREE_BLOCKS_VARIANT_2)
        / sizeof(TERRAIN_SMALL_PINE_TREE_BLOCKS_VARIANT_2[0]))
};

static const terrain_tree_template TERRAIN_SMALL_CACTUS_TREE_TEMPLATE_VARIANT_1 =
{
    TERRAIN_SMALL_CACTUS_TREE_BLOCKS_VARIANT_1,
    static_cast<uint32_t>(sizeof(TERRAIN_SMALL_CACTUS_TREE_BLOCKS_VARIANT_1)
        / sizeof(TERRAIN_SMALL_CACTUS_TREE_BLOCKS_VARIANT_1[0]))
};

static const terrain_tree_template TERRAIN_SMALL_CACTUS_TREE_TEMPLATE_VARIANT_2 =
{
    TERRAIN_SMALL_CACTUS_TREE_BLOCKS_VARIANT_2,
    static_cast<uint32_t>(sizeof(TERRAIN_SMALL_CACTUS_TREE_BLOCKS_VARIANT_2)
        / sizeof(TERRAIN_SMALL_CACTUS_TREE_BLOCKS_VARIANT_2[0]))
};

int32_t terrain_floor_div(int32_t value, int32_t divisor) noexcept
{
    int32_t quotient;
    int32_t remainder;

    quotient = value / divisor;
    remainder = value % divisor;
    if (remainder < 0)
        quotient -= 1;
    return (quotient);
}

uint64_t terrain_mix_u64(uint64_t value) noexcept
{
    value ^= value >> 30;
    value *= UINT64_C(0xBF58476D1CE4E5B9);
    value ^= value >> 27;
    value *= UINT64_C(0x94D049BB133111EB);
    value ^= value >> 31;
    return (value);
}

double terrain_lerp(double left_value, double right_value,
    double factor) noexcept
{
    return (left_value + ((right_value - left_value) * factor));
}

double terrain_smooth_factor(double factor) noexcept
{
    return (factor * factor * (3.0 - (2.0 * factor)));
}

uint64_t terrain_seed_value(const char *seed_string) noexcept
{
    return (static_cast<uint64_t>(rng_seed_value(seed_string)));
}

uint64_t terrain_feature_seed(uint64_t seed_value, int32_t world_block_x,
    int32_t world_block_z, uint64_t salt) noexcept
{
    uint64_t feature_seed;

    feature_seed = terrain_mix_u64(seed_value ^ salt
        ^ (static_cast<uint64_t>(static_cast<int64_t>(world_block_x))
            * UINT64_C(0x9E3779B97F4A7C15))
        ^ (static_cast<uint64_t>(static_cast<int64_t>(world_block_z))
            * UINT64_C(0xBF58476D1CE4E5B9)));
    return (feature_seed);
}

double terrain_signed_unit_noise(uint64_t seed_value, int32_t grid_x,
    int32_t grid_z) noexcept
{
    uint64_t mixed_value;

    mixed_value = terrain_mix_u64(seed_value
        ^ (static_cast<uint64_t>(static_cast<int64_t>(grid_x))
            * UINT64_C(0x9E3779B97F4A7C15))
        ^ (static_cast<uint64_t>(static_cast<int64_t>(grid_z))
            * UINT64_C(0xBF58476D1CE4E5B9)));
    mixed_value = terrain_mix_u64(mixed_value);
    return (static_cast<double>(mixed_value >> 11)
        * (1.0 / 9007199254740992.0) * 2.0 - 1.0);
}

double terrain_value_noise(uint64_t seed_value, int32_t world_block_x,
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

    grid_x0 = terrain_floor_div(world_block_x, scale);
    grid_z0 = terrain_floor_div(world_block_z, scale);
    grid_x1 = grid_x0 + 1;
    grid_z1 = grid_z0 + 1;
    local_x = world_block_x - (grid_x0 * scale);
    local_z = world_block_z - (grid_z0 * scale);
    factor_x = static_cast<double>(local_x) / static_cast<double>(scale);
    factor_z = static_cast<double>(local_z) / static_cast<double>(scale);
    smooth_x = terrain_smooth_factor(factor_x);
    smooth_z = terrain_smooth_factor(factor_z);
    noise_left = terrain_signed_unit_noise(seed_value, grid_x0, grid_z0);
    noise_right = terrain_signed_unit_noise(seed_value, grid_x1, grid_z0);
    noise_top = terrain_lerp(noise_left, noise_right, smooth_x);
    noise_left = terrain_signed_unit_noise(seed_value, grid_x0, grid_z1);
    noise_right = terrain_signed_unit_noise(seed_value, grid_x1, grid_z1);
    noise_bottom = terrain_lerp(noise_left, noise_right, smooth_x);
    noise_value = terrain_lerp(noise_top, noise_bottom, smooth_z);
    return (noise_value);
}

terrain_biome terrain_pick_biome(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z,
    int32_t biome_zone_width) noexcept
{
    int32_t biome_zone_x;
    int32_t biome_zone_z;
    int64_t biome_selector;

    biome_zone_x = terrain_floor_div(world_block_x, biome_zone_width);
    biome_zone_z = terrain_floor_div(world_block_z, biome_zone_width);
    biome_selector = static_cast<int64_t>(seed_value % 5U)
        + static_cast<int64_t>(biome_zone_x)
        + static_cast<int64_t>(biome_zone_z);
    biome_selector %= 5;
    if (biome_selector < 0)
        biome_selector += 5;
    if (biome_selector == 0)
        return (TERRAIN_BIOME_PLAINS);
    if (biome_selector == 1)
        return (TERRAIN_BIOME_HILLS);
    if (biome_selector == 2)
        return (TERRAIN_BIOME_DESERT);
    if (biome_selector == 3)
        return (TERRAIN_BIOME_SNOW);
    return (TERRAIN_BIOME_MOUNTAINS);
}

static uint32_t terrain_pick_biome_with_individual_sizes(
    const terrain_generation_config &config, uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z) noexcept
{
    uint32_t biome_index;
    uint32_t best_biome;
    double best_score;

    best_biome = 0U;
    best_score = 1.0e30;
    biome_index = 0U;
    while (biome_index < config.biome_count
        && biome_index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        const int32_t width = terrain_get_biome_zone_width_for_biome(
            config, seed_value, biome_index);
        const int32_t cell_x = terrain_floor_div(world_block_x, width);
        const int32_t cell_z = terrain_floor_div(world_block_z, width);
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
                    + static_cast<int32_t>(terrain_signed_unit_noise(
                        seed_value ^ UINT64_C(0xA24BAED4963EE407)
                            ^ (static_cast<uint64_t>(biome_index + 1U)
                                * UINT64_C(0xD6E8FEB86659FD93)),
                        candidate_cell_x, candidate_cell_z)
                        * static_cast<double>(width) * 0.35);
                const int32_t site_z = origin_z + width / 2
                    + static_cast<int32_t>(terrain_signed_unit_noise(
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
                const double tie_break = (terrain_signed_unit_noise(
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

uint32_t terrain_surface_block_for_biome(terrain_biome biome) noexcept
{
    if (biome == TERRAIN_BIOME_DESERT)
        return (TERRAIN_GENERATOR_SAND_BLOCK);
    if (biome == TERRAIN_BIOME_SNOW)
        return (TERRAIN_GENERATOR_SNOW_BLOCK);
    if (biome == TERRAIN_BIOME_MOUNTAINS)
        return (TERRAIN_GENERATOR_SLATE_BLOCK);
    return (TERRAIN_GENERATOR_GRASS_BLOCK);
}

uint32_t terrain_subsurface_block_for_biome(terrain_biome biome) noexcept
{
    if (biome == TERRAIN_BIOME_DESERT)
        return (TERRAIN_GENERATOR_CANYON_ROCK_BLOCK);
    if (biome == TERRAIN_BIOME_SNOW)
        return (TERRAIN_GENERATOR_PERMAFROST_BLOCK);
    if (biome == TERRAIN_BIOME_MOUNTAINS)
        return (TERRAIN_GENERATOR_ANDESITE_BLOCK);
    return (TERRAIN_GENERATOR_DIRT_BLOCK);
}

uint32_t terrain_deep_block_for_biome(terrain_biome biome) noexcept
{
    if (biome == TERRAIN_BIOME_MOUNTAINS)
        return (TERRAIN_GENERATOR_BASALT_BLOCK);
    if (biome == TERRAIN_BIOME_DESERT)
        return (TERRAIN_GENERATOR_LIMESTONE_BLOCK);
    if (biome == TERRAIN_BIOME_SNOW)
        return (TERRAIN_GENERATOR_FROZEN_STONE_BLOCK);
    if (biome == TERRAIN_BIOME_HILLS)
        return (TERRAIN_GENERATOR_GRANITE_BLOCK);
    return (TERRAIN_GENERATOR_STONE_BLOCK);
}

ft_bool terrain_biome_has_shrubs(terrain_biome biome) noexcept
{
    if (biome == TERRAIN_BIOME_PLAINS)
        return (FT_TRUE);
    if (biome == TERRAIN_BIOME_HILLS)
        return (FT_TRUE);
    if (biome == TERRAIN_BIOME_DESERT)
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool terrain_biome_has_trees(terrain_biome biome) noexcept
{
    if (biome == TERRAIN_BIOME_PLAINS)
        return (FT_TRUE);
    if (biome == TERRAIN_BIOME_HILLS)
        return (FT_TRUE);
    if (biome == TERRAIN_BIOME_DESERT)
        return (FT_TRUE);
    if (biome == TERRAIN_BIOME_SNOW)
        return (FT_TRUE);
    if (biome == TERRAIN_BIOME_MOUNTAINS)
        return (FT_TRUE);
    return (FT_FALSE);
}

static uint32_t terrain_normalise_tree_variant(uint32_t variant_index,
    uint32_t variant_count) noexcept
{
    if (variant_count == 0U)
        return (0U);
    return (variant_index % variant_count);
}

const terrain_block_metadata &terrain_get_block_metadata(uint32_t block_id)
    noexcept
{
    const terrain_block_metadata *runtime_metadata;

    runtime_metadata = terrain_runtime_find_block_metadata(block_id);
    if (runtime_metadata != ft_nullptr)
        return (*runtime_metadata);
    if (block_id >= static_cast<uint32_t>(sizeof(TERRAIN_BLOCK_REGISTRY)
            / sizeof(TERRAIN_BLOCK_REGISTRY[0])))
    {
        terrain_abort_unknown_block_id(block_id,
            "terrain_get_block_metadata");
        return (TERRAIN_BLOCK_REGISTRY[GAME_VOXEL_AIR_BLOCK]);
    }
    return (TERRAIN_BLOCK_REGISTRY[block_id]);
}

ft_bool terrain_block_is_known(uint32_t block_id) noexcept
{
    if (terrain_runtime_block_is_known(block_id) == FT_TRUE)
        return (FT_TRUE);
    if (block_id >= static_cast<uint32_t>(sizeof(TERRAIN_BLOCK_REGISTRY)
            / sizeof(TERRAIN_BLOCK_REGISTRY[0])))
        return (FT_FALSE);
    return (FT_TRUE);
}

ft_bool terrain_block_is_solid(uint32_t block_id) noexcept
{
    return (terrain_get_block_metadata(block_id).solid);
}

ft_bool terrain_block_is_transparent(uint32_t block_id) noexcept
{
    return (terrain_get_block_metadata(block_id).transparent);
}

ft_bool terrain_block_is_liquid(uint32_t block_id) noexcept
{
    return (terrain_get_block_metadata(block_id).liquid);
}

ft_bool terrain_block_is_replaceable(uint32_t block_id) noexcept
{
    return (terrain_get_block_metadata(block_id).replaceable);
}

ft_bool terrain_block_can_host_ore(uint32_t block_id) noexcept
{
    if (terrain_get_block_metadata(block_id).can_host_ore == FT_TRUE)
        return (FT_TRUE);
    if (block_id == TERRAIN_GENERATOR_STONE_BLOCK
        || block_id == TERRAIN_GENERATOR_GRANITE_BLOCK
        || block_id == TERRAIN_GENERATOR_ANDESITE_BLOCK
        || block_id == TERRAIN_GENERATOR_DIORITE_BLOCK
        || block_id == TERRAIN_GENERATOR_LIMESTONE_BLOCK
        || block_id == TERRAIN_GENERATOR_BASALT_BLOCK)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t terrain_get_biome_zone_width(
    const terrain_generation_config &config, uint64_t seed_value) noexcept
{
    int64_t range;

    if (config.enable_biome_size_control == FT_FALSE
        || config.biome_size_min < TERRAIN_BIOME_SIZE_MINIMUM
        || config.biome_size_max < config.biome_size_min)
        return (TERRAIN_BIOME_ZONE_WIDTH);
    range = static_cast<int64_t>(config.biome_size_max)
        - static_cast<int64_t>(config.biome_size_min) + 1;
    if (range <= 1)
        return (config.biome_size_min);
    return (config.biome_size_min + static_cast<int32_t>(seed_value
        % static_cast<uint64_t>(range)));
}

int32_t terrain_get_biome_zone_width_for_biome(
    const terrain_generation_config &config, uint64_t seed_value,
    uint32_t biome_index) noexcept
{
    int32_t minimum_size;
    int32_t maximum_size;
    int64_t range;

    if (config.enable_biome_size_control == FT_FALSE
        || biome_index >= TERRAIN_MAX_CUSTOM_BIOMES)
        return (TERRAIN_BIOME_ZONE_WIDTH);
    minimum_size = config.biome_size_min;
    maximum_size = config.biome_size_max;
    if (config.biome_size_override_enabled[biome_index] == FT_TRUE)
    {
        minimum_size = config.biome_size_min_by_biome[biome_index];
        maximum_size = config.biome_size_max_by_biome[biome_index];
    }
    if (minimum_size < TERRAIN_BIOME_SIZE_MINIMUM
        || maximum_size < minimum_size)
        return (TERRAIN_BIOME_ZONE_WIDTH);
    range = static_cast<int64_t>(maximum_size)
        - static_cast<int64_t>(minimum_size) + 1;
    if (range <= 1)
        return (minimum_size);
    return (minimum_size + static_cast<int32_t>((seed_value
        ^ (UINT64_C(0x9E3779B97F4A7C15)
            * static_cast<uint64_t>(biome_index + 1U)))
        % static_cast<uint64_t>(range)));
}

ft_bool terrain_block_is_ore(uint32_t block_id) noexcept
{
    if (terrain_get_block_metadata(block_id).is_ore == FT_TRUE)
        return (FT_TRUE);
    if (block_id == TERRAIN_GENERATOR_COAL_ORE_BLOCK
        || block_id == TERRAIN_GENERATOR_IRON_ORE_BLOCK
        || block_id == TERRAIN_GENERATOR_GOLD_ORE_BLOCK
        || block_id == TERRAIN_GENERATOR_DIAMOND_ORE_BLOCK
        || block_id == TERRAIN_GENERATOR_EMERALD_ORE_BLOCK
        || block_id == TERRAIN_GENERATOR_COPPER_ORE_BLOCK)
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool terrain_block_emits_light(uint32_t block_id) noexcept
{
    return (terrain_get_block_metadata(block_id).light_emitting);
}

ft_bool terrain_block_occludes_faces(uint32_t block_id) noexcept
{
    return (terrain_get_block_metadata(block_id).occludes_faces);
}

uint32_t terrain_block_hardness(uint32_t block_id) noexcept
{
    return (terrain_get_block_metadata(block_id).hardness);
}

ft_bool terrain_block_is_breakable(uint32_t block_id) noexcept
{
    return (terrain_get_block_metadata(block_id).breakable);
}

const terrain_tree_template &terrain_small_oak_tree_template_variant(
    uint32_t variant_index) noexcept
{
    variant_index = terrain_normalise_tree_variant(variant_index, 3U);
    if (variant_index == 1U)
        return (TERRAIN_SMALL_OAK_TREE_TEMPLATE_VARIANT_1);
    if (variant_index == 2U)
        return (TERRAIN_SMALL_OAK_TREE_TEMPLATE_VARIANT_2);
    return (TERRAIN_SMALL_OAK_TREE_TEMPLATE);
}

const terrain_tree_template &terrain_small_pine_tree_template_variant(
    uint32_t variant_index) noexcept
{
    variant_index = terrain_normalise_tree_variant(variant_index, 3U);
    if (variant_index == 1U)
        return (TERRAIN_SMALL_PINE_TREE_TEMPLATE_VARIANT_1);
    if (variant_index == 2U)
        return (TERRAIN_SMALL_PINE_TREE_TEMPLATE_VARIANT_2);
    return (TERRAIN_SMALL_PINE_TREE_TEMPLATE);
}

const terrain_tree_template &terrain_small_cactus_tree_template_variant(
    uint32_t variant_index) noexcept
{
    variant_index = terrain_normalise_tree_variant(variant_index, 3U);
    if (variant_index == 1U)
        return (TERRAIN_SMALL_CACTUS_TREE_TEMPLATE_VARIANT_1);
    if (variant_index == 2U)
        return (TERRAIN_SMALL_CACTUS_TREE_TEMPLATE_VARIANT_2);
    return (TERRAIN_SMALL_CACTUS_TREE_TEMPLATE);
}

const terrain_tree_template &terrain_small_oak_tree_template(
    uint32_t variant_index) noexcept
{
    return (terrain_small_oak_tree_template_variant(variant_index));
}

const terrain_tree_template &terrain_small_pine_tree_template(
    uint32_t variant_index) noexcept
{
    return (terrain_small_pine_tree_template_variant(variant_index));
}

const terrain_tree_template &terrain_small_cactus_tree_template(
    uint32_t variant_index) noexcept
{
    return (terrain_small_cactus_tree_template_variant(variant_index));
}

const terrain_tree_template &terrain_large_oak_tree_template(
    uint32_t variant_index) noexcept
{
    return (terrain_large_oak_tree_template_variant(variant_index));
}

const terrain_tree_template &terrain_large_pine_tree_template(
    uint32_t variant_index) noexcept
{
    return (terrain_large_pine_tree_template_variant(variant_index));
}

const terrain_tree_template &terrain_large_oak_tree_template_variant(
    uint32_t variant_index) noexcept
{
    variant_index = terrain_normalise_tree_variant(variant_index, 2U);
    if (variant_index == 1U)
        return (TERRAIN_LARGE_OAK_TREE_TEMPLATE_VARIANT_1);
    return (TERRAIN_LARGE_OAK_TREE_TEMPLATE);
}

const terrain_tree_template &terrain_large_pine_tree_template_variant(
    uint32_t variant_index) noexcept
{
    variant_index = terrain_normalise_tree_variant(variant_index, 2U);
    if (variant_index == 1U)
        return (TERRAIN_LARGE_PINE_TREE_TEMPLATE_VARIANT_1);
    return (TERRAIN_LARGE_PINE_TREE_TEMPLATE);
}

const terrain_tree_template &terrain_tree_template_for_biome(
    terrain_biome biome) noexcept
{
    return (terrain_tree_template_for_biome(biome, 0U));
}

const terrain_tree_template &terrain_tree_template_for_biome(
    terrain_biome biome, uint64_t seed_value) noexcept
{
    uint32_t variant_index;

    variant_index = static_cast<uint32_t>(seed_value % 5U);
    if (biome == TERRAIN_BIOME_DESERT)
        return (terrain_small_cactus_tree_template(variant_index));
    if (biome == TERRAIN_BIOME_SNOW)
    {
        if (variant_index < 3U)
            return (terrain_small_pine_tree_template(variant_index));
        return (terrain_large_pine_tree_template(variant_index - 3U));
    }
    if (biome == TERRAIN_BIOME_MOUNTAINS)
    {
        if (variant_index < 3U)
            return (terrain_small_pine_tree_template(variant_index));
        return (terrain_large_pine_tree_template(variant_index - 3U));
    }
    if (biome == TERRAIN_BIOME_HILLS)
    {
        if (variant_index < 3U)
            return (terrain_small_oak_tree_template(variant_index));
        return (terrain_large_oak_tree_template(variant_index - 3U));
    }
    if (variant_index < 3U)
        return (terrain_small_oak_tree_template(variant_index));
    return (terrain_large_oak_tree_template(variant_index - 3U));
}

terrain_biome terrain_get_biome(int32_t world_block_x, int32_t world_block_z,
    const char *seed_string) noexcept
{
    return (terrain_pick_biome(terrain_seed_value(seed_string), world_block_x,
        world_block_z, TERRAIN_BIOME_ZONE_WIDTH));
}

uint32_t terrain_select_biome(const terrain_generation_config &config,
    uint64_t seed_value, int32_t world_block_x, int32_t world_block_z) noexcept
{
    uint32_t selected;

    if (config.biome_count == 0U)
        return (0U);
    if (config.biome_selector != ft_nullptr)
        selected = config.biome_selector(seed_value, world_block_x,
            world_block_z, config.biome_count, config.biome_selector_user_data);
    else if (config.enable_biome_size_control == FT_TRUE)
        selected = terrain_pick_biome_with_individual_sizes(config, seed_value,
            world_block_x, world_block_z);
    else
        selected = static_cast<uint32_t>(terrain_pick_biome(seed_value,
            world_block_x, world_block_z,
            terrain_get_biome_zone_width(config, seed_value)));
    return (selected % config.biome_count);
}

uint32_t terrain_get_biome_index(const terrain_generation_config &config,
    int32_t world_block_x, int32_t world_block_z,
    const char *seed_string) noexcept
{
    return (terrain_select_biome(config, terrain_seed_value(seed_string),
        world_block_x, world_block_z));
}

terrain_generation_config::terrain_generation_config() noexcept
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
      cross_chunk_block_writer_user_data(ft_nullptr)
{
    return ;
}

terrain_generation_config::~terrain_generation_config() noexcept
{
    this->destroy();
    return ;
}

int32_t terrain_generation_config::initialize() noexcept
{
    uint32_t index;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    index = 0U;
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        this->biomes[index].initialize();
        index += 1U;
    }
    index = 0U;
    while (index < TERRAIN_MAX_FEATURE_RULES)
    {
        this->features[index].initialize();
        index += 1U;
    }
    index = 0U;
    while (index < TERRAIN_MAX_ORE_RULES)
    {
        this->ores[index].initialize();
        index += 1U;
    }
    this->underground_structures.initialize();
    this->fluids.initialize();
    this->layers.initialize();
    if (terrain_apply_default_generation_config(*this)
        != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::initialize(
    const terrain_generation_config &other) noexcept
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
    uint32_t index;

    index = 0U;
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
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
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
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
    while (index < TERRAIN_MAX_FEATURE_RULES)
    {
        this->features[index].initialize(other.features[index]);
        index += 1U;
    }
    index = 0U;
    while (index < TERRAIN_MAX_ORE_RULES)
    {
        this->ores[index].initialize(other.ores[index]);
        index += 1U;
    }
    this->_initialised_state = other._initialised_state;
    return (FT_ERR_SUCCESS);
}

uint32_t terrain_generation_config::destroy() noexcept
{
    uint32_t index;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    index = 0U;
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        this->biomes[index].destroy();
        index += 1U;
    }
    ft_memset(this->tree_templates, 0, sizeof(this->tree_templates));
    ft_memset(this->tree_template_blocks, 0,
        sizeof(this->tree_template_blocks));
    index = 0U;
    while (index < TERRAIN_MAX_FEATURE_RULES)
    {
        this->features[index].destroy();
        index += 1U;
    }
    index = 0U;
    while (index < TERRAIN_MAX_ORE_RULES)
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
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t terrain_generation_config::move(
    terrain_generation_config &other) noexcept
{
    int32_t error_code;

    error_code = this->initialize(other);
    if (error_code != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(error_code));
    other.destroy();
    return (FT_ERR_SUCCESS);
}

ft_bool terrain_generation_config::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

static int32_t terrain_apply_default_generation_config(
    terrain_generation_config &config) noexcept
{
    uint32_t index;

    config.sea_level = TERRAIN_GENERATOR_SEA_LEVEL;
    config.large_noise_scale = 32;
    config.detail_noise_scale = 8;
    config.detail_noise_percent = 50;
    config.enable_biome_size_control = FT_TRUE;
    config.biome_size_min = TERRAIN_BIOME_ZONE_WIDTH;
    config.biome_size_max = TERRAIN_BIOME_ZONE_WIDTH;
    index = 0U;
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        config.biome_size_min_by_biome[index] = config.biome_size_min;
        config.biome_size_max_by_biome[index] = config.biome_size_max;
        config.biome_size_override_enabled[index] = FT_FALSE;
        index += 1U;
    }
    config.water_chance_percent = 0U;
    config.biome_count = 5U;
    config.tree_template_count = 13U;
    terrain_copy_tree_template(terrain_small_oak_tree_template_variant(0U),
        config.tree_template_blocks[0], &config.tree_templates[0]);
    terrain_copy_tree_template(terrain_small_oak_tree_template_variant(1U),
        config.tree_template_blocks[1], &config.tree_templates[1]);
    terrain_copy_tree_template(terrain_small_oak_tree_template_variant(2U),
        config.tree_template_blocks[2], &config.tree_templates[2]);
    terrain_copy_tree_template(terrain_small_pine_tree_template_variant(0U),
        config.tree_template_blocks[3], &config.tree_templates[3]);
    terrain_copy_tree_template(terrain_small_pine_tree_template_variant(1U),
        config.tree_template_blocks[4], &config.tree_templates[4]);
    terrain_copy_tree_template(terrain_small_pine_tree_template_variant(2U),
        config.tree_template_blocks[5], &config.tree_templates[5]);
    terrain_copy_tree_template(terrain_small_cactus_tree_template_variant(0U),
        config.tree_template_blocks[6], &config.tree_templates[6]);
    terrain_copy_tree_template(terrain_small_cactus_tree_template_variant(1U),
        config.tree_template_blocks[7], &config.tree_templates[7]);
    terrain_copy_tree_template(terrain_small_cactus_tree_template_variant(2U),
        config.tree_template_blocks[8], &config.tree_templates[8]);
    terrain_copy_tree_template(terrain_large_oak_tree_template_variant(0U),
        config.tree_template_blocks[9], &config.tree_templates[9]);
    terrain_copy_tree_template(terrain_large_oak_tree_template_variant(1U),
        config.tree_template_blocks[10], &config.tree_templates[10]);
    terrain_copy_tree_template(terrain_large_pine_tree_template_variant(0U),
        config.tree_template_blocks[11], &config.tree_templates[11]);
    terrain_copy_tree_template(terrain_large_pine_tree_template_variant(1U),
        config.tree_template_blocks[12], &config.tree_templates[12]);
    config.biome_selector = ft_nullptr;
    config.biome_selector_user_data = ft_nullptr;
    config.ore_rule_count = 18U;
    config.ores[0].block_id = TERRAIN_GENERATOR_COAL_ORE_BLOCK;
    config.ores[0].set_range(8, 120);
    config.ores[0].set_vein(8U, 12U);
    config.ores[0].set_enabled(FT_TRUE);
    config.ores[1].block_id = TERRAIN_GENERATOR_IRON_ORE_BLOCK;
    config.ores[1].set_range(4, 80);
    config.ores[1].set_vein(6U, 8U);
    config.ores[1].set_enabled(FT_TRUE);
    config.ores[2].block_id = TERRAIN_GENERATOR_GOLD_ORE_BLOCK;
    config.ores[2].set_range(4, 48);
    config.ores[2].set_vein(4U, 4U);
    config.ores[2].set_enabled(FT_TRUE);
    config.ores[3].block_id = TERRAIN_GENERATOR_COPPER_ORE_BLOCK;
    config.ores[3].set_range(16, 90);
    config.ores[3].set_vein(6U, 10U);
    config.ores[3].set_enabled(FT_TRUE);
    config.ores[4].block_id = TERRAIN_GENERATOR_DIAMOND_ORE_BLOCK;
    config.ores[4].set_range(4, 24);
    config.ores[4].set_vein(3U, 2U);
    config.ores[4].set_enabled(FT_TRUE);
    config.ores[5].block_id = TERRAIN_GENERATOR_EMERALD_ORE_BLOCK;
    config.ores[5].set_range(8, 36);
    config.ores[5].set_vein(2U, 2U);
    config.ores[5].set_enabled(FT_TRUE);
    config.ores[6].block_id = TERRAIN_GENERATOR_DIORITE_BLOCK;
    config.ores[6].set_range(4, 55);
    config.ores[6].set_vein(10U, 14U);
    config.ores[6].set_enabled(FT_TRUE);
    config.ores[7].block_id = TERRAIN_GENERATOR_GRAVEL_BLOCK;
    config.ores[7].set_range(20, 55);
    config.ores[7].set_vein(10U, 14U);
    config.ores[7].set_enabled(FT_TRUE);
    config.ores[8].block_id = TERRAIN_GENERATOR_CLAY_BLOCK;
    config.ores[8].set_range(40, 60);
    config.ores[8].set_vein(6U, 10U);
    config.ores[8].set_enabled(FT_TRUE);
    config.ores[9].block_id = TERRAIN_GENERATOR_MOSSY_STONE_BLOCK;
    config.ores[9].set_range(8, 55);
    config.ores[9].set_vein(5U, 6U);
    config.ores[9].set_enabled(FT_TRUE);
    config.ores[10].block_id = TERRAIN_GENERATOR_CRACKED_STONE_BLOCK;
    config.ores[10].set_range(4, 40);
    config.ores[10].set_vein(5U, 6U);
    config.ores[10].set_enabled(FT_TRUE);
    config.ores[11].block_id = TERRAIN_GENERATOR_OBSIDIAN_BLOCK;
    config.ores[11].set_range(4, 16);
    config.ores[11].set_vein(2U, 2U);
    config.ores[11].set_enabled(FT_TRUE);
    config.ores[12].block_id = TERRAIN_GENERATOR_QUARTZ_BLOCK;
    config.ores[12].set_range(4, 48);
    config.ores[12].set_vein(4U, 5U);
    config.ores[12].set_enabled(FT_TRUE);
    config.ores[13].block_id = TERRAIN_GENERATOR_AMETHYST_BLOCK;
    config.ores[13].set_range(4, 32);
    config.ores[13].set_vein(3U, 2U);
    config.ores[13].set_enabled(FT_TRUE);
    config.ores[14].block_id = TERRAIN_GENERATOR_VOLCANIC_ROCK_BLOCK;
    config.ores[14].set_range(4, 24);
    config.ores[14].set_vein(4U, 3U);
    config.ores[14].set_enabled(FT_TRUE);
    config.ores[15].block_id = TERRAIN_GENERATOR_FROST_CRYSTAL_BLOCK;
    config.ores[15].set_range(4, 32);
    config.ores[15].set_vein(3U, 2U);
    config.ores[15].set_enabled(FT_TRUE);
    config.ores[16].block_id = TERRAIN_GENERATOR_SHIMMER_STONE_BLOCK;
    config.ores[16].set_range(4, 40);
    config.ores[16].set_vein(4U, 3U);
    config.ores[16].set_enabled(FT_TRUE);
    config.ores[17].block_id = TERRAIN_GENERATOR_AMBER_BLOCK;
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
    config.fluids.set_river_settings(96, 3);
    config.fluids.set_lake_settings(48, 4U);
    config.layers.set_enabled(FT_TRUE, FT_TRUE);
    config.layers.set_depths(3U, 2U, 2U);
    config.layers.set_snowline(84);
    config.layers.set_block_palette(TERRAIN_GENERATOR_SAND_BLOCK,
        TERRAIN_GENERATOR_SAND_BLOCK, TERRAIN_GENERATOR_SNOW_BLOCK);
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
    index = 0U;
    while (index < config.biome_count)
    {
        terrain_biome biome = static_cast<terrain_biome>(index);
        config.biomes[index].profile = terrain_get_biome_profile(biome);
        config.biomes[index].surface_block_id = terrain_surface_block_for_biome(biome);
        config.biomes[index].subsurface_block_id = terrain_subsurface_block_for_biome(biome);
        config.biomes[index].deep_block_id = terrain_deep_block_for_biome(biome);
        config.biomes[index].allow_shrubs = terrain_biome_has_shrubs(biome);
        config.biomes[index].allow_trees = terrain_biome_has_trees(biome);
        config.biomes[index].allow_snow_caps = FT_TRUE;
        config.biomes[index].allow_mountain_ridges = FT_TRUE;
        config.biomes[index].shrub_chance_percent = 6U;
        config.biomes[index].tree_chance_percent = 18U;
        config.biomes[index].tree_template_count = 0U;
        while (config.biomes[index].tree_template_count
            < TERRAIN_MAX_BIOME_TREE_TEMPLATES)
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
    config.biomes[TERRAIN_BIOME_PLAINS].tree_template_count = 5U;
    config.biomes[TERRAIN_BIOME_HILLS].tree_template_count = 5U;
    config.biomes[TERRAIN_BIOME_DESERT].tree_template_count = 3U;
    config.biomes[TERRAIN_BIOME_SNOW].tree_template_count = 5U;
    config.biomes[TERRAIN_BIOME_MOUNTAINS].tree_template_count = 5U;
    index = 0U;
    while (index < 5U)
    {
        config.biomes[TERRAIN_BIOME_PLAINS].tree_template_indices[index] = index;
        config.biomes[TERRAIN_BIOME_HILLS].tree_template_indices[index] = index;
        index += 1U;
    }
    config.biomes[TERRAIN_BIOME_SNOW].tree_template_indices[0] = 3U;
    config.biomes[TERRAIN_BIOME_SNOW].tree_template_indices[1] = 4U;
    config.biomes[TERRAIN_BIOME_SNOW].tree_template_indices[2] = 5U;
    config.biomes[TERRAIN_BIOME_SNOW].tree_template_indices[3] = 11U;
    config.biomes[TERRAIN_BIOME_SNOW].tree_template_indices[4] = 12U;
    config.biomes[TERRAIN_BIOME_MOUNTAINS].tree_template_indices[0] = 3U;
    config.biomes[TERRAIN_BIOME_MOUNTAINS].tree_template_indices[1] = 4U;
    config.biomes[TERRAIN_BIOME_MOUNTAINS].tree_template_indices[2] = 5U;
    config.biomes[TERRAIN_BIOME_MOUNTAINS].tree_template_indices[3] = 11U;
    config.biomes[TERRAIN_BIOME_MOUNTAINS].tree_template_indices[4] = 12U;
    index = 0U;
    while (index < 3U)
    {
        config.biomes[TERRAIN_BIOME_DESERT].tree_template_indices[index]
            = index + 6U;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t terrain_default_generation_config(
    terrain_generation_config &config) noexcept
{
    return (config.initialize());
}

static int32_t terrain_config_require_initialised(
    const terrain_generation_config &config) noexcept
{
    if (config.is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    return (FT_ERR_SUCCESS);
}

static uint32_t terrain_generation_config_count_template_references(
    const terrain_generation_config &config,
    const terrain_tree_template *tree_template) noexcept
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
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        if (config.biomes[index].tree_template == tree_template)
            reference_count += 1U;
        index += 1U;
    }
    index = 0U;
    while (index < TERRAIN_MAX_FEATURE_RULES)
    {
        if (config.features[index].template_data == tree_template)
            reference_count += 1U;
        index += 1U;
    }
    if (template_index < config.tree_template_count)
    {
        index = 0U;
        while (index < TERRAIN_MAX_CUSTOM_BIOMES)
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

int32_t terrain_generation_config::set_sea_level(int32_t value) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->sea_level = value;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_noise_scales(int32_t large_scale,
    int32_t detail_scale, int32_t detail_percent) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (large_scale <= 0 || detail_scale <= 0 || detail_percent < 0
        || detail_percent > 100)
        return (FT_ERR_INVALID_ARGUMENT);
    this->large_noise_scale = large_scale;
    this->detail_noise_scale = detail_scale;
    this->detail_noise_percent = detail_percent;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_biome_size_range(
    int32_t minimum_size, int32_t maximum_size) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (minimum_size < TERRAIN_BIOME_SIZE_MINIMUM
        || maximum_size < minimum_size
        || maximum_size > TERRAIN_BIOME_SIZE_MAXIMUM)
        return (FT_ERR_INVALID_ARGUMENT);
    this->biome_size_min = minimum_size;
    this->biome_size_max = maximum_size;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_biome_size_control_enabled(
    ft_bool enabled) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (enabled != FT_FALSE && enabled != FT_TRUE)
        return (FT_ERR_INVALID_ARGUMENT);
    this->enable_biome_size_control = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_biome_size_range_for_biome(
    uint32_t biome_index, int32_t minimum_size,
    int32_t maximum_size) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES
        || minimum_size < TERRAIN_BIOME_SIZE_MINIMUM
        || maximum_size < minimum_size
        || maximum_size > TERRAIN_BIOME_SIZE_MAXIMUM)
        return (FT_ERR_INVALID_ARGUMENT);
    this->biome_size_min_by_biome[biome_index] = minimum_size;
    this->biome_size_max_by_biome[biome_index] = maximum_size;
    this->biome_size_override_enabled[biome_index] = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_biome_size_override_enabled(
    uint32_t biome_index, ft_bool enabled) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES
        || (enabled != FT_FALSE && enabled != FT_TRUE))
        return (FT_ERR_INVALID_ARGUMENT);
    this->biome_size_override_enabled[biome_index] = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_water_chance_percent(
    uint32_t value) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (value > 100U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->water_chance_percent = value;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_biome_count(uint32_t value) noexcept
{
    uint32_t index;

    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (value == 0U || value > TERRAIN_MAX_CUSTOM_BIOMES)
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

int32_t terrain_generation_config::set_biome_selector(
    terrain_biome_selector selector, void *user_data) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->biome_selector = selector;
    this->biome_selector_user_data = user_data;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_cross_chunk_writer(
    terrain_cross_chunk_block_writer writer, void *user_data) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->cross_chunk_block_writer = writer;
    this->cross_chunk_block_writer_user_data = user_data;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_biome(uint32_t biome_index,
    const terrain_biome_definition &biome) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].initialize(biome));
}

int32_t terrain_generation_config::set_biome_profile(uint32_t biome_index,
    const terrain_biome_profile &profile) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].set_profile(profile));
}

int32_t terrain_generation_config::set_biome_height_profile(
    uint32_t biome_index, int32_t surface_height, int32_t height_variation,
    int32_t topsoil_depth) noexcept
{
    terrain_biome_profile profile;

    profile.surface_height = surface_height;
    profile.height_variation = height_variation;
    profile.topsoil_depth = topsoil_depth;
    return (this->set_biome_profile(biome_index, profile));
}

int32_t terrain_generation_config::set_biome_block_palette(
    uint32_t biome_index, uint32_t surface_block, uint32_t subsurface_block,
    uint32_t deep_block) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].set_block_palette(surface_block,
        subsurface_block, deep_block));
}

int32_t terrain_generation_config::set_biome_decoration_policy(
    uint32_t biome_index, ft_bool shrubs, ft_bool trees,
    uint32_t shrub_chance, uint32_t tree_chance) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].set_decoration_policy(shrubs, trees,
        shrub_chance, tree_chance));
}

int32_t terrain_generation_config::set_biome_snow_caps_enabled(
    uint32_t biome_index, ft_bool enabled) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].set_snow_cap_policy(enabled));
}

int32_t terrain_generation_config::set_biome_mountain_ridges_enabled(
    uint32_t biome_index, ft_bool enabled) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->biomes[biome_index].set_mountain_ridge_policy(enabled));
}

int32_t terrain_generation_config::set_biome_tree_template_override(
    uint32_t biome_index, const terrain_tree_template *value) noexcept
{
    int32_t error_code;
    uint32_t template_index;

    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES)
        return (FT_ERR_OUT_OF_RANGE);
    if (value == ft_nullptr)
    {
        template_index = 0U;
        while (template_index < this->tree_template_count)
        {
            if (this->biomes[biome_index].tree_template
                == &this->tree_templates[template_index])
            {
                if (terrain_generation_config_count_template_references(*this,
                        &this->tree_templates[template_index]) <= 1U)
                    return (terrain_generation_config_remove_tree_template(
                        *this, template_index));
                return (this->biomes[biome_index]
                    .set_tree_template_override(value));
            }
            template_index += 1U;
        }
        return (this->biomes[biome_index].set_tree_template_override(value));
    }
    if (terrain_template_is_valid(value) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    template_index = 0U;
    while (template_index < this->tree_template_count)
    {
        if (this->biomes[biome_index].tree_template
            == &this->tree_templates[template_index])
        {
            error_code = terrain_copy_tree_template(*value,
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
    if (this->tree_template_count >= TERRAIN_MAX_TREE_TEMPLATES)
        return (FT_ERR_FULL);
    error_code = terrain_copy_tree_template(*value,
            this->tree_template_blocks[this->tree_template_count],
            &this->tree_templates[this->tree_template_count]);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    this->biomes[biome_index].tree_template =
        &this->tree_templates[this->tree_template_count];
    this->tree_template_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_feature(uint32_t feature_index,
    const terrain_feature_rule &feature) noexcept
{
    int32_t error_code;
    uint32_t template_index;
    const terrain_tree_template *old_template;

    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (feature_index >= TERRAIN_MAX_FEATURE_RULES)
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
            if (terrain_generation_config_count_template_references(*this,
                    old_template) == 0U)
            {
                error_code = terrain_generation_config_remove_tree_template(
                    *this, template_index);
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
            }
        }
    }
    if (feature.template_data != ft_nullptr)
    {
        if (terrain_template_is_valid(feature.template_data) == FT_FALSE)
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
            if (this->tree_template_count >= TERRAIN_MAX_TREE_TEMPLATES)
                return (FT_ERR_FULL);
            template_index = this->tree_template_count;
        }
    }
    else
        template_index = this->tree_template_count;
    if (feature.template_data != ft_nullptr
        && template_index < this->tree_template_count)
    {
        error_code = terrain_copy_tree_template(*feature.template_data,
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

int32_t terrain_generation_config::set_ore_rule(uint32_t ore_index,
    const terrain_ore_rule &ore) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (ore_index >= TERRAIN_MAX_ORE_RULES)
        return (FT_ERR_OUT_OF_RANGE);
    return (this->ores[ore_index].initialize(ore));
}

int32_t terrain_generation_config::set_underground_structures(
    const terrain_underground_structure_config &value) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    return (this->underground_structures.initialize(value));
}

int32_t terrain_generation_config::set_fluids(
    const terrain_fluid_config &value) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    return (this->fluids.initialize(value));
}

int32_t terrain_generation_config::set_layers(
    const terrain_layer_config &value) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    return (this->layers.initialize(value));
}

int32_t terrain_generation_config::set_feature_count(uint32_t value) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (value > TERRAIN_MAX_FEATURE_RULES)
        return (FT_ERR_OUT_OF_RANGE);
    this->feature_count = value;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_ore_rule_count(uint32_t value) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (value > TERRAIN_MAX_ORE_RULES)
        return (FT_ERR_OUT_OF_RANGE);
    this->ore_rule_count = value;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_biome_transitions_enabled(
    ft_bool enabled) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_biome_transitions = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_biome_transition_settings(
    int32_t noise_scale, uint32_t noise_strength) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (noise_scale <= 0 || noise_strength > 100U)
        return (FT_ERR_INVALID_ARGUMENT);
    this->biome_transition_noise_scale = noise_scale;
    this->biome_transition_noise_strength = noise_strength;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_mountain_ridges_enabled(
    ft_bool enabled) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_mountain_ridges = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_erosion_enabled(
    ft_bool enabled) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->enable_erosion = enabled;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_mountain_ridge_settings(
    int32_t scale, uint32_t strength) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (scale <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->mountain_ridge_scale = scale;
    this->mountain_ridge_strength = strength;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_erosion_settings(
    int32_t scale, uint32_t strength) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (scale <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    this->erosion_noise_scale = scale;
    this->erosion_strength = strength;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config::set_cross_chunk_features_enabled(
    ft_bool enabled) noexcept
{
    if (terrain_config_require_initialised(*this) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    this->allow_cross_chunk_features = enabled;
    return (FT_ERR_SUCCESS);
}

static void terrain_signature_add(uint64_t &signature,
    uint64_t value) noexcept
{
    signature ^= value;
    signature = terrain_mix_u64(signature);
    return ;
}

static void terrain_signature_add_template(uint64_t &signature,
    const terrain_tree_template *tree_template) noexcept
{
    uint32_t index;

    if (tree_template == ft_nullptr)
    {
        terrain_signature_add(signature, 0U);
        return ;
    }
    if (tree_template->blocks == ft_nullptr && tree_template->block_count != 0U)
    {
        terrain_signature_add(signature, UINT64_MAX);
        return ;
    }
    terrain_signature_add(signature, 1U);
    terrain_signature_add(signature, tree_template->block_count);
    index = 0U;
    while (index < tree_template->block_count)
    {
        terrain_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(tree_template->blocks[index].offset_x)));
        terrain_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(tree_template->blocks[index].offset_y)));
        terrain_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(tree_template->blocks[index].offset_z)));
        terrain_signature_add(signature,
            tree_template->blocks[index].block_id);
        index += 1U;
    }
    return ;
}

uint32_t terrain_generation_config_signature(
    const terrain_generation_config &config) noexcept
{
    uint64_t signature;
    uint32_t index;
    uint32_t biome_template_index;

    signature = UINT64_C(0x5445525241494E31);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(config.sea_level));
    signature = terrain_mix_u64(signature);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.large_noise_scale));
    signature = terrain_mix_u64(signature);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.detail_noise_scale));
    signature = terrain_mix_u64(signature);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.detail_noise_percent));
    signature = terrain_mix_u64(signature);
    signature ^= static_cast<uint64_t>(config.enable_biome_size_control);
    signature = terrain_mix_u64(signature);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.biome_size_min));
    signature = terrain_mix_u64(signature);
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.biome_size_max));
    signature = terrain_mix_u64(signature);
    signature ^= static_cast<uint64_t>(config.water_chance_percent);
    signature = terrain_mix_u64(signature);
    signature ^= static_cast<uint64_t>(config.biome_count);
    index = 0U;
    while (index < config.biome_count && index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
            config.biome_size_min_by_biome[index]));
        signature = terrain_mix_u64(signature);
        signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
            config.biome_size_max_by_biome[index]));
        signature = terrain_mix_u64(signature);
        signature ^= static_cast<uint64_t>(
            config.biome_size_override_enabled[index]);
        signature = terrain_mix_u64(signature);
        signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
            config.biomes[index].profile.surface_height));
        signature = terrain_mix_u64(signature);
        signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
            config.biomes[index].profile.height_variation));
        signature = terrain_mix_u64(signature);
        signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
            config.biomes[index].profile.topsoil_depth));
        signature = terrain_mix_u64(signature);
        signature ^= static_cast<uint64_t>(config.biomes[index].surface_block_id);
        signature ^= static_cast<uint64_t>(config.biomes[index].subsurface_block_id)
            << 8;
        signature ^= static_cast<uint64_t>(config.biomes[index].deep_block_id)
            << 16;
        signature ^= static_cast<uint64_t>(config.biomes[index].shrub_chance_percent)
            << 24;
        signature ^= static_cast<uint64_t>(config.biomes[index].tree_chance_percent)
            << 32;
        signature = terrain_mix_u64(signature);
        index += 1U;
    }
    signature ^= static_cast<uint64_t>(config.ore_rule_count) << 7;
    index = 0U;
    while (index < config.ore_rule_count && index < TERRAIN_MAX_ORE_RULES)
    {
        signature ^= static_cast<uint64_t>(config.ores[index].block_id)
            + static_cast<uint64_t>(config.ores[index].enabled) * 17U
            + static_cast<uint64_t>(config.ores[index].chance_percent) * 31U
            + static_cast<uint64_t>(config.ores[index]
                .allow_ore_replacement) * 43U;
        signature = terrain_mix_u64(signature);
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
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.fluids.river_noise_scale)) << 9;
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.fluids.river_width)) << 13;
    signature ^= static_cast<uint64_t>(static_cast<uint32_t>(
        config.fluids.lake_noise_scale)) << 17;
    signature ^= static_cast<uint64_t>(config.fluids.lake_chance_percent)
        << 21;
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
    terrain_signature_add(signature, static_cast<uint64_t>(
        config.biome_selector != ft_nullptr));
    index = 0U;
    while (index < config.biome_count && index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        terrain_signature_add(signature, static_cast<uint64_t>(
            config.biomes[index].allow_shrubs));
        terrain_signature_add(signature, static_cast<uint64_t>(
            config.biomes[index].allow_trees));
        terrain_signature_add(signature, static_cast<uint64_t>(
            config.biomes[index].allow_snow_caps));
        terrain_signature_add(signature, static_cast<uint64_t>(
            config.biomes[index].allow_mountain_ridges));
        terrain_signature_add(signature, static_cast<uint64_t>(
            config.biomes[index].tree_template_count));
        biome_template_index = 0U;
        while (biome_template_index
            < config.biomes[index].tree_template_count)
        {
            terrain_signature_add(signature, static_cast<uint64_t>(
                config.biomes[index].tree_template_indices[
                    biome_template_index]));
            biome_template_index += 1U;
        }
        terrain_signature_add_template(signature,
            config.biomes[index].tree_template);
        index += 1U;
    }
    terrain_signature_add(signature, static_cast<uint64_t>(
        config.tree_template_count));
    index = 0U;
    while (index < config.tree_template_count)
    {
        terrain_signature_add(signature, static_cast<uint64_t>(
            config.tree_templates[index].block_count));
        terrain_signature_add_template(signature,
            &config.tree_templates[index]);
        index += 1U;
    }
    index = 0U;
    while (index < config.ore_rule_count && index < TERRAIN_MAX_ORE_RULES)
    {
        terrain_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(config.ores[index].minimum_height)));
        terrain_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(config.ores[index].maximum_height)));
        terrain_signature_add(signature, static_cast<uint64_t>(
            config.ores[index].vein_size));
        index += 1U;
    }
    terrain_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.underground_structures.minimum_height)));
    terrain_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.underground_structures.maximum_height)));
    terrain_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.layers.beach_block_id)));
    terrain_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.layers.underwater_block_id)));
    terrain_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.layers.snow_cap_block_id)));
    terrain_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.mountain_ridge_scale)));
    terrain_signature_add(signature, static_cast<uint64_t>(
        config.mountain_ridge_strength));
    terrain_signature_add(signature, static_cast<uint64_t>(
        static_cast<uint32_t>(config.erosion_noise_scale)));
    terrain_signature_add(signature, static_cast<uint64_t>(
        config.erosion_strength));
    terrain_signature_add(signature, static_cast<uint64_t>(config.feature_count));
    index = 0U;
    while (index < config.feature_count && index < TERRAIN_MAX_FEATURE_RULES)
    {
        terrain_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(config.features[index].biome_index)));
        terrain_signature_add(signature, static_cast<uint64_t>(
            config.features[index].chance_percent));
        terrain_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(config.features[index].minimum_height)));
        terrain_signature_add(signature, static_cast<uint64_t>(
            static_cast<uint32_t>(config.features[index].maximum_height)));
        terrain_signature_add(signature, static_cast<uint64_t>(
            config.features[index].requires_dry_land));
        terrain_signature_add_template(signature,
            config.features[index].template_data);
        index += 1U;
    }
    terrain_signature_add(signature, static_cast<uint64_t>(
        config.cross_chunk_block_writer != ft_nullptr));
    signature = terrain_mix_u64(signature);
    return (static_cast<uint32_t>(signature ^ (signature >> 32)));
}

static ft_bool terrain_template_is_valid(
    const terrain_tree_template *tree_template) noexcept
{
    uint32_t index;

    if (tree_template == ft_nullptr)
        return (FT_TRUE);
    if (tree_template->blocks == ft_nullptr && tree_template->block_count != 0U)
        return (FT_FALSE);
    index = 0U;
    while (index < tree_template->block_count)
    {
        if (terrain_block_is_known(tree_template->blocks[index].block_id)
            == FT_FALSE)
            return (FT_FALSE);
        index += 1U;
    }
    return (FT_TRUE);
}

int32_t terrain_generation_config_add_tree_template(
    terrain_generation_config &config,
    const terrain_tree_template &tree_template,
    uint32_t *template_index_out) noexcept
{
    uint32_t template_index;

    if (terrain_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (template_index_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (terrain_template_is_valid(&tree_template) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (config.tree_template_count >= TERRAIN_MAX_TREE_TEMPLATES)
        return (FT_ERR_FULL);
    template_index = config.tree_template_count;
    if (terrain_copy_tree_template(tree_template,
            config.tree_template_blocks[template_index],
            &config.tree_templates[template_index]) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    config.tree_template_count += 1U;
    *template_index_out = template_index;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config_remove_tree_template(
    terrain_generation_config &config, uint32_t template_index) noexcept
{
    uint32_t index;
    uint32_t biome_index;
    uint32_t biome_template_index;
    uint32_t kept_count;
    uint32_t old_template_index;
    uint32_t feature_index;

    if (terrain_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (template_index >= config.tree_template_count)
        return (FT_ERR_OUT_OF_RANGE);
    feature_index = 0U;
    while (feature_index < TERRAIN_MAX_FEATURE_RULES)
    {
        if (config.features[feature_index].template_data
            == &config.tree_templates[template_index])
            return (FT_ERR_INVALID_OPERATION);
        feature_index += 1U;
    }
    biome_index = 0U;
    while (biome_index < TERRAIN_MAX_CUSTOM_BIOMES)
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
        while (biome_index < TERRAIN_MAX_CUSTOM_BIOMES)
        {
            if (config.biomes[biome_index].tree_template
                == &config.tree_templates[index + 1U])
                config.biomes[biome_index].tree_template =
                    &config.tree_templates[index];
            biome_index += 1U;
        }
        feature_index = 0U;
        while (feature_index < TERRAIN_MAX_FEATURE_RULES)
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

int32_t terrain_generation_config_clear_tree_templates(
    terrain_generation_config &config) noexcept
{
    uint32_t index;

    if (terrain_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    index = 0U;
    while (index < TERRAIN_MAX_FEATURE_RULES)
    {
        if (config.features[index].template_data != ft_nullptr)
            return (FT_ERR_INVALID_OPERATION);
        index += 1U;
    }
    config.tree_template_count = 0U;
    index = 0U;
    while (index < TERRAIN_MAX_TREE_TEMPLATES)
    {
        config.tree_templates[index].blocks = ft_nullptr;
        config.tree_templates[index].block_count = 0U;
        ft_memset(config.tree_template_blocks[index], 0,
            sizeof(config.tree_template_blocks[index]));
        index += 1U;
    }
    index = 0U;
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        config.biomes[index].tree_template_count = 0U;
        config.biomes[index].tree_template = ft_nullptr;
        index += 1U;
    }
    index = 0U;
    while (index < TERRAIN_MAX_FEATURE_RULES)
    {
        config.features[index].template_data = ft_nullptr;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config_assign_tree_template_to_biome(
    terrain_generation_config &config, uint32_t biome_index,
    uint32_t template_index) noexcept
{
    uint32_t index;

    if (terrain_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES
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
    if (index >= TERRAIN_MAX_BIOME_TREE_TEMPLATES)
        return (FT_ERR_FULL);
    config.biomes[biome_index].tree_template_indices[index] = template_index;
    config.biomes[biome_index].tree_template_count += 1U;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config_remove_tree_template_from_biome(
    terrain_generation_config &config, uint32_t biome_index,
    uint32_t template_index) noexcept
{
    uint32_t index;
    uint32_t kept_count;

    if (terrain_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (FT_ERR_NOT_INITIALISED);
    if (biome_index >= TERRAIN_MAX_CUSTOM_BIOMES)
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

const terrain_tree_template *terrain_generation_config_get_tree_template(
    const terrain_generation_config &config, uint32_t template_index) noexcept
{
    if (terrain_config_require_initialised(config) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (template_index >= config.tree_template_count)
        return (ft_nullptr);
    return (&config.tree_templates[template_index]);
}

ft_bool terrain_generation_config_is_valid(
    const terrain_generation_config &config) noexcept
{
    uint32_t index;
    uint32_t biome_template_index;

    if (config.is_initialised() == FT_FALSE)
        return (FT_FALSE);
    if (config.biome_count == 0U || config.biome_count > TERRAIN_MAX_CUSTOM_BIOMES
        || (config.enable_biome_size_control != FT_FALSE
            && config.enable_biome_size_control != FT_TRUE)
        || config.tree_template_count > TERRAIN_MAX_TREE_TEMPLATES
        || config.feature_count > TERRAIN_MAX_FEATURE_RULES
        || config.ore_rule_count > TERRAIN_MAX_ORE_RULES
        || config.large_noise_scale <= 0 || config.detail_noise_scale <= 0
        || config.detail_noise_percent < 0 || config.detail_noise_percent > 100
        || config.biome_size_min < TERRAIN_BIOME_SIZE_MINIMUM
        || config.biome_size_max < config.biome_size_min
        || config.biome_size_max > TERRAIN_BIOME_SIZE_MAXIMUM
        || config.water_chance_percent > 100
        || config.biome_transition_noise_scale <= 0
        || config.biome_transition_noise_strength > 100U
        || config.fluids.river_noise_scale <= 0
        || config.fluids.lake_noise_scale <= 0
        || config.fluids.river_width < 0
        || config.fluids.lake_chance_percent > 100
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
        if (terrain_template_is_valid(&config.tree_templates[index])
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
                < TERRAIN_BIOME_SIZE_MINIMUM
            || config.biome_size_max_by_biome[index]
                < config.biome_size_min_by_biome[index]
            || config.biome_size_max_by_biome[index]
                > TERRAIN_BIOME_SIZE_MAXIMUM
            || (config.biome_size_override_enabled[index] != FT_FALSE
                && config.biome_size_override_enabled[index] != FT_TRUE))
            return (FT_FALSE);
        if (config.biomes[index].is_initialised() == FT_FALSE
            || config.biomes[index].profile.height_variation < 0
            || config.biomes[index].profile.topsoil_depth < 0
            || config.biomes[index].shrub_chance_percent > 100
            || config.biomes[index].tree_chance_percent > 100
            || config.biomes[index].tree_template_count
                > TERRAIN_MAX_BIOME_TREE_TEMPLATES
            || terrain_block_is_known(config.biomes[index].surface_block_id)
                == FT_FALSE
            || terrain_block_is_known(config.biomes[index].subsurface_block_id)
                == FT_FALSE
            || terrain_block_is_known(config.biomes[index].deep_block_id)
                == FT_FALSE
            || terrain_template_is_valid(config.biomes[index].tree_template)
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
            || terrain_block_is_known(config.ores[index].block_id) == FT_FALSE
            || config.ores[index].minimum_height
                > config.ores[index].maximum_height
            || config.ores[index].minimum_height < 0
            || config.ores[index].maximum_height >= GAME_VOXEL_CHUNK_HEIGHT
            || config.ores[index].vein_size == 0U
            || config.ores[index].chance_percent > 100U)
            return (FT_FALSE);
        index += 1U;
    }
    if (terrain_block_is_known(config.layers.beach_block_id) == FT_FALSE
        || terrain_block_is_known(config.layers.underwater_block_id)
            == FT_FALSE
        || terrain_block_is_known(config.layers.snow_cap_block_id)
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
            || terrain_template_is_valid(config.features[index].template_data)
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

terrain_generation_context::terrain_generation_context() noexcept
    : _config(), _configuration_signature(0U),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

terrain_generation_context::~terrain_generation_context() noexcept
{
    this->destroy();
    return ;
}

int32_t terrain_generation_context::initialize(
    const terrain_generation_config &config) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_OPERATION);
    if (terrain_generation_config_is_valid(config) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_config.initialize(config) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_configuration_signature = terrain_generation_config_signature(
        this->_config);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_context::destroy() noexcept
{
    this->_config.destroy();
    this->_configuration_signature = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t terrain_generation_context::move(
    terrain_generation_context &other) noexcept
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

ft_bool terrain_generation_context::is_initialised() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_TRUE);
    return (FT_FALSE);
}

const terrain_generation_config &terrain_generation_context::config() const noexcept
{
    return (this->_config);
}

uint32_t terrain_generation_context::configuration_signature() const noexcept
{
    return (this->_configuration_signature);
}

int32_t terrain_generation_context_initialize(
    terrain_generation_context &context,
    const terrain_generation_config &config) noexcept
{
    return (context.initialize(config));
}

ft_bool terrain_generation_context_is_initialised(
    const terrain_generation_context &context) noexcept
{
    return (context.is_initialised());
}

terrain_biome_profile terrain_get_biome_profile(terrain_biome biome) noexcept
{
    terrain_biome_profile biome_profile;

    if (biome == TERRAIN_BIOME_HILLS)
    {
        biome_profile.surface_height = 80;
        biome_profile.height_variation = 8;
        biome_profile.topsoil_depth = 4;
        return (biome_profile);
    }
    if (biome == TERRAIN_BIOME_DESERT)
    {
        biome_profile.surface_height = 70;
        biome_profile.height_variation = 3;
        biome_profile.topsoil_depth = 5;
        return (biome_profile);
    }
    if (biome == TERRAIN_BIOME_SNOW)
    {
        biome_profile.surface_height = 84;
        biome_profile.height_variation = 6;
        biome_profile.topsoil_depth = 4;
        return (biome_profile);
    }
    if (biome == TERRAIN_BIOME_MOUNTAINS)
    {
        biome_profile.surface_height = 100;
        biome_profile.height_variation = 14;
        biome_profile.topsoil_depth = 2;
        return (biome_profile);
    }
    biome_profile.surface_height = TERRAIN_GENERATOR_SURFACE_HEIGHT;
    biome_profile.height_variation = 2;
    biome_profile.topsoil_depth = 3;
    return (biome_profile);
}

ft_bool terrain_can_place_tree_template(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    const terrain_tree_template &tree_template) noexcept
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
        if (terrain_block_is_replaceable(block_id) == FT_FALSE)
            return (FT_FALSE);
        block_index += 1;
    }
    return (FT_TRUE);
}

int32_t terrain_place_tree_template(game_voxel_chunk &chunk,
    int32_t local_origin_x, int32_t local_origin_y, int32_t local_origin_z,
    const terrain_tree_template &tree_template) noexcept
{
    if (terrain_can_place_tree_template(chunk, local_origin_x, local_origin_y,
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
