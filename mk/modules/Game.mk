TARGET := Game.a
DEBUG_TARGET := Game_debug.a

SRCS := game_map3d.cpp \
        game_character_constructor.cpp \
        game_character_getters_setters.cpp \
        game_character_add_remove.cpp \
        game_character_misc.cpp \
        game_character_metrics.cpp \
        game_character_save_load.cpp \
	game_quest.cpp \
	game_achievement.cpp \
	game_reputation.cpp \
	game_buff.cpp \
	game_debuff.cpp \
	game_skill.cpp \
	game_upgrade.cpp \
        game_event.cpp \
        game_event_scheduler.cpp \
        game_event_scheduler_telemetry.cpp \
        game_world.cpp \
        game_voxel_chunk.cpp \
        game_voxel_region.cpp \
        game_world_replay.cpp \
        game_server.cpp \
        game_item.cpp \
	game_inventory.cpp \
	game_equipment.cpp \
	game_save.cpp \
	game_load.cpp \
	game_experience_table.cpp \
        game_resistance.cpp \
        game_pathfinding.cpp \
        game_crafting.cpp \
        game_data_catalog.cpp \
        game_dialogue_line.cpp \
        game_dialogue_script.cpp \
        game_dialogue_table.cpp \
        game_behavior_action.cpp \
        game_behavior_profile.cpp \
        game_behavior_table.cpp \
        game_region_definition.cpp \
        game_world_region.cpp \
        game_world_registry.cpp \
        game_price_definition.cpp \
        game_rarity_band.cpp \
        game_vendor_profile.cpp \
        game_currency_rate.cpp \
        game_economy_table.cpp \
        game_progress_tracker.cpp \
        game_hooks.cpp \
        game_state.cpp \
        game_behavior_tree.cpp \
        game_lua_runtime.cpp \
        game_scripting_bridge.cpp

HEADERS := game_map3d.hpp \
	game_character.hpp \
	game_quest.hpp \
	game_achievement.hpp \
	game_reputation.hpp \
	game_buff.hpp \
	game_debuff.hpp \
	game_skill.hpp \
	game_upgrade.hpp \
	game_event.hpp \
        game_event_scheduler.hpp \
        game_event_scheduler_telemetry.hpp \
        game_world.hpp \
        game_voxel_chunk.hpp \
        game_voxel_region.hpp \
        game_region_backend.hpp \
        game_world_replay.hpp \
        game_server.hpp \
	game_item.hpp \
	game_inventory.hpp \
	game_equipment.hpp \
        game_experience_table.hpp \
        game_resistance.hpp \
        game_pathfinding.hpp \
        game_crafting.hpp \
        game_data_catalog.hpp \
        game_dialogue_line.hpp \
        game_dialogue_script.hpp \
        game_dialogue_table.hpp \
        game_behavior_action.hpp \
        game_behavior_profile.hpp \
        game_behavior_table.hpp \
        game_region_definition.hpp \
        game_world_region.hpp \
        game_world_registry.hpp \
        game_price_definition.hpp \
        game_rarity_band.hpp \
        game_vendor_profile.hpp \
        game_currency_rate.hpp \
        game_economy_table.hpp \
        game_progress_tracker.hpp \
        game_hooks.hpp \
        game_state.hpp \
        game_behavior_tree.hpp \
        game_scripting_bridge.hpp

include $(dir $(lastword $(MAKEFILE_LIST)))common/module_defaults.mk
