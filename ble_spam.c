#include "ble_spam.h"
#include <gui/gui.h>
#include <furi_hal_bt.h>
#include <extra_beacon.h>
#include <gui/elements.h>

#include "protocols/_protocols.h"

// MagicBand+ Lights — haw8411 / WillyJL

// ─── Attack table ─────────────────────────────────────────────────────────────
// Indices 0-4: fully configurable (hold OK or enter via menu).
// Index 5+:    named presets.

#define MB_ADV(TITLE, TEXT, CAT, IDX, COL5, VIBE)                                           \
    {                                                                                       \
        .title = TITLE, .text = TEXT, .protocol = &protocol_magicband,                      \
        .payload =                                                                          \
        {.random_mac = true,                                                                \
         .cfg.magicband = {                                                                 \
             .category = CAT, .index = IDX, .color5 = COL5, .vibe = VIBE, .timing = 0x8F} } \
    }

static Attack attacks[] = {
    // ── Configurable ──────────────────────────────────────────────────────────────
    {
        // [0] Single Color – E9-05, hold OK to set color/vibe/mask/timing
        .title = "Single Color",
        .text = "Hold OK: color/vibe/mask/timing",
        .protocol = &protocol_magicband,
        .payload =
            {.random_mac = true,
             .cfg.magicband =
                 {
                     .category = MB_Cat_E905_Single,
                     .color5 = 0,
                     .vibe = 0xB,
                     .mask = 0,
                     .timing = 0x8F,
                 }},
    },
    {
        // [1] Dual Color – E9-06, hold OK to set outer/inner
        .title = "Dual Color",
        .text = "Hold OK: outer+inner color",
        .protocol = &protocol_magicband,
        .payload =
            {.random_mac = true,
             .cfg.magicband =
                 {
                     .category = MB_Cat_E906_Dual,
                     .color5_outer = 21,
                     .color5_inner = 0,
                     .vibe = 0xB,
                 }},
    },
    {
        // [2] RGB Custom – E9-08 6-bit, hold OK to set R/G/B
        .title = "RGB Color",
        .text = "Hold OK: R/G/B (0-63 each)",
        .protocol = &protocol_magicband,
        .payload =
            {.random_mac = true,
             .cfg.magicband =
                 {
                     .category = MB_Cat_E908_RGB,
                     .r6 = 63,
                     .g6 = 0,
                     .b6 = 0,
                     .vibe = 0,
                 }},
    },
    {
        // [3] 5-LED Custom – E9-09, hold OK for per-LED colors
        .title = "5-LED Color",
        .text = "Hold OK: per-LED palette",
        .protocol = &protocol_magicband,
        .payload =
            {.random_mac = true,
             .cfg.magicband =
                 {
                     .category = MB_Cat_E909_5LED,
                     .led5 = {25, 22, 22, 22, 22},
                     .vibe = 0,
                 }},
    },
    {
        // [4] Custom Hex – MB_Cat_Custom, filled from Menu > Custom Command
        .title = "Custom Hex",
        .text = "Menu > Custom Command to set",
        .protocol = &protocol_magicband,
        .payload =
            {.random_mac = true,
             .cfg.magicband =
                 {
                     .category = MB_Cat_Custom,
                     .custom = {0xE1, 0x00, 0xE9, 0x05, 0x00, 0x8F, 0x0E, 0x00, 0xB0},
                     .custom_len = 9,
                 }},
    },
    // ── CC ping codes ─────────────────────────────────────────────────────────────
    MB_ADV("CC Ping", "cc03 000000 – park ping", MB_Cat_CC, 0, 0, 0),
    MB_ADV("CC Fast Mode", "cc03 000100 – keeps band awake", MB_Cat_CC, 1, 0, 0),
    MB_ADV("CC RRC", "cc03 132000 – Rock'n'Roller?", MB_Cat_CC, 2, 0, 0),
    // ── E9-0C animations ─────────────────────────────────────────────────────────
    MB_ADV("Blink White", "E9-0C flash", MB_Cat_E90C_Anim, 0, 0, 0),
    MB_ADV("Orange Blink", "E9-0C orange flash", MB_Cat_E90C_Anim, 1, 0, 0),
    MB_ADV("Color Cycle", "E9-0C 5-palette cycle", MB_Cat_E90C_Anim, 2, 0, 0),
    MB_ADV("Rainbow", "E9-0C taste the rainbow", MB_Cat_E90C_Anim, 3, 0, 0),
    // ── Cross fade ───────────────────────────────────────────────────────────────
    MB_ADV("Cross Fade #1", "E9-11 center vs ring", MB_Cat_E911_Crossfade, 0, 0, 0),
    MB_ADV("Cross Fade #2", "E9-11 center vs ring", MB_Cat_E911_Crossfade, 1, 0, 0),
    MB_ADV("Cross Fade #3", "E9-11 center vs ring", MB_Cat_E911_Crossfade, 2, 0, 0),
    MB_ADV("Cross Fade #4", "E9-11 center vs ring", MB_Cat_E911_Crossfade, 3, 0, 0),
    MB_ADV("Cross Fade #5", "E9-11 center vs ring", MB_Cat_E911_Crossfade, 4, 0, 0),
    MB_ADV("Cross Fade #6", "E9-11 center vs ring", MB_Cat_E911_Crossfade, 5, 0, 0),
    MB_ADV("Cross Fade #7", "E9-11 center vs ring", MB_Cat_E911_Crossfade, 6, 0, 0),
    // ── Circle + vibe ────────────────────────────────────────────────────────────
    MB_ADV("Circle+Vibe #1", "E9-12 haptic circle", MB_Cat_E912_CircVibe, 0, 0, 0),
    MB_ADV("Circle+Vibe #2", "E9-12 haptic circle", MB_Cat_E912_CircVibe, 1, 0, 0),
    MB_ADV("Circle+Vibe #3", "E9-12 haptic circle", MB_Cat_E912_CircVibe, 2, 0, 0),
    // ── Other effects ────────────────────────────────────────────────────────────
    MB_ADV("Circle", "E9-0B circle anim", MB_Cat_E90B_Circle, 0, 0, 0),
    MB_ADV("Alternating", "E9-10 alt colors", MB_Cat_E910_Alt, 0, 0, 0),
    MB_ADV("E9-0E #1", "E9-0E pattern 01", MB_Cat_E90E, 0, 0, 0),
    MB_ADV("E9-0E #2", "E9-0E pattern 02", MB_Cat_E90E, 1, 0, 0),
    MB_ADV("E9-0E #3", "E9-0E pattern 11", MB_Cat_E90E, 2, 0, 0),
    MB_ADV("E9-0E #4", "E9-0E pattern 15", MB_Cat_E90E, 3, 0, 0),
    MB_ADV("E9-0E #5", "E9-0E pattern 83", MB_Cat_E90E, 4, 0, 0),
    MB_ADV("E9-0F #1", "E9-0F pattern 11", MB_Cat_E90F, 0, 0, 0),
    MB_ADV("E9-0F #2", "E9-0F pattern 2A", MB_Cat_E90F, 1, 0, 0),
    MB_ADV("Anim 13 #1", "E9-13 animation", MB_Cat_E913_Anim, 0, 0, 0),
    MB_ADV("Anim 13 #2", "E9-13 animation", MB_Cat_E913_Anim, 1, 0, 0),
    MB_ADV("Anim 14 #1", "E9-14 animation", MB_Cat_E914_Anim, 0, 0, 0),
    MB_ADV("Anim 14 #2", "E9-14 animation", MB_Cat_E914_Anim, 1, 0, 0),
    MB_ADV("Anim 14 #3", "E9-14 animation", MB_Cat_E914_Anim, 2, 0, 0),
    MB_ADV("Unknown 07 #1", "E9-07 not yet decoded", MB_Cat_E907_Unknown, 0, 0, 0),
    MB_ADV("Unknown 07 #2", "E9-07 not yet decoded", MB_Cat_E907_Unknown, 1, 0, 0),
};

