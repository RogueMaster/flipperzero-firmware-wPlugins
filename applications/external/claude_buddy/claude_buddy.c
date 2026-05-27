/**
 * Claude Buddy - Flipper Zero companion for Claude Code
 *
 * Physical remote control + audio/haptic feedback device.
 * Buttons: UP(short)=voice, UP(long)=hold Space, LEFT=ESC, RIGHT=menu,
 * OK=enter, BACK(short)=dismiss, BACK(long)=exit
 *
 * Threading model:
 *   Serial RX callback runs on a worker thread — it must NOT call UI functions.
 *   Instead it queues parsed messages into a FuriMessageQueue and wakes the
 *   GUI thread via view_dispatcher_send_custom_event.  The custom-event
 *   callback drains the queue and performs all UI updates safely.
 *
 * Transport selection (runtime, automatic):
 *   If a USB cable is detected at startup → USB CDC transport.
 *   Otherwise → BLE serial transport.
 *   The active mode is shown in the header ("USB" or "BLE").
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_power.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <gui/gui.h>
#include <notification/notification_messages.h>

#include "transport.h"
#include "protocol.h"
#include "nus_protocol.h"
#include "nus_state.h"
#include "nus_stats.h"
#include "nus_transcript.h"
#include "nus_charpack.h"
#include "notifications.h"
#include "ui.h"
#include "app_settings.h"

#define TAG "ClaudeBuddy"

/* Forward decl — registered as the BT link-state observer. */
static void on_bt_connect(bool connected, void* context);

/* Pick the BLE transport based on the persisted user setting:
 *   Bridge  → custom-UUID serial profile, talks to our Python host bridge
 *   Desktop → Nordic UART Service profile, talks to Claude Desktop/Cowork
 *
 * The link-state observer is attached here so every code path that
 * rebuilds the BT transport (USB handover, toggle mode, readvertise,
 * initial boot) gets the same behaviour. */
static Transport* transport_ble_alloc(void* app_ctx) {
    Transport* t = (app_settings_get_ble_mode() == BleModeDesktop)
        ? transport_nus_alloc()
        : transport_bt_alloc();
    transport_bt_set_connect_callback(t, on_bt_connect, app_ctx);
    return t;
}

#define SERIAL_QUEUE_SIZE 8

enum {
    CustomEventSerialMsg      = 100,
    // UI_CUSTOM_EVENT_TRANSITION = 101 is defined in ui.h
    CustomEventUsbDisconnect  = 102,
    CustomEventNusTick        = 103, // forward-declared below, see nus_tick_cb
    CustomEventNusReadvertise = 104, // rebuild BLE transport to pick up new adv name
    CustomEventBtLinkDown     = 105, // bridge-mode BT link dropped (bridge died)
};

typedef struct {
    UiState* ui;
    Transport* transport;
    NotificationApp* notifications;
    FuriMessageQueue* serial_queue;
    FuriTimer* usb_poll_timer; // non-NULL only in USB mode; polls for cable removal
    bool hello_sent;
    bool muted;      // suppress all sounds (toggled by long-press Down)
    bool is_working;    // blue LED active — alerts should flash cyan then return to blue
    bool dictating;     // true while dictation is active (set by voice_start / cleared by voice_stop)
    ProtocolMessage rx_msg_buf;     // Buffer for parsing on transport thread
    ProtocolMessage process_msg_buf; // Buffer for executing on GUI thread
    /* Kept off the BLE RX thread's stack — NusMessage is ~430 bytes and
     * was contributing to MemManage stack-overflow faults on long idle
     * sessions.  Single-threaded relative to itself (only the transport
     * RX callback writes here). */
    NusMessage nus_rx_buf;
    char tx_buf[256];               // Shared TX building buffer (GUI thread)
    /* Cached at every transport_start so on_serial_data (transport thread)
     * can pick the right parser without doing file I/O per line. */
    BleMode current_ble_mode;
    /* Last permission prompt id seen in NUS mode; echoed back when the
     * user makes a decision. Empty in Bridge mode. */
    char last_perm_id[40];
    /* Desktop-mode state machine + persisted counters. Only active while
     * current_ble_mode == BleModeDesktop; otherwise untouched. */
    NusStateCtx nus_state;
    NusStats nus_stats;
    FuriTimer* nus_tick_timer; /* 1 Hz while in Desktop mode; drives sleep detection */
} App;

// Play a sound unless muted. Interrupt/mute toggle always plays regardless.
static void app_notify(App* app, SoundType snd) {
    if(!app || !app->notifications) return;
    if(!app->muted) notify_play(app->notifications, snd,
        app->is_working ? LedStateWorking : LedStateOff);
}

