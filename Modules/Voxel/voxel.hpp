#ifndef VOXEL_HPP
# define VOXEL_HPP

#ifdef GAME_USE_VOXEL_REGION_BACKEND

# include "../Game/game_voxel_chunk.hpp"
# include "../Basic/class_nullptr.hpp"
# include "../CPP_class/class_big_number.hpp"
# include "../Buffer/byte_buffer.hpp"
# include <stdint.h>

class game_voxel_region;

# define TERRAIN_GENERATOR_SURFACE_HEIGHT 76
# define TERRAIN_GENERATOR_SEA_LEVEL      72
# define TERRAIN_GENERATOR_WATER_BLOCK 8U
# define TERRAIN_GENERATOR_GRASS_BLOCK 1U
# define TERRAIN_GENERATOR_DIRT_BLOCK 2U
# define TERRAIN_GENERATOR_STONE_BLOCK 3U
# define TERRAIN_GENERATOR_SHRUB_BLOCK 4U
# define TERRAIN_GENERATOR_OAK_LOG_BLOCK 5U
# define TERRAIN_GENERATOR_OAK_LEAVES_BLOCK 6U
# define TERRAIN_GENERATOR_CACTUS_BLOCK 7U
# define TERRAIN_GENERATOR_BEDROCK_BLOCK 9U
# define TERRAIN_GENERATOR_SAND_BLOCK 10U
# define TERRAIN_GENERATOR_SNOW_BLOCK 11U
# define TERRAIN_GENERATOR_PERMAFROST_BLOCK 12U
# define TERRAIN_GENERATOR_CANYON_ROCK_BLOCK 13U
# define TERRAIN_GENERATOR_SLATE_BLOCK 14U
# define TERRAIN_GENERATOR_MOSS_ROCK_BLOCK 15U
# define TERRAIN_GENERATOR_COAL_ORE_BLOCK 16U
# define TERRAIN_GENERATOR_IRON_ORE_BLOCK 17U
# define TERRAIN_GENERATOR_GOLD_ORE_BLOCK 18U
# define TERRAIN_GENERATOR_GRANITE_BLOCK 19U
# define TERRAIN_GENERATOR_ANDESITE_BLOCK 20U
# define TERRAIN_GENERATOR_DIORITE_BLOCK 21U
# define TERRAIN_GENERATOR_GRAVEL_BLOCK 22U
# define TERRAIN_GENERATOR_CLAY_BLOCK 23U
# define TERRAIN_GENERATOR_OBSIDIAN_BLOCK 24U
# define TERRAIN_GENERATOR_MOSSY_STONE_BLOCK 25U
# define TERRAIN_GENERATOR_CRACKED_STONE_BLOCK 26U
# define TERRAIN_GENERATOR_LIMESTONE_BLOCK 27U
# define TERRAIN_GENERATOR_BASALT_BLOCK 28U
# define TERRAIN_GENERATOR_DIAMOND_ORE_BLOCK 29U
# define TERRAIN_GENERATOR_EMERALD_ORE_BLOCK 30U
# define TERRAIN_GENERATOR_COPPER_ORE_BLOCK 31U
# define TERRAIN_GENERATOR_PINE_LOG_BLOCK 32U
# define TERRAIN_GENERATOR_PINE_LEAVES_BLOCK 33U
# define TERRAIN_GENERATOR_BIRCH_LOG_BLOCK 34U
# define TERRAIN_GENERATOR_BIRCH_LEAVES_BLOCK 35U
# define TERRAIN_GENERATOR_ICE_BLOCK 36U
# define TERRAIN_GENERATOR_PACKED_ICE_BLOCK 37U
# define TERRAIN_GENERATOR_RED_FLOWER_BLOCK 38U
# define TERRAIN_GENERATOR_YELLOW_FLOWER_BLOCK 39U
# define TERRAIN_GENERATOR_TALL_GRASS_BLOCK 40U
# define TERRAIN_GENERATOR_FERN_BLOCK 41U
# define TERRAIN_GENERATOR_DEAD_BUSH_BLOCK 42U
# define TERRAIN_GENERATOR_RED_MUSHROOM_BLOCK 43U
# define TERRAIN_GENERATOR_BROWN_MUSHROOM_BLOCK 44U
# define TERRAIN_GENERATOR_MUSHROOM_STEM_BLOCK 45U
# define TERRAIN_GENERATOR_LILY_PAD_BLOCK 46U
# define TERRAIN_GENERATOR_SEAGRASS_BLOCK 47U
# define TERRAIN_GENERATOR_COARSE_DIRT_BLOCK 48U
# define TERRAIN_GENERATOR_PODZOL_BLOCK 49U
# define TERRAIN_GENERATOR_MUD_BLOCK 50U
# define TERRAIN_GENERATOR_FROZEN_STONE_BLOCK 51U
# define TERRAIN_GENERATOR_CHALK_BLOCK 52U
# define TERRAIN_GENERATOR_RED_SAND_BLOCK 53U
# define TERRAIN_GENERATOR_TERRACOTTA_BLOCK 54U
# define TERRAIN_GENERATOR_SALT_BLOCK 55U
# define TERRAIN_GENERATOR_VOLCANIC_ROCK_BLOCK 56U
# define TERRAIN_GENERATOR_QUARTZ_BLOCK 57U
# define TERRAIN_GENERATOR_AMETHYST_BLOCK 58U
# define TERRAIN_GENERATOR_PACKED_SNOW_BLOCK 59U
# define TERRAIN_GENERATOR_WET_SAND_BLOCK 60U
# define TERRAIN_GENERATOR_AMBER_BLOCK 61U
# define TERRAIN_GENERATOR_FROST_CRYSTAL_BLOCK 62U
# define TERRAIN_GENERATOR_SHIMMER_STONE_BLOCK 63U
# define TERRAIN_RUNTIME_BLOCK_CAPACITY 256U
# define TERRAIN_BIOME_ZONE_WIDTH 128
# define TERRAIN_BIOME_SIZE_MINIMUM 16
# define TERRAIN_BIOME_SIZE_MAXIMUM 32768
# define TERRAIN_MAX_CUSTOM_BIOMES 16U
# define TERRAIN_MAX_FEATURE_RULES 16U
# define TERRAIN_MAX_ORE_RULES 32U
# define TERRAIN_MAX_TREE_TEMPLATES 64U
# define TERRAIN_MAX_BIOME_TREE_TEMPLATES 16U
# define TERRAIN_MAX_TREE_TEMPLATE_BLOCKS 32U
# define TERRAIN_GENERATOR_VERSION 5U
# define TERRAIN_STAGE_BASE_TERRAIN 1U
# define TERRAIN_STAGE_CAVES 2U
# define TERRAIN_STAGE_FLUIDS 4U
# define TERRAIN_STAGE_DECORATION 8U
# define TERRAIN_STAGE_STRUCTURES 16U
# define TERRAIN_STAGE_ORES 32U