#define ATTACKS_COUNT ((int)COUNT_OF(attacks))

// ─── State ────────────────────────────────────────────────────────────────────

typedef struct {
    Ctx ctx; // FIRST field — safe to cast (Ctx* ↔ State*)
    View* main_view;
    bool lock_warning;
    uint8_t lock_count;
    FuriTimer* lock_timer;

    bool advertising;
    uint8_t delay; // used for non-MB attacks (bruteforce etc.)
    GapExtraBeaconConfig config;
    FuriThread* thread;
    bool ignore_bruteforce;

    // Fast mode internal state (ble_spam.c only)
    uint32_t fast_mode_prewarm_left_ms;
    bool fast_mode_warming;
    bool fast_mode_skip;

    // Custom hex saved from byte_input (copied to attacks[CUSTOM_ATTACK_IDX] on next tx)
    // Also used as the byte_input buffer via ctx.byte_store
} State;

static uint16_t delays[] = {20, 50, 100, 200, 500};

// ─── LED notification sequences ──────────────────────────────────────────────

const NotificationSequence solid_message = {
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_do_not_reset,
    &message_delay_10,
    NULL,
};
NotificationMessage blink_message = {
    .type = NotificationMessageTypeLedBlinkStart,
    .data.led_blink = {.color = LightBlue | LightGreen, .on_time = 10, .period = 100},
};
const NotificationSequence blink_sequence = {&blink_message, &message_do_not_reset, NULL};

