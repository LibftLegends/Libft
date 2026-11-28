#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_character_armor_multiplier_matches_repeated_rounding)
{
    game_character character;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    character.set_physical_armor(-1);
    character.set_magic_armor(-1);
    FT_ASSERT(character._physical_damage_multiplier == 1.0);
    FT_ASSERT(character._magic_damage_multiplier == 1.0);

    double expected = 1.0;
    int32_t armor = 0;
    while (armor <= 1024)
    {
        character.set_physical_armor(armor);
        character.set_magic_armor(armor);
        FT_ASSERT(character._physical_damage_multiplier == expected);
        FT_ASSERT(character._magic_damage_multiplier == expected);
        expected *= FT_ARMOR_POINT_REDUCTION;
        ++armor;
    }
    while (armor < 100000)
    {
        expected *= FT_ARMOR_POINT_REDUCTION;
        ++armor;
    }
    FT_ASSERT(expected * FT_ARMOR_POINT_REDUCTION == expected);
    character.set_physical_armor(INT32_MAX);
    character.set_magic_armor(INT32_MAX);
    FT_ASSERT(character._physical_damage_multiplier == expected);
    FT_ASSERT(character._magic_damage_multiplier == expected);
    FT_ASSERT_EQ(INT32_MAX, character.get_current_physical_armor());
    FT_ASSERT_EQ(INT32_MAX, character.get_current_magic_armor());
    return (1);
}

FT_TEST(test_game_character_large_damage_does_not_wrap_hit_points)
{
    game_character character;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    uint8_t rule = FT_DAMAGE_RULE_FLAT;
    while (rule <= FT_DAMAGE_RULE_MAGIC_SHIELD)
    {
        character.set_damage_rule(rule);
        character.set_hit_points(INT32_MAX);
        character.set_physical_armor(3);
        character.set_magic_armor(5);
        character.take_damage(INT64_C(1) << 33, FT_DAMAGE_PHYSICAL);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
        FT_ASSERT_EQ(0, character.get_hit_points());
        if (rule == FT_DAMAGE_RULE_BUFFER)
            FT_ASSERT_EQ(0, character.get_current_physical_armor());
        if (rule == FT_DAMAGE_RULE_MAGIC_SHIELD)
            FT_ASSERT_EQ(0, character.get_current_magic_armor());
        ++rule;
    }

    character.set_hit_points(50);
    character.set_physical_armor(-10);
    character.take_damage_flat(INT64_MAX, FT_DAMAGE_PHYSICAL);
    FT_ASSERT_EQ(0, character.get_hit_points());
    character.set_hit_points(50);
    character.set_magic_armor(0);
    character.take_damage_scaled(INT64_MAX, FT_DAMAGE_MAGICAL);
    FT_ASSERT_EQ(0, character.get_hit_points());
    character.set_hit_points(50);
    character.take_damage_buffer(INT64_MIN, FT_DAMAGE_MAGICAL);
    FT_ASSERT_EQ(50, character.get_hit_points());
    return (1);
}

FT_TEST(test_game_character_skill_damage_preserves_integer_rounding_at_limits)
{
    game_character character;
    game_skill skill;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.initialize());
    skill.set_id(1);
    skill.set_modifier2(-50);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(skill));
    auto *entry = character.get_skills().find(1);
    FT_ASSERT(entry != character.get_skills().end());

    FT_ASSERT_EQ(2, character.apply_skill_modifiers(3));
    FT_ASSERT_EQ(INT64_C(4611686018427387904), character.apply_skill_modifiers(INT64_MAX));
    entry->value.set_modifier2(200);
    entry->value.set_modifier1(5);
    FT_ASSERT_EQ(INT64_MAX, character.apply_skill_modifiers(INT64_MAX));
    entry->value.set_modifier2(-200);
    entry->value.set_modifier1(0);
    FT_ASSERT_EQ(INT64_MAX, character.apply_skill_modifiers(INT64_MIN));
    FT_ASSERT_EQ(0, character.apply_skill_modifiers(INT64_MAX));
    entry->value.set_modifier2(-100);
    FT_ASSERT_EQ(0, character.apply_skill_modifiers(INT64_MAX));
    entry->value.set_cooldown(1);
    FT_ASSERT_EQ(17, character.apply_skill_modifiers(17));
    entry->value.set_modifier1(INT32_MAX);
    entry->value.add_modifier1(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, entry->value.get_error());
    FT_ASSERT_EQ(INT32_MAX, entry->value.get_modifier1());
    return (1);
}

FT_TEST(test_game_character_damage_preserves_health_and_armor_on_skill_lock_failure)
{
    game_character character;
    game_skill skill;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.initialize());
    skill.set_id(1);
    skill.set_modifier1(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(skill));
    auto *entry = character.get_skills().find(1);
    FT_ASSERT(entry != character.get_skills().end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry->value.enable_thread_safety());
    character.set_hit_points(40);
    character.set_magic_armor(15);

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    character.take_damage_buffer(20, FT_DAMAGE_MAGICAL);
    const int32_t result = character.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(40, character.get_hit_points());
    FT_ASSERT_EQ(15, character.get_current_magic_armor());
    character.take_damage_buffer(20, FT_DAMAGE_MAGICAL);
    FT_ASSERT_EQ(30, character.get_hit_points());
    FT_ASSERT_EQ(0, character.get_current_magic_armor());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_skills().enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    character.take_damage_flat(20, FT_DAMAGE_PHYSICAL);
    const int32_t map_result = character.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, map_result);
    FT_ASSERT_EQ(30, character.get_hit_points());
    return (1);
}

