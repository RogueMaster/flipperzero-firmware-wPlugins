#pragma once

#include <furi/core/string.h>
#include <input/input.h>

#include "desktop.h"

typedef enum {
    DesktopKeybindTypePress,
    DesktopKeybindTypeHold,
    DesktopKeybindTypeMAX,
} DesktopKeybindType;

typedef enum {
    DesktopKeybindKeyUp,
    DesktopKeybindKeyDown,
    DesktopKeybindKeyRight,
    DesktopKeybindKeyLeft,
    DesktopKeybindKeyOk,
    DesktopKeybindKeyMAX,
} DesktopKeybindKey;

typedef FuriString* DesktopKeybinds[DesktopKeybindTypeMAX][DesktopKeybindKeyMAX];

void desktop_keybinds_migrate(Desktop* desktop);
void desktop_keybinds_load(Desktop* desktop, DesktopKeybinds* keybinds);
void desktop_keybinds_save(Desktop* desktop, const DesktopKeybinds* keybinds);
void desktop_keybinds_load_profile(Desktop* desktop, DesktopKeybinds* keybinds, bool game_mode);
void desktop_keybinds_save_profile(
    Desktop* desktop,
    const DesktopKeybinds* keybinds,
    bool game_mode);
void desktop_keybinds_free(DesktopKeybinds* keybinds);
void desktop_run_keybind(Desktop* desktop, InputType _type, InputKey _key);
