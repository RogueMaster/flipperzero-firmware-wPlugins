#include "mouse_click_recorder.h"
#include "hid_helper.h"
#include <gui/elements.h>

#define TAG "ClickRecorder"

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void app_draw_callback(Canvas* canvas, void* ctx);
static void app_input_callback(InputEvent* input_event, void* ctx);
static void app_timer_callback(void* ctx);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static uint32_t ticks_now(void) {
    return furi_get_tick();
}

static uint32_t ticks_to_ms(uint32_t ticks) {
    return ticks * 1000 / furi_kernel_get_tick_frequency();
}

static AppMode mode_from_selection(uint8_t sel) {
    return (AppMode)sel;
}

static uint8_t spam_btn_hid(SpamButton btn) {
    switch(btn) {
    case SpamBtnLeft:
        return HID_MOUSE_BTN_LEFT;
    case SpamBtnMiddle:
        return HID_MOUSE_BTN_WHEEL;
    case SpamBtnRight:
        return HID_MOUSE_BTN_RIGHT;
    default:
        return HID_MOUSE_BTN_LEFT;
    }
}

static uint8_t click_type_hid(ClickType ct) {
    switch(ct) {
    case ClickLeft:
        return HID_MOUSE_BTN_LEFT;
    case ClickRight:
        return HID_MOUSE_BTN_RIGHT;
    case ClickBoth:
        return HID_MOUSE_BTN_LEFT | HID_MOUSE_BTN_RIGHT;
    default:
        return HID_MOUSE_BTN_LEFT;
    }
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------
static void enter_mode_select(App* app) {
    app->state = StateModeSelect;
    furi_timer_stop(app->timer);
}

static void enter_template_select(App* app) {
    app->state = StateTemplateSelect;
    furi_timer_stop(app->timer);
}

static void enter_countdown(App* app) {
    app->state = StateCountdown;
    app->countdown_value = COUNTDOWN_SECS;
    furi_timer_start(app->timer, furi_kernel_get_tick_frequency());
}

static void enter_recording(App* app) {
    app->state = StateRecording;
    app->recording.duration_sec = app->selected_duration;
    app->recording.num_keyframes = 0;
    app->record_start_tick = ticks_now();
    app->elapsed_ms = 0;
    furi_timer_start(app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
}

static void enter_positioning(App* app) {
    app->state = StatePositioning;
    app->dir_up = false;
    app->dir_down = false;
    app->dir_left = false;
    app->dir_right = false;
    furi_timer_stop(app->timer);
}

static void enter_tune_track(App* app) {
    app->state = StateTuneTrack;
    furi_timer_stop(app->timer);
    app->original_duration_sec = app->recording.duration_sec;
    app->tuned_duration_sec = app->recording.duration_sec;
    memcpy(
        app->original_keyframes,
        app->recording.keyframes,
        sizeof(Keyframe) * app->recording.num_keyframes);
}

static void tune_track_recalc(App* app) {
    if(app->tuned_duration_sec == app->original_duration_sec) {
        memcpy(
            app->recording.keyframes,
            app->original_keyframes,
            sizeof(Keyframe) * app->recording.num_keyframes);
    } else {
        uint32_t orig_ms = (uint32_t)app->original_duration_sec * 1000;
        uint32_t new_ms = (uint32_t)app->tuned_duration_sec * 1000;
        for(uint8_t i = 0; i < app->recording.num_keyframes; i++) {
            app->recording.keyframes[i].timestamp_ms =
                (app->original_keyframes[i].timestamp_ms * new_ms) / orig_ms;
        }
    }
    app->recording.duration_sec = app->tuned_duration_sec;
}

static void enter_shuffler_click_count(App* app) {
    app->state = StateShufflerClickCount;
    furi_timer_stop(app->timer);
}

static void shuffle_generate_keyframes(App* app) {
    uint32_t dur_ms = (uint32_t)app->recording.duration_sec * 1000;
    if(dur_ms == 0) return;
    uint32_t min_gap = CLICK_HOLD_MS + 10;
    for(uint8_t i = 0; i < app->recording.num_keyframes; i++) {
        app->recording.keyframes[i].timestamp_ms = furi_hal_random_get() % dur_ms;
        app->recording.keyframes[i].click_type = ClickLeft;
    }
    // Bubble sort
    for(uint8_t i = 0; i < app->recording.num_keyframes; i++) {
        for(uint8_t j = i + 1; j < app->recording.num_keyframes; j++) {
            if(app->recording.keyframes[j].timestamp_ms <
               app->recording.keyframes[i].timestamp_ms) {
                Keyframe tmp = app->recording.keyframes[i];
                app->recording.keyframes[i] = app->recording.keyframes[j];
                app->recording.keyframes[j] = tmp;
            }
        }
    }
    // Enforce minimum gap, drop excess
    uint8_t valid = 0;
    for(uint8_t i = 0; i < app->recording.num_keyframes; i++) {
        if(i > 0 && app->recording.keyframes[i].timestamp_ms <
                        app->recording.keyframes[valid - 1].timestamp_ms + min_gap) {
            app->recording.keyframes[i].timestamp_ms =
                app->recording.keyframes[valid - 1].timestamp_ms + min_gap;
        }
        if(app->recording.keyframes[i].timestamp_ms >= dur_ms) break;
        if(i != valid) app->recording.keyframes[valid] = app->recording.keyframes[i];
        valid++;
    }
    app->recording.num_keyframes = valid;
}

static void enter_play_mode_select(App* app) {
    app->state = StatePlayModeSelect;
    app->play_mode_selection = 0;
}

static void enter_playback(App* app) {
    AppMode mode = mode_from_selection(app->menu_selection);
    if(mode == ModeShuffler) {
        shuffle_generate_keyframes(app);
    }
    app->state = StatePlayback;
    app->playback_start_tick = ticks_now();
    app->playback_ms = 0;
    app->playback_keyframe_idx = 0;
    app->playback_click_pending = false;
    furi_timer_start(app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
}

static void enter_paused(App* app) {
    app->state = StatePaused;
    app->pause_selection = PauseMenuContinue;
    furi_timer_stop(app->timer);
}

static void enter_spam_click_config(App* app) {
    app->state = StateSpamClickConfig;
    furi_timer_stop(app->timer);
}

static void enter_spam_clicking(App* app) {
    app->state = StateSpamClicking;
    app->spam_clicks_done = 0;
    app->spam_paused = false;
    app->playback_click_pending = false;
    furi_timer_start(app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
    app->playback_start_tick = ticks_now();
}

static void enter_computer_mouse(App* app) {
    app->state = StateComputerMouse;
    furi_timer_stop(app->timer);
    app->dir_up = false;
    app->dir_down = false;
    app->dir_left = false;
    app->dir_right = false;
    app->mouse_left_pressed = false;
    app->mouse_right_pressed = false;
}

static void enter_quit_confirm(App* app) {
    app->state_before_quit = app->state;
    if(app->state_before_quit == StatePlayback || app->state_before_quit == StateRecording ||
       app->state_before_quit == StateCountdown || app->state_before_quit == StateSpamClicking) {
        furi_timer_stop(app->timer);
    }
    app->state = StateQuitConfirm;
    app->quit_selection = 0;
}

// ---------------------------------------------------------------------------
// Timer callback (runs in timer thread)
// ---------------------------------------------------------------------------
static void app_timer_callback(void* ctx) {
    App* app = ctx;

    if(app->state == StateCountdown) {
        if(app->countdown_value > 1) {
            app->countdown_value--;
        } else {
            enter_recording(app);
        }
    } else if(app->state == StateRecording) {
        app->elapsed_ms = ticks_to_ms(ticks_now() - app->record_start_tick);
        uint32_t duration_ms = (uint32_t)app->recording.duration_sec * 1000;
        if(app->elapsed_ms >= duration_ms) {
            app->elapsed_ms = duration_ms;
            enter_tune_track(app);
        }
    } else if(app->state == StatePlayback) {
        // Handle pending click release
        if(app->playback_click_pending) {
            uint32_t held = ticks_to_ms(ticks_now() - app->click_press_tick);
            if(held >= CLICK_HOLD_MS) {
                hid_mouse_release_btn(app, click_type_hid(app->playback_click_type));
                app->playback_click_pending = false;
            }
        }

        app->playback_ms = ticks_to_ms(ticks_now() - app->playback_start_tick);
        uint32_t duration_ms = (uint32_t)app->recording.duration_sec * 1000;

        // Fire keyframes whose timestamp has been reached
        while(app->playback_keyframe_idx < app->recording.num_keyframes &&
              app->playback_ms >=
                  app->recording.keyframes[app->playback_keyframe_idx].timestamp_ms) {
            if(!app->playback_click_pending) {
                ClickType ct = app->recording.keyframes[app->playback_keyframe_idx].click_type;
                hid_mouse_press_btn(app, click_type_hid(ct));
                app->playback_click_pending = true;
                app->playback_click_type = ct;
                app->click_press_tick = ticks_now();
            }
            app->playback_keyframe_idx++;
        }

        // Check if playback finished
        if(app->playback_ms >= duration_ms &&
           app->playback_keyframe_idx >= app->recording.num_keyframes &&
           !app->playback_click_pending) {
            AppMode mode = mode_from_selection(app->menu_selection);
            if(app->loop_mode == LoopForever) {
                if(mode == ModeShuffler) {
                    shuffle_generate_keyframes(app);
                }
                app->playback_start_tick = ticks_now();
                app->playback_ms = 0;
                app->playback_keyframe_idx = 0;
            } else {
                // Play once done — always return to positioning
                enter_positioning(app);
            }
        }
    } else if(app->state == StateSpamClicking) {
        // Handle pending click release
        if(app->playback_click_pending) {
            uint32_t held = ticks_to_ms(ticks_now() - app->click_press_tick);
            if(held >= CLICK_HOLD_MS) {
                hid_mouse_release_btn(app, spam_btn_hid(app->spam_button));
                app->playback_click_pending = false;
                app->spam_clicks_done++;
                if(app->spam_clicks_done >= app->spam_click_count) {
                    if(app->loop_mode == LoopForever) {
                        app->spam_clicks_done = 0;
                        app->playback_start_tick = ticks_now();
                    } else {
                        enter_positioning(app);
                    }
                }
            }
        } else {
            // Check if interval passed since last action
            uint32_t elapsed = ticks_to_ms(ticks_now() - app->playback_start_tick);
            uint32_t expected = (uint32_t)app->spam_clicks_done * SPAM_INTERVAL_MS;
            if(elapsed >= expected && app->spam_clicks_done < app->spam_click_count) {
                hid_mouse_press_btn(app, spam_btn_hid(app->spam_button));
                app->playback_click_pending = true;
                app->click_press_tick = ticks_now();
            }
        }
    }

    if(app->view_port) {
        view_port_update(app->view_port);
    }
}

// ---------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------
static void draw_header(Canvas* canvas, const char* title, App* app) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, title);
    canvas_set_font(canvas, FontSecondary);
    if(app->transport == TransportBle) {
        const char* ble_str = app->ble_connected ? "BLE*" : "BLE";
        canvas_draw_str_aligned(canvas, 124, 2, AlignRight, AlignTop, ble_str);
    } else {
        canvas_draw_str_aligned(canvas, 124, 2, AlignRight, AlignTop, "USB");
    }
    canvas_set_color(canvas, ColorBlack);
}

static void draw_progress_bar(Canvas* canvas, uint8_t y, uint32_t current, uint32_t total) {
    const uint8_t bar_x = 4;
    const uint8_t bar_w = 120;
    const uint8_t bar_h = 7;
    canvas_draw_rframe(canvas, bar_x, y, bar_w, bar_h, 2);
    if(total > 0) {
        uint32_t fill = (current * (uint32_t)(bar_w - 4)) / total;
        if(fill > (uint32_t)(bar_w - 4)) fill = bar_w - 4;
        if(fill > 0) {
            canvas_draw_rbox(canvas, bar_x + 2, y + 2, fill, bar_h - 4, 1);
        }
    }
}

static void
    draw_hint_bar(Canvas* canvas, const char* left, const char* center, const char* right) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 53, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    if(left) canvas_draw_str_aligned(canvas, 4, 54, AlignLeft, AlignTop, left);
    if(center) canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, center);
    if(right) canvas_draw_str_aligned(canvas, 124, 54, AlignRight, AlignTop, right);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_keyframe_ticks(Canvas* canvas, uint8_t bar_y, App* app, uint32_t total_ms) {
    if(total_ms == 0) return;
    const uint8_t bar_x = 4;
    const uint8_t bar_w = 120;
    for(uint8_t i = 0; i < app->recording.num_keyframes; i++) {
        uint32_t x =
            bar_x + 2 + (app->recording.keyframes[i].timestamp_ms * (bar_w - 4)) / total_ms;
        if(x > (uint32_t)(bar_x + bar_w - 3)) x = bar_x + bar_w - 3;
        canvas_draw_line(canvas, x, bar_y - 2, x, bar_y);
    }
}

static void draw_crosshair(Canvas* canvas, App* app, uint8_t cx, uint8_t cy) {
    canvas_draw_circle(canvas, cx, cy, 6);
    canvas_draw_line(canvas, cx - 3, cy, cx + 3, cy);
    canvas_draw_line(canvas, cx, cy - 3, cx, cy + 3);

    // Up
    if(app->dir_up) {
        canvas_draw_line(canvas, cx - 3, cy - 9, cx, cy - 13);
        canvas_draw_line(canvas, cx + 3, cy - 9, cx, cy - 13);
        canvas_draw_line(canvas, cx - 3, cy - 9, cx + 3, cy - 9);
    } else {
        canvas_draw_line(canvas, cx, cy - 11, cx - 2, cy - 9);
        canvas_draw_line(canvas, cx, cy - 11, cx + 2, cy - 9);
    }
    // Down
    if(app->dir_down) {
        canvas_draw_line(canvas, cx - 3, cy + 9, cx, cy + 13);
        canvas_draw_line(canvas, cx + 3, cy + 9, cx, cy + 13);
        canvas_draw_line(canvas, cx - 3, cy + 9, cx + 3, cy + 9);
    } else {
        canvas_draw_line(canvas, cx, cy + 11, cx - 2, cy + 9);
        canvas_draw_line(canvas, cx, cy + 11, cx + 2, cy + 9);
    }
    // Left
    if(app->dir_left) {
        canvas_draw_line(canvas, cx - 9, cy - 3, cx - 13, cy);
        canvas_draw_line(canvas, cx - 9, cy + 3, cx - 13, cy);
        canvas_draw_line(canvas, cx - 9, cy - 3, cx - 9, cy + 3);
    } else {
        canvas_draw_line(canvas, cx - 11, cy, cx - 9, cy - 2);
        canvas_draw_line(canvas, cx - 11, cy, cx - 9, cy + 2);
    }
    // Right
    if(app->dir_right) {
        canvas_draw_line(canvas, cx + 9, cy - 3, cx + 13, cy);
        canvas_draw_line(canvas, cx + 9, cy + 3, cx + 13, cy);
        canvas_draw_line(canvas, cx + 9, cy - 3, cx + 9, cy + 3);
    } else {
        canvas_draw_line(canvas, cx + 11, cy, cx + 9, cy - 2);
        canvas_draw_line(canvas, cx + 11, cy, cx + 9, cy + 2);
    }
}

// Pixel art bold ƒ glyph (5 wide x 9 tall, 2px stroke)
static void draw_florin(Canvas* canvas, uint8_t x, uint8_t y) {
    // Top hook: .###
    canvas_draw_dot(canvas, x + 2, y + 0);
    canvas_draw_dot(canvas, x + 3, y + 0);
    canvas_draw_dot(canvas, x + 4, y + 0);
    // ##
    canvas_draw_dot(canvas, x + 1, y + 1);
    canvas_draw_dot(canvas, x + 2, y + 1);
    // ## (stem)
    canvas_draw_dot(canvas, x + 1, y + 2);
    canvas_draw_dot(canvas, x + 0, y + 2);
    // #### (crossbar)
    canvas_draw_dot(canvas, x + 0, y + 3);
    canvas_draw_dot(canvas, x + 1, y + 3);
    canvas_draw_dot(canvas, x + 2, y + 3);
    canvas_draw_dot(canvas, x + 3, y + 3);
    // ##
    canvas_draw_dot(canvas, x + 0, y + 4);
    canvas_draw_dot(canvas, x + 1, y + 4);
    // ##
    canvas_draw_dot(canvas, x + 0, y + 5);
    canvas_draw_dot(canvas, x + 1, y + 5);
    // ##
    canvas_draw_dot(canvas, x + 0, y + 6);
    canvas_draw_dot(canvas, x + 1, y + 6);
    // Descender hook
    canvas_draw_dot(canvas, x - 1, y + 7);
    canvas_draw_dot(canvas, x + 0, y + 7);
    canvas_draw_dot(canvas, x - 2, y + 8);
    canvas_draw_dot(canvas, x - 1, y + 8);
}

// ---------------------------------------------------------------------------
// Draw callback — per-state screens
// ---------------------------------------------------------------------------
static void draw_mode_select(Canvas* canvas, App* app) {
    draw_header(canvas, "Click Recorder", app);

    canvas_set_font(canvas, FontSecondary);
    const char* items[] = {"Record Clicks", "Shuffle Clicks", "Spam Click", "Computer Mouse"};

    for(uint8_t i = 0; i < ModeCount; i++) {
        uint8_t y = 15 + i * 9;
        if(i == app->menu_selection) {
            canvas_draw_rbox(canvas, 14, y - 1, 100, 10, 3);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, items[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, items[i]);
        }
    }

    draw_hint_bar(canvas, "<> USB/BLE", NULL, "OK");
}

static void draw_template_select(Canvas* canvas, App* app) {
    AppMode mode = mode_from_selection(app->menu_selection);
    const char* title = (mode == ModeShuffler)  ? "Shuffle Clicks" :
                        (mode == ModeSpamClick) ? "Spam Click" :
                                                  "Record Clicks";
    draw_header(canvas, title, app);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", app->selected_duration);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, buf);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "seconds");

    if(app->selected_duration < DURATION_MAX) {
        canvas_draw_line(canvas, 64, 14, 60, 17);
        canvas_draw_line(canvas, 64, 14, 68, 17);
    }
    if(app->selected_duration > DURATION_MIN) {
        canvas_draw_line(canvas, 64, 50, 60, 47);
        canvas_draw_line(canvas, 64, 50, 68, 47);
    }

    draw_hint_bar(canvas, "Back", "OK", NULL);
}

static void draw_countdown(Canvas* canvas, App* app) {
    draw_header(canvas, "Get Ready", app);

    char buf[4];
    snprintf(buf, sizeof(buf), "%d", app->countdown_value);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, buf);

    canvas_set_font(canvas, FontSecondary);
    char dur_buf[16];
    snprintf(dur_buf, sizeof(dur_buf), "%ds recording", app->selected_duration);
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, dur_buf);
}

