#include "flipper_printer.h"
#include <gui/elements.h>

// Global app reference as a workaround for context issues
static FlipperPrinterApp* g_app = NULL;

// Notification sequence for successful flip
static const NotificationSequence sequence_coin_flip = {
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

// Generate random coin flip result
static bool coin_flip_random(void) {
    return (furi_get_tick() % 2) == 0;
}

// Draw callback for coin flip view
static void coin_flip_view_draw_callback(Canvas* canvas, void* context) {
    if(!canvas) {
        FURI_LOG_E(TAG, "Canvas is NULL in draw callback");
        return;
    }

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    FURI_LOG_I(TAG, "Draw callback called with context: %p, g_app: %p", context, g_app);

    // Use global app reference if context is null
    FlipperPrinterApp* app = context ? (FlipperPrinterApp*)context : g_app;

    if(!app) {
        FURI_LOG_E(TAG, "Both context and g_app are NULL in draw callback");
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "No App Available");
        return;
    }

    // Title
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Coin Flip Game");

    if(app->total_flips == 0) {
        // Initial state - show instructions
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignTop, "Press OK to flip!");
    } else {
        // Show last result - big and centered
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(
            canvas, 64, 18, AlignCenter, AlignTop, app->last_flip_was_heads ? "HEADS" : "TAILS");

        // Simple stats - centered
        canvas_set_font(canvas, FontSecondary);
        char stats_line[64];

        snprintf(
            stats_line,
            sizeof(stats_line),
            "Total: %ld   H: %ld   T: %ld",
            app->total_flips,
            app->heads_count,
            app->tails_count);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignTop, stats_line);
    }

    // Button hints
    elements_button_center(canvas, "Flip");
    elements_button_left(canvas, "Back");
}

// Input callback for coin flip view
static bool coin_flip_view_input_callback(InputEvent* event, void* context) {
    FURI_LOG_I(TAG, "Input callback called with context: %p, g_app: %p", context, g_app);
    if(!event) return false;

    // Use global app reference if context is null
    FlipperPrinterApp* app = context ? (FlipperPrinterApp*)context : g_app;
    if(!app) return false;
    if(!app->view_dispatcher) return false;

    bool consumed = false;

    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKeyOk:
            // Perform coin flip
            app->last_flip_was_heads = coin_flip_random();
            app->total_flips++;

            if(app->last_flip_was_heads) {
                app->heads_count++;
            } else {
                app->tails_count++;
            }

            // Update streak tracking
            if(app->total_flips == 1) {
                // First flip - start streak
                app->current_streak = 1;
                app->streak_is_heads = app->last_flip_was_heads;
                app->longest_streak = 1;
            } else {
                // Check if streak continues
                if(app->last_flip_was_heads == app->streak_is_heads) {
                    // Streak continues
                    app->current_streak++;
                    if(app->current_streak > app->longest_streak) {
                        app->longest_streak = app->current_streak;
                    }
                } else {
                    // Streak broken, start new streak
                    app->current_streak = 1;
                    app->streak_is_heads = app->last_flip_was_heads;
                }
            }

            // Print result
            printer_print_coin_flip(app->total_flips, app->last_flip_was_heads);

            // Notify user (with safety check)
            if(app->notification) {
                notification_message(app->notification, &sequence_coin_flip);
            }

            FURI_LOG_I(
                TAG,
                "Coin flip completed: total=%ld, heads=%ld, tails=%ld",
                app->total_flips,
                app->heads_count,
                app->tails_count);

            // Force view redraw using the standard Flipper pattern
            if(app->coin_flip_view) {
                // This is the proper way to trigger a redraw in Flipper apps
                view_commit_model(app->coin_flip_view, true);
            }

            consumed = true;
            break;

        case InputKeyLeft:
        case InputKeyBack:
            // Return to menu
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);
            consumed = true;
            break;

        default:
            break;
        }
    }

    return consumed;
}

// Enter callback for coin flip view (called when view becomes active)
static void coin_flip_view_enter_callback(void* context) {
    UNUSED(context);
    // The view dispatcher should automatically trigger a draw when entering the view
}

// Exit callback for coin flip view (called when leaving view)
static void coin_flip_view_exit_callback(void* context) {
    UNUSED(context);
}

// Allocate and initialize coin flip view
View* coin_flip_view_alloc(FlipperPrinterApp* app) {
    if(!app) {
        FURI_LOG_E(TAG, "App is NULL in coin_flip_view_alloc");
        return NULL;
    }

    // Set global app reference as workaround
    g_app = app;
    FURI_LOG_I(TAG, "Set global app reference: %p", g_app);

    View* view = view_alloc();
    if(!view) {
        FURI_LOG_E(TAG, "Failed to allocate view");
        return NULL;
    }

    FURI_LOG_I(TAG, "Setting context for coin flip view: %p", app);
    view_set_context(view, app);
    view_set_draw_callback(view, coin_flip_view_draw_callback);
    view_set_input_callback(view, coin_flip_view_input_callback);
    view_set_enter_callback(view, coin_flip_view_enter_callback);
    view_set_exit_callback(view, coin_flip_view_exit_callback);
    return view;
}

// Free coin flip view
void coin_flip_view_free(View* view) {
    if(view) {
        view_free(view);
    }
    // Clear global app reference
    g_app = NULL;
}
