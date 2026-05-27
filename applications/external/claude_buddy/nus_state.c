#include "nus_state.h"

#include <furi.h>
#include <string.h>

#define TAG "NusState"

/* 30 s of silence → assume the desktop bridge dropped. */
#define HB_TIMEOUT_MS 30000
/* Heart transient threshold — matches the ESP32 reference firmware. */
#define HEART_WINDOW_MS 5000
/* Tokens per level-up (Anthropic reference: 50 000). */
#define LEVEL_TOKENS 50000u

/* ── helpers ────────────────────────────────────────────────── */

static BuddyState derive_state(int total, int running, int waiting, bool has_prompt) {
    if(has_prompt || waiting > 0) return BuddyStateAttention;
    if(running > 0) return BuddyStateBusy;
    /* total==0 is "no session" but we're still connected to Desktop — treat
     * as Idle, not Sleep.  Sleep is only entered when HB timeout fires. */
    (void)total;
    return BuddyStateIdle;
}

/* Play the "entering state X" cue.  Each case picks the sound *and* the
 * LED-restore hint so a Busy→Idle→Busy cycle doesn't accidentally leave
 * the LED in the wrong persistent color. */
static void apply_entry(NusStateCtx* ctx, BuddyState from, BuddyState to) {
    switch(to) {
    case BuddyStateSleep:
        if(ctx->app_is_working) *ctx->app_is_working = false;
        notify_play(ctx->notifications, SoundDisconnect, LedStateOff);
        ui_set_pose(ctx->ui, PoseSleeping);
        ui_set_claude_connected(ctx->ui, false);
        break;

    case BuddyStateIdle:
        if(ctx->app_is_working) *ctx->app_is_working = false;
        if(from == BuddyStateSleep) {
            /* First real connection (bridge alive for the first time or
             * after a drop) — warm greeting. */
            notify_play(ctx->notifications, SoundConnect, LedStateOff);
            ui_set_pose(ctx->ui, PoseExcited);
        } else if(from == BuddyStateBusy) {
            /* Assistant finished generating — happy chirp. */
            notify_play(ctx->notifications, SoundReady, LedStateOff);
            ui_set_pose(ctx->ui, PoseHappy);
        } else {
            /* e.g. returning from Attention without a fast approve. */
            notify_play(ctx->notifications, SoundLedOff, LedStateOff);
            ui_set_pose(ctx->ui, PoseIdle);
        }
        ui_set_claude_connected(ctx->ui, true);
        break;

    case BuddyStateBusy:
        if(ctx->app_is_working) *ctx->app_is_working = true;
        notify_play(ctx->notifications, SoundLedWorking, LedStateOff);
        ui_set_pose(ctx->ui, PoseThinking);
        ui_set_claude_connected(ctx->ui, true);
        break;

    case BuddyStateAttention:
        /* Magenta blink + ring is intentionally persistent until the user
         * decides.  is_working is unchanged — if Claude was generating
         * when the prompt arrived, keep that LED-under-blink bookkeeping. */
        notify_play(ctx->notifications, SoundPerm, LedStateOff);
        ui_set_pose(ctx->ui, PoseWorried);
        ui_set_claude_connected(ctx->ui, true);
        break;
    }
    FURI_LOG_D(TAG, "state %d → %d", from, to);
}

/* ── public API ─────────────────────────────────────────────── */

void nus_state_init(NusStateCtx* ctx, NotificationApp* n, UiState* ui, bool* is_working) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->notifications = n;
    ctx->ui = ui;
    ctx->app_is_working = is_working;
    ctx->current = BuddyStateSleep;
}

void nus_state_reset(NusStateCtx* ctx) {
    ctx->current = BuddyStateSleep;
    ctx->last_hb_tick = 0;
    ctx->attention_tick = 0;
    ctx->last_msg[0] = '\0';
    ctx->initialized = false;
    ctx->sleep_timeout_fired = false;
}

