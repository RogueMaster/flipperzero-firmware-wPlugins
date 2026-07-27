#pragma once

#include <stdint.h>

#include "../nfc_magic_app.h"
#include <views/write_problems.h>

// Shared event handler for the two write-check scenes (Gen2 and MFC). They differ only in which
// menu Left/"Retry" returns to, so the body lives here and each scene passes its own exit menu.
// Centralizing removes the copy-paste divergence that once made the Gen2 scene's Left a silent
// no-op (it targeted the Classic menu, which isn't on the Gen2 navigation stack).
void nfc_magic_write_check_handle_event(
    NfcMagicApp* instance,
    WriteProblemsEvent event,
    uint32_t exit_menu_scene);
