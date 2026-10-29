#ifndef GAME_CHARACTER_HPP
# define GAME_CHARACTER_HPP

#include "../Template/map.hpp"
#include "game_quest.hpp"
#include "game_achievement.hpp"
#include "game_reputation.hpp"
#include "game_buff.hpp"
#include "game_debuff.hpp"
#include "game_skill.hpp"
#include "game_upgrade.hpp"
#include "game_inventory.hpp"
#include "game_equipment.hpp"
#include "game_experience_table.hpp"
#include "game_resistance.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/mutex.hpp"
#include <cstdint>

#define FT_DAMAGE_PHYSICAL 0
#define FT_DAMAGE_MAGICAL 1
#define FT_ARMOR_POINT_REDUCTION 0.95
#define FT_DAMAGE_RULE_FLAT 0
#define FT_DAMAGE_RULE_SCALED 1
#define FT_DAMAGE_RULE_BUFFER 2
#define FT_DAMAGE_RULE_MAGIC_SHIELD 3

#ifndef FT_PHYSICAL_DAMAGE_REDUCTION
# define FT_PHYSICAL_DAMAGE_REDUCTION FT_DAMAGE_RULE_FLAT
#endif
#ifndef FT_MAGIC_DAMAGE_REDUCTION
# define FT_MAGIC_DAMAGE_REDUCTION FT_DAMAGE_RULE_FLAT
#endif

struct json_group;