static void draw_recording(Canvas* canvas, App* app) {
    uint32_t duration_ms = (uint32_t)app->recording.duration_sec * 1000;

    draw_header(canvas, "Recording", app);

    if((app->elapsed_ms / 500) % 2 == 0) {
        canvas_draw_disc(canvas, 7, 6, 3);
    }

    char buf[32];
    canvas_set_font(canvas, FontPrimary);
    snprintf(
        buf,
        sizeof(buf),
        "%lu.%01lu / %ds",
        (unsigned long)(app->elapsed_ms / 1000),
        (unsigned long)((app->elapsed_ms % 1000) / 100),
        app->recording.duration_sec);
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, buf);

    draw_progress_bar(canvas, 28, app->elapsed_ms, duration_ms);
    draw_keyframe_ticks(canvas, 28, app, duration_ms);

    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "Clicks: %d", app->recording.num_keyframes);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, buf);

    if(app->recording.num_keyframes >= MAX_KEYFRAMES) {
        draw_hint_bar(canvas, NULL, "MAX", NULL);
    } else {
        draw_hint_bar(canvas, "Back=R", "OK=L click", NULL);
    }
}

static void draw_tune_track(Canvas* canvas, App* app) {
    draw_header(canvas, "Tune Track", app);

    char buf[32];
    canvas_set_font(canvas, FontPrimary);
    snprintf(buf, sizeof(buf), "< %ds >", app->tuned_duration_sec);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, buf);

    if(app->tuned_duration_sec != app->original_duration_sec) {
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "(was %ds)", app->original_duration_sec);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, buf);
    }

    uint32_t orig_ms = (uint32_t)app->original_duration_sec * 1000;
    uint32_t tuned_ms = (uint32_t)app->tuned_duration_sec * 1000;
    draw_progress_bar(canvas, 34, tuned_ms, orig_ms);
    draw_keyframe_ticks(canvas, 34, app, orig_ms);

    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%d clicks", app->recording.num_keyframes);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, buf);

    draw_hint_bar(canvas, "Back", "OK", "<> Tune");
}

