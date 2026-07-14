/*
 * Purpose: Expose the Flipper external app entry point.
 * Owns: boot-run-shutdown handoff for the app dispatcher.
 * Depends on: morse_flipper_app_i.h and Flipper app loader.
 * Tests: firmware build; execution is hardware-only.
 */

#include "morse_flipper_app_i.h"

int32_t morse_flipper_fap(void* p) {
    MorseFlipperApp* app;
    UNUSED(p);

    app = morse_flipper_boot();
    if(app == NULL) return 1;

    view_dispatcher_run(morse_flipper_view_dispatcher_get(app));
    morse_flipper_shutdown(app);
    return 0;
}