enum terrain_builtin_block_id : uint32_t
{
    TERRAIN_GENERATOR_AIR_BLOCK = 0U,
    TERRAIN_GENERATOR_GRASS_BLOCK_ID = TERRAIN_GENERATOR_GRASS_BLOCK,
    TERRAIN_GENERATOR_DIRT_BLOCK_ID = TERRAIN_GENERATOR_DIRT_BLOCK,
    TERRAIN_GENERATOR_STONE_BLOCK_ID = TERRAIN_GENERATOR_STONE_BLOCK,
    TERRAIN_GENERATOR_SHRUB_BLOCK_ID = TERRAIN_GENERATOR_SHRUB_BLOCK,
    TERRAIN_GENERATOR_OAK_LOG_BLOCK_ID = TERRAIN_GENERATOR_OAK_LOG_BLOCK,
    TERRAIN_GENERATOR_OAK_LEAVES_BLOCK_ID = TERRAIN_GENERATOR_OAK_LEAVES_BLOCK,
    TERRAIN_GENERATOR_CACTUS_BLOCK_ID = TERRAIN_GENERATOR_CACTUS_BLOCK,
    TERRAIN_GENERATOR_WATER_BLOCK_ID = TERRAIN_GENERATOR_WATER_BLOCK,
    TERRAIN_GENERATOR_BEDROCK_BLOCK_ID = TERRAIN_GENERATOR_BEDROCK_BLOCK,
    TERRAIN_GENERATOR_SAND_BLOCK_ID = TERRAIN_GENERATOR_SAND_BLOCK,
    TERRAIN_GENERATOR_SNOW_BLOCK_ID = TERRAIN_GENERATOR_SNOW_BLOCK,
    TERRAIN_GENERATOR_PERMAFROST_BLOCK_ID = TERRAIN_GENERATOR_PERMAFROST_BLOCK,
    TERRAIN_GENERATOR_CANYON_ROCK_BLOCK_ID = TERRAIN_GENERATOR_CANYON_ROCK_BLOCK,
    TERRAIN_GENERATOR_SLATE_BLOCK_ID = TERRAIN_GENERATOR_SLATE_BLOCK,
    TERRAIN_GENERATOR_MOSS_ROCK_BLOCK_ID = TERRAIN_GENERATOR_MOSS_ROCK_BLOCK,
    TERRAIN_GENERATOR_COAL_ORE_BLOCK_ID = TERRAIN_GENERATOR_COAL_ORE_BLOCK,
    TERRAIN_GENERATOR_IRON_ORE_BLOCK_ID = TERRAIN_GENERATOR_IRON_ORE_BLOCK,
    TERRAIN_GENERATOR_GOLD_ORE_BLOCK_ID = TERRAIN_GENERATOR_GOLD_ORE_BLOCK,
    TERRAIN_GENERATOR_GRANITE_BLOCK_ID = TERRAIN_GENERATOR_GRANITE_BLOCK,
    TERRAIN_GENERATOR_ANDESITE_BLOCK_ID = TERRAIN_GENERATOR_ANDESITE_BLOCK,
    TERRAIN_GENERATOR_DIORITE_BLOCK_ID = TERRAIN_GENERATOR_DIORITE_BLOCK,
    TERRAIN_GENERATOR_GRAVEL_BLOCK_ID = TERRAIN_GENERATOR_GRAVEL_BLOCK,
    TERRAIN_GENERATOR_CLAY_BLOCK_ID = TERRAIN_GENERATOR_CLAY_BLOCK,
    TERRAIN_GENERATOR_OBSIDIAN_BLOCK_ID = TERRAIN_GENERATOR_OBSIDIAN_BLOCK,
    TERRAIN_GENERATOR_MOSSY_STONE_BLOCK_ID = TERRAIN_GENERATOR_MOSSY_STONE_BLOCK,
    TERRAIN_GENERATOR_CRACKED_STONE_BLOCK_ID = TERRAIN_GENERATOR_CRACKED_STONE_BLOCK,
    TERRAIN_GENERATOR_LIMESTONE_BLOCK_ID = TERRAIN_GENERATOR_LIMESTONE_BLOCK,
    TERRAIN_GENERATOR_BASALT_BLOCK_ID = TERRAIN_GENERATOR_BASALT_BLOCK,
    TERRAIN_GENERATOR_DIAMOND_ORE_BLOCK_ID = TERRAIN_GENERATOR_DIAMOND_ORE_BLOCK,
    TERRAIN_GENERATOR_EMERALD_ORE_BLOCK_ID = TERRAIN_GENERATOR_EMERALD_ORE_BLOCK,
    TERRAIN_GENERATOR_COPPER_ORE_BLOCK_ID = TERRAIN_GENERATOR_COPPER_ORE_BLOCK,
    TERRAIN_GENERATOR_PINE_LOG_BLOCK_ID = TERRAIN_GENERATOR_PINE_LOG_BLOCK,
    TERRAIN_GENERATOR_PINE_LEAVES_BLOCK_ID = TERRAIN_GENERATOR_PINE_LEAVES_BLOCK,
    TERRAIN_GENERATOR_BIRCH_LOG_BLOCK_ID = TERRAIN_GENERATOR_BIRCH_LOG_BLOCK,
    TERRAIN_GENERATOR_BIRCH_LEAVES_BLOCK_ID = TERRAIN_GENERATOR_BIRCH_LEAVES_BLOCK,
    TERRAIN_GENERATOR_ICE_BLOCK_ID = TERRAIN_GENERATOR_ICE_BLOCK,
    TERRAIN_GENERATOR_PACKED_ICE_BLOCK_ID = TERRAIN_GENERATOR_PACKED_ICE_BLOCK,
    TERRAIN_GENERATOR_RED_FLOWER_BLOCK_ID = TERRAIN_GENERATOR_RED_FLOWER_BLOCK,
    TERRAIN_GENERATOR_YELLOW_FLOWER_BLOCK_ID = TERRAIN_GENERATOR_YELLOW_FLOWER_BLOCK,
    TERRAIN_GENERATOR_TALL_GRASS_BLOCK_ID = TERRAIN_GENERATOR_TALL_GRASS_BLOCK,
    TERRAIN_GENERATOR_FERN_BLOCK_ID = TERRAIN_GENERATOR_FERN_BLOCK,
    TERRAIN_GENERATOR_DEAD_BUSH_BLOCK_ID = TERRAIN_GENERATOR_DEAD_BUSH_BLOCK,
    TERRAIN_GENERATOR_RED_MUSHROOM_BLOCK_ID = TERRAIN_GENERATOR_RED_MUSHROOM_BLOCK,
    TERRAIN_GENERATOR_BROWN_MUSHROOM_BLOCK_ID = TERRAIN_GENERATOR_BROWN_MUSHROOM_BLOCK,
    TERRAIN_GENERATOR_MUSHROOM_STEM_BLOCK_ID = TERRAIN_GENERATOR_MUSHROOM_STEM_BLOCK,
    TERRAIN_GENERATOR_LILY_PAD_BLOCK_ID = TERRAIN_GENERATOR_LILY_PAD_BLOCK,
    TERRAIN_GENERATOR_SEAGRASS_BLOCK_ID = TERRAIN_GENERATOR_SEAGRASS_BLOCK,
    TERRAIN_GENERATOR_COARSE_DIRT_BLOCK_ID = TERRAIN_GENERATOR_COARSE_DIRT_BLOCK,
    TERRAIN_GENERATOR_PODZOL_BLOCK_ID = TERRAIN_GENERATOR_PODZOL_BLOCK,
    TERRAIN_GENERATOR_MUD_BLOCK_ID = TERRAIN_GENERATOR_MUD_BLOCK,
    TERRAIN_GENERATOR_FROZEN_STONE_BLOCK_ID = TERRAIN_GENERATOR_FROZEN_STONE_BLOCK,
    TERRAIN_GENERATOR_CHALK_BLOCK_ID = TERRAIN_GENERATOR_CHALK_BLOCK,
    TERRAIN_GENERATOR_RED_SAND_BLOCK_ID = TERRAIN_GENERATOR_RED_SAND_BLOCK,
    TERRAIN_GENERATOR_TERRACOTTA_BLOCK_ID = TERRAIN_GENERATOR_TERRACOTTA_BLOCK,
    TERRAIN_GENERATOR_SALT_BLOCK_ID = TERRAIN_GENERATOR_SALT_BLOCK,
    TERRAIN_GENERATOR_VOLCANIC_ROCK_BLOCK_ID = TERRAIN_GENERATOR_VOLCANIC_ROCK_BLOCK,
    TERRAIN_GENERATOR_QUARTZ_BLOCK_ID = TERRAIN_GENERATOR_QUARTZ_BLOCK,
    TERRAIN_GENERATOR_AMETHYST_BLOCK_ID = TERRAIN_GENERATOR_AMETHYST_BLOCK,
    TERRAIN_GENERATOR_PACKED_SNOW_BLOCK_ID = TERRAIN_GENERATOR_PACKED_SNOW_BLOCK,
    TERRAIN_GENERATOR_WET_SAND_BLOCK_ID = TERRAIN_GENERATOR_WET_SAND_BLOCK,
    TERRAIN_GENERATOR_AMBER_BLOCK_ID = TERRAIN_GENERATOR_AMBER_BLOCK,
    TERRAIN_GENERATOR_FROST_CRYSTAL_BLOCK_ID = TERRAIN_GENERATOR_FROST_CRYSTAL_BLOCK,
    TERRAIN_GENERATOR_SHIMMER_STONE_BLOCK_ID = TERRAIN_GENERATOR_SHIMMER_STONE_BLOCK,
    TERRAIN_BUILTIN_BLOCK_COUNT = 64U
};