FT_TEST(test_game_character_flat_physical_damage_equal_to_armor_is_absorbed)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_physical_armor(5);
    value.take_damage_flat(5, FT_DAMAGE_PHYSICAL);
    FT_ASSERT_EQ(20, value.get_hit_points());
    return (1);
}

FT_TEST(test_game_character_flat_physical_damage_above_armor_reduces_hit_points)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_physical_armor(5);
    value.take_damage_flat(8, FT_DAMAGE_PHYSICAL);
    FT_ASSERT_EQ(17, value.get_hit_points());
    return (1);
}

FT_TEST(test_game_character_flat_magical_damage_equal_to_armor_is_absorbed)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_magic_armor(5);
    value.take_damage_flat(5, FT_DAMAGE_MAGICAL);
    FT_ASSERT_EQ(20, value.get_hit_points());
    return (1);
}

FT_TEST(test_game_character_flat_magical_damage_above_armor_reduces_hit_points)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_magic_armor(5);
    value.take_damage_flat(8, FT_DAMAGE_MAGICAL);
    FT_ASSERT_EQ(17, value.get_hit_points());
    return (1);
}

FT_TEST(test_game_character_buffer_physical_damage_equal_to_armor_is_absorbed)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_current_physical_armor(5);
    value.take_damage_buffer(5, FT_DAMAGE_PHYSICAL);
    FT_ASSERT_EQ(20, value.get_hit_points());
    FT_ASSERT_EQ(0, value.get_current_physical_armor());
    return (1);
}

FT_TEST(test_game_character_buffer_physical_damage_above_armor_spills)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_current_physical_armor(5);
    value.take_damage_buffer(8, FT_DAMAGE_PHYSICAL);
    FT_ASSERT_EQ(17, value.get_hit_points());
    FT_ASSERT_EQ(0, value.get_current_physical_armor());
    return (1);
}

FT_TEST(test_game_character_buffer_magical_damage_equal_to_armor_is_absorbed)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_current_magic_armor(5);
    value.take_damage_buffer(5, FT_DAMAGE_MAGICAL);
    FT_ASSERT_EQ(20, value.get_hit_points());
    FT_ASSERT_EQ(0, value.get_current_magic_armor());
    return (1);
}

FT_TEST(test_game_character_buffer_magical_damage_above_armor_spills)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_current_magic_armor(5);
    value.take_damage_buffer(8, FT_DAMAGE_MAGICAL);
    FT_ASSERT_EQ(17, value.get_hit_points());
    FT_ASSERT_EQ(0, value.get_current_magic_armor());
    return (1);
}

FT_TEST(
    test_game_character_magic_shield_physical_damage_uses_current_magic_armor)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_current_magic_armor(5);
    value.take_damage_magic_shield(5, FT_DAMAGE_PHYSICAL);
    FT_ASSERT_EQ(20, value.get_hit_points());
    FT_ASSERT_EQ(0, value.get_current_magic_armor());
    return (1);
}

FT_TEST(
    test_game_character_magic_shield_magical_damage_uses_current_magic_armor)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_current_magic_armor(5);
    value.take_damage_magic_shield(5, FT_DAMAGE_MAGICAL);
    FT_ASSERT_EQ(20, value.get_hit_points());
    FT_ASSERT_EQ(0, value.get_current_magic_armor());
    return (1);
}

FT_TEST(test_game_character_buffer_damage_with_zero_armor_reaches_hit_points)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_current_physical_armor(0);
    value.take_damage_buffer(3, FT_DAMAGE_PHYSICAL);
    FT_ASSERT_EQ(17, value.get_hit_points());
    return (1);
}

FT_TEST(
    test_game_character_magic_shield_damage_with_zero_armor_reaches_hit_points)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(20);
    value.set_current_magic_armor(0);
    value.take_damage_magic_shield(3, FT_DAMAGE_MAGICAL);
    FT_ASSERT_EQ(17, value.get_hit_points());
    return (1);
}

FT_TEST(test_game_character_remove_missing_skill_preserves_empty_collection)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.remove_skill(404);
    FT_ASSERT_EQ(0, value.get_skills().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_character_restore_physical_armor_restores_current_value)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_physical_armor(9);
    value.set_current_physical_armor(0);
    value.restore_physical_armor();
    FT_ASSERT_EQ(9, value.get_current_physical_armor());
    return (1);
}

FT_TEST(test_game_character_restore_magic_armor_restores_current_value)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_magic_armor(9);
    value.set_current_magic_armor(0);
    value.restore_magic_armor();
    FT_ASSERT_EQ(9, value.get_current_magic_armor());
    return (1);
}

FT_TEST(test_game_character_damage_cannot_reduce_hit_points_below_zero)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(2);
    value.take_damage_flat(100, FT_DAMAGE_PHYSICAL);
    FT_ASSERT_EQ(0, value.get_hit_points());
    FT_ASSERT_EQ(FT_FALSE, value.is_alive());
    return (1);
}