/* ── Transport auto-detection ─────────────────────────────────── */

static bool detect_usb_cable(void) {
    /* Detect USB cable by checking either:
       - actively charging (furi_hal_power_is_charging()), OR
       - fully charged but still plugged in (furi_hal_power_is_charging_done()).
       Together these cover all USB-connected states. */
    return furi_hal_power_is_charging() || furi_hal_power_is_charging_done();
}

/* ── USB disconnect polling (timer thread) ────────────────────── */

static void usb_poll_cb(void* context) {
    App* app = context;
    if(!detect_usb_cable()) {
        view_dispatcher_send_custom_event(
            app->ui->view_dispatcher, CustomEventUsbDisconnect);
    }
}

/* ── BT link-state callback (BT stack thread) ────────────────── */

/* Fires on Bridge-mode BT connect/disconnect transitions.  Runs on the
 * BT stack thread — must not touch UI or call transport_send.  We only
 * care about disconnects here: when the host bridge dies its BLE link
 * drops, and the NEXT bridge that reconnects is a fresh process that
 * expects a fresh handshake (hello → "Claude Code / Connected" notify).
 * Resetting app->hello_sent on the GUI thread causes the next ping to
 * re-trigger hello, restoring the session-start notification. */
static void on_bt_connect(bool connected, void* context) {
    App* app = context;
    if(!app || !app->ui || !app->ui->view_dispatcher) return;
    if(connected) return;
    view_dispatcher_send_custom_event(
        app->ui->view_dispatcher, CustomEventBtLinkDown);
}

/* ── NUS state-machine tick (timer thread) ───────────────────── */

/* The state machine cares about wall-clock elapsed time for the Sleep
 * timeout; run nus_state_tick on the GUI thread via a custom event so
 * it can safely touch UI / notifications. */

static void nus_tick_cb(void* context) {
    App* app = context;
    if(app && app->ui && app->ui->view_dispatcher) {
        view_dispatcher_send_custom_event(app->ui->view_dispatcher, CustomEventNusTick);
    }
}

/* Spin up the 1 Hz tick timer if we're in Desktop mode and it isn't
 * already running. Safe to call repeatedly. */
static void nus_tick_ensure_started(App* app) {
    if(app->nus_tick_timer) return;
    app->nus_tick_timer = furi_timer_alloc(nus_tick_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->nus_tick_timer, 1000);
}

static void nus_tick_stop(App* app) {
    if(!app->nus_tick_timer) return;
    furi_timer_stop(app->nus_tick_timer);
    furi_timer_free(app->nus_tick_timer);
    app->nus_tick_timer = NULL;
}

/* Apply the side effects of switching transports: cache the BLE mode (so
 * on_serial_data can pick the right parser), clear the pending perm-id,
 * and start/stop the state machine + its tick timer. */
static void app_on_transport_mode_changed(App* app, bool on_ble) {
    app->current_ble_mode = on_ble ? app_settings_get_ble_mode() : BleModeBridge;
    app->last_perm_id[0] = '\0';
    if(app->current_ble_mode == BleModeDesktop) {
        nus_state_reset(&app->nus_state);
        nus_transcript_reset();
        nus_charpack_reset();
        nus_tick_ensure_started(app);
    } else {
        nus_tick_stop(app);
    }
}

/* ── Helpers ──────────────────────────────────────────────────── */

static SoundType sound_from_string(const char* name) {
    if(strcmp(name, "success") == 0) return SoundSuccess;
    if(strcmp(name, "error") == 0) return SoundError;
    if(strcmp(name, "alert") == 0) return SoundAlert;
    if(strcmp(name, "voice_start") == 0) return SoundVoiceStart;
    if(strcmp(name, "voice_start_led") == 0) return SoundVoiceStartLed;
    if(strcmp(name, "voice_stop") == 0) return SoundVoiceStop;
    if(strcmp(name, "voice_stop_quiet") == 0) return SoundVoiceStopQuiet;
    if(strcmp(name, "disconnect") == 0) return SoundDisconnect;
    if(strcmp(name, "connect") == 0) return SoundConnect;
    if(strcmp(name, "led_working") == 0) return SoundLedWorking;
    if(strcmp(name, "led_off") == 0) return SoundLedOff;
    if(strcmp(name, "led_compact") == 0) return SoundLedCompact;
    if(strcmp(name, "compact_done") == 0) return SoundCompactDone;
    if(strcmp(name, "interrupt") == 0) return SoundInterrupt;
    if(strcmp(name, "session_end") == 0) return SoundSessionEnd;
    if(strcmp(name, "ready") == 0) return SoundReady;
    return SoundAlert;
}

/* ── Serial RX (worker thread) ────────────────────────────────── */

