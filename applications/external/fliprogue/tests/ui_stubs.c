#include <assert.h>

#include "score_store.h"
#include "ui_feedback.h"
#include "ui_screens.h"

// Only device feedback is suppressed; input, camera, and gameplay run unchanged.
uint32_t furi_get_tick(void) {
    return 1000;
}

uint32_t furi_hal_random_get(void) {
    return 122;
}

FeedbackBefore feedback_capture(AppContext* app) {
    return (FeedbackBefore){app->game->player.hp, app->game->player.level, false};
}

void finish_action(AppContext* app, FeedbackBefore before, FrActionResult result) {
    (void)app;
    (void)before;
    (void)result;
}

// Uncovered screen/storage paths fail loudly instead of silently simulating UI.
void load_score_store(AppContext* app) {
    (void)app;
    assert(false);
}

void save_sound_setting(AppContext* app) {
    (void)app;
    assert(false);
}

uint8_t inventory_visible_count(const FrGame* game, uint8_t tab) {
    (void)game;
    (void)tab;
    assert(false);
    return 0;
}

uint8_t inventory_slot_index_at(const FrGame* game, uint8_t tab, uint8_t visible_index) {
    (void)game;
    (void)tab;
    (void)visible_index;
    assert(false);
    return 0;
}

uint8_t identify_visible_count(const FrGame* game, uint8_t source_index) {
    (void)game;
    (void)source_index;
    assert(false);
    return 0;
}

uint8_t identify_slot_index_at(const FrGame* game, uint8_t source_index, uint8_t visible_index) {
    (void)game;
    (void)source_index;
    (void)visible_index;
    assert(false);
    return 0;
}

void inventory_clamp_selection(AppContext* app) {
    (void)app;
    assert(false);
}

uint8_t item_choice_count(const FrInvSlot* slot) {
    (void)slot;
    assert(false);
    return 0;
}

const char* look_hint(AppContext* app) {
    (void)app;
    assert(false);
    return "";
}

uint8_t help_line_count(uint8_t topic) {
    (void)topic;
    assert(false);
    return 0;
}
