#include "keymap.h"

#include <stdlib.h>

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

static const Mode keymap_link_up_modes[] = {
    [ModeIdle] = ModeIdle,
    [ModeVoice] = ModeVoice,
    [ModeNoLink] = ModeIdle,
    [ModeVoiceCut] = ModeIdle,
};

static const Mode keymap_link_down_modes[] = {
    [ModeIdle] = ModeNoLink,
    [ModeVoice] = ModeVoiceCut,
    [ModeNoLink] = ModeNoLink,
    [ModeVoiceCut] = ModeVoiceCut,
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
    }

    return action == ActionExit ? ActionExit : ActionNone;
}

Action keymap_set_connected(Keymap* keymap, bool connected) {
    Mode previous = keymap->mode;
    keymap->mode = connected ? keymap_link_up_modes[previous] : keymap_link_down_modes[previous];
    return previous == ModeVoice && keymap->mode == ModeVoiceCut ? ActionVoiceStop : ActionNone;
}

Mode keymap_mode(const Keymap* keymap) {
    return keymap->mode;
}
