#include "include/domain/avocado_rules.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_apply_days_adds_dirt(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.last_timestamp = 1000;
    avocado_rules_apply_elapsed_days(&d, 1000 + 86400);
    assert(d.dirty_level == 1);
    assert(d.last_timestamp == 1000 + 86400);
}

static void test_apply_no_partial_day(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.last_timestamp = 0;
    d.dirty_level = 0;
    avocado_rules_apply_elapsed_days(&d, 3600);
    assert(d.dirty_level == 0);
}

static void test_clock_skew_resets_anchor(void) {
    AvocadoData d;
    d.last_timestamp = 5000;
    d.dirty_level = 3;
    avocado_rules_apply_elapsed_days(&d, 1000);
    assert(d.last_timestamp == 1000);
    assert(d.dirty_level == 3);
}

static void test_clean_grows_roots_when_grime(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.dirty_level = 2;
    d.roots_length = 0;
    avocado_rules_on_primary_action(&d);
    assert(d.dirty_level == 0);
    assert(d.roots_length == 1);
}

static void test_clean_clear_water_no_roots(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.dirty_level = 0;
    d.roots_length = 3;
    for (int i = 0; i < 80; i++) {
        avocado_rules_on_primary_action(&d);
    }
    assert(d.dirty_level == 0);
    assert(d.roots_length == 3);
}

static void test_clean_second_press_same_session_no_extra_roots(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.dirty_level = 1;
    d.roots_length = 0;
    avocado_rules_on_primary_action(&d);
    assert(d.roots_length == 1);
    avocado_rules_on_primary_action(&d);
    assert(d.roots_length == 1);
}

static void test_game_over_restart(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.dirty_level = AVOCADO_GRIME_GAME_OVER;
    d.roots_length = 5;
    d.victory_seen = 1;
    avocado_rules_on_primary_action(&d);
    assert(d.dirty_level == 0);
    assert(d.roots_length == 0);
    assert(d.victory_seen == 0);
}

static void test_reaching_max_roots_sets_victory_pending(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.dirty_level = 1;
    d.roots_length = AVOCADO_ROOTS_MAX - 1;
    d.victory_seen = 1;
    avocado_rules_on_primary_action(&d);
    assert(d.roots_length == AVOCADO_ROOTS_MAX);
    assert(d.victory_seen == 0);
    assert(avocado_rules_should_show_victory(&d));
}

static void test_acknowledge_victory(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.roots_length = AVOCADO_ROOTS_MAX;
    d.victory_seen = 0;
    d.dirty_level = 3;
    d.last_timestamp = 100;
    avocado_rules_acknowledge_victory(&d, 9999u);
    assert(d.victory_seen == 1);
    assert(d.roots_length == 0);
    assert(d.dirty_level == 0);
    assert(d.last_timestamp == 9999u);
    assert(!avocado_rules_should_show_victory(&d));
}

static void test_is_game_over(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.dirty_level = AVOCADO_GRIME_GAME_OVER - 1;
    assert(!avocado_rules_is_game_over(&d));
    d.dirty_level = AVOCADO_GRIME_GAME_OVER;
    assert(avocado_rules_is_game_over(&d));
}

static void test_debug_add_simulated_days(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.last_timestamp = 1000;
    avocado_rules_debug_add_simulated_days(&d, 3);
    assert(d.dirty_level == 3);
    assert(d.last_timestamp == 1000 + 3u * 86400u);
}

static void test_debug_add_days_noop_when_game_over(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.dirty_level = AVOCADO_GRIME_GAME_OVER;
    d.last_timestamp = 500;
    avocado_rules_debug_add_simulated_days(&d, 5);
    assert(d.dirty_level == AVOCADO_GRIME_GAME_OVER);
    assert(d.last_timestamp == 500);
}

static void test_debug_bump_roots(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.roots_length = 2;
    avocado_rules_debug_bump_roots(&d);
    assert(d.roots_length == 3);
    assert(d.dirty_level == 0);
}

static void test_debug_preset_victory_pending(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    d.dirty_level = 5;
    d.roots_length = 1;
    d.victory_seen = 1;
    avocado_rules_debug_preset_victory_pending(&d);
    assert(d.dirty_level == 0);
    assert(d.roots_length == AVOCADO_ROOTS_MAX);
    assert(d.victory_seen == 0);
    assert(avocado_rules_should_show_victory(&d));
}

static void test_debug_preset_game_over(void) {
    AvocadoData d;
    memset(&d, 0, sizeof(d));
    avocado_rules_debug_preset_game_over(&d);
    assert(avocado_rules_is_game_over(&d));
}

int main(void) {
    test_apply_days_adds_dirt();
    test_apply_no_partial_day();
    test_clock_skew_resets_anchor();
    test_clean_grows_roots_when_grime();
    test_clean_clear_water_no_roots();
    test_clean_second_press_same_session_no_extra_roots();
    test_game_over_restart();
    test_reaching_max_roots_sets_victory_pending();
    test_acknowledge_victory();
    test_is_game_over();
    test_debug_add_simulated_days();
    test_debug_add_days_noop_when_game_over();
    test_debug_bump_roots();
    test_debug_preset_victory_pending();
    test_debug_preset_game_over();
    puts("avocado_rules tests OK");
    return 0;
}