static_assert(static_cast<uint64_t>(TERRAIN_BUILTIN_BLOCK_COUNT)
        + static_cast<uint64_t>(TERRAIN_RUNTIME_BLOCK_CAPACITY)
        <= static_cast<uint64_t>(UINT32_MAX),
    "terrain block ID range exceeds uint32_t");

enum terrain_biome
{
    TERRAIN_BIOME_PLAINS = 0,
    TERRAIN_BIOME_HILLS = 1,
    TERRAIN_BIOME_DESERT = 2,
    TERRAIN_BIOME_SNOW = 3,
    TERRAIN_BIOME_MOUNTAINS = 4
};

struct terrain_biome_profile
{
    int32_t surface_height;
    int32_t height_variation;
    int32_t topsoil_depth;
};

struct terrain_block_metadata
{
    ft_bool solid;
    ft_bool transparent;
    ft_bool liquid;
    ft_bool replaceable;
    ft_bool light_emitting;
    ft_bool occludes_faces;
    uint32_t hardness;
    ft_bool breakable;
    ft_bool can_host_ore = FT_FALSE;
    ft_bool is_ore = FT_FALSE;
};

enum terrain_block_asset_face
{
    TERRAIN_BLOCK_ASSET_FACE_TOP = 0,
    TERRAIN_BLOCK_ASSET_FACE_BOTTOM = 1,
    TERRAIN_BLOCK_ASSET_FACE_NORTH = 2,
    TERRAIN_BLOCK_ASSET_FACE_SOUTH = 3,
    TERRAIN_BLOCK_ASSET_FACE_EAST = 4,
    TERRAIN_BLOCK_ASSET_FACE_WEST = 5,
    TERRAIN_BLOCK_ASSET_FACE_COUNT = 6
};