static void start_blink(State* state) {
    if(!state->ctx.led_indicator) return;
    uint16_t period = delays[state->delay];
    blink_message.data.led_blink.period = (period <= 100) ? period + 30 : period;
    notification_message_block(state->ctx.notification, &blink_sequence);
}
static void stop_blink(State* state) {
    if(!state->ctx.led_indicator) return;
    notification_message_block(state->ctx.notification, &sequence_blink_stop);
}

// ─── BLE helpers ─────────────────────────────────────────────────────────────

static void randomize_mac(State* state) {
    furi_hal_random_fill_buf(state->config.address, sizeof(state->config.address));
}

// One stop→new-MAC→start cycle at 20 ms interval.  No sleep — returns after start().
static void fire_one(State* state, const uint8_t* pkt, uint8_t size) {
    GapExtraBeaconConfig* cfg = &state->config;
    cfg->min_adv_interval_ms = 20;
    cfg->max_adv_interval_ms = 25;
    randomize_mac(state);
    furi_check(furi_hal_bt_extra_beacon_set_config(cfg));
    furi_check(furi_hal_bt_extra_beacon_set_data(pkt, size));
    furi_check(furi_hal_bt_extra_beacon_start());
}

static void beacon_stop_if_active(void) {
    if(furi_hal_bt_extra_beacon_is_active()) furi_check(furi_hal_bt_extra_beacon_stop());
}

// ─── Advertising thread ───────────────────────────────────────────────────────
//
// Fast mode (ctx->fast_mode):
//   1. PRE-WARM PHASE: broadcast CC cc03000100 ("fast mode ping") for N seconds.
//      This transitions the band from its normal very-low-duty BLE scan into
//      continuous-listen mode — reducing response time from ~15 s to <2 s.
//   2. EFFECT PHASE: transmit the actual effect packet, but inject a CC fast-ping
//      every ~10 cycles to maintain the continuous-listen state.
//
// Without fast mode: standard stop/start MAC-cycling loop (original behavior).