/* Translate a parsed Anthropic message into the existing ProtocolMessage
 * shape so process_message can stay protocol-agnostic.  Returns true if
 * the result should be queued; false to drop (e.g. evt:turn). */
static bool translate_nus_to_protocol(const NusMessage* in, ProtocolMessage* out) {
    memset(out, 0, sizeof(*out));
    switch(in->kind) {
    case NusMsgHeartbeat:
        /* Carry the counters no matter which branch we emit so the state
         * machine can see them on both prompt and non-prompt paths. */
        out->hb_total = in->total;
        out->hb_running = in->running;
        out->hb_waiting = in->waiting;
        out->hb_tokens = in->tokens;
        out->hb_tokens_today = in->tokens_today;
        if(in->has_prompt) {
            out->type = MsgTypePerm;
            strlcpy(out->text, in->prompt_tool[0] ? in->prompt_tool : "Permission",
                    sizeof(out->text));
            strlcpy(out->text2, in->prompt_hint, sizeof(out->text2));
            strlcpy(out->perm_id, in->prompt_id, sizeof(out->perm_id));
        } else {
            out->type = MsgTypeAnthropicHB;
            const char* fallback = (in->running > 0) ? "Working..." :
                                   (in->total > 0)   ? "Connected"  :
                                                       "Idle";
            strlcpy(out->text, in->msg[0] ? in->msg : fallback, sizeof(out->text));
        }
        return true;

    case NusMsgCmdStatus:
    case NusMsgCmdOwner:
    case NusMsgCmdName:
    case NusMsgCmdUnpair:
    case NusMsgCmdCharBegin:
    case NusMsgCmdFile:
    case NusMsgCmdChunk:
    case NusMsgCmdFileEnd:
    case NusMsgCmdCharEnd: {
        out->type = MsgTypeUnknown;
        const char* cmd = "";
        switch(in->kind) {
        case NusMsgCmdStatus:     cmd = "status"; break;
        case NusMsgCmdOwner:      cmd = "owner";  strlcpy(out->nus_name, in->name, sizeof(out->nus_name)); break;
        case NusMsgCmdName:       cmd = "name";   strlcpy(out->nus_name, in->name, sizeof(out->nus_name)); break;
        case NusMsgCmdUnpair:     cmd = "unpair"; break;
        case NusMsgCmdCharBegin:  cmd = "char_begin"; strlcpy(out->nus_name, in->pack_name, sizeof(out->nus_name)); break;
        case NusMsgCmdFile:       cmd = "file";       strlcpy(out->text, in->file_path, sizeof(out->text)); break;
        case NusMsgCmdChunk:      cmd = "chunk";
            /* Copy the base64 body out of the (transient) parse buffer so
             * the GUI thread can decode+write storage without racing. */
            if(in->chunk_body && in->chunk_body_len > 0) {
                int copy = in->chunk_body_len;
                if(copy >= (int)sizeof(out->menu_data)) copy = sizeof(out->menu_data) - 1;
                memcpy(out->menu_data, in->chunk_body, copy);
                out->menu_data[copy] = '\0';
            }
            break;
        case NusMsgCmdFileEnd:    cmd = "file_end"; break;
        case NusMsgCmdCharEnd:    cmd = "char_end"; break;
        default: break;
        }
        strlcpy(out->pending_ack, cmd, sizeof(out->pending_ack));
        return true;
    }

    case NusMsgTime:
        /* time has no ack per spec — queue a plain carrier message with
         * just the epoch/tz fields.  process_message sets the RTC on the
         * GUI thread without attempting an ack. */
        out->type = MsgTypeUnknown;
        out->nus_time_epoch = in->time_epoch;
        out->nus_time_tz = in->time_tz_offset;
        return true;

    case NusMsgTurn:
    case NusMsgUnknown:
    default:
        return false;
    }
}

/* Callback invoked per text block in a turn event; appends to the
 * transcript ring buffer with an "A: " marker so it's distinguishable
 * from heartbeat entries. */
static void on_turn_text(const char* text, int text_len, void* ctx) {
    (void)ctx;
    char line[NUS_TRANSCRIPT_LINE_MAX];
    int prefix = snprintf(line, sizeof(line), "A: ");
    int copy = text_len;
    int space = (int)sizeof(line) - prefix - 1;
    if(copy > space) copy = space;
    if(copy > 0) memcpy(line + prefix, text, copy);
    line[prefix + (copy > 0 ? copy : 0)] = '\0';
    nus_transcript_append(line);
}

