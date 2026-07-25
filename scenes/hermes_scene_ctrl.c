#include "../hermes_i.h"

/* The keys you actually reach for on a console the Flipper's keyboard cannot
 * type: interrupt a boot, end a session, stop a pager. */
typedef struct {
    const char* label;
    uint8_t byte;
} HermesCtrlKey;

static const HermesCtrlKey hermes_ctrl_keys[] = {
    {"Ctrl+C  break", 0x03},
    {"Ctrl+D  EOF", 0x04},
    {"Ctrl+Z  suspend", 0x1A},
    {"Ctrl+X", 0x18},
    {"Esc", 0x1B},
    {"Tab  complete", 0x09},
    {"Backspace", 0x08},
    {"Space", 0x20},
};
#define HERMES_CTRL_KEY_COUNT (sizeof(hermes_ctrl_keys) / sizeof(hermes_ctrl_keys[0]))

/* "Hit any key to stop autoboot" gives you a second or two, and doing it by
 * hand means watching for a prompt that has already gone. This hammers the key
 * across the whole window instead. */

/** Arm the burst and let the console tick actually send it.
 *
 * Spinning here with furi_delay_ms would block the GUI thread for the whole
 * window: no redraw, no input, an app that looks hung at exactly the moment
 * the user is watching for boot output.
 */
static void hermes_ctrl_arm_autoboot(HermesApp* app) {
    session_log_note(app->log, "autoboot interrupt");
    app->autoboot_until = furi_get_tick() + furi_ms_to_ticks(HERMES_AUTOBOOT_MS);
}

/* Step the live link to the next/previous standard rate without leaving the
 * console - for when detection landed one row off, or you want to sweep
 * neighbours by eye. Re-opens the tap at the new rate, keeping framing, TX,
 * logging and the armed watch. */
static void hermes_ctrl_nudge_baud(HermesApp* app, int dir) {
    size_t idx = hermes_baud_nearest_index(app->link.baud);
    if(dir > 0 && idx + 1 < HERMES_BAUD_COUNT) {
        idx++;
    } else if(dir < 0 && idx > 0) {
        idx--;
    } else {
        return; // already at an end of the table
    }

    app->link.baud = hermes_baud_table[idx].baud;
    app->link.verified = false; // nudged by hand, no longer a measured result

    uart_tap_close(app->tap);
    uart_tap_open(app->tap, app->port, app->link.baud, app->link.framing, app->tx_enabled);

    char note[32];
    snprintf(note, sizeof(note), "baud -> %lu", (unsigned long)app->link.baud);
    session_log_note(app->log, note);
}

/* The actions below are not single bytes, so they live past the key table. */
typedef enum {
    HermesCtrlActionAutoboot = HERMES_CTRL_KEY_COUNT,
    HermesCtrlActionBreak,
    HermesCtrlActionWatch,
    HermesCtrlActionScript,
    HermesCtrlActionHex,
    HermesCtrlActionMarker,
    HermesCtrlActionBaudUp,
    HermesCtrlActionBaudDown,

    HermesCtrlActionCount,
} HermesCtrlAction;

static void hermes_scene_ctrl_callback(void* context, uint32_t index) {
    HermesApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void hermes_scene_ctrl_on_enter(void* context) {
    HermesApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, uart_tap_tx_enabled(app->tap) ? "Send key" : "TX is off");

    for(uint32_t i = 0; i < HERMES_CTRL_KEY_COUNT; i++) {
        submenu_add_item(
            submenu, hermes_ctrl_keys[i].label, i, hermes_scene_ctrl_callback, app);
    }
    submenu_add_item(
        submenu, "Stop autoboot (4s)", HermesCtrlActionAutoboot, hermes_scene_ctrl_callback, app);
    submenu_add_item(
        submenu, "Send break", HermesCtrlActionBreak, hermes_scene_ctrl_callback, app);
    submenu_add_item(
        submenu,
        trigger_is_armed(app->trigger) ? "Watch for... (armed)" : "Watch for...",
        HermesCtrlActionWatch,
        hermes_scene_ctrl_callback,
        app);
    submenu_add_item(
        submenu, "Run script...", HermesCtrlActionScript, hermes_scene_ctrl_callback, app);
    submenu_add_item(
        submenu, "Send hex...", HermesCtrlActionHex, hermes_scene_ctrl_callback, app);
    submenu_add_item(
        submenu, "Baud +", HermesCtrlActionBaudUp, hermes_scene_ctrl_callback, app);
    submenu_add_item(
        submenu, "Baud -", HermesCtrlActionBaudDown, hermes_scene_ctrl_callback, app);
    if(session_log_is_open(app->log)) {
        submenu_add_item(
            submenu, "Drop marker", HermesCtrlActionMarker, hermes_scene_ctrl_callback, app);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, HermesSceneCtrl));

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewSubmenu);
}

bool hermes_scene_ctrl_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event >= HermesCtrlActionCount) return false;

    scene_manager_set_scene_state(app->scene_manager, HermesSceneCtrl, event.event);

    switch(event.event) {
    case HermesCtrlActionAutoboot:
        hermes_ctrl_arm_autoboot(app);
        break;

    case HermesCtrlActionBreak:
        session_log_note(app->log, "break sent");
        uart_tap_send_break(app->tap);
        break;

    /* These open a picker, so they hand off rather than returning to the
     * console themselves. */
    case HermesCtrlActionWatch:
        scene_manager_next_scene(app->scene_manager, HermesSceneWatch);
        return true;

    case HermesCtrlActionScript:
        scene_manager_next_scene(app->scene_manager, HermesSceneScript);
        return true;

    case HermesCtrlActionHex:
        scene_manager_next_scene(app->scene_manager, HermesSceneHexSend);
        return true;

    case HermesCtrlActionMarker: {
        char note[24];
        snprintf(note, sizeof(note), "marker %lu", (unsigned long)(++app->marker_count));
        session_log_note(app->log, note);
        break;
    }

    case HermesCtrlActionBaudUp:
        hermes_ctrl_nudge_baud(app, +1);
        break;

    case HermesCtrlActionBaudDown:
        hermes_ctrl_nudge_baud(app, -1);
        break;

    default: {
        const uint8_t byte = hermes_ctrl_keys[event.event].byte;
        uart_tap_send_byte(app->tap, byte);
        if(app->local_echo) term_feed_echo(app->term, byte);
        break;
    }
    }

    hermes_notify_blip(app);
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void hermes_scene_ctrl_on_exit(void* context) {
    HermesApp* app = context;
    submenu_reset(app->submenu);
}
