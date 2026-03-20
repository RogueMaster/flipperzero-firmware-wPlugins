#ifndef HOLDEM_APP_INTERNAL_H
#define HOLDEM_APP_INTERNAL_H

#include "holdem_types.h"
#include "holdem_storage.h"
#include "holdem_eval.h"
#include "holdem_ai.h"
#include "holdem_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    HoldemNavGlyphUpDown = 0,
    HoldemNavGlyphUp,
    HoldemNavGlyphDown,
} HoldemNavGlyph;

extern const char* k_stage_name[];

void holdem_draw_line(Canvas* canvas, uint8_t y, const char* text);
void holdem_draw_line_right(Canvas* canvas, uint8_t y, const char* text);
void holdem_draw_back_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y);
void holdem_draw_ok_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y);
void holdem_draw_chip_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y);
uint8_t holdem_display_text_width(Canvas* canvas, const char* text);
void holdem_draw_display_text(Canvas* canvas, uint8_t x, uint8_t baseline_y, const char* text);
uint8_t holdem_centered_x_for_width(uint8_t width);
void holdem_draw_text_centered(Canvas* canvas, uint8_t baseline_y, const char* text);
void holdem_draw_centered_inline_lr_text(
    Canvas* canvas,
    uint8_t baseline_y,
    const char* prefix,
    bool left_present,
    const char* middle,
    bool right_present,
    const char* suffix);
uint8_t holdem_ok_sequence_width(Canvas* canvas, const char* before_text, const char* after_text);
uint8_t holdem_ok_sequence_width_with_gap(
    Canvas* canvas,
    const char* before_text,
    const char* after_text,
    uint8_t after_gap_px);
void holdem_draw_ok_sequence_with_gap(
    Canvas* canvas,
    uint8_t x,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text,
    uint8_t after_gap_px);
void holdem_draw_ok_sequence(
    Canvas* canvas,
    uint8_t x,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text);
void holdem_draw_ok_sequence_right(
    Canvas* canvas,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text);
void holdem_draw_ok_sequence_center_with_gap(
    Canvas* canvas,
    uint8_t center_x,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text,
    uint8_t after_gap_px);
void holdem_draw_left_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y);
void holdem_draw_right_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y);
void holdem_draw_ff_icon(Canvas* canvas, uint8_t x, uint8_t baseline_y);
void holdem_draw_ok_ff_hint(Canvas* canvas, uint8_t baseline_y);
void holdem_draw_nav_glyph(Canvas* canvas, uint8_t x, uint8_t baseline_y, HoldemNavGlyph glyph);
void holdem_draw_nav_icon(
    Canvas* canvas,
    uint8_t x,
    uint8_t baseline_y,
    uint8_t page,
    uint8_t total_pages);
void holdem_draw_back_sequence(
    Canvas* canvas,
    uint8_t x,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text);
void holdem_draw_back_sequence_right(
    Canvas* canvas,
    uint8_t baseline_y,
    const char* before_text,
    const char* after_text);
void holdem_draw_back_cancel_hint(Canvas* canvas, uint8_t baseline_y);

const char* holdem_ai_label(uint8_t level_pct);
uint8_t holdem_prev_ai_level(uint8_t level_pct);
uint8_t holdem_next_ai_level(uint8_t level_pct);
bool holdem_is_menu_mode(UiMode mode);
void holdem_set_foreground_mode(HoldemApp* app, UiMode mode);
const char* holdem_display_name(const Player* player);
bool holdem_player_is_eliminated(const HoldemGame* game, const Player* player);
char holdem_role_char(const HoldemGame* game, int player_index);
char holdem_status_char(const HoldemGame* game, const Player* player);
void holdem_build_display_order(const HoldemGame* game, size_t order[HOLDEM_MAX_PLAYERS]);

void holdem_draw_callback(Canvas* canvas, void* ctx);
void holdem_input_callback(InputEvent* event, void* ctx);

void show_help(HoldemApp* app);
void show_exit_prompt(HoldemApp* app);
void reset_to_new_game(HoldemApp* app);
bool prompt_showdown_inspect(HoldemApp* app);
int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi);
void set_ai_level(HoldemApp* app, uint8_t level_pct);
void set_bot_action_delay(HoldemApp* app, bool enabled, uint16_t delay_ms);
int32_t holdem_default_small_blind(void);
int32_t holdem_active_small_blind(const HoldemApp* app);
bool holdem_blind_edit_is_dirty(const HoldemApp* app);
bool holdem_bot_edit_is_dirty(const HoldemApp* app);
void holdem_reset_progressive_schedule(HoldemApp* app);
void holdem_apply_blind_values_now(HoldemApp* app, int32_t new_small_blind);
bool holdem_apply_progressive_increase_if_due(HoldemApp* app);
void apply_or_defer_blind_edit(HoldemApp* app, int32_t new_small_blind);
void holdem_commit_blind_edit(HoldemApp* app);
bool process_global_event(HoldemApp* app, const InputEvent* ev);
void flush_input_queue(HoldemApp* app);
bool holdem_can_skip_folded_autoplay(const HoldemApp* app);
void startup_splash_and_jingle(HoldemApp* app);
void startup_choose_load_or_new(HoldemApp* app);
bool wait_for_start_confirmation(HoldemApp* app);

int32_t holdem_total_bankroll(const HoldemGame* game);
bool qualifies_for_big_win(const HoldemApp* app, const PayoutResult* payout);
bool show_interstitial_screen(
    HoldemApp* app,
    const char* text,
    bool fireworks,
    uint32_t duration_ms,
    bool allow_back_skip,
    bool show_continue_hint);
bool show_big_win_screen(HoldemApp* app);
bool show_progressive_blinds_screen(HoldemApp* app);
void show_hand_result_screen(HoldemApp* app, const PayoutResult* payout);
bool play_one_hand(HoldemApp* app, PayoutResult* payout);
bool resume_loaded_hand(HoldemApp* app, PayoutResult* payout);

#endif