static void on_serial_data(const char* line, void* context) {
    App* app = context;
    if(!app || !line) return;

    if(app->current_ble_mode == BleModeDesktop) {
        /* Log raw line for desktop-mode protocol debugging. Enable with
         * `log debug` on the Flipper CLI. Heartbeats arrive every 10 s
         * (plus change-driven) so this is not too noisy. */
        FURI_LOG_D(TAG, "rx: %s", line);
        NusMessage* nus = &app->nus_rx_buf;
        if(!nus_protocol_parse(line, nus)) {
            FURI_LOG_D(TAG, "rx: parse failed");
            return;
        }
        FURI_LOG_D(
            TAG,
            "rx: kind=%d total=%d running=%d waiting=%d prompt=%d msg='%s'",
            nus->kind, nus->total, nus->running, nus->waiting,
            nus->has_prompt ? 1 : 0, nus->msg);
        /* Transcript is the only side effect that MUST happen inline —
         * the entries_body / turn_content_body pointers live inside the
         * parse-time JSON buffer and go stale after return.  The ring
         * buffer itself is thread-safe via its internal mutex. */
        if(nus->kind == NusMsgHeartbeat && nus->entries_body && nus->entries_body_len > 0) {
            nus_transcript_replace_from_entries(nus->entries_body, nus->entries_body_len);
        } else if(nus->kind == NusMsgTurn && nus->turn_content_body) {
            nus_protocol_foreach_turn_text(
                nus->turn_content_body, nus->turn_content_body_len, on_turn_text, NULL);
        }
        if(!translate_nus_to_protocol(nus, &app->rx_msg_buf)) return;
    } else {
        if(!protocol_parse(line, &app->rx_msg_buf)) return;
    }

    /* Non-blocking put; queue copies structure by value. */
    if(furi_message_queue_put(app->serial_queue, &app->rx_msg_buf, 0) == FuriStatusOk) {
        view_dispatcher_send_custom_event(app->ui->view_dispatcher, CustomEventSerialMsg);
    }
}

/* ── Message processing (GUI thread) ──────────────────────────── */

