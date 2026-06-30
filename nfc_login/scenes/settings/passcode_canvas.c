#include "passcode_canvas.h"
#include "../../crypto/nfc_login_passcode.h"
#include "../scene_manager.h"
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <furi_hal_resources.h>

#define PASSCODE_DOT_SIZE 6
#define PASSCODE_DOT_SPACING 10
#define PASSCODE_DOT_Y 28

// Calculate start_x - use fixed left position for simplicity
static int calculate_start_x(size_t button_count) {
    (void)button_count; // Not used, but kept for API compatibility
    // Start from left with minimal margin
    return 3;
}

// Store app pointer statically to ensure it's always available
static App* g_app_context = NULL;

static void passcode_canvas_draw_callback(Canvas* canvas, void* context) {
    App* app = (App*)context;
    
    // Fallback to static context if context is null
    if(!app) {
        app = g_app_context;
    }
    
    if(!app || !canvas) {
        if(canvas) {
            canvas_clear(canvas);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 0, 11, "Error: No context");
        }
        return;
    }
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    
    // Determine if we're in lockscreen (state 7) or setup (state 6)
    bool is_lockscreen = (app->widget_state == 7);
    bool is_setup = (app->widget_state == 6);
    
    // If state is unclear but passcode prompt is active, default to setup
    if(!is_lockscreen && !is_setup) {
        if(app->passcode_prompt_active) {
            is_setup = true;
            app->widget_state = 6; // Set state to match
        } else {
            // Show default message - always draw something
            canvas_draw_str(canvas, 0, 11, "Passcode");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 0, 24, "Initializing...");
            return;
        }
    }
    
    if(is_lockscreen) {
        // Show failed attempts first if any (overwrites title area)
        if(app->passcode_failed_attempts > 0) {
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "Wrong! (%u/5 attempts)", app->passcode_failed_attempts);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 0, 10, error_msg);
        } else {
            canvas_draw_str(canvas, 0, 10, "Enter Passcode");
        }
        
        // Get stored sequence to know how many buttons to expect
        char stored_sequence[MAX_PASSCODE_SEQUENCE_LEN];
        bool has_stored = get_passcode_sequence(stored_sequence, sizeof(stored_sequence));
        
        if(!has_stored) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 0, 24, "Error: No stored sequence");
            return;
        }
        
        // Count buttons in stored sequence
        size_t stored_button_count = count_buttons_in_sequence(stored_sequence);
        
        // Count buttons in current input
        size_t input_button_count = count_buttons_in_sequence(app->passcode_sequence);
        
        // Draw passcode indicators (centered)
        int start_x = calculate_start_x(stored_button_count);
        for(size_t i = 0; i < stored_button_count; i++) {
            int x = start_x + (i * PASSCODE_DOT_SPACING);
            int y = PASSCODE_DOT_Y;
            
            if(i < input_button_count) {
                // Filled circle for entered buttons
                canvas_draw_disc(canvas, x, y, PASSCODE_DOT_SIZE / 2);
            } else {
                // Empty circle for remaining buttons
                canvas_draw_circle(canvas, x, y, PASSCODE_DOT_SIZE / 2);
            }
        }
        
        // Show progress text
        canvas_set_font(canvas, FontSecondary);
        char progress[32];
        snprintf(progress, sizeof(progress), "%zu / %zu", input_button_count, stored_button_count);
        canvas_draw_str(canvas, 0, 40, progress);
        
        if(input_button_count >= stored_button_count) {
            canvas_draw_str(canvas, 0, 52, "Press OK to verify");
        } else {
            canvas_draw_str(canvas, 0, 52, "Enter passcode...");
        }
    } else if(is_setup) {
        canvas_draw_str(canvas, 0, 10, "Setup Passcode");
        
        // Count buttons in current input
        size_t input_button_count = count_buttons_in_sequence(app->passcode_sequence);
        
        // Show button count
        char count_msg[32];
        snprintf(count_msg, sizeof(count_msg), "%zu/%d-%d buttons", input_button_count, MIN_PASSCODE_BUTTONS, MAX_PASSCODE_BUTTONS);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 20, count_msg);
        
        // Draw passcode indicators (up to MAX_PASSCODE_BUTTONS, centered)
        int start_x = calculate_start_x(MAX_PASSCODE_BUTTONS);
        for(size_t i = 0; i < MAX_PASSCODE_BUTTONS; i++) {
            int x = start_x + (i * PASSCODE_DOT_SPACING);
            int y = PASSCODE_DOT_Y;
            
            if(i < input_button_count) {
                // Filled circle for entered buttons
                canvas_draw_disc(canvas, x, y, PASSCODE_DOT_SIZE / 2);
            } else {
                // Empty circle for remaining buttons
                canvas_draw_circle(canvas, x, y, PASSCODE_DOT_SIZE / 2);
            }
        }
        
        // Show instruction
        if(input_button_count >= MIN_PASSCODE_BUTTONS && input_button_count <= MAX_PASSCODE_BUTTONS) {
            canvas_draw_str(canvas, 0, 40, "Press OK when done");
        } else {
            canvas_draw_str(canvas, 0, 40, "Need 4-8 buttons");
        }
    } else {
        // Fallback: show something if state is unclear
        canvas_draw_str(canvas, 0, 11, "Passcode");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 24, "Press buttons");
    }
}

static bool passcode_canvas_input_callback(InputEvent* event, void* context) {
    // Forward input to widget view input handler
    App* app = context;
    if(!app) {
        app = g_app_context;
    }
    if(!app || !event) {
        return false;
    }
    
    // Process input
    bool handled = app_widget_view_input_handler(event, app);
    
    // Force view to redraw by switching to it again (triggers redraw)
    // But only if we're still in lockscreen or setup state
    if(handled && (app->widget_state == 6 || app->widget_state == 7)) {
        app_switch_to_view(app, ViewPasscodeCanvas);
    }
    
    return handled;
}

View* passcode_canvas_view_alloc(App* app) {
    if(!app) {
        return NULL;
    }
    
    // Store app pointer statically
    g_app_context = app;
    
    View* view = view_alloc();
    if(!view) {
        g_app_context = NULL;
        return NULL;
    }
    view_set_context(view, app);
    view_set_draw_callback(view, passcode_canvas_draw_callback);
    view_set_input_callback(view, passcode_canvas_input_callback);
    return view;
}

void passcode_canvas_view_free(View* view) {
    g_app_context = NULL;
    view_free(view);
}