static void draw_shuffler_click_count(Canvas* canvas, App* app) {
    draw_header(canvas, "Shuffle Clicks", app);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", app->shuffler_click_count);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, buf);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "clicks");

    if(app->shuffler_click_count < SHUFFLER_CLICKS_MAX) {
        canvas_draw_line(canvas, 64, 14, 60, 17);
        canvas_draw_line(canvas, 64, 14, 68, 17);
    }
    if(app->shuffler_click_count > SHUFFLER_CLICKS_MIN) {
        canvas_draw_line(canvas, 64, 50, 60, 47);
        canvas_draw_line(canvas, 64, 50, 68, 47);
    }

    draw_hint_bar(canvas, "Back", "OK", NULL);
}

static void draw_spam_click_config(Canvas* canvas, App* app) {
    draw_header(canvas, "Spam Click", app);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", app->spam_click_count);
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, buf);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, "clicks");

    // Button type selector
    const char* btn_names[] = {"Left", "Middle", "Right"};
    canvas_set_font(canvas, FontSecondary);
    char btn_buf[16];
    snprintf(btn_buf, sizeof(btn_buf), "< %s >", btn_names[app->spam_button]);
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignTop, btn_buf);

    draw_hint_bar(canvas, "Back", "OK", "<> Button");
}

