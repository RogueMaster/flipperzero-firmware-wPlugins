#include "keymap.h"

#include <stdlib.h>

/* The firmware's fast advertising lasts 60 s (gap.c:430-435), and stopping sooner than the 30 s
 * Bluetooth gives pairing, plus the walk to the Mac's settings, cuts the link just paired. */
static const uint32_t keymap_link_wait_ms = 60000;

struct Keymap {
    Mode mode;
};

/* Esc acts on press: past the long press a slow one would otherwise do nothing. */
static const Action keymap_actions[KeymapKeyMAX][KeymapEventMAX] = {
    [KeymapKeyUp] = {[KeymapEventShort] = ActionArrowUp, [KeymapEventLong] = ActionEffortHigh},
    [KeymapKeyDown] = {[KeymapEventShort] = ActionArrowDown, [KeymapEventLong] = ActionEffortMax},
    [KeymapKeyLeft] = {[KeymapEventLong] = ActionClear},
    [KeymapKeyRight] = {[KeymapEventPress] = ActionEsc},
    [KeymapKeyOk] = {[KeymapEventShort] = ActionEnter, [KeymapEventLong] = ActionVoiceStart},
    [KeymapKeyBack] = {[KeymapEventLong] = ActionExit},
};

/* ModeStopped stays: stopping waits for the radio to go idle (furi_hal_bt.c:261-267), and stopping
 * reports a link down itself (bt.c:312), so a report read there is stale. */
static const Mode keymap_link_up_modes[] = {
    [ModeIdle] = ModeIdle,
    [ModeVoice] = ModeVoice,
    [ModeNoLink] = ModeIdle,
    [ModeVoiceCut] = ModeIdle,
    [ModeStopped] = ModeStopped,
};

static const Mode keymap_link_down_modes[] = {
    [ModeIdle] = ModeNoLink,
    [ModeVoice] = ModeVoiceCut,
    [ModeNoLink] = ModeNoLink,
    [ModeVoiceCut] = ModeVoiceCut,
    [ModeStopped] = ModeStopped,
};

Keymap* keymap_alloc(void) {
    Keymap* keymap = malloc(sizeof(Keymap));
    keymap->mode = ModeNoLink;
    return keymap;
}

void keymap_free(Keymap* keymap) {
    free(keymap);
}

Action keymap_handle(Keymap* keymap, KeymapKey key, KeymapEvent event) {
    bool ok_released = key == KeymapKeyOk && event == KeymapEventRelease;
    Action action = keymap_actions[key][event];

    switch(keymap->mode) {
    case ModeIdle:
        if(action == ActionVoiceStart) {
            keymap->mode = ModeVoice;
        }
        return action;
    case ModeVoice:
        if(!ok_released) {
            return ActionNone;
        }
        keymap->mode = ModeIdle;
        return ActionVoiceStop;
    case ModeVoiceCut:
        if(ok_released) {
            keymap->mode = ModeNoLink;
        }
        break;
    case ModeNoLink:
        break;
    case ModeStopped:
        if(event != KeymapEventPress) {
            break;
        }
        if(key == KeymapKeyBack) {
            return ActionExit;
        }
        keymap->mode = ModeNoLink;
        return ActionStartOver;
    }

    return action == ActionExit ? ActionExit : ActionNone;
}

Action keymap_set_connected(Keymap* keymap, bool connected) {
    Mode previous = keymap->mode;
    keymap->mode = connected ? keymap_link_up_modes[previous] : keymap_link_down_modes[previous];
    return previous == ModeVoice && keymap->mode == ModeVoiceCut ? ActionVoiceStop : ActionNone;
}

Action keymap_time_out(Keymap* keymap) {
    if(keymap_wait_ms(keymap) == 0) {
        return ActionNone;
    }
    keymap->mode = ModeStopped;
    return ActionGiveUp;
}

uint32_t keymap_wait_ms(const Keymap* keymap) {
    return keymap->mode == ModeNoLink || keymap->mode == ModeVoiceCut ? keymap_link_wait_ms : 0;
}

Mode keymap_mode(const Keymap* keymap) {
    return keymap->mode;
}
