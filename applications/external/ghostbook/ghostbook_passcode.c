/*
 * GhostBook v0.6.0
 * Secure encrypted contacts for Flipper Zero
 * 
 * Original Author: Digi
 * Created: December 2025
 * License: MIT
 * 
 * If you fork this, keep this header intact.
 * https://github.com/digitard/ghostbook
 */

#include "ghostbook.h"

// ============================================================================
// PASSCODE VIEW - Custom view for 6-button passcode entry
// ============================================================================

typedef struct {
    GhostApp* app;
    const char* header;
    bool show_attempts;
} PasscodeViewModel;

static void passcode_draw_cb(Canvas* canvas, void* model) {
    PasscodeViewModel* m = model;
    GhostApp* app = m->app;

    // Determine the passcode length to use
    uint8_t len = app->auth.initialized ? app->auth.passcode_length : app->setup_length;
    if(len < MIN_PASSCODE_LENGTH) len = DEFAULT_PASSCODE_LENGTH;

    canvas_clear(canvas);

    // Header
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, m->header);

    // Draw entered buttons
    canvas_set_font(canvas, FontSecondary);

    char code_display[32];
    for(uint8_t i = 0; i < len; i++) {
        code_display[i * 2] = (i < app->passcode_pos) ? '*' : '_';
        code_display[i * 2 + 1] = ' ';
    }
    code_display[len * 2] = '\0';
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, code_display);

    // Show which buttons
    canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignTop, "UP DOWN LEFT RIGHT OK BACK");

    // Show attempts warning if applicable
    if(m->show_attempts && app->auth.failed_attempts > 0) {
        char warn[48];
        uint8_t remaining = app->auth.max_attempts - app->auth.failed_attempts;
        snprintf(warn, sizeof(warn), "!! %d attempts left !!", remaining);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, warn);
    } else {
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignTop, "Enter code");
    }
}

static bool passcode_input_cb(InputEvent* event, void* ctx) {
    GhostApp* app = ctx;

    if(event->type != InputTypeShort) return false;

    PasscodeButton btn;
    switch(event->key) {
    case InputKeyUp:
        btn = PasscodeUp;
        break;
    case InputKeyDown:
        btn = PasscodeDown;
        break;
    case InputKeyLeft:
        btn = PasscodeLeft;
        break;
    case InputKeyRight:
        btn = PasscodeRight;
        break;
    case InputKeyOk:
        btn = PasscodeOk;
        break;
    case InputKeyBack:
        btn = PasscodeBack;
        break;
    default:
        return false;
    }

    // Determine the passcode length to use
    uint8_t len = app->auth.initialized ? app->auth.passcode_length : app->setup_length;
    if(len < MIN_PASSCODE_LENGTH) len = DEFAULT_PASSCODE_LENGTH;

    // Add to input
    if(app->passcode_pos < len) {
        app->passcode_input[app->passcode_pos++] = btn;
        notification_message(app->notif, &sequence_blink_cyan_10);

        // Trigger view redraw by touching the model
        with_view_model(app->passcode_view, PasscodeViewModel * m, { UNUSED(m); }, true);

        // Check if complete
        if(app->passcode_pos == len) {
            view_dispatcher_send_custom_event(app->view, 100); // GhostCustomEventPasscodeComplete
        }
    }

    return true;
}

View* ghost_passcode_view_alloc(GhostApp* app) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(PasscodeViewModel));
    view_set_draw_callback(view, passcode_draw_cb);
    view_set_input_callback(view, passcode_input_cb);
    view_set_context(view, app);

    with_view_model(
        view,
        PasscodeViewModel * m,
        {
            m->app = app;
            m->header = "UNLOCK";
            m->show_attempts = true;
        },
        true);

    return view;
}

void ghost_passcode_view_free(View* view) {
    view_free(view);
}

void ghost_passcode_view_reset(GhostApp* app) {
    app->passcode_pos = 0;
    memset(app->passcode_input, 0, MAX_PASSCODE_LENGTH);
}

void ghost_passcode_view_set_mode(GhostApp* app, const char* header, bool show_attempts) {
    with_view_model(
        app->passcode_view,
        PasscodeViewModel * m,
        {
            m->header = header;
            m->show_attempts = show_attempts;
        },
        true);
}
