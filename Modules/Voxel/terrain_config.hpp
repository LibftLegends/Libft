#ifndef TERRAIN_CONFIG_HPP
# define TERRAIN_CONFIG_HPP

#ifdef GAME_USE_VOXEL_REGION_BACKEND

# include "terrain_types.hpp"
# include "../CPP_class/class_string.hpp"

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
        int32_t serialize_json(ft_string &output) const noexcept;
        int32_t save_json_file(const char *file_path,
            terrain_json_file_mode mode) const noexcept;

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
        int32_t serialize_json(ft_string &output) const noexcept;
        int32_t save_json_file(const char *file_path,
            terrain_json_file_mode mode) const noexcept;

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
        int32_t set_depth_range(int32_t minimum_depth,
            int32_t maximum_depth) noexcept;
        int32_t set_vein(uint32_t size, uint32_t chance) noexcept;
        int32_t set_vein_size_range(uint32_t minimum_size,
            uint32_t maximum_size) noexcept;
        int32_t set_frequency_range(uint32_t minimum_veins,
            uint32_t maximum_veins) noexcept;
        int32_t set_ore_replacement(ft_bool value) noexcept;
        int32_t set_enabled(ft_bool value) noexcept;
        int32_t serialize_json(ft_string &output) const noexcept;
        int32_t save_json_file(const char *file_path,
            terrain_json_file_mode mode) const noexcept;

        uint32_t block_id;
        int32_t minimum_height;
        int32_t maximum_height;
        int32_t minimum_depth;
        int32_t maximum_depth;
        uint32_t vein_size;
        uint32_t vein_size_min;
        uint32_t vein_size_max;
        uint32_t veins_per_chunk_min;
        uint32_t veins_per_chunk_max;
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
        int32_t serialize_json(ft_string &output) const noexcept;
        int32_t save_json_file(const char *file_path,
            terrain_json_file_mode mode) const noexcept;

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
        int32_t set_underground_lakes_enabled(ft_bool enabled) noexcept;
        int32_t set_river_settings(int32_t scale, int32_t width) noexcept;
        int32_t set_lake_settings(int32_t scale, uint32_t chance) noexcept;
        int32_t set_underground_lake_settings(uint32_t chance,
            int32_t minimum_y, int32_t maximum_y, uint32_t depth,
            uint32_t floor_thickness, uint32_t roof_thickness) noexcept;
        int32_t serialize_json(ft_string &output) const noexcept;
        int32_t save_json_file(const char *file_path,
            terrain_json_file_mode mode) const noexcept;

        ft_bool enable_rivers;
        ft_bool enable_lakes;
        ft_bool enable_underground_lakes;
        int32_t river_noise_scale;
        int32_t river_width;
        int32_t lake_noise_scale;
        uint32_t lake_chance_percent;
        uint32_t underground_lake_chance_percent;
        int32_t underground_lake_minimum_y;
        int32_t underground_lake_maximum_y;
        uint32_t underground_lake_depth;
        uint32_t underground_lake_floor_thickness;
        uint32_t underground_lake_roof_thickness;
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
        int32_t serialize_json(ft_string &output) const noexcept;
        int32_t save_json_file(const char *file_path,
            terrain_json_file_mode mode) const noexcept;

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

typedef int32_t (*terrain_cross_chunk_block_reader)(int32_t world_block_x,
    int32_t world_block_y, int32_t world_block_z, uint32_t *block_id,
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
        int32_t set_cross_chunk_reader(
            terrain_cross_chunk_block_reader reader, void *user_data) noexcept;
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
        int32_t serialize_json(ft_string &output) const noexcept;
        int32_t save_json_file(const char *file_path,
            terrain_json_file_mode mode) const noexcept;

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
        terrain_cross_chunk_block_reader cross_chunk_block_reader;
        void *cross_chunk_block_reader_user_data;
};

#endif

#endif