struct terrain_block_registration
{
    const char *name;
    terrain_block_metadata metadata;
    const char *asset_paths[TERRAIN_BLOCK_ASSET_FACE_COUNT];
};

struct terrain_tree_template_block
{
    int32_t offset_x;
    int32_t offset_y;
    int32_t offset_z;
    uint32_t block_id;
};

struct terrain_tree_template
{
    const terrain_tree_template_block *blocks;
    uint32_t block_count;
};

class terrain_biome_definition
{
    private:
        uint8_t _initialised_state;

    public:
        terrain_biome_definition() noexcept;
        terrain_biome_definition(const terrain_biome_definition &other)
            noexcept = delete;
        terrain_biome_definition(terrain_biome_definition &&other)
            noexcept = delete;
        ~terrain_biome_definition() noexcept;
        terrain_biome_definition &operator=(
            const terrain_biome_definition &other) noexcept = delete;
        terrain_biome_definition &operator=(
            terrain_biome_definition &&other) noexcept = delete;
        int32_t initialize() noexcept;
        int32_t initialize(const terrain_biome_definition &other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(terrain_biome_definition &other) noexcept;
        ft_bool is_initialised() const noexcept;
        int32_t set_profile(const terrain_biome_profile &value) noexcept;
        int32_t set_block_palette(uint32_t surface_block,
            uint32_t subsurface_block, uint32_t deep_block) noexcept;
        int32_t set_decoration_policy(ft_bool shrubs, ft_bool trees,
            uint32_t shrub_chance, uint32_t tree_chance) noexcept;
        int32_t set_snow_cap_policy(ft_bool enabled) noexcept;
        int32_t set_mountain_ridge_policy(ft_bool enabled) noexcept;
        int32_t set_tree_template_override(
            const terrain_tree_template *value) noexcept;

        terrain_biome_profile profile;
        uint32_t surface_block_id;
        uint32_t subsurface_block_id;
        uint32_t deep_block_id;
        ft_bool allow_shrubs;
        ft_bool allow_trees;
        ft_bool allow_snow_caps;
        ft_bool allow_mountain_ridges;
        uint32_t shrub_chance_percent;
        uint32_t tree_chance_percent;
        uint32_t tree_template_count;
        uint32_t tree_template_indices[TERRAIN_MAX_BIOME_TREE_TEMPLATES];
        const terrain_tree_template *tree_template;
};

class terrain_feature_rule
{
    private:
        uint8_t _initialised_state;