class game_character
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        int32_t _hit_points;
        int32_t _physical_armor;
        int32_t _magic_armor;
        int32_t _current_physical_armor;
        int32_t _current_magic_armor;
        double _physical_damage_multiplier;
        double _magic_damage_multiplier;
        uint8_t _damage_rule;
        int32_t _might;
        int32_t _agility;
        int32_t _endurance;
        int32_t _reason;
        int32_t _insigh;
        int32_t _presence;
        int32_t _coins;
        int32_t _valor;
        int32_t _experience;
        int32_t _x;
        int32_t _y;
        int32_t _z;

        game_experience_table        _experience_table;
        game_resistance             _fire_res;
        game_resistance             _frost_res;
        game_resistance             _lightning_res;
        game_resistance             _air_res;
        game_resistance             _earth_res;
        game_resistance             _chaos_res;
        game_resistance             _physical_res;
        ft_map<int32_t, game_skill>     _skills;
        ft_map<int32_t, game_buff>      _buffs;
        ft_map<int32_t, game_debuff>     _debuffs;
        ft_map<int32_t, game_upgrade> _upgrades;
        ft_map<int32_t, game_quest>     _quests;
        ft_map<int32_t, game_achievement> _achievements;
        game_reputation             _reputation;
        game_inventory            _inventory;
        game_equipment            _equipment;
        static thread_local int32_t _last_error;
        mutable pt_recursive_mutex          *_mutex;
        uint8_t _initialised_state;

        static int32_t set_error(int32_t error_code) noexcept;
        int32_t     lock_internal(ft_bool *lock_acquired) const noexcept;
        void    unlock_internal(ft_bool lock_acquired) const noexcept;
        void    apply_modifier(const game_item_modifier &mod, int32_t sign) noexcept;
        void    apply_modifier_internal(const game_item_modifier &mod, int32_t sign) noexcept;
        int64_t apply_skill_modifiers(int64_t damage) const noexcept;
        int64_t apply_skill_modifiers_internal(int64_t damage) const noexcept;
        ft_bool    handle_component_error(int32_t error) noexcept;
        ft_bool    check_internal_errors() noexcept;
        void    emit_game_metric(const char *event_name, const char *attribute,
                int64_t delta_value, int64_t total_value,
                const char *unit) const noexcept;
        void    set_physical_armor_internal(int32_t armor) noexcept;
        void    set_magic_armor_internal(int32_t armor) noexcept;
        void    restore_physical_armor_internal() noexcept;
        void    restore_magic_armor_internal() noexcept;
        void    take_damage_flat_internal(int64_t damage, uint8_t type) noexcept;
        void    take_damage_scaled_internal(int64_t damage, uint8_t type) noexcept;
        void    take_damage_buffer_internal(int64_t damage, uint8_t type) noexcept;
        void    take_damage_magic_shield_internal(int64_t damage, uint8_t type) noexcept;
    public:
        game_character() noexcept;
        virtual ~game_character() noexcept;
        int32_t initialize() noexcept;
        int32_t destroy() noexcept;
        int32_t move(game_character &other) noexcept;
        game_character(const game_character &other) noexcept = delete;
        game_character &operator=(const game_character &other) noexcept = delete;
        game_character(game_character &&other) noexcept = delete;
        game_character &operator=(game_character &&other) noexcept = delete;
        int32_t enable_thread_safety() noexcept;
        int32_t disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;

        int32_t get_hit_points() const noexcept;
        void set_hit_points(int32_t hp) noexcept;

        ft_bool is_alive() const noexcept;

        int32_t get_physical_armor() const noexcept;
        void set_physical_armor(int32_t armor) noexcept;
        int32_t get_magic_armor() const noexcept;
        void set_magic_armor(int32_t armor) noexcept;
        int32_t get_current_physical_armor() const noexcept;
        void set_current_physical_armor(int32_t armor) noexcept;
        int32_t get_current_magic_armor() const noexcept;
        void set_current_magic_armor(int32_t armor) noexcept;
        void restore_physical_armor() noexcept;
        void restore_magic_armor() noexcept;
        void restore_armor() noexcept;
        void set_damage_rule(uint8_t rule) noexcept;
        uint8_t get_damage_rule() const noexcept;
        void take_damage(int64_t damage, uint8_t type) noexcept;
        void take_damage_flat(int64_t damage, uint8_t type) noexcept;
        void take_damage_scaled(int64_t damage, uint8_t type) noexcept;
        void take_damage_buffer(int64_t damage, uint8_t type) noexcept;
        void take_damage_magic_shield(int64_t damage, uint8_t type) noexcept;

        int32_t get_might() const noexcept;
        void set_might(int32_t might) noexcept;

        int32_t get_agility() const noexcept;
        void set_agility(int32_t agility) noexcept;

        int32_t get_endurance() const noexcept;
        void set_endurance(int32_t endurance) noexcept;

        int32_t get_reason() const noexcept;
        void set_reason(int32_t reason) noexcept;

        int32_t get_insigh() const noexcept;
        void set_insigh(int32_t insigh) noexcept;

        int32_t get_presence() const noexcept;
        void set_presence(int32_t presence) noexcept;

        int32_t get_coins() const noexcept;
        void set_coins(int32_t coins) noexcept;
        void add_coins(int32_t coins) noexcept;
        void sub_coins(int32_t coins) noexcept;

        int32_t get_valor() const noexcept;
        void set_valor(int32_t valor) noexcept;
        void add_valor(int32_t valor) noexcept;
        void sub_valor(int32_t valor) noexcept;

        int32_t get_experience() const noexcept;
        void set_experience(int32_t experience) noexcept;
        void add_experience(int32_t experience) noexcept;
        void sub_experience(int32_t experience) noexcept;

        int32_t get_x() const noexcept;
        void set_x(int32_t x) noexcept;

        int32_t get_y() const noexcept;
        void set_y(int32_t y) noexcept;

        int32_t get_z() const noexcept;
        void set_z(int32_t z) noexcept;

        void move(int32_t dx, int32_t dy, int32_t dz) noexcept;

        const game_resistance &get_fire_res() const noexcept;
        void set_fire_res(int32_t percent, int32_t flat) noexcept;

        const game_resistance &get_frost_res() const noexcept;
        void set_frost_res(int32_t percent, int32_t flat) noexcept;

        const game_resistance &get_lightning_res() const noexcept;
        void set_lightning_res(int32_t percent, int32_t flat) noexcept;

        const game_resistance &get_air_res() const noexcept;
        void set_air_res(int32_t percent, int32_t flat) noexcept;

        const game_resistance &get_earth_res() const noexcept;
        void set_earth_res(int32_t percent, int32_t flat) noexcept;

        const game_resistance &get_chaos_res() const noexcept;
        void set_chaos_res(int32_t percent, int32_t flat) noexcept;

        const game_resistance &get_physical_res() const noexcept;
        void set_physical_res(int32_t percent, int32_t flat) noexcept;
        ft_map<int32_t, game_skill>       &get_skills() noexcept;
        const ft_map<int32_t, game_skill> &get_skills() const noexcept;
        game_skill *get_skill(int32_t id) noexcept;
        const game_skill *get_skill(int32_t id) const noexcept;
        int32_t add_skill(const game_skill &skill) noexcept;
        void remove_skill(int32_t id) noexcept;

        ft_map<int32_t, game_buff>       &get_buffs() noexcept;
        const ft_map<int32_t, game_buff> &get_buffs() const noexcept;

        ft_map<int32_t, game_debuff>       &get_debuffs() noexcept;
        const ft_map<int32_t, game_debuff> &get_debuffs() const noexcept;
        ft_map<int32_t, game_upgrade>       &get_upgrades() noexcept;
        const ft_map<int32_t, game_upgrade> &get_upgrades() const noexcept;

        ft_map<int32_t, game_quest>       &get_quests() noexcept;
        const ft_map<int32_t, game_quest> &get_quests() const noexcept;

        ft_map<int32_t, game_achievement>       &get_achievements() noexcept;
        const ft_map<int32_t, game_achievement> &get_achievements() const noexcept;

        game_reputation       &get_reputation() noexcept;
        const game_reputation &get_reputation() const noexcept;

        /* Server-owned inventory access. Inventory contents are revised by
         * game_inventory operations; the item map is private in production builds
         * concurrently with revision-checked transfers. */
        game_inventory       &get_inventory() noexcept;
        const game_inventory &get_inventory() const noexcept;

        game_experience_table       &get_experience_table() noexcept;
        const game_experience_table &get_experience_table() const noexcept;

        int32_t equip_item(int32_t slot, const ft_sharedptr<game_item> &item) noexcept;
        void unequip_item(int32_t slot) noexcept;
        ft_sharedptr<game_item> *get_equipped_item(int32_t slot) noexcept;
        ft_sharedptr<game_item> *get_equipped_item(int32_t slot) const noexcept;

        int32_t get_level() const noexcept;

        int32_t get_error() const noexcept;
        const char *get_error_str() const noexcept;
};

json_group *serialize_character(const game_character &character);
int32_t deserialize_character(game_character &character, json_group *group);

#endif
