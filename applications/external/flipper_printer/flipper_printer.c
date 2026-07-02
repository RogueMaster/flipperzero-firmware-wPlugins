#include "flipper_printer.h"

// Text input callback
static void text_input_callback(void* context) {
    FlipperPrinterApp* app = context;

    // Print the entered text
    printer_print_text(app->text_buffer);

    // Clear buffer for next use
    memset(app->text_buffer, 0, TEXT_BUFFER_SIZE);

    // Return to menu
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);
}

// Navigation event callback (handles back button)
static bool navigation_event_callback(void* context) {
    UNUSED(context);
    // We handle back button in individual views
    return false;
}

// Allocate and initialize application
static FlipperPrinterApp* flipper_printer_app_alloc(void) {
    // Use calloc to ensure zero initialization
    FlipperPrinterApp* app = calloc(1, sizeof(FlipperPrinterApp));
    if(!app) return NULL;

    // Explicitly initialize coin statistics (even though calloc should zero them)
    app->total_flips = 0;
    app->heads_count = 0;
    app->tails_count = 0;
    app->last_flip_was_heads = false;

    // Initialize streak tracking
    app->current_streak = 0;
    app->longest_streak = 0;
    app->streak_is_heads = false;

    // Open required services
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    // Initialize view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_event_callback);

    // Create menu
    app->menu = menu_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMenu, submenu_get_view(app->menu));

    // Create coin flip view
    app->coin_flip_view = coin_flip_view_alloc(app);
    if(!app->coin_flip_view) {
        // Handle allocation failure - clean up and return NULL
        menu_free(app->menu);
        view_dispatcher_free(app->view_dispatcher);
        furi_record_close(RECORD_NOTIFICATION);
        furi_record_close(RECORD_GUI);
        free(app);
        return NULL;
    }
    view_dispatcher_add_view(app->view_dispatcher, ViewIdCoinFlip, app->coin_flip_view);

    // Re-set context after adding to dispatcher (some Flipper versions require this)
    view_set_context(app->coin_flip_view, app);
    FURI_LOG_I(TAG, "Re-set context for coin flip view after adding to dispatcher: %p", app);

    // Create text input
    app->text_input = text_input_alloc();
    text_input_set_header_text(app->text_input, "Enter text to print:");
    text_input_set_result_callback(
        app->text_input, text_input_callback, app, app->text_buffer, TEXT_BUFFER_SIZE, true);
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdTextInput, text_input_get_view(app->text_input));

    // DISABLED: Printer setup view causes system crashes
    // app->printer_setup_view = printer_setup_view_alloc();
    // view_dispatcher_add_view(app->view_dispatcher, ViewIdPrinterSetup, app->printer_setup_view);

    return app;
}

// Free application resources
static void flipper_printer_app_free(FlipperPrinterApp* app) {
    // Remove views
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdCoinFlip);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdTextInput);
    // DISABLED: view_dispatcher_remove_view(app->view_dispatcher, ViewIdPrinterSetup);

    // Free views
    menu_free(app->menu);
    coin_flip_view_free(app->coin_flip_view);
    text_input_free(app->text_input);
    // DISABLED: printer_setup_view_free(app->printer_setup_view);

    // Free view dispatcher
    view_dispatcher_free(app->view_dispatcher);

    // Close services
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

// Main entry point
int32_t flipper_printer_main(void* p) {
    UNUSED(p);

    // Initialize UART for printer
    printer_init();

    // Create application
    FlipperPrinterApp* app = flipper_printer_app_alloc();
    if(!app) {
        printer_deinit();
        return -1;
    }

    // Start with menu
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdMenu);

    // Run event loop
    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    flipper_printer_app_free(app);
    printer_deinit();

    return 0;
}