    public:
        terrain_feature_rule() noexcept;
        terrain_feature_rule(const terrain_feature_rule &other)
            noexcept = delete;
        terrain_feature_rule(terrain_feature_rule &&other) noexcept = delete;
        ~terrain_feature_rule() noexcept;
        terrain_feature_rule &operator=(const terrain_feature_rule &other)
            noexcept = delete;
        terrain_feature_rule &operator=(terrain_feature_rule &&other)
            noexcept = delete;
        int32_t initialize() noexcept;
        int32_t initialize(const terrain_feature_rule &other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(terrain_feature_rule &other) noexcept;
        ft_bool is_initialised() const noexcept;
        int32_t set_template(const terrain_tree_template *value) noexcept;
        int32_t set_biome_range(int32_t biome, int32_t minimum,
            int32_t maximum) noexcept;
        int32_t set_chance(uint32_t value) noexcept;
        int32_t set_requires_dry_land(ft_bool value) noexcept;

        const terrain_tree_template *template_data;
        int32_t biome_index;
        uint32_t chance_percent;
        int32_t minimum_height;
        int32_t maximum_height;
        ft_bool requires_dry_land;
};

class terrain_ore_rule
{
    private:
        uint8_t _initialised_state;

    public:
        terrain_ore_rule() noexcept;
        terrain_ore_rule(const terrain_ore_rule &other) noexcept = delete;
        terrain_ore_rule(terrain_ore_rule &&other) noexcept = delete;
        ~terrain_ore_rule() noexcept;
        terrain_ore_rule &operator=(const terrain_ore_rule &other)
            noexcept = delete;
        terrain_ore_rule &operator=(terrain_ore_rule &&other)
            noexcept = delete;
        int32_t initialize() noexcept;
        int32_t initialize(const terrain_ore_rule &other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(terrain_ore_rule &other) noexcept;
        ft_bool is_initialised() const noexcept;
        int32_t set_range(int32_t minimum, int32_t maximum) noexcept;
        int32_t set_vein(uint32_t size, uint32_t chance) noexcept;
        int32_t set_ore_replacement(ft_bool value) noexcept;
        int32_t set_enabled(ft_bool value) noexcept;

        uint32_t block_id;
        int32_t minimum_height;
        int32_t maximum_height;
        uint32_t vein_size;
        uint32_t chance_percent;
        ft_bool allow_ore_replacement;
        ft_bool enabled;
};

class terrain_underground_structure_config
{
    private:
        uint8_t _initialised_state;