static void process_message(App* app, ProtocolMessage* msg) {
    if(!app || !msg) return;
    /* Any host-originated message implies Claude is connected.
       Handles the case where Flipper launches after session-start.
       Exception: disconnect / session_end notifications must clear the
       indicator — otherwise the session-end notify would overwrite the
       claude_disconnect state message that fires just before it. */
    if(msg->type == MsgTypeStatus || msg->type == MsgTypePing) {
        ui_set_claude_connected(app->ui, true);
    } else if(msg->type == MsgTypeNotify) {
        SoundType snd_check = sound_from_string(msg->sound);
        if(snd_check == SoundDisconnect || snd_check == SoundSessionEnd) {
            ui_set_claude_connected(app->ui, false);
        } else {
            ui_set_claude_connected(app->ui, true);
        }
    }

    switch(msg->type) {
    case MsgTypeNotify: {
        SoundType snd = sound_from_string(msg->sound);
        bool is_voice = (snd == SoundVoiceStart || snd == SoundVoiceStartLed ||
                         snd == SoundVoiceStop || snd == SoundVoiceStopQuiet);

        // Non-LED sounds clear the working/compacting state (turn complete, error, interrupt, etc.)
        if(snd != SoundLedWorking && snd != SoundAlert && snd != SoundLedCompact) {
            app->is_working = false;
        }

        if(!app->muted) notify_play(app->notifications, snd,
            app->is_working ? LedStateWorking : LedStateOff);

        if(snd == SoundVoiceStart || snd == SoundVoiceStartLed) {
            app->dictating = true;
            ui_set_pose(app->ui, PoseListening);
        } else if(snd == SoundVoiceStop || snd == SoundVoiceStopQuiet) {
            app->dictating = false;
            ui_set_pose(app->ui, PoseIdle);
        } else if(snd == SoundSuccess || snd == SoundReady) {
            ui_set_pose(app->ui, PoseHappy);
        } else if(snd == SoundConnect) {
            ui_set_pose(app->ui, PoseExcited);
        } else if(snd == SoundError || snd == SoundAlert || snd == SoundInterrupt) {
            ui_set_pose(app->ui, PoseAlert);
        } else if(snd == SoundSessionEnd) {
            ui_set_pose(app->ui, PoseSleeping);
        }

        if(msg->text[0] && !is_voice) {
            if(msg->text2[0]) {
                ui_show_status2(app->ui, msg->text, msg->text2, true);
            } else {
                ui_show_status(app->ui, msg->text, true);
            }
            if(app->is_working) {
            }
        }
        break;
    }

    case MsgTypePing: {
        int len;
        if(msg->has_rssi) {
            ui_set_rssi(app->ui, msg->rssi);
        } else {
            ui_set_rssi(app->ui, 0);
        }
        /* Send hello once per connection (first received ping triggers it).
         * This was previously done in the BLE RX callback, but calling
         * ble_profile_serial_tx from inside that callback deadlocks on
         * Momentum firmware.  Sending from the GUI thread is safe. */
        if(!app->hello_sent) {
            app->hello_sent = true;
            len = protocol_build_hello(app->tx_buf, sizeof(app->tx_buf));
            transport_send(app->transport, app->tx_buf, len);
        }
        len = protocol_build_pong(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;
    }

    case MsgTypeStatus:
        app->is_working = true;
        notify_play(app->notifications, SoundLedWorking, LedStateOff); // LED-only, not subject to mute
        ui_set_pose(app->ui, PoseThinking);
        ui_show_status2(
                app->ui,
                msg->text[0] ? msg->text : "Thinking...",
                msg->text2[0] ? msg->text2 : NULL,
                true);
        break;

    case MsgTypeMenu:
        if(msg->menu_data[0]) {
            ui_update_menu(app->ui, msg->menu_data);
        }
        break;

    case MsgTypePerm: {
        bool desktop = app->current_ble_mode == BleModeDesktop;
        /* State-machine entry is idempotent — safe on every heartbeat. */
        if(desktop) {
            nus_state_on_heartbeat(
                &app->nus_state, &app->nus_stats,
                msg->hb_total, msg->hb_running, msg->hb_waiting,
                /* has_prompt */ true, msg->text, 0);
            /* Remember the prompt id for echo-back on the decision. */
            if(msg->perm_id[0]) {
                strlcpy(app->last_perm_id, msg->perm_id, sizeof(app->last_perm_id));
            }
        } else {
            notify_play(app->notifications, SoundPerm, LedStateOff);
        }
        /* Desktop wire protocol has no "always" decision — hide the
         * toggle there. Bridge keeps it. */
        ui_show_permission(app->ui, msg->text, msg->text2, /*allow_always*/ !desktop);
        break;
    }

    case MsgTypeState:
        ui_set_claude_connected(app->ui, msg->claude_connected);
        break;

    case MsgTypeAnthropicHB: {
        /* State machine handles all transitions (LED/audio/pose).  We only
         * do the status-text update here. Tokens come in via a separate
         * path because the heartbeat parser stores them on the message;
         * pass 0 for now — level-up still fires once tokens are threaded
         * through. */
        nus_state_on_heartbeat(
            &app->nus_state,
            &app->nus_stats,
            msg->hb_total,
            msg->hb_running,
            msg->hb_waiting,
            /* has_prompt: false at this point — prompt heartbeats go
             * through MsgTypePerm instead. */
            false,
            msg->text,
            /* tokens: not yet threaded through ProtocolMessage; safe to
             * pass 0 (level-up waits until we see a real count). */
            0);
        /* Desktop keeps embedding the prompt in heartbeats until answered.
         * A heartbeat without a prompt means the decision landed (ours or
         * another surface's) — forget the id so the next prompt is treated
         * as new, and drop the perm view if it's still up. */
        if(app->last_perm_id[0]) {
            app->last_perm_id[0] = '\0';
            if(app->ui->current_view == ViewIdPerm) {
                ui_back_to_status(app->ui);
            }
        }
        /* When the desktop has nothing to summarize (empty msg or the
         * literal "(no messages)"), show the heartbeat counters instead
         * so the screen isn't stuck on a useless placeholder.
         * Three lines separated by '\n' — wrap_text honors explicit
         * breaks, so the desktop status view renders them one per line:
         *   line 1: running count (or waiting / idle)
         *   line 2: cumulative tokens since Claude Desktop started
         *   line 3: today's tokens (resets at local midnight) */
        char status_line[96];
        const char* text = msg->text;
        if(!text[0] || strcmp(text, "(no messages)") == 0) {
            unsigned long session_k = msg->hb_tokens / 1000u;
            unsigned long today_k = msg->hb_tokens_today / 1000u;
            const char* first;
            char first_buf[24];
            if(msg->hb_running > 0) {
                snprintf(first_buf, sizeof(first_buf), "%d running", msg->hb_running);
                first = first_buf;
            } else if(msg->hb_waiting > 0) {
                snprintf(first_buf, sizeof(first_buf), "%d waiting", msg->hb_waiting);
                first = first_buf;
            } else {
                first = "Idle";
            }
            snprintf(status_line, sizeof(status_line),
                     "%s\nTokens: %luk\nToday: %luk",
                     first, session_k, today_k);
            text = status_line;
        }
        ui_show_status2(app->ui, text, NULL, msg->hb_total > 0);
        break;
    }

    default:
        break;
    }

    /* Anthropic protocol: cmds require an ack + some carry deferred
     * side effects (storage writes, RTC, etc.) that we moved off the
     * BLE event-callback thread for safety. */
    if(msg->nus_time_epoch > 0) {
        /* {"time":[epoch,tz]} — set local wall time in Flipper RTC. */
        DateTime dt;
        datetime_timestamp_to_datetime(
            (uint32_t)(msg->nus_time_epoch + msg->nus_time_tz), &dt);
        furi_hal_rtc_set_datetime(&dt);
    }
    if(msg->pending_ack[0]) {
        uint32_t n = msg->ack_n;
        /* Perform the cmd's deferred side effect, then build the ack. */
        if(strcmp(msg->pending_ack, "owner") == 0) {
            app_settings_set_owner_name(msg->nus_name);
        } else if(strcmp(msg->pending_ack, "name") == 0) {
            app_settings_set_device_name(msg->nus_name);
            view_dispatcher_send_custom_event(
                app->ui->view_dispatcher, CustomEventNusReadvertise);
        } else if(strcmp(msg->pending_ack, "unpair") == 0) {
            transport_nus_forget_bonds();
        } else if(strcmp(msg->pending_ack, "char_begin") == 0) {
            if(!nus_charpack_begin(msg->nus_name)) {
                /* Spec: don't ack char_begin on failure — desktop will
                 * time out and surface an error to the user. */
                return;
            }
        } else if(strcmp(msg->pending_ack, "file") == 0) {
            nus_charpack_file_open(msg->text);
        } else if(strcmp(msg->pending_ack, "chunk") == 0) {
            int32_t written = -1;
            int body_len = (int)strlen(msg->menu_data);
            if(body_len > 0) {
                written = nus_charpack_chunk_write(msg->menu_data, body_len);
            }
            if(written >= 0) n = (uint32_t)written;
        } else if(strcmp(msg->pending_ack, "file_end") == 0) {
            int32_t size = nus_charpack_file_close();
            if(size >= 0) n = (uint32_t)size;
        } else if(strcmp(msg->pending_ack, "char_end") == 0) {
            nus_charpack_end();
        }

        int len = 0;
        if(strcmp(msg->pending_ack, "status") == 0) {
            len = nus_build_status_ack(
                app->tx_buf, sizeof(app->tx_buf), "Claude",
                transport_nus_is_secure(),
                &app->nus_stats);
        } else {
            len = nus_build_ack(app->tx_buf, sizeof(app->tx_buf), msg->pending_ack, n);
        }
        if(len > 0) transport_send(app->transport, app->tx_buf, len);
    }
}

/* ── ViewDispatcher callbacks (GUI thread) ────────────────────── */

static bool on_custom_event(void* context, uint32_t event) {
    App* app = context;
    if(!app) return false;
    if(event == CustomEventSerialMsg) {
        while(furi_message_queue_get(app->serial_queue, &app->process_msg_buf, 0) == FuriStatusOk) {
            process_message(app, &app->process_msg_buf);
        }
        return true;
    }
    if(event == CustomEventNusTick) {
        nus_state_tick(&app->nus_state);
        return true;
    }
    if(event == CustomEventBtLinkDown) {
        /* Bridge-mode BT link dropped — likely the host bridge process
         * exited (session ended).  Reset hello so the next ping from a
         * freshly-started bridge retriggers the handshake and the
         * "Claude Code / Connected" notify fires again. */
        app->hello_sent = false;
        return true;
    }
    if(event == CustomEventNusReadvertise) {
        /* User asked (via cmd:name) for a new advertised name. Only a
         * BLE profile restart picks the new GAP config up; skip when
         * we're on USB — next USB-disconnect handover will pick it up. */
        if(detect_usb_cable()) return true;
        if(app->transport) {
            transport_stop(app->transport);
            transport_free(app->transport);
            app->transport = NULL;
        }
        app->transport = transport_ble_alloc(app);
        app->hello_sent = false;
        app_on_transport_mode_changed(app, true);
        transport_start(app->transport, on_serial_data, app);
        return true;
    }
    if(event == CustomEventUsbDisconnect) {
        // Stop USB poll timer first so it won't fire again
        if(app->usb_poll_timer) {
            furi_timer_stop(app->usb_poll_timer);
        }

        // Tear down USB transport
        if(app->transport) {
            transport_stop(app->transport);
            transport_free(app->transport);
            app->transport = NULL;
        }

        // Switch to BLE
        app->transport = transport_ble_alloc(app);
        app->hello_sent = false;
        app_on_transport_mode_changed(app, true);
        ui_set_transport_mode(app->ui, true);
        transport_start(app->transport, on_serial_data, app);

        notify_play(app->notifications, SoundConnect, LedStateOff);
        return true;
    }
    return false;
}

/* ── UI event callback (GUI thread — from view input handlers) ── */

static void on_ui_event(UiEventType event, const char* data, void* context) {
    App* app = context;
    if(!app) return;
    int len = 0;

    switch(event) {
    case UiEventBackspace:
        app_notify(app, SoundLedFlash);
        len = protocol_build_backspace(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventEnter:
        app_notify(app, SoundEnter);
        len = protocol_build_enter(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventEsc:
        app_notify(app, SoundEsc);
        len = protocol_build_esc(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventDown:
        app_notify(app, SoundLedFlash);
        len = protocol_build_down(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventInterrupt:
        // Long-press Left: send Ctrl+C to host (always audible, bypasses mute)
        notify_play(app->notifications, SoundAlert, LedStateOff);
        ui_set_pose(app->ui, PoseAlert);
        app->is_working = false;
        len = protocol_build_interrupt(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventToggleMute:
        // Long-press Down: toggle mute (the toggle sound itself always plays)
        app->muted = !app->muted;
        ui_set_muted(app->ui, app->muted);
        notify_play(app->notifications, app->muted ? SoundMuteOn : SoundMuteOff, LedStateOff);
        break;

    case UiEventVoice:
        // Immediate local feedback: sound + red flash before host responds.
        // If we're currently dictating (app will receive voice_stop from host),
        // play the stop sound right away; otherwise signal "requesting" dictation.
        app_notify(app, app->dictating ? SoundVoiceStop : SoundVoiceRequest);
        len = protocol_build_voice(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventSpaceHoldStart:
        if(!app->dictating) {
            notify_play(app->notifications, SoundVoiceStartLed, LedStateOff);
        }
        len = protocol_build_space_down(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventSpaceHoldEnd:
        if(!app->dictating) {
            notify_play(
                app->notifications,
                SoundVoiceStopQuiet,
                app->is_working ? LedStateWorking : LedStateOff);
        }
        len = protocol_build_space_up(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventOpenMenu:
        ui_show_menu(app->ui);
        break;

    case UiEventOpenInfo:
        ui_show_info(app->ui);
        break;

    case UiEventMenuSelect:
        if(data) {
            app_notify(app, SoundCmd);
            len = protocol_build_cmd(app->tx_buf, sizeof(app->tx_buf), data);
            transport_send(app->transport, app->tx_buf, len);
        }
        ui_back_to_status(app->ui);
        break;

    case UiEventMenuBack:
        ui_back_to_status(app->ui);
        break;

    case UiEventDismiss:
        ui_back_to_status(app->ui);
        break;

    case UiEventPermAllow:
    case UiEventPermAlways:
    case UiEventPermDeny:
    case UiEventPermEsc: {
        bool allow = (event == UiEventPermAllow || event == UiEventPermAlways);
        bool always = (event == UiEventPermAlways);
        bool esc = (event == UiEventPermEsc);
        notify_play(app->notifications, SoundLedOff, LedStateOff);
        app_notify(app, allow ? SoundSuccess : SoundEsc);

        if(app->current_ble_mode == BleModeDesktop && app->last_perm_id[0]) {
            /* Anthropic protocol only has once/deny — collapse Always/Allow
             * to "once".  Esc has no spec-defined response; reuse "deny". */
            len = nus_build_perm_decision(
                app->tx_buf, sizeof(app->tx_buf), app->last_perm_id, allow);
            FURI_LOG_D(TAG, "tx perm (len=%d): %.*s", len, len, app->tx_buf);
            /* Tally stats (persisted) + fire Heart if decision was fast. */
            nus_state_on_permission_decision(&app->nus_state, &app->nus_stats, allow);
        } else {
            len = protocol_build_perm_resp(
                app->tx_buf, sizeof(app->tx_buf), allow, always, esc);
        }
        transport_send(app->transport, app->tx_buf, len);
        ui_back_to_status(app->ui);
        break;
    }

    case UiEventYes:
        app_notify(app, SoundCmd);
        len = protocol_build_yes(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventPageUp:
        app_notify(app, SoundLedFlash);
        len = protocol_build_pgup(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventPageDown:
        app_notify(app, SoundLedFlash);
        len = protocol_build_pgdown(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventCtrlO:
        app_notify(app, SoundLedFlash);
        len = protocol_build_ctrl_o(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventCtrlE:
        app_notify(app, SoundLedFlash);
        len = protocol_build_ctrl_e(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventShiftTab:
        app_notify(app, SoundCmd);
        len = protocol_build_shift_tab(app->tx_buf, sizeof(app->tx_buf));
        transport_send(app->transport, app->tx_buf, len);
        break;

    case UiEventToggleBleMode: {
        /* UI already persisted the new setting. Desktop mode always runs
         * on BLE (USB is irrelevant there); Bridge mode picks transport
         * based on the cable. Rebuild the transport live so the user
         * doesn't have to restart the app. */
        app_notify(app, SoundCmd);
        BleMode new_mode = app_settings_get_ble_mode();
        bool want_bt = (new_mode == BleModeDesktop) || !detect_usb_cable();

        if(app->transport) {
            transport_stop(app->transport);
            transport_free(app->transport);
            app->transport = NULL;
        }
        if(app->usb_poll_timer) {
            furi_timer_stop(app->usb_poll_timer);
            furi_timer_free(app->usb_poll_timer);
            app->usb_poll_timer = NULL;
        }

        app->transport = want_bt ? transport_ble_alloc(app) : transport_usb_alloc();
        app->hello_sent = false;
        app_on_transport_mode_changed(app, want_bt);
        ui_set_transport_mode(app->ui, want_bt);
        /* Clear any stale status text from the previous mode (e.g. a
         * Bridge "Turn complete" lingering into Desktop, or a Desktop
         * heartbeat msg lingering into Bridge) before the new transport
         * has a chance to repaint. */
        ui_show_status(app->ui, NULL, false);
        transport_start(app->transport, on_serial_data, app);

        if(!want_bt) {
            /* Poll for USB removal so we auto-swap back to BLE. */
            app->usb_poll_timer = furi_timer_alloc(usb_poll_cb, FuriTimerTypePeriodic, app);
            furi_timer_start(app->usb_poll_timer, 5000);
        }
        break;
    }

    case UiEventExitApp:
        ui_stop(app->ui);
        break;
    }
}

/* ── App entry point ──────────────────────────────────────────── */

int32_t claude_buddy_app(void* p) {
    UNUSED(p);


    App* app = malloc(sizeof(App));
    furi_check(app != NULL);
    memset(app, 0, sizeof(App));

    Gui* gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* Serial → GUI message queue (allocates memory internally to copy ProtocolMessage by value) */
    app->serial_queue = furi_message_queue_alloc(SERIAL_QUEUE_SIZE, sizeof(ProtocolMessage));

    /* UI */
    app->ui = ui_alloc(gui);
    ui_set_event_callback(app->ui, on_ui_event, app);

    /* Route custom events from serial queue to the GUI thread */
    view_dispatcher_set_event_callback_context(app->ui->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->ui->view_dispatcher, on_custom_event);

    ui_show_status(app->ui, NULL, false);

    /* Init the Desktop-mode state machine + load persisted stats. The
     * state machine only runs while current_ble_mode == Desktop; init
     * here is cheap. */
    nus_state_init(&app->nus_state, app->notifications, app->ui, &app->is_working);
    nus_stats_load(&app->nus_stats);
    nus_transcript_init();
    nus_charpack_init();

    /* Auto-select transport: USB if cable is plugged in, BT otherwise.
     * Desktop mode overrides — it talks directly to Claude Desktop over
     * BLE and the USB/host-bridge path is never useful, so stay on BLE
     * regardless of cable state. */
    bool use_bt = !detect_usb_cable() ||
                  app_settings_get_ble_mode() == BleModeDesktop;
    app->transport = use_bt ? transport_ble_alloc(app) : transport_usb_alloc();
    app_on_transport_mode_changed(app, use_bt);
    ui_set_transport_mode(app->ui, use_bt);

    /* In USB mode, poll every 5 s for cable removal and auto-switch to BLE */
    if(!use_bt) {
        app->usb_poll_timer = furi_timer_alloc(usb_poll_cb, FuriTimerTypePeriodic, app);
        furi_timer_start(app->usb_poll_timer, 5000);
    }

    transport_start(app->transport, on_serial_data, app);

    /* Hello + connect sound */
    char buf[128];
    int len = protocol_build_hello(buf, sizeof(buf));
    transport_send(app->transport, buf, len);
    notify_play(app->notifications, SoundConnect, LedStateOff);
    ui_show_status(app->ui, NULL, true);

    /* Run event loop (blocks until ui_stop) */
    ui_run(app->ui);

    /* Cleanup */
    notify_play(app->notifications, SoundDisconnect, LedStateOff);
    furi_delay_ms(500);

    if(app->usb_poll_timer) {
        furi_timer_stop(app->usb_poll_timer);
        furi_timer_free(app->usb_poll_timer);
    }
    nus_tick_stop(app);

    transport_stop(app->transport);
    transport_free(app->transport);
    furi_message_queue_free(app->serial_queue);
    nus_charpack_free();
    nus_transcript_free();
    ui_free(app->ui);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);

    return 0;
}