static int32_t adv_thread(void* _ctx) {
    State* state = _ctx;
    int8_t idx = state->ctx.preset_index;
    Payload* payload = &attacks[idx].payload;
    const Protocol* protocol = attacks[idx].protocol;

    // If this is the Custom Hex attack, copy the byte_store into the payload now
    if(idx == CUSTOM_ATTACK_IDX) {
        MagicbandCfg* mc = &attacks[CUSTOM_ATTACK_IDX].payload.cfg.magicband;
        mc->custom_len = 16;
        memcpy(mc->custom, state->ctx.byte_store, 16);
    }

    if(!payload->random_mac) randomize_mac(state);
    start_blink(state);
    beacon_stop_if_active();

    // Build fast-mode CC ping (reused for pre-warm and maintenance)
    uint8_t fm_pkt[16];
    uint8_t fm_size = (uint8_t)mb_build_fast_ping(fm_pkt);

    // ── PRE-WARM PHASE ──────────────────────────────────────────────────────
    uint32_t prewarm_ms = (uint32_t)mb_prewarm_secs[state->ctx.fast_mode_prewarm_idx] * 1000;

    if(state->ctx.fast_mode && prewarm_ms > 0) {
        state->fast_mode_warming = true;
        state->fast_mode_skip = false;
        uint32_t start_tick = furi_get_tick();

        while(state->advertising && !state->fast_mode_skip) {
            uint32_t elapsed = furi_get_tick() - start_tick;
            if(elapsed >= prewarm_ms) break;

            state->fast_mode_prewarm_left_ms = prewarm_ms - elapsed;
            // Poke the view to redraw the countdown bar
            with_view_model(state->main_view, State * *m, { (void)m; }, true);

            beacon_stop_if_active();
            fire_one(state, fm_pkt, fm_size);
            furi_thread_flags_wait(true, FuriFlagWaitAny, 20);
        }

        state->fast_mode_warming = false;
        state->fast_mode_prewarm_left_ms = 0;
        with_view_model(state->main_view, State * *m, { (void)m; }, true);
        beacon_stop_if_active();
    }

    if(!state->advertising) {
        stop_blink(state);
        return 0;
    }

    // ── EFFECT PHASE ────────────────────────────────────────────────────────
    uint8_t pkt_size;
    uint8_t* pkt;
    protocol->make_packet(&pkt_size, &pkt, payload);

    // 30-packet initial burst so we hit any currently-open scan window fast
    for(int i = 0; i < 30 && state->advertising; i++) {
        beacon_stop_if_active();
        fire_one(state, pkt, pkt_size);
        // No sleep — hardware overhead alone (~1 ms) fires one adv event
    }

    // Main loop: MAC-cycle the effect packet; inject CC ping every 10 cycles
    // to maintain fast-mode if enabled.
    int pkt_ctr = 0;
    while(state->advertising) {
        beacon_stop_if_active();
        bool send_fm = state->ctx.fast_mode && ((pkt_ctr % 10) < 2);
        fire_one(state, send_fm ? fm_pkt : pkt, send_fm ? fm_size : pkt_size);
        pkt_ctr++;
        furi_thread_flags_wait(true, FuriFlagWaitAny, 20);
    }

    beacon_stop_if_active();
    free(pkt);
    stop_blink(state);
    return 0;
}

static void toggle_adv(State* state) {
    if(state->advertising) {
        state->advertising = false;
        furi_thread_flags_set(furi_thread_get_id(state->thread), true);
        furi_thread_join(state->thread);
    } else {
        state->advertising = true;
        furi_thread_start(state->thread);
    }
}

// Exposed helper for scene files
void ble_spam_stop_adv(Ctx* ctx) {
    State* state = (State*)ctx;
    if(state->advertising) toggle_adv(state);
}

void ble_spam_set_custom_payload(Ctx* ctx, const uint8_t* data, uint8_t len) {
    uint8_t copy = len < sizeof(ctx->byte_store) ? len : (uint8_t)sizeof(ctx->byte_store);
    memcpy(ctx->byte_store, data, copy);
    MagicbandCfg* mc = &attacks[CUSTOM_ATTACK_IDX].payload.cfg.magicband;
    uint8_t mc_copy = copy < sizeof(mc->custom) ? copy : (uint8_t)sizeof(mc->custom);
    memcpy(mc->custom, data, mc_copy);
    mc->custom_len = copy;
    ctx->preset_index = CUSTOM_ATTACK_IDX;
}

// ─── Draw callback (preset browser + pre-warm overlay) ───────────────────────