    public:
        terrain_underground_structure_config() noexcept;
        terrain_underground_structure_config(
            const terrain_underground_structure_config &other)
            noexcept = delete;
        terrain_underground_structure_config(
            terrain_underground_structure_config &&other) noexcept = delete;
        ~terrain_underground_structure_config() noexcept;
        terrain_underground_structure_config &operator=(
            const terrain_underground_structure_config &other) noexcept = delete;
        terrain_underground_structure_config &operator=(
            terrain_underground_structure_config &&other) noexcept = delete;
        int32_t initialize() noexcept;
        int32_t initialize(
            const terrain_underground_structure_config &other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(terrain_underground_structure_config &other) noexcept;
        ft_bool is_initialised() const noexcept;
        int32_t set_enabled(ft_bool ravines, ft_bool cave_rooms) noexcept;
        int32_t set_chances(uint32_t ravine, uint32_t cave_room) noexcept;
        int32_t set_height_range(int32_t minimum, int32_t maximum) noexcept;
        int32_t set_shape(uint32_t width, uint32_t depth) noexcept;
        int32_t set_cave_shape(uint32_t small_radius, uint32_t large_radius,
            uint32_t large_chance) noexcept;
        int32_t set_cave_entrances(uint32_t chance,
            uint32_t radius) noexcept;
        int32_t set_cavern_rooms(ft_bool enabled, uint32_t chance,
            uint32_t radius) noexcept;

        ft_bool enable_ravines;
        ft_bool enable_cave_rooms;
        uint32_t ravine_chance_percent;
        uint32_t cave_room_chance_percent;
        int32_t minimum_height;
        int32_t maximum_height;
        uint32_t ravine_width;
        uint32_t ravine_depth;
        uint32_t cave_small_radius;
        uint32_t cave_large_radius;
        uint32_t cave_large_chance_percent;
        uint32_t cave_entrance_chance_percent;
        uint32_t cave_entrance_radius;
        ft_bool enable_cavern_rooms;
        uint32_t cavern_room_chance_percent;
        uint32_t cavern_room_radius;
};

class terrain_fluid_config
{
    private:
        uint8_t _initialised_state;

    public:
        terrain_fluid_config() noexcept;
        terrain_fluid_config(const terrain_fluid_config &other) noexcept = delete;
        terrain_fluid_config(terrain_fluid_config &&other) noexcept = delete;
        ~terrain_fluid_config() noexcept;
        terrain_fluid_config &operator=(const terrain_fluid_config &other)
            noexcept = delete;
        terrain_fluid_config &operator=(terrain_fluid_config &&other)
            noexcept = delete;
        int32_t initialize() noexcept;
        int32_t initialize(const terrain_fluid_config &other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(terrain_fluid_config &other) noexcept;
        ft_bool is_initialised() const noexcept;
        int32_t set_enabled(ft_bool rivers, ft_bool lakes) noexcept;
        int32_t set_river_settings(int32_t scale, int32_t width) noexcept;
        int32_t set_lake_settings(int32_t scale, uint32_t chance) noexcept;

        ft_bool enable_rivers;
        ft_bool enable_lakes;
        int32_t river_noise_scale;
        int32_t river_width;
        int32_t lake_noise_scale;
        uint32_t lake_chance_percent;
};

class terrain_layer_config
{
    private:
        uint8_t _initialised_state;

