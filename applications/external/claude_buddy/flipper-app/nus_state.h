/**
 * Desktop-mode state machine.
 *
 * Translates the Anthropic heartbeat snapshot + permission decisions into
 * a small set of states, each with a distinct LED / sound / pose trigger.
 * The aim is for the user to feel Claude's working state without looking
 * at the screen.
 *
 *   Sleep       — no HB in 30 s (or never connected)
 *   Idle        — connected, nothing active
 *   Busy        — running > 0 (assistant generating)
 *   Attention   — waiting > 0 && prompt present (needs approval)
 *   Celebrate   — transient: tokens just crossed a 50 k boundary (level up)
 *   Heart       — transient: permission approved within 5 s of prompt
 *
 * State entry emits a sound / LED via the notification service and sets
 * a pose on the UI.  Transient states auto-revert (UI poses auto-reset,
 * LEDs managed by notify_play restore semantics).
 *
 * All public functions must be called from the GUI thread (they call
 * notify_play, view_commit_model, etc).
 */

#pragma once

#include "notifications.h"
#include "nus_stats.h"
#include "ui.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BuddyStateSleep = 0,
    BuddyStateIdle,
    BuddyStateBusy,
    BuddyStateAttention,
} BuddyState;

typedef struct {
    NotificationApp* notifications;
    UiState* ui;
    bool* app_is_working; /* shared with App — we toggle on Busy entry/exit */

    BuddyState current;
    uint32_t last_hb_tick;       /* furi ticks of the last heartbeat received */
    uint32_t attention_tick;     /* tick when Attention entered; 0 otherwise */
    char last_msg[64];           /* last heartbeat msg (for tool-call change detection) */
    bool initialized;
    bool sleep_timeout_fired;    /* guard so Sleep sound plays only once per drop */
} NusStateCtx;

/* Initialize the context.  Safe to call multiple times (resets state). */
void nus_state_init(NusStateCtx* ctx, NotificationApp* n, UiState* ui, bool* is_working);

/* Drop all transient state (called when BLE transport is torn down). */
void nus_state_reset(NusStateCtx* ctx);

/* Drive the state machine from a parsed heartbeat.  Also updates
 * stats->tokens_seen / ->level and fires Celebrate on level-up. */
void nus_state_on_heartbeat(
    NusStateCtx* ctx,
    NusStats* stats,
    int total,
    int running,
    int waiting,
    bool has_prompt,
    const char* msg,
    uint32_t tokens);

/* Called after the user makes a permission decision.  Updates stats and
 * fires the Heart transient if the decision landed inside the 5 s window. */
void nus_state_on_permission_decision(NusStateCtx* ctx, NusStats* stats, bool allowed);

/* Periodic tick (e.g. 1 Hz) — detects the Sleep timeout (>30 s no HB). */
void nus_state_tick(NusStateCtx* ctx);

#ifdef __cplusplus
}
#endif