static void draw_callback(Canvas* canvas, void* _ctx) {
    State* state = *(State**)_ctx;
    int8_t idx = state->ctx.preset_index;
    bool is_attack = (idx >= 0 && idx < ATTACKS_COUNT);
    const Attack* attack = is_attack ? &attacks[idx] : NULL;
    const Payload* payload = attack ? &attack->payload : NULL;
    const Protocol* protocol = attack ? attack->protocol : NULL;
    char str[40];

    // ── Pre-warm overlay ─────────────────────────────────────────────────────
    if(state->fast_mode_warming) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_icon(canvas, 4, 3, &I_ble);
        canvas_draw_str(canvas, 14, 12, "MB+ Fast Mode Warm-up");

        // Progress bar
        uint32_t total_ms = (uint32_t)mb_prewarm_secs[state->ctx.fast_mode_prewarm_idx] * 1000;
        uint32_t left_ms = state->fast_mode_prewarm_left_ms;
        if(total_ms > 0 && left_ms <= total_ms) {
            uint8_t fill = (uint8_t)(120 * (total_ms - left_ms) / total_ms);
            canvas_draw_frame(canvas, 4, 16, 120, 8);
            if(fill > 0) canvas_draw_box(canvas, 4, 16, fill, 8);
        }
        snprintf(str, sizeof(str), "%lu s remaining", (unsigned long)(left_ms / 1000));
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignBottom, str);
        canvas_draw_str_aligned(
            canvas, 64, 44, AlignCenter, AlignBottom, "Broadcasting CC fast ping...");
        elements_button_center(canvas, "Skip");
        return;
    }

    // ── Header (y=12): icon | title | [FM] | delay ───────────────────────────
    canvas_set_font(canvas, FontSecondary);
    const Icon* icon = protocol ? protocol->icon : &I_ble_spam;
    canvas_draw_icon(canvas, 4 - (icon == &I_ble_spam), 3, icon);
    canvas_draw_str(canvas, 14, 12, state->ctx.fast_mode ? "MB+(FM)" : "MB+ Presets");

    // Delay / interval indicator (top-right, above up/down arrows)
    if(is_attack) {
        if(protocol == &protocol_magicband) {
            canvas_draw_str_aligned(canvas, 116, 12, AlignRight, AlignBottom, "20ms");
        } else {
            snprintf(str, sizeof(str), "%ims", delays[state->delay]);
            canvas_draw_str_aligned(canvas, 116, 12, AlignRight, AlignBottom, str);
        }
        canvas_draw_icon(canvas, 119, 6, &I_SmallArrowUp_3x5);
        canvas_draw_icon(canvas, 119, 10, &I_SmallArrowDown_3x5);
    }

    // Thin separator line
    canvas_draw_line(canvas, 0, 13, 128, 13);

    // ── Counter row (y=23) ────────────────────────────────────────────────────
    canvas_set_font(canvas, FontSecondary);
    if(is_attack) {
        snprintf(str, sizeof(str), "%02i/%02i", idx + 1, ATTACKS_COUNT);
        canvas_draw_str(canvas, 4, 23, str);
        // LIVE indicator right-aligned — never overlaps counter
        if(state->advertising) {
            canvas_draw_str_aligned(canvas, 124, 23, AlignRight, AlignBottom, "< LIVE");
        }
    }

    // ── Attack title (y=34, FontPrimary) ─────────────────────────────────────
    if(attack) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 34, attack->title);
    }

    // ── Subtitle (y=46): color name for E9-05/08, else attack text ───────────
    if(attack && payload) {
        canvas_set_font(canvas, FontSecondary);
        bool is_mb = (protocol == &protocol_magicband);
        const MagicbandCfg* mc = is_mb ? &payload->cfg.magicband : NULL;

        if(mc && mc->category == MB_Cat_E905_Single) {
            uint8_t v = mc->vibe;
            if(v)
                snprintf(str, sizeof(str), "%s + %s", mb_color_name(mc->color5), mb_vibe_name(v));
            else
                snprintf(str, sizeof(str), "%s", mb_color_name(mc->color5));
            canvas_draw_str(canvas, 4, 46, str);

        } else if(mc && mc->category == MB_Cat_E906_Dual) {
            snprintf(
                str,
                sizeof(str),
                "%s | %s",
                mb_color_name(mc->color5_outer),
                mb_color_name(mc->color5_inner));
            canvas_draw_str(canvas, 4, 46, str);

        } else if(mc && mc->category == MB_Cat_E908_RGB) {
            snprintf(str, sizeof(str), "R:%u G:%u B:%u", mc->r6, mc->g6, mc->b6);
            canvas_draw_str(canvas, 4, 46, str);

        } else {
            canvas_draw_str(canvas, 4, 46, attack->text);
        }
    }

    // ── Bottom buttons ────────────────────────────────────────────────────────
    if(is_attack) {
        if(idx > 0)
            elements_button_left(canvas, "< Back");
        else
            elements_button_left(canvas, "Menu");
        elements_button_center(canvas, state->advertising ? "Stop" : "Send");
        if(idx < ATTACKS_COUNT - 1) elements_button_right(canvas, "Next >");
    }

    // ── Keyboard lock warning overlay ─────────────────────────────────────────
    if(state->lock_warning) {
        canvas_set_font(canvas, FontSecondary);
        elements_bold_rounded_frame(canvas, 14, 8, 99, 48);
        elements_multiline_text(canvas, 65, 26, "To unlock\npress:");
        canvas_draw_icon(canvas, 65, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 80, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 95, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 16, 13, &I_WarningDolphin_45x42);
        canvas_draw_dot(canvas, 17, 61);
    }
}