    public:
        terrain_layer_config() noexcept;
        terrain_layer_config(const terrain_layer_config &other) noexcept = delete;
        terrain_layer_config(terrain_layer_config &&other) noexcept = delete;
        ~terrain_layer_config() noexcept;
        terrain_layer_config &operator=(const terrain_layer_config &other)
            noexcept = delete;
        terrain_layer_config &operator=(terrain_layer_config &&other)
            noexcept = delete;
        int32_t initialize() noexcept;
        int32_t initialize(const terrain_layer_config &other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(terrain_layer_config &other) noexcept;
        ft_bool is_initialised() const noexcept;
        int32_t set_enabled(ft_bool beaches, ft_bool snow_caps) noexcept;
        int32_t set_depths(uint32_t beach, uint32_t underwater,
            uint32_t snow) noexcept;
        int32_t set_snowline(int32_t minimum_height) noexcept;
        int32_t set_block_palette(uint32_t beach, uint32_t underwater,
            uint32_t snow) noexcept;

        ft_bool enable_beaches;
        ft_bool enable_snow_caps;
        uint32_t beach_depth;
        uint32_t underwater_depth;
        uint32_t snow_cap_depth;
        int32_t snow_cap_minimum_height;
        uint32_t beach_block_id;
        uint32_t underwater_block_id;
        uint32_t snow_cap_block_id;
};

typedef int32_t (*terrain_cross_chunk_block_writer)(int32_t world_block_x,
    int32_t world_block_y, int32_t world_block_z, uint32_t block_id,
    void *user_data) noexcept;

typedef uint32_t (*terrain_biome_selector)(uint64_t seed_value,
    int32_t world_block_x, int32_t world_block_z, uint32_t biome_count,
    void *user_data) noexcept;

class terrain_generation_config
{
    private:
        uint8_t _initialised_state;

    public:
        terrain_generation_config() noexcept;
        terrain_generation_config(const terrain_generation_config &other)
            noexcept = delete;
        terrain_generation_config(terrain_generation_config &&other)
            noexcept = delete;
        ~terrain_generation_config() noexcept;

        terrain_generation_config &operator=(
            const terrain_generation_config &other) noexcept = delete;
        terrain_generation_config &operator=(
            terrain_generation_config &&other) noexcept = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const terrain_generation_config &other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(terrain_generation_config &other) noexcept;
        ft_bool is_initialised() const noexcept;

        int32_t set_sea_level(int32_t value) noexcept;
        int32_t set_noise_scales(int32_t large_scale, int32_t detail_scale,
            int32_t detail_percent) noexcept;
        int32_t set_biome_size_control_enabled(ft_bool enabled) noexcept;
        int32_t set_biome_size_range(int32_t minimum_size,
            int32_t maximum_size) noexcept;
        int32_t set_biome_size_range_for_biome(uint32_t biome_index,
            int32_t minimum_size, int32_t maximum_size) noexcept;
        int32_t set_biome_size_override_enabled(uint32_t biome_index,
            ft_bool enabled) noexcept;
        int32_t set_water_chance_percent(uint32_t value) noexcept;
        int32_t set_biome_count(uint32_t value) noexcept;
        int32_t set_biome_selector(terrain_biome_selector selector,
            void *user_data) noexcept;
        int32_t set_cross_chunk_writer(
            terrain_cross_chunk_block_writer writer, void *user_data) noexcept;
        int32_t set_biome(uint32_t biome_index,
            const terrain_biome_definition &biome) noexcept;
        int32_t set_biome_profile(uint32_t biome_index,
            const terrain_biome_profile &profile) noexcept;
        int32_t set_biome_height_profile(uint32_t biome_index,
            int32_t surface_height, int32_t height_variation,
            int32_t topsoil_depth) noexcept;
        int32_t set_biome_block_palette(uint32_t biome_index,
            uint32_t surface_block, uint32_t subsurface_block,
            uint32_t deep_block) noexcept;
        int32_t set_biome_decoration_policy(uint32_t biome_index,
            ft_bool shrubs, ft_bool trees, uint32_t shrub_chance,
            uint32_t tree_chance) noexcept;
        int32_t set_biome_snow_caps_enabled(uint32_t biome_index,
            ft_bool enabled) noexcept;
        int32_t set_biome_mountain_ridges_enabled(uint32_t biome_index,
            ft_bool enabled) noexcept;
        int32_t set_biome_tree_template_override(uint32_t biome_index,
            const terrain_tree_template *value) noexcept;
        int32_t set_feature(uint32_t feature_index,
            const terrain_feature_rule &feature) noexcept;
        int32_t set_ore_rule(uint32_t ore_index,
            const terrain_ore_rule &ore) noexcept;
        int32_t set_underground_structures(
            const terrain_underground_structure_config &value) noexcept;
        int32_t set_fluids(const terrain_fluid_config &value) noexcept;
        int32_t set_layers(const terrain_layer_config &value) noexcept;
        int32_t set_feature_count(uint32_t value) noexcept;
        int32_t set_ore_rule_count(uint32_t value) noexcept;
        int32_t set_biome_transitions_enabled(ft_bool enabled) noexcept;
        int32_t set_biome_transition_settings(int32_t noise_scale,
            uint32_t noise_strength) noexcept;
        int32_t set_mountain_ridges_enabled(ft_bool enabled) noexcept;
        int32_t set_erosion_enabled(ft_bool enabled) noexcept;
        int32_t set_mountain_ridge_settings(int32_t scale,
            uint32_t strength) noexcept;
        int32_t set_erosion_settings(int32_t scale,
            uint32_t strength) noexcept;
        int32_t set_cross_chunk_features_enabled(ft_bool enabled) noexcept;
        int32_t sea_level;
        int32_t large_noise_scale;
        int32_t detail_noise_scale;
        int32_t detail_noise_percent;
        ft_bool enable_biome_size_control;
        int32_t biome_size_min;
        int32_t biome_size_max;
        int32_t biome_size_min_by_biome[TERRAIN_MAX_CUSTOM_BIOMES];
        int32_t biome_size_max_by_biome[TERRAIN_MAX_CUSTOM_BIOMES];
        ft_bool biome_size_override_enabled[TERRAIN_MAX_CUSTOM_BIOMES];
        uint32_t water_chance_percent;
        uint32_t biome_count;
        terrain_biome_definition biomes[TERRAIN_MAX_CUSTOM_BIOMES];
        uint32_t tree_template_count;
        terrain_tree_template tree_templates[TERRAIN_MAX_TREE_TEMPLATES];
        terrain_tree_template_block tree_template_blocks[
            TERRAIN_MAX_TREE_TEMPLATES][TERRAIN_MAX_TREE_TEMPLATE_BLOCKS];
        terrain_biome_selector biome_selector;
        void *biome_selector_user_data;
        uint32_t feature_count;
        terrain_feature_rule features[TERRAIN_MAX_FEATURE_RULES];
        uint32_t ore_rule_count;
        terrain_ore_rule ores[TERRAIN_MAX_ORE_RULES];
        terrain_underground_structure_config underground_structures;
        terrain_fluid_config fluids;
        terrain_layer_config layers;
        ft_bool enable_biome_transitions;
        int32_t biome_transition_noise_scale;
        uint32_t biome_transition_noise_strength;
        ft_bool enable_mountain_ridges;
        ft_bool enable_erosion;
        int32_t mountain_ridge_scale;
        uint32_t mountain_ridge_strength;
        int32_t erosion_noise_scale;
        uint32_t erosion_strength;
        ft_bool allow_cross_chunk_features;
        terrain_cross_chunk_block_writer cross_chunk_block_writer;
        void *cross_chunk_block_writer_user_data;
};

class terrain_generation_context
{
    private:
        terrain_generation_config _config;
        uint32_t _configuration_signature;
        uint8_t _initialised_state;

