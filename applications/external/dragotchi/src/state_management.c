#include <furi.h>
#include <stdio.h>
#include "state_management.h"
#include "constants.h"
#include "game_model.h"
#include "game_logic.h"
#include "states.h"
#include "clock.h"
#include "save_restore.h"
#include "hunt.h"
#include "hunt_hw.h"
#include "esp_link.h"
#include "economy.h"
#include "expedition.h"

GameEventFlags init_state(struct GameState *gs) {
    if(!load_state_from_file(&gs->persistent)) {
        game_state_init(gs, game_now());
        return EVT_NONE;
    }
    // Loaded: transient fields are not persisted, initialise them, then
    // fast-forward the simulation to now.
    gs->next_animation_index = 0;
    gs->display_state = DISP_IDLE;
    return advance_state(gs, game_now());
}

void persist_state(struct GameState *gs) {
    if(!save_state_to_file(&gs->persistent)) {
        furi_crash("Unable to save state to storage");
    }
}

void reset_state(struct GameState *gs) {
    game_state_init(gs, game_now());
}

GameEventFlags tick_state(struct GameState *gs) {
    GameEventFlags f = advance_state(gs, game_now());
    if(gs->reveal_ticks) gs->reveal_ticks--;
    return f;
}

GameEventFlags do_action(struct GameState *gs, enum ThreadsMessageType type) {
    uint32_t now = game_now();
    switch(type) {
        case PROCESS_FEED:     return do_feed(gs, now);
        case PROCESS_PLAY:     return do_play(gs, now);
        case PROCESS_CLEAN:    return do_clean(gs, now);
        case PROCESS_MEDICINE: return do_medicine(gs, now);
        case PROCESS_SCOLD:    return do_scold(gs, now);
        case TOGGLE_LIGHTS:    return do_lights(gs, now);
        default:               return EVT_NONE;
    }
}

GameEventFlags do_forage(struct GameState *gs) {
    uint32_t now = game_now();
    if(!forage_ready(gs, now)) {
        uint32_t left = forage_cooldown_remaining(gs, now);
        snprintf(gs->reveal_text, sizeof(gs->reveal_text), "Hunt ready in %lus", (unsigned long)left);
        gs->reveal_ticks = 3;
        return EVT_NONE;
    }
    uint8_t activity = 0, band = 1;
    hunt_sense(&activity, &band);

    /* Optional WiFi devboard: dense airwaves boost the catch, guarantee a floor,
     * and can drop a board-exclusive "storm egg" ("signal storm"). */
    uint8_t wifi = 0;
    int8_t wrssi = 0;
    gs->board_present = esp_probe(&wifi, &wrssi) ? 1 : 0;
    gs->last_wifi = wifi;
    gs->last_rssi = wrssi;

    struct Catch c;
    if(gs->board_present) {
        c = storm_catch_roll(activity, band, wifi);
        apply_catch(gs, c, now);
        gs->last_catch = c;
        catch_describe_storm(c, gs->reveal_text, sizeof(gs->reveal_text));
        /* Board-assisted: show the dedicated animated Signal Storm screen
         * instead of the bottom banner (main scene switches on the next tick). */
        gs->storm_ready = 1;
        gs->reveal_ticks = 0;
    } else {
        c = catch_roll(activity, band);
        apply_catch(gs, c, now);
        gs->last_catch = c;
        catch_describe(c, gs->reveal_text, sizeof(gs->reveal_text));
        gs->reveal_ticks = 4;
    }
    return EVT_CAUGHT;
}

void do_expedition(struct GameState *gs, uint16_t minutes) {
    expedition_start(gs, minutes, game_now());
}

void do_hatch_heir(struct GameState *gs) {
    if(has_heir_egg(gs)) {
        hatch_heir(gs, game_now());
        snprintf(gs->reveal_text, sizeof(gs->reveal_text), "An heir hatches!");
        gs->reveal_ticks = 4;
    }
}

bool state_is_night_now(void) {
    return is_night(game_now());
}

uint32_t state_expedition_remaining(const struct GameState *gs) {
    return expedition_remaining_sec(gs, game_now());
}

static const char *care_word(int32_t care) {
    if(care >= 80) return "Thriving";
    if(care >= 60) return "Content";
    if(care >= 40) return "OK";
    if(care >= 20) return "Poor";
    return "Neglected";
}

void get_state_str(const struct GameState *gs, char *str, size_t size) {
    const struct PersistentGameState *p = &gs->persistent;
    uint32_t now = game_now();
    uint32_t age_days = (now > p->birth_timestamp) ? (now - p->birth_timestamp) / 86400u : 0;
    const char *stage_line = (p->stage == ADULT && p->alignment != ALIGN_NONE)
        ? ALIGNMENT_STRING[p->alignment] : "";
    snprintf(str, size,
             "%s%s%s  %lud\nCare: %s\n%s\nHoard %lu  Eggs %u/%u",
             stage_line, stage_line[0] ? " " : "",
             LIFE_STAGE_STRING[p->stage],
             (unsigned long)age_days,
             care_word(p->care_score),
             hoard_rank(p->hoard),
             (unsigned long)p->hoard,
             (unsigned)p->eggs_common, (unsigned)p->eggs_rare);
}