static void draw_spam_clicking(Canvas* canvas, App* app) {
    draw_header(canvas, app->spam_paused ? "Spam Paused" : "Spamming", app);

    char buf[32];
    canvas_set_font(canvas, FontPrimary);
    snprintf(buf, sizeof(buf), "%d / %d", app->spam_clicks_done, app->spam_click_count);
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, buf);

    canvas_set_font(canvas, FontSecondary);
    const char* btn_names[] = {"Left", "Middle", "Right"};
    canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, btn_names[app->spam_button]);

    draw_progress_bar(canvas, 38, app->spam_clicks_done, app->spam_click_count);

    draw_hint_bar(canvas, "Back=stop", app->spam_paused ? "OK=resume" : "OK=pause", NULL);
}

static void draw_computer_mouse(Canvas* canvas, App* app) {
    draw_header(canvas, "Computer Mouse", app);

    draw_crosshair(canvas, app, 64, 30);

    // Button indicators above hint bar
    // Left button
    if(app->mouse_left_pressed) {
        canvas_draw_rbox(canvas, 30, 42, 24, 9, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 42, 43, AlignCenter, AlignTop, "L");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, 30, 42, 24, 9, 2);
        canvas_draw_str_aligned(canvas, 42, 43, AlignCenter, AlignTop, "L");
    }

    // Right button
    if(app->mouse_right_pressed) {
        canvas_draw_rbox(canvas, 74, 42, 24, 9, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 86, 43, AlignCenter, AlignTop, "R");
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, 74, 42, 24, 9, 2);
        canvas_draw_str_aligned(canvas, 86, 43, AlignCenter, AlignTop, "R");
    }

    draw_hint_bar(canvas, "Bk=R", "OK=L", NULL);
}

