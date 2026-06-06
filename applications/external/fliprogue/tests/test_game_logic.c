#include "test_support.h"

#include "categories/test_core.h"
#include "categories/test_world.h"
#include "categories/test_items.h"
#include "categories/test_monsters.h"
#include "categories/test_perks.h"
#include "categories/test_scores.h"

int main(void) {
    test_new_game_defaults();
    test_v072_memory_budget();
    test_v10_ui_page_math();
    test_v12_look_text_reports_status_and_terrain_effects();
    test_v12_score_format_and_orb_sorting();
    test_v02_class_starts();
    test_closed_door_opens_and_closes();
    test_bump_attack_kills_rat();
    test_ranger_line_shot_and_warrior_walks();
    test_ranged_resource_gate();
    test_ranged_hit_wakes_non_chasing_bat();
    test_terrain_traps_and_puddle();
    test_unknown_item_labels_are_unique_and_class_aware();
    test_hunger_states_and_food();
    test_v07_hunger_ticks_less_often();
    test_v07_orb_slows_hunger_clock();
    test_v071_potion_stacks_consume_one_at_a_time();
    test_v03_monster_table_and_boss_floor();
    test_v11_mimic_chest_reveals_and_attacks_once();
    test_v11_lurker_hidden_in_grass_and_revealed_by_search();
    test_v11_lurker_decorator_spawns_rarely();
    test_v11_eels_move_only_in_water();
    test_v12_eels_attack_only_waterborne_player();
    test_v11_land_monsters_path_around_deep_water();
    test_v11_eel_packs_spawn_in_some_flooded_rooms();
    test_v08_monster_hit_effects();
    test_v08_wight_visibility_and_fear();
    test_v04_layout_regressions();
    test_every_floor_has_expected_stairs_and_supplies();
    test_v11_room_templates_keep_floor_contracts();
    test_v11_special_floors_and_maze_template_keep_floor_contracts();
    test_v11_shrine_placement_and_interaction_log();
    test_v11_grates_block_and_open_from_same_floor_key_or_button();
    test_v12_gold_pickup_log_is_compact();
    test_v12_chests_block_movement_pathing_and_fire();
    test_v11_opened_grate_persists_and_reward_pocket_is_visible();
    test_v11_generated_grate_rewards_have_same_floor_unlocks();
    test_v11_room_decorators_keep_objects_on_valid_tiles();
    test_v07_food_spawn_scales_by_depth();
    test_stairs_orb_and_victory_summary();
    test_v071_floor_state_persists_when_returning();
    test_v12_tile_delta_budget_keeps_important_changes();
    test_v12_path_queue_overflow_fails_safely();
    test_v12_coarse_explored_does_not_restore_extra_walls();
    test_v072_compact_floor_state_restores_full_runtime_floor();
    test_xp_level_skills();
    test_v12_warrior_perks_have_gameplay_effects();
    test_v12_ranger_perks_have_gameplay_effects();
    test_v12_mage_perks_have_gameplay_effects();
    test_inventory_drop_actions();
    test_v101_consumable_stacks_merge_anywhere_and_consume_one();
    test_v12_successful_item_use_spends_full_game_tick();
    test_v11_chest_choose_one_and_persists_when_closed_or_full();
    test_v101_ranged_classes_start_with_more_native_resource();
    test_wand_and_charge_scroll();
    test_potion_effect_matrix();
    test_v12_fire_potions_and_ash_bead_rules();
    test_bad_player_effects_change_play();
    test_v071_chase_paths_to_remembered_corner();
    test_v071_far_warden_moves_every_turn();
    test_v071_warden_keeps_chasing_between_floors();
    test_v07_dew_from_grass_heals_and_records_event();
    test_v07_trap_detection_scales_with_dex();
    test_v11_directional_traps_have_wall_sources();
    test_v11_arrow_trap_damage_projectile_and_reveal();
    test_v11_fire_launcher_trap_projectile_and_fire_field();
    test_v11_snare_trap_paralyzes_for_three_ticks();
    test_v11_terrain_fire_capacity_ticks_and_spread();
    test_v12_glass_charm_peeks_one_tile_past_walls();
    test_v12_fire_extinguish_and_refresh_merge();
    test_v07_hidden_door_search_and_connectivity();
    test_v102_secret_search_radius_bump_reveal_and_generation();
    test_v11_water_gated_secret_rooms_are_reachable();
    test_v12_arrows_spawn_only_for_ranger();
    test_v07_death_cause_status();
    test_v10_starving_blocks_wait_without_spending_turn();
    test_v11_stunned_player_loses_turns_and_can_die();
    test_scroll_effect_matrix();
    test_v102_scroll_pickup_log_uses_runes_only_when_unknown();
    test_v10_secret_reveal_event_debounces();
    test_wand_effect_matrix();
    test_identify_scroll_marks_one_unknown();
    test_inventory_healing_and_fire_scroll();
    test_v09_throwable_stack_trinket_and_identify_contracts();
    test_v12_inventory_hints_hide_unknowns_and_describe_charms();
    test_v12_cinder_charm_bites_every_ten_turns_for_random_damage();
    test_v09_pack_aggro_and_bat_blink_strike();
    test_v09_skeleton_dodges_projectiles_but_not_burst();
    test_v10_skeleton_archer_short_name_and_ranged_ai();
    test_v09_cube_blocks_throwing_and_fire_burst_hits_inside();
    test_v09_fire_burst_changes_terrain_and_respects_puddles();
    test_v10_lone_wounded_monster_can_break_and_flee();
    test_v10_slime_splits_after_surviving_player_hit();
    test_v09_log_braid_caps_two_phrases();
    test_v09_sleeping_monsters_wake_and_idle_monsters_wander();
    test_v091_death_log_has_one_cause();
    test_v091_bat_blink_can_land_behind_player();
    test_v091_bat_cannot_attack_diagonal_without_blink();
    test_v091_attacked_bat_keeps_pursuing();
    test_v101_early_bats_return_without_pack_spike();
    test_v102_puddles_can_spread_during_generation();
    test_v12_sand_generates_as_patch_and_blocks_fire();
    test_v11_flooded_rooms_have_deep_water_and_shallow_edges();
    test_v11_water_movement_cost_and_extinguish();
    test_v12_ice_terrain_freeze_spread_and_slide();
    test_v102_dragonlings_are_rare_deep_packs();
    puts("game_logic tests passed");
    return 0;
}