// ─── Input callback ───────────────────────────────────────────────────────────

static bool input_callback(InputEvent* input, void* _ctx) {
    View* view = _ctx;
    State* state = *(State**)view_get_model(view);
    bool consumed = false;

    // ── Keyboard lock ─────────────────────────────────────────────────────────
    if(state->ctx.lock_keyboard) {
        consumed = true;
        state->lock_warning = true;
        if(state->lock_count == 0) {
            furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
            furi_timer_start(state->lock_timer, 1000);
        }
        if(input->type == InputTypeShort && input->key == InputKeyBack) state->lock_count++;
        if(state->lock_count >= 3) {
            furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
            furi_timer_start(state->lock_timer, 1);
        }
        view_commit_model(view, consumed);
        return consumed;
    }

    // ── Pre-warm skip ─────────────────────────────────────────────────────────
    if(state->fast_mode_warming) {
        if(input->type == InputTypeShort && input->key == InputKeyOk) {
            state->fast_mode_skip = true;
            consumed = true;
        }
        view_commit_model(view, consumed);
        return consumed;
    }

    // ── Normal input ──────────────────────────────────────────────────────────
    if(input->type == InputTypeShort || input->type == InputTypeLong ||
       input->type == InputTypeRepeat) {
        consumed = true;

        int8_t idx = state->ctx.preset_index;
        bool is_attack = (idx >= 0 && idx < ATTACKS_COUNT);
        Payload* payload = is_attack ? &attacks[idx].payload : NULL;
        bool advertising = state->advertising;

        switch(input->key) {
        case InputKeyOk:
            if(is_attack) {
                if(input->type == InputTypeLong) {
                    // Hold OK → per-attack config
                    if(advertising) toggle_adv(state);
                    state->ctx.attack = &attacks[idx];
                    scene_manager_set_scene_state(state->ctx.scene_manager, SceneConfig, 0);
                    view_commit_model(view, consumed);
                    scene_manager_next_scene(state->ctx.scene_manager, SceneConfig);
                    return consumed;
                } else if(input->type == InputTypeShort) {
                    toggle_adv(state);
                }
            }
            break;

        case InputKeyUp:
            if(is_attack && payload && payload->mode == PayloadModeBruteforce) {
                payload->bruteforce.counter = 0;
                payload->bruteforce.value =
                    (payload->bruteforce.value + 1) % (1 << (payload->bruteforce.size * 8));
            } else if(state->delay < COUNT_OF(delays) - 1) {
                state->delay++;
                if(advertising) start_blink(state);
            }
            break;

        case InputKeyDown:
            if(is_attack && payload && payload->mode == PayloadModeBruteforce) {
                payload->bruteforce.counter = 0;
                payload->bruteforce.value =
                    (payload->bruteforce.value - 1) % (1 << (payload->bruteforce.size * 8));
            } else if(state->delay > 0) {
                state->delay--;
                if(advertising) start_blink(state);
            }
            break;

        case InputKeyLeft:
            if(input->type == InputTypeLong) {
                state->ignore_bruteforce = payload ? (payload->mode != PayloadModeBruteforce) :
                                                     true;
            }
            if(input->type == InputTypeShort || !is_attack || state->ignore_bruteforce ||
               !payload || payload->mode != PayloadModeBruteforce) {
                if(advertising) toggle_adv(state);
                if(idx > 0) {
                    state->ctx.preset_index--;
                } else {
                    // At first attack → back to main menu
                    view_commit_model(view, consumed);
                    scene_manager_previous_scene(state->ctx.scene_manager);
                    return consumed;
                }
            } else {
                // Bruteforce manual send
                if(!advertising) {
                    beacon_stop_if_active();
                    GapExtraBeaconConfig* cfg = &state->config;
                    cfg->min_adv_interval_ms = delays[state->delay];
                    cfg->max_adv_interval_ms = delays[state->delay];
                    randomize_mac(state);
                    furi_check(furi_hal_bt_extra_beacon_set_config(cfg));
                    uint8_t sz;
                    uint8_t* p;
                    attacks[idx].protocol->make_packet(&sz, &p, payload);
                    furi_check(furi_hal_bt_extra_beacon_set_data(p, sz));
                    free(p);
                    furi_check(furi_hal_bt_extra_beacon_start());
                    if(state->ctx.led_indicator)
                        notification_message(state->ctx.notification, &solid_message);
                    furi_delay_ms(10);
                    beacon_stop_if_active();
                    if(state->ctx.led_indicator)
                        notification_message_block(state->ctx.notification, &sequence_reset_rgb);
                }
            }
            break;

        case InputKeyRight:
            if(input->type == InputTypeLong) {
                state->ignore_bruteforce = payload ? (payload->mode != PayloadModeBruteforce) :
                                                     true;
            }
            if(input->type == InputTypeShort || !is_attack || state->ignore_bruteforce ||
               !payload || payload->mode != PayloadModeBruteforce) {
                if(advertising) toggle_adv(state);
                if(idx < ATTACKS_COUNT - 1) state->ctx.preset_index++;
            } else if(input->type == InputTypeLong) {
                state->delay = (state->delay + 1) % COUNT_OF(delays);
                if(advertising) start_blink(state);
            }
            break;

        case InputKeyBack:
            if(advertising) toggle_adv(state);
            consumed = false; // let scene manager handle back
            break;

        default:
            break;
        }
    }

    view_commit_model(view, consumed);
    return consumed;
}