static void draw_about(Canvas* canvas, App* app) {
    UNUSED(app);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Created by");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "0x78f1935");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, "aka");

    // "unde" + pixel art ƒ + "ined" — centered at x=64
    // FontPrimary chars are ~6px wide. "unde"=24px, ƒ=~6px, "ined"=24px => total ~54px
    // Start at 64 - 27 = 37
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 37, 55, "unde");
    draw_florin(canvas, 61, 46);
    canvas_draw_str(canvas, 67, 55, "ined");
}

static void draw_positioning(Canvas* canvas, App* app) {
    draw_header(canvas, "Position Cursor", app);

    canvas_set_font(canvas, FontSecondary);
    char buf[32];
    AppMode mode = mode_from_selection(app->menu_selection);
    if(mode == ModeSpamClick) {
        const char* btn_names[] = {"L", "M", "R"};
        snprintf(
            buf,
            sizeof(buf),
            "%d %s clicks | %ds",
            app->spam_click_count,
            btn_names[app->spam_button],
            app->recording.duration_sec);
    } else {
        snprintf(
            buf,
            sizeof(buf),
            "%d clicks | %ds",
            app->recording.num_keyframes,
            app->recording.duration_sec);
    }
    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignTop, buf);

    draw_crosshair(canvas, app, 64, 34);

    draw_hint_bar(canvas, "Back", "OK = play", NULL);
}

static void draw_play_mode_select(Canvas* canvas, App* app) {
    draw_header(canvas, "Play Mode", app);

    AppMode mode = mode_from_selection(app->menu_selection);
    const char* label0 = (mode == ModeShuffler)  ? "Shuffle Once" :
                         (mode == ModeSpamClick) ? "Spam Once" :
                                                   "Play Once";
    const char* label1 = (mode == ModeShuffler)  ? "Shuffle Forever" :
                         (mode == ModeSpamClick) ? "Spam Forever" :
                                                   "Loop Forever";

    canvas_set_font(canvas, FontPrimary);

    uint8_t y0 = 20;
    if(app->play_mode_selection == 0) {
        canvas_draw_rbox(canvas, 14, y0 - 2, 100, 15, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, y0 + 1, AlignCenter, AlignTop, label0);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, 14, y0 - 2, 100, 15, 3);
        canvas_draw_str_aligned(canvas, 64, y0 + 1, AlignCenter, AlignTop, label0);
    }

    uint8_t y1 = 38;
    if(app->play_mode_selection == 1) {
        canvas_draw_rbox(canvas, 14, y1 - 2, 100, 15, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, y1 + 1, AlignCenter, AlignTop, label1);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_rframe(canvas, 14, y1 - 2, 100, 15, 3);
        canvas_draw_str_aligned(canvas, 64, y1 + 1, AlignCenter, AlignTop, label1);
    }

    draw_hint_bar(canvas, NULL, "OK", NULL);
}

static void draw_playback(Canvas* canvas, App* app) {
    uint32_t duration_ms = (uint32_t)app->recording.duration_sec * 1000;

    draw_header(canvas, "Playing", app);

    canvas_set_font(canvas, FontPrimary);
    char buf[32];
    snprintf(
        buf,
        sizeof(buf),
        "%lu.%01lu / %ds",
        (unsigned long)(app->playback_ms / 1000),
        (unsigned long)((app->playback_ms % 1000) / 100),
        app->recording.duration_sec);
    canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignTop, buf);

    draw_progress_bar(canvas, 28, app->playback_ms, duration_ms);
    draw_keyframe_ticks(canvas, 28, app, duration_ms);

    if(duration_ms > 0) {
        uint32_t marker_x = 6 + (app->playback_ms * 116) / duration_ms;
        if(marker_x > 122) marker_x = 122;
        canvas_draw_line(canvas, marker_x - 2, 25, marker_x + 2, 25);
        canvas_draw_line(canvas, marker_x - 1, 26, marker_x + 1, 26);
        canvas_draw_dot(canvas, marker_x, 27);
    }

    canvas_set_font(canvas, FontSecondary);
    snprintf(
        buf, sizeof(buf), "Click %d/%d", app->playback_keyframe_idx, app->recording.num_keyframes);
    canvas_draw_str_aligned(canvas, 34, 38, AlignCenter, AlignTop, buf);

    const char* loop_str = (app->loop_mode == LoopOnce) ? "Once" : "Loop";
    canvas_draw_rframe(canvas, 78, 37, 36, 11, 3);
    canvas_draw_str_aligned(canvas, 96, 39, AlignCenter, AlignTop, loop_str);

    draw_hint_bar(canvas, "<> mode", "OK = pause", NULL);
}

static void draw_paused(Canvas* canvas, App* app) {
    draw_header(canvas, "Paused", app);

    canvas_set_font(canvas, FontSecondary);
    const char* items[] = {"Continue", "Reset", "Quit"};
    for(uint8_t i = 0; i < PauseMenuCount; i++) {
        uint8_t y = 18 + i * 13;
        if(i == (uint8_t)app->pause_selection) {
            canvas_draw_rbox(canvas, 14, y - 1, 100, 12, 3);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, y + 1, AlignCenter, AlignTop, items[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str_aligned(canvas, 64, y + 1, AlignCenter, AlignTop, items[i]);
        }
    }
}

static void draw_quit_confirm(Canvas* canvas, App* app) {
    draw_header(canvas, "Menu", app);

    canvas_set_font(canvas, FontSecondary);
    const char* items[] = {"Resume", "Menu", "About", "Quit"};
    for(uint8_t i = 0; i < 4; i++) {
        uint8_t y = 15 + i * 10;
        if(i == app->quit_selection) {
            canvas_draw_rbox(canvas, 14, y - 1, 100, 10, 3);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, items[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignTop, items[i]);
        }
    }
}

static void app_draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_assert(app);
    canvas_clear(canvas);

    switch(app->state) {
    case StateModeSelect:
        draw_mode_select(canvas, app);
        break;
    case StateTemplateSelect:
        draw_template_select(canvas, app);
        break;
    case StateShufflerClickCount:
        draw_shuffler_click_count(canvas, app);
        break;
    case StateCountdown:
        draw_countdown(canvas, app);
        break;
    case StateRecording:
        draw_recording(canvas, app);
        break;
    case StateTuneTrack:
        draw_tune_track(canvas, app);
        break;
    case StatePositioning:
        draw_positioning(canvas, app);
        break;
    case StatePlayModeSelect:
        draw_play_mode_select(canvas, app);
        break;
    case StatePlayback:
        draw_playback(canvas, app);
        break;
    case StatePaused:
        draw_paused(canvas, app);
        break;
    case StateSpamClickConfig:
        draw_spam_click_config(canvas, app);
        break;
    case StateSpamClicking:
        draw_spam_clicking(canvas, app);
        break;
    case StateComputerMouse:
        draw_computer_mouse(canvas, app);
        break;
    case StateQuitConfirm:
        draw_quit_confirm(canvas, app);
        break;
    case StateAbout:
        draw_about(canvas, app);
        break;
    }
}

