#include "keymap.h"

#include <stdlib.h>
#include <string.h>

/* The firmware's fast advertising lasts 60 s (gap.c:430-435), and stopping sooner than the 30 s
 * Bluetooth gives pairing, plus the walk to the Mac's settings, cuts the link just paired. */
static const uint32_t keymap_link_wait_ms = 60000;

struct Keymap {
    Mode mode;
    bool pressed[KeymapKeyMAX];
    Link link;
    bool legend;
    KeymapWait wait;
    bool options[OptionMAX];
    Option cursor;
};

static const Action keymap_actions[KeymapKeyMAX][KeymapEventMAX] = {
    [KeymapKeyUp] = {[KeymapEventShort] = ActionArrowUp, [KeymapEventLong] = ActionEffortHigh},
    [KeymapKeyDown] = {[KeymapEventShort] = ActionArrowDown, [KeymapEventLong] = ActionEffortMax},
    [KeymapKeyLeft] = {[KeymapEventShort] = ActionArrowLeft, [KeymapEventLong] = ActionClear},
    [KeymapKeyRight] = {[KeymapEventShort] = ActionArrowRight, [KeymapEventLong] = ActionEsc},
    [KeymapKeyOk] = {[KeymapEventShort] = ActionEnter, [KeymapEventLong] = ActionVoiceStart},
    [KeymapKeyBack] = {[KeymapEventLong] = ActionExit},
};

static const Link keymap_other_links[LinkMAX] = {
    [LinkBluetooth] = LinkUsb,
    [LinkUsb] = LinkBluetooth,
};

static const Option keymap_other_options[OptionMAX] = {
    [OptionVibration] = OptionBacklight,
    [OptionBacklight] = OptionVibration,
};

static uint32_t keymap_mode_wait_ms(Mode mode) {
    switch(mode) {
    case ModeNoLink:
    case ModeVoiceCut:
    case ModeOptionsNoLink:
        return keymap_link_wait_ms;
    case ModeMain:
    case ModeVoice:
    case ModeOptions:
    case ModeStopped:
        break;
    }
    return 0;
}

static KeymapWait keymap_wait_entering(const Keymap* keymap, Mode mode, Link link) {
    bool waited = keymap_mode_wait_ms(keymap->mode) > 0;
    bool waits = keymap_mode_wait_ms(mode) > 0;

    if(waits && (!waited || link != keymap->link)) {
        return KeymapWaitStart;
    }
    if(waited && !waits) {
        return KeymapWaitStop;
    }
    return KeymapWaitKeep;
}

static void keymap_forget_presses(Keymap* keymap) {
    memset(keymap->pressed, 0, sizeof(keymap->pressed));
}

static void keymap_set_screen(Keymap* keymap, Mode mode, Link link) {
    if(mode != keymap->mode || link != keymap->link) {
        keymap->wait = keymap_wait_entering(keymap, mode, link);
        keymap->mode = mode;
        keymap->link = link;
        keymap_forget_presses(keymap);
    }
}

static void keymap_set_mode(Keymap* keymap, Mode mode) {
    keymap_set_screen(keymap, mode, keymap->link);
}

/* Each option has two values and the options two rows, so either key gives the other. */
static void keymap_tap_options(Keymap* keymap, KeymapKey key, Mode closed) {
    if(key == KeymapKeyBack) {
        keymap_set_mode(keymap, closed);
    } else if(key == KeymapKeyUp || key == KeymapKeyDown) {
        keymap->cursor = keymap_other_options[keymap->cursor];
    } else if(key == KeymapKeyLeft || key == KeymapKeyRight) {
        keymap->options[keymap->cursor] = !keymap->options[keymap->cursor];
    }
}

Keymap* keymap_alloc(void) {
    Keymap* keymap = malloc(sizeof(Keymap));
    keymap->mode = ModeNoLink;
    keymap_forget_presses(keymap);
    keymap->link = LinkBluetooth;
    keymap->legend = true;
    keymap->wait = KeymapWaitStart;
    for(Option option = OptionVibration; option < OptionMAX; option++) {
        keymap->options[option] = true;
    }
    keymap->cursor = OptionVibration;
    return keymap;
}

void keymap_free(Keymap* keymap) {
    free(keymap);
}