void nus_state_on_heartbeat(
    NusStateCtx* ctx,
    NusStats* stats,
    int total,
    int running,
    int waiting,
    bool has_prompt,
    const char* msg,
    uint32_t tokens) {
    if(!ctx || !msg) return;
    ctx->last_hb_tick = furi_get_tick();
    ctx->sleep_timeout_fired = false;

    BuddyState next = derive_state(total, running, waiting, has_prompt);

    /* Record the moment we entered Attention so the Heart window can be
     * measured from the prompt's arrival. */
    if(next == BuddyStateAttention && ctx->current != BuddyStateAttention) {
        ctx->attention_tick = furi_get_tick();
    } else if(next != BuddyStateAttention) {
        ctx->attention_tick = 0;
    }

    if(next != ctx->current || !ctx->initialized) {
        BuddyState from = ctx->initialized ? ctx->current : BuddyStateSleep;
        apply_entry(ctx, from, next);
        ctx->current = next;
        ctx->initialized = true;
    }

    /* Tool-call beats: while Busy, each msg change (e.g. "(called Bash)"
     * → "(called Read)") plays a brief audible tap so the user hears
     * every tool invocation without looking. SoundLedFlash was LED-only
     * and inaudible. SoundEnter returns to the working LED afterwards. */
    if(ctx->current == BuddyStateBusy && msg[0] && strcmp(msg, ctx->last_msg) != 0) {
        notify_play(ctx->notifications, SoundAlert, LedStateWorking);
    }
    strlcpy(ctx->last_msg, msg, sizeof(ctx->last_msg));

    /* Tokens monotonically increase within a session; detect the 50 k
     * boundary for Celebrate.  Persisted across restarts via NusStats. */
    if(stats && tokens > stats->tokens_seen) {
        uint32_t old_level = stats->tokens_seen / LEVEL_TOKENS;
        stats->tokens_seen = tokens;
        uint32_t new_level = tokens / LEVEL_TOKENS;
        if(new_level > old_level) {
            stats->level = new_level;
            /* Celebrate transient — doesn't change steady state, just a
             * one-shot flourish.  Pose auto-resets via its built-in
             * animation timer. */
            notify_play(ctx->notifications, SoundSuccess, LedStateOff);
            ui_set_pose(ctx->ui, PoseExcited);
            nus_stats_save(stats);
        }
    }
}

void nus_state_on_permission_decision(NusStateCtx* ctx, NusStats* stats, bool allowed) {
    if(!ctx) return;

    bool fast = false;
    if(ctx->attention_tick != 0) {
        uint32_t freq = furi_kernel_get_tick_frequency();
        uint32_t elapsed_ms = (furi_get_tick() - ctx->attention_tick) * 1000 / freq;
        fast = elapsed_ms < HEART_WINDOW_MS;
    }
    ctx->attention_tick = 0;

    if(stats) {
        if(allowed) {
            stats->approvals++;
            if(fast) stats->approvals_fast++;
        } else {
            stats->denies++;
        }
        nus_stats_save(stats);
    }

    if(fast && allowed) {
        /* Heart transient — reward snappy decisions. */
        notify_play(ctx->notifications, SoundSuccess, LedStateOff);
        ui_set_pose(ctx->ui, PoseHappy);
    }
    /* Steady state will be re-applied by the next heartbeat. */
}

void nus_state_tick(NusStateCtx* ctx) {
    if(!ctx || !ctx->initialized) return;
    if(ctx->current == BuddyStateSleep) return;
    if(ctx->last_hb_tick == 0) return;

    uint32_t freq = furi_kernel_get_tick_frequency();
    uint32_t since_ms = (furi_get_tick() - ctx->last_hb_tick) * 1000 / freq;
    if(since_ms < HB_TIMEOUT_MS) return;

    if(!ctx->sleep_timeout_fired) {
        apply_entry(ctx, ctx->current, BuddyStateSleep);
        ctx->current = BuddyStateSleep;
        ctx->sleep_timeout_fired = true;
    }
}