    public:
        terrain_generation_context() noexcept;
        terrain_generation_context(
            const terrain_generation_context &other) noexcept = delete;
        terrain_generation_context(terrain_generation_context &&other)
            noexcept = delete;
        ~terrain_generation_context() noexcept;

        terrain_generation_context &operator=(
            const terrain_generation_context &other) noexcept = delete;
        terrain_generation_context &operator=(
            terrain_generation_context &&other) noexcept = delete;

        int32_t initialize(const terrain_generation_config &config) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(terrain_generation_context &other) noexcept;
        ft_bool is_initialised() const noexcept;
        const terrain_generation_config &config() const noexcept;
        uint32_t configuration_signature() const noexcept;
};

class terrain_world_chunk_coordinate
{
    private:
        ft_big_number _chunk_x;
        ft_big_number _chunk_z;
        uint8_t _initialised_state;

    public:
        terrain_world_chunk_coordinate() noexcept;
        terrain_world_chunk_coordinate(
            const terrain_world_chunk_coordinate &other) noexcept = delete;
        terrain_world_chunk_coordinate(
            terrain_world_chunk_coordinate &&other) noexcept = delete;
        ~terrain_world_chunk_coordinate() noexcept;
        terrain_world_chunk_coordinate &operator=(
            const terrain_world_chunk_coordinate &other) noexcept = delete;
        terrain_world_chunk_coordinate &operator=(
            terrain_world_chunk_coordinate &&other) noexcept = delete;
        int32_t initialize() noexcept;
        int32_t initialize(
            const terrain_world_chunk_coordinate &other) noexcept;
        uint32_t destroy() noexcept;
        uint32_t move(terrain_world_chunk_coordinate &other) noexcept;
        ft_bool is_initialised() const noexcept;
        int32_t set_chunk_coordinates(const char *chunk_x,
            const char *chunk_z) noexcept;
        const ft_big_number &chunk_x() const noexcept;
        const ft_big_number &chunk_z() const noexcept;
        uint64_t hash() const noexcept;
};

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