Action keymap_handle(Keymap* keymap, KeymapKey key, KeymapEvent event) {
    keymap->wait = KeymapWaitKeep;

    bool ok_released = key == KeymapKeyOk && event == KeymapEventRelease;
    bool gesture = event == KeymapEventShort || event == KeymapEventLong;
    Action action = keymap_actions[key][event];

    if(event == KeymapEventPress) {
        keymap->pressed[key] = true;
    } else if(event == KeymapEventRelease) {
        keymap->pressed[key] = false;
    }

    /* The long press fires 300 ms after the press and the tap comes with the release, so a gesture
     * begun on another screen would act here on a press meant for that one: starting over, then
     * Enter or voice on the main screen. Back's hold only leaves, which every screen but voice
     * allows. */
    if(gesture && !keymap->pressed[key] && action != ActionExit) {
        return ActionNone;
    }

    switch(keymap->mode) {
    case ModeMain:
        if(event == KeymapEventPress) {
            keymap->legend = false;
        }
        if(action == ActionVoiceStart) {
            keymap_set_mode(keymap, ModeVoice);
        } else if(key == KeymapKeyBack && event == KeymapEventShort) {
            keymap_set_mode(keymap, ModeOptions);
        }
        return action;
    case ModeVoice:
        if(!ok_released) {
            return ActionNone;
        }
        keymap_set_mode(keymap, ModeMain);
        return ActionVoiceStop;
    case ModeOptions:
        if(event == KeymapEventShort) {
            keymap_tap_options(keymap, key, ModeMain);
        }
        break;
    case ModeVoiceCut:
        if(ok_released) {
            keymap_set_mode(keymap, ModeNoLink);
        }
        break;
    case ModeNoLink:
        if(key == KeymapKeyRight && event == KeymapEventLong) {
            keymap_set_screen(keymap, ModeNoLink, keymap_other_links[keymap->link]);
        }
        break;
    case ModeOptionsNoLink:
        if(event == KeymapEventShort) {
            keymap_tap_options(keymap, key, ModeNoLink);
        }
        break;
    case ModeStopped:
        if(event != KeymapEventPress) {
            break;
        }
        if(key == KeymapKeyBack) {
            return ActionExit;
        }
        keymap_set_mode(keymap, ModeNoLink);
        return ActionStartOver;
    }

    return action == ActionExit ? ActionExit : ActionNone;
}

/* ModeStopped stays: stopping waits for the radio to go idle (furi_hal_bt.c:261-267), and stopping
 * reports a link down itself (bt.c:312), so a report read there is stale. */
Action keymap_set_connected(Keymap* keymap, bool connected) {
    keymap->wait = KeymapWaitKeep;

    switch(keymap->mode) {
    case ModeMain:
        if(!connected) {
            keymap_set_mode(keymap, ModeNoLink);
        }
        break;
    case ModeVoice:
        if(!connected) {
            keymap_set_mode(keymap, ModeVoiceCut);
            return ActionVoiceStop;
        }
        break;
    case ModeOptions:
        if(!connected) {
            keymap_set_mode(keymap, ModeOptionsNoLink);
        }
        break;
    case ModeNoLink:
    case ModeVoiceCut:
        if(connected) {
            keymap_set_mode(keymap, ModeMain);
        }
        break;
    case ModeOptionsNoLink:
        if(connected) {
            keymap_set_mode(keymap, ModeOptions);
        }
        break;
    case ModeStopped:
        break;
    }
    return ActionNone;
}

Action keymap_time_out(Keymap* keymap) {
    keymap->wait = KeymapWaitKeep;

    if(keymap_wait_ms(keymap) == 0) {
        return ActionNone;
    }
    keymap_set_mode(keymap, ModeStopped);
    return ActionGiveUp;
}

void keymap_set_option(Keymap* keymap, Option option, bool on) {
    keymap->options[option] = on;
}

KeymapWait keymap_wait(const Keymap* keymap) {
    return keymap->wait;
}

uint32_t keymap_wait_ms(const Keymap* keymap) {
    return keymap_mode_wait_ms(keymap->mode);
}

Mode keymap_mode(const Keymap* keymap) {
    return keymap->mode;
}

Link keymap_link(const Keymap* keymap) {
    return keymap->link;
}

bool keymap_legend(const Keymap* keymap) {
    return keymap->legend;
}

bool keymap_option(const Keymap* keymap, Option option) {
    return keymap->options[option];
}

Option keymap_cursor(const Keymap* keymap) {
    return keymap->cursor;
}