// ---------------------------------------------------------------------------
// Input callback
// ---------------------------------------------------------------------------
static void app_input_callback(InputEvent* input_event, void* ctx) {
    App* app = ctx;
    furi_assert(app);
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

// ---------------------------------------------------------------------------
// Input handlers per state
// ---------------------------------------------------------------------------
static void handle_mode_select(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;

    switch(ev->key) {
    case InputKeyUp:
        if(app->menu_selection > 0)
            app->menu_selection--;
        else
            app->menu_selection = ModeCount - 1;
        break;
    case InputKeyDown:
        if(app->menu_selection < ModeCount - 1)
            app->menu_selection++;
        else
            app->menu_selection = 0;
        break;
    case InputKeyLeft:
    case InputKeyRight:
        hid_deinit(app);
        app->transport = (app->transport == TransportUsb) ? TransportBle : TransportUsb;
        hid_init(app);
        break;
    case InputKeyOk: {
        AppMode mode = mode_from_selection(app->menu_selection);
        switch(mode) {
        case ModeRecorder:
        case ModeShuffler:
            enter_template_select(app);
            break;
        case ModeSpamClick:
            enter_template_select(app);
            break;
        case ModeComputerMouse:
            enter_computer_mouse(app);
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void handle_template_select(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    switch(ev->key) {
    case InputKeyUp:
        if(app->selected_duration < DURATION_MAX) app->selected_duration++;
        break;
    case InputKeyDown:
        if(app->selected_duration > DURATION_MIN) app->selected_duration--;
        break;
    case InputKeyOk: {
        AppMode mode = mode_from_selection(app->menu_selection);
        if(mode == ModeShuffler) {
            enter_shuffler_click_count(app);
        } else if(mode == ModeSpamClick) {
            enter_spam_click_config(app);
        } else {
            enter_countdown(app);
        }
        break;
    }
    case InputKeyBack:
        enter_mode_select(app);
        break;
    default:
        break;
    }
}

static void handle_countdown(App* app, InputEvent* ev) {
    if(ev->type == InputTypeShort && ev->key == InputKeyBack) {
        furi_timer_stop(app->timer);
        enter_template_select(app);
    }
}

static void handle_recording(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;

    if(ev->key == InputKeyOk || ev->key == InputKeyBack) {
        if(app->recording.num_keyframes < MAX_KEYFRAMES) {
            ClickType ct = (ev->key == InputKeyOk) ? ClickLeft : ClickRight;
            // If last keyframe has same timestamp, upgrade to ClickBoth
            if(app->recording.num_keyframes > 0 &&
               app->recording.keyframes[app->recording.num_keyframes - 1].timestamp_ms ==
                   app->elapsed_ms) {
                app->recording.keyframes[app->recording.num_keyframes - 1].click_type = ClickBoth;
            } else {
                app->recording.keyframes[app->recording.num_keyframes].timestamp_ms =
                    app->elapsed_ms;
                app->recording.keyframes[app->recording.num_keyframes].click_type = ct;
                app->recording.num_keyframes++;
            }
        }
    }
}

static void handle_tune_track(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    if(ev->key == InputKeyLeft) {
        if(app->tuned_duration_sec > DURATION_MIN) {
            app->tuned_duration_sec--;
            tune_track_recalc(app);
        }
    } else if(ev->key == InputKeyRight) {
        if(app->tuned_duration_sec < DURATION_MAX) {
            app->tuned_duration_sec++;
            tune_track_recalc(app);
        }
    } else if(ev->key == InputKeyOk && ev->type == InputTypeShort) {
        enter_positioning(app);
    } else if(ev->key == InputKeyBack && ev->type == InputTypeShort) {
        enter_template_select(app);
    }
}

static void handle_shuffler_click_count(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    switch(ev->key) {
    case InputKeyUp:
        if(app->shuffler_click_count < SHUFFLER_CLICKS_MAX) app->shuffler_click_count++;
        break;
    case InputKeyDown:
        if(app->shuffler_click_count > SHUFFLER_CLICKS_MIN) app->shuffler_click_count--;
        break;
    case InputKeyOk:
        app->recording.duration_sec = app->selected_duration;
        app->recording.num_keyframes = app->shuffler_click_count;
        shuffle_generate_keyframes(app);
        enter_positioning(app);
        break;
    case InputKeyBack:
        enter_template_select(app);
        break;
    default:
        break;
    }
}

static void handle_spam_click_config(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    switch(ev->key) {
    case InputKeyUp:
        if(app->spam_click_count >= SPAM_CLICKS_MAX)
            app->spam_click_count = SPAM_CLICKS_MIN;
        else
            app->spam_click_count++;
        break;
    case InputKeyDown:
        if(app->spam_click_count <= SPAM_CLICKS_MIN)
            app->spam_click_count = SPAM_CLICKS_MAX;
        else
            app->spam_click_count--;
        break;
    case InputKeyLeft:
        if(app->spam_button == 0)
            app->spam_button = SpamBtnCount - 1;
        else
            app->spam_button--;
        break;
    case InputKeyRight:
        app->spam_button = (app->spam_button + 1) % SpamBtnCount;
        break;
    case InputKeyOk:
        app->recording.duration_sec = app->selected_duration;
        app->recording.num_keyframes = app->spam_click_count;
        enter_positioning(app);
        break;
    case InputKeyBack:
        enter_template_select(app);
        break;
    default:
        break;
    }
}

static void handle_about(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;
    if(ev->key == InputKeyBack || ev->key == InputKeyOk) {
        app->state = StateQuitConfirm;
    }
}

static void handle_mouse_arrows(App* app, InputEvent* ev) {
    if(ev->key == InputKeyUp) {
        if(ev->type == InputTypePress || ev->type == InputTypeShort) {
            app->dir_up = true;
            hid_mouse_move(app, 0, -MOUSE_MOVE_SHORT);
        } else if(ev->type == InputTypeRepeat) {
            hid_mouse_move(app, 0, -MOUSE_MOVE_LONG);
        } else if(ev->type == InputTypeRelease) {
            app->dir_up = false;
        }
    } else if(ev->key == InputKeyDown) {
        if(ev->type == InputTypePress || ev->type == InputTypeShort) {
            app->dir_down = true;
            hid_mouse_move(app, 0, MOUSE_MOVE_SHORT);
        } else if(ev->type == InputTypeRepeat) {
            hid_mouse_move(app, 0, MOUSE_MOVE_LONG);
        } else if(ev->type == InputTypeRelease) {
            app->dir_down = false;
        }
    } else if(ev->key == InputKeyLeft) {
        if(ev->type == InputTypePress || ev->type == InputTypeShort) {
            app->dir_left = true;
            hid_mouse_move(app, -MOUSE_MOVE_SHORT, 0);
        } else if(ev->type == InputTypeRepeat) {
            hid_mouse_move(app, -MOUSE_MOVE_LONG, 0);
        } else if(ev->type == InputTypeRelease) {
            app->dir_left = false;
        }
    } else if(ev->key == InputKeyRight) {
        if(ev->type == InputTypePress || ev->type == InputTypeShort) {
            app->dir_right = true;
            hid_mouse_move(app, MOUSE_MOVE_SHORT, 0);
        } else if(ev->type == InputTypeRepeat) {
            hid_mouse_move(app, MOUSE_MOVE_LONG, 0);
        } else if(ev->type == InputTypeRelease) {
            app->dir_right = false;
        }
    }
}

static void handle_positioning(App* app, InputEvent* ev) {
    AppMode mode = mode_from_selection(app->menu_selection);

    if(ev->key == InputKeyOk && ev->type == InputTypeShort) {
        if(mode == ModeSpamClick || app->recording.num_keyframes > 0) {
            enter_play_mode_select(app);
        }
        return;
    }

    if(ev->key == InputKeyBack && ev->type == InputTypeShort) {
        switch(mode) {
        case ModeShuffler:
            enter_shuffler_click_count(app);
            break;
        case ModeSpamClick:
            enter_spam_click_config(app);
            break;
        default:
            enter_tune_track(app);
            break;
        }
        return;
    }

    handle_mouse_arrows(app, ev);
}

static void handle_spam_clicking(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;

    if(ev->key == InputKeyOk) {
        if(app->spam_paused) {
            app->spam_paused = false;
            app->playback_start_tick =
                ticks_now() - ((uint32_t)app->spam_clicks_done * SPAM_INTERVAL_MS *
                               furi_kernel_get_tick_frequency() / 1000);
            furi_timer_start(app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
        } else {
            app->spam_paused = true;
            furi_timer_stop(app->timer);
            if(app->playback_click_pending) {
                hid_mouse_release_btn(app, spam_btn_hid(app->spam_button));
                app->playback_click_pending = false;
            }
        }
    } else if(ev->key == InputKeyBack) {
        furi_timer_stop(app->timer);
        hid_mouse_release_all(app);
        app->playback_click_pending = false;
        app->spam_paused = false;
        enter_positioning(app);
    }
}

static void handle_computer_mouse(App* app, InputEvent* ev) {
    // OK = left click (press/release)
    if(ev->key == InputKeyOk) {
        if(ev->type == InputTypePress || ev->type == InputTypeShort) {
            hid_mouse_press_btn(app, HID_MOUSE_BTN_LEFT);
            app->mouse_left_pressed = true;
        } else if(ev->type == InputTypeRelease) {
            hid_mouse_release_btn(app, HID_MOUSE_BTN_LEFT);
            app->mouse_left_pressed = false;
        }
        return;
    }

    // Back = right click (press/release) — short only, long is intercepted
    if(ev->key == InputKeyBack) {
        if(ev->type == InputTypePress || ev->type == InputTypeShort) {
            hid_mouse_press_btn(app, HID_MOUSE_BTN_RIGHT);
            app->mouse_right_pressed = true;
        } else if(ev->type == InputTypeRelease) {
            hid_mouse_release_btn(app, HID_MOUSE_BTN_RIGHT);
            app->mouse_right_pressed = false;
        }
        return;
    }

    handle_mouse_arrows(app, ev);
}

static void handle_playback(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;

    switch(ev->key) {
    case InputKeyOk:
        enter_paused(app);
        break;
    case InputKeyLeft:
    case InputKeyRight:
        app->loop_mode = (app->loop_mode == LoopOnce) ? LoopForever : LoopOnce;
        break;
    case InputKeyBack:
        enter_paused(app);
        break;
    default:
        break;
    }
}

static void handle_paused(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;

    switch(ev->key) {
    case InputKeyUp:
        if(app->pause_selection > 0) app->pause_selection--;
        break;
    case InputKeyDown:
        if(app->pause_selection < PauseMenuCount - 1) app->pause_selection++;
        break;
    case InputKeyBack:
        app->state = StatePlayback;
        app->playback_start_tick =
            ticks_now() - (app->playback_ms * furi_kernel_get_tick_frequency() / 1000);
        furi_timer_start(app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
        break;
    case InputKeyOk:
        switch(app->pause_selection) {
        case PauseMenuContinue:
            app->state = StatePlayback;
            app->playback_start_tick =
                ticks_now() - (app->playback_ms * furi_kernel_get_tick_frequency() / 1000);
            furi_timer_start(app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
            break;
        case PauseMenuReset:
            app->recording.num_keyframes = 0;
            app->loop_mode = LoopOnce;
            enter_mode_select(app);
            break;
        case PauseMenuQuit:
            app->running = false;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

static void handle_play_mode_select(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;

    switch(ev->key) {
    case InputKeyUp:
    case InputKeyDown:
        app->play_mode_selection = (app->play_mode_selection == 0) ? 1 : 0;
        break;
    case InputKeyOk: {
        app->loop_mode = (app->play_mode_selection == 0) ? LoopOnce : LoopForever;
        AppMode mode = mode_from_selection(app->menu_selection);
        if(mode == ModeSpamClick) {
            enter_spam_clicking(app);
        } else {
            enter_playback(app);
        }
        break;
    }
    case InputKeyBack:
        enter_positioning(app);
        break;
    default:
        break;
    }
}

static void handle_quit_confirm(App* app, InputEvent* ev) {
    if(ev->type != InputTypeShort) return;

    switch(ev->key) {
    case InputKeyUp:
        if(app->quit_selection > 0) app->quit_selection--;
        break;
    case InputKeyDown:
        if(app->quit_selection < 3) app->quit_selection++;
        break;
    case InputKeyOk:
        if(app->quit_selection == 0) {
            // Resume
            app->state = app->state_before_quit;
            if(app->state == StatePlayback) {
                app->playback_start_tick =
                    ticks_now() - (app->playback_ms * furi_kernel_get_tick_frequency() / 1000);
                furi_timer_start(
                    app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
            } else if(app->state == StateRecording) {
                app->record_start_tick =
                    ticks_now() - (app->elapsed_ms * furi_kernel_get_tick_frequency() / 1000);
                furi_timer_start(
                    app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
            } else if(app->state == StateCountdown) {
                furi_timer_start(app->timer, furi_kernel_get_tick_frequency());
            } else if(app->state == StateSpamClicking) {
                furi_timer_start(
                    app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
            }
        } else if(app->quit_selection == 1) {
            // Menu
            enter_mode_select(app);
        } else if(app->quit_selection == 2) {
            // About
            app->state = StateAbout;
        } else {
            // Quit
            app->running = false;
        }
        break;
    case InputKeyBack:
        app->state = app->state_before_quit;
        if(app->state == StatePlayback) {
            app->playback_start_tick =
                ticks_now() - (app->playback_ms * furi_kernel_get_tick_frequency() / 1000);
            furi_timer_start(app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
        } else if(app->state == StateRecording) {
            app->record_start_tick =
                ticks_now() - (app->elapsed_ms * furi_kernel_get_tick_frequency() / 1000);
            furi_timer_start(app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
        } else if(app->state == StateCountdown) {
            furi_timer_start(app->timer, furi_kernel_get_tick_frequency());
        } else if(app->state == StateSpamClicking) {
            furi_timer_start(app->timer, furi_kernel_get_tick_frequency() * TIMER_TICK_MS / 1000);
        }
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int32_t mouse_click_recorder_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->running = true;
    app->state = StateModeSelect;
    app->menu_selection = ModeRecorder;
    app->transport = TransportUsb;
    app->loop_mode = LoopOnce;
    app->selected_duration = DURATION_DEFAULT;
    app->shuffler_click_count = SHUFFLER_CLICKS_DEFAULT;
    app->spam_click_count = SPAM_CLICKS_DEFAULT;
    app->spam_button = SpamBtnLeft;

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->timer = furi_timer_alloc(app_timer_callback, FuriTimerTypePeriodic, app);
    app->bt = furi_record_open(RECORD_BT);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    hid_init(app);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, app_draw_callback, app);
    view_port_input_callback_set(app->view_port, app_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // -----------------------------------------------------------------------
    // Main event loop
    // -----------------------------------------------------------------------
    InputEvent event;
    while(app->running) {
        FuriStatus status = furi_message_queue_get(app->event_queue, &event, FuriWaitForever);
        if(status != FuriStatusOk) continue;

        // Long-press Back handling
        if(event.type == InputTypeLong && event.key == InputKeyBack) {
            if(app->state == StateQuitConfirm || app->state == StateAbout) {
                // Already in menu — do nothing
            } else if(app->state == StateModeSelect) {
                break; // Exit app
            } else if(app->state == StateComputerMouse) {
                // Release any held mouse buttons first
                hid_mouse_release_all(app);
                app->mouse_left_pressed = false;
                app->mouse_right_pressed = false;
                enter_quit_confirm(app);
                view_port_update(app->view_port);
                continue;
            } else if(app->state == StateSpamClicking) {
                furi_timer_stop(app->timer);
                hid_mouse_release_all(app);
                app->playback_click_pending = false;
                enter_quit_confirm(app);
                view_port_update(app->view_port);
                continue;
            } else {
                enter_quit_confirm(app);
                view_port_update(app->view_port);
                continue;
            }
        }

        // Dispatch to current state handler
        switch(app->state) {
        case StateModeSelect:
            handle_mode_select(app, &event);
            break;
        case StateTemplateSelect:
            handle_template_select(app, &event);
            break;
        case StateShufflerClickCount:
            handle_shuffler_click_count(app, &event);
            break;
        case StateCountdown:
            handle_countdown(app, &event);
            break;
        case StateRecording:
            handle_recording(app, &event);
            break;
        case StateTuneTrack:
            handle_tune_track(app, &event);
            break;
        case StatePositioning:
            handle_positioning(app, &event);
            break;
        case StatePlayModeSelect:
            handle_play_mode_select(app, &event);
            break;
        case StatePlayback:
            handle_playback(app, &event);
            break;
        case StatePaused:
            handle_paused(app, &event);
            break;
        case StateSpamClickConfig:
            handle_spam_click_config(app, &event);
            break;
        case StateSpamClicking:
            handle_spam_clicking(app, &event);
            break;
        case StateComputerMouse:
            handle_computer_mouse(app, &event);
            break;
        case StateQuitConfirm:
            handle_quit_confirm(app, &event);
            break;
        case StateAbout:
            handle_about(app, &event);
            break;
        }

        view_port_update(app->view_port);
    }

    // -----------------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------------
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    hid_mouse_release_all(app);
    hid_deinit(app);

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);

    furi_record_close(RECORD_BT);
    furi_record_close(RECORD_NOTIFICATION);

    furi_message_queue_free(app->event_queue);
    free(app);

    return 0;
}