// ─── Lock timer ───────────────────────────────────────────────────────────────

static void lock_timer_callback(void* _ctx) {
    State* state = _ctx;
    if(state->lock_count < 3) {
        notification_message_block(state->ctx.notification, &sequence_display_backlight_off);
    } else {
        state->ctx.lock_keyboard = false;
    }
    with_view_model(state->main_view, State * *m, { (*m)->lock_warning = false; }, true);
    state->lock_count = 0;
    furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
}

// ─── Scene / dispatcher callbacks ────────────────────────────────────────────

static bool custom_event_callback(void* _ctx, uint32_t event) {
    State* state = _ctx;
    return scene_manager_handle_custom_event(state->ctx.scene_manager, event);
}
static void tick_event_callback(void* _ctx) {
    State* state = _ctx;
    bool adv;
    with_view_model(state->main_view, State * *m, { adv = (*m)->advertising; }, adv);
    scene_manager_handle_tick_event(state->ctx.scene_manager);
}
static bool back_event_callback(void* _ctx) {
    State* state = _ctx;
    return scene_manager_handle_back_event(state->ctx.scene_manager);
}

// ─── App entry point ─────────────────────────────────────────────────────────

int32_t ble_spam(void* p) {
    UNUSED(p);

    GapExtraBeaconConfig prev_cfg;
    const GapExtraBeaconConfig* prev_cfg_ptr = furi_hal_bt_extra_beacon_get_config();
    if(prev_cfg_ptr) memcpy(&prev_cfg, prev_cfg_ptr, sizeof(prev_cfg));
    uint8_t prev_data[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t prev_data_len = furi_hal_bt_extra_beacon_get_data(prev_data);
    bool prev_active = furi_hal_bt_extra_beacon_is_active();

    State* state = malloc(sizeof(State));
    memset(state, 0, sizeof(State));

    // BLE config
    state->config.adv_channel_map = GapAdvChannelMapAll;
    state->config.adv_power_level = GapAdvPowerLevel_6dBm;
    state->config.address_type = GapAddressTypePublic;

    // Thread
    state->thread = furi_thread_alloc();
    furi_thread_set_callback(state->thread, adv_thread);
    furi_thread_set_context(state->thread, state);
    furi_thread_set_stack_size(state->thread, 2048);

    // Ctx defaults
    state->ctx.led_indicator = true;
    state->ctx.fast_mode = false;
    state->ctx.fast_mode_prewarm_idx = 3; // default 15 s pre-warm
    state->ctx.preset_index = 0;

    // Initialize custom hex buffer with E9-05 Cyan as a sensible default
    {
        static const uint8_t def[] = {0xE1, 0x00, 0xE9, 0x05, 0x00, 0x8F, 0x0E, 0x00, 0xB0};
        memcpy(state->ctx.byte_store, def, sizeof(def));
    }

    state->lock_timer = furi_timer_alloc(lock_timer_callback, FuriTimerTypeOnce, state);

    state->ctx.notification = furi_record_open(RECORD_NOTIFICATION);
    Gui* gui = furi_record_open(RECORD_GUI);
    state->ctx.view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_set_event_callback_context(state->ctx.view_dispatcher, state);
    view_dispatcher_set_custom_event_callback(state->ctx.view_dispatcher, custom_event_callback);
    view_dispatcher_set_tick_event_callback(state->ctx.view_dispatcher, tick_event_callback, 100);
    view_dispatcher_set_navigation_event_callback(state->ctx.view_dispatcher, back_event_callback);
    state->ctx.scene_manager = scene_manager_alloc(&scene_handlers, &state->ctx);

    // Main (preset browser) view
    state->main_view = view_alloc();
    view_allocate_model(state->main_view, ViewModelTypeLocking, sizeof(State*));
    with_view_model(state->main_view, State * *m, { *m = state; }, false);
    view_set_context(state->main_view, state->main_view);
    view_set_draw_callback(state->main_view, draw_callback);
    view_set_input_callback(state->main_view, input_callback);
    view_dispatcher_add_view(state->ctx.view_dispatcher, ViewMain, state->main_view);

    // Byte input
    state->ctx.byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        state->ctx.view_dispatcher, ViewByteInput, byte_input_get_view(state->ctx.byte_input));

    // Submenu
    state->ctx.submenu = submenu_alloc();
    view_dispatcher_add_view(
        state->ctx.view_dispatcher, ViewSubmenu, submenu_get_view(state->ctx.submenu));

    // Text input
    state->ctx.text_input = text_input_alloc();
    view_dispatcher_add_view(
        state->ctx.view_dispatcher, ViewTextInput, text_input_get_view(state->ctx.text_input));

    // Variable item list
    state->ctx.variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        state->ctx.view_dispatcher,
        ViewVariableItemList,
        variable_item_list_get_view(state->ctx.variable_item_list));

    view_dispatcher_attach_to_gui(state->ctx.view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(state->ctx.scene_manager, SceneStart); // open at main menu
    view_dispatcher_run(state->ctx.view_dispatcher);

    // ── Cleanup ───────────────────────────────────────────────────────────────
    view_dispatcher_remove_view(state->ctx.view_dispatcher, ViewByteInput);
    byte_input_free(state->ctx.byte_input);
    view_dispatcher_remove_view(state->ctx.view_dispatcher, ViewSubmenu);
    submenu_free(state->ctx.submenu);
    view_dispatcher_remove_view(state->ctx.view_dispatcher, ViewTextInput);
    text_input_free(state->ctx.text_input);
    view_dispatcher_remove_view(state->ctx.view_dispatcher, ViewVariableItemList);
    variable_item_list_free(state->ctx.variable_item_list);
    view_dispatcher_remove_view(state->ctx.view_dispatcher, ViewMain);
    view_free(state->main_view);
    scene_manager_free(state->ctx.scene_manager);
    view_dispatcher_free(state->ctx.view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_timer_free(state->lock_timer);
    furi_thread_free(state->thread);
    free(state);

    // Restore previous beacon state
    beacon_stop_if_active();
    if(prev_cfg_ptr) furi_check(furi_hal_bt_extra_beacon_set_config(&prev_cfg));
    furi_check(furi_hal_bt_extra_beacon_set_data(prev_data, prev_data_len));
    if(prev_active) furi_check(furi_hal_bt_extra_beacon_start());

    return 0;
}
