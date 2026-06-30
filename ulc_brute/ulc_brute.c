//#pragma GCC optimize("O3")
//#pragma GCC optimize("tree-vectorize")
//#pragma GCC optimize("unroll-loops")
//#pragma GCC optimize("unroll-all-loops")
#pragma GCC diagnostic push

// Development warnings to catch potential issues
#pragma GCC diagnostic warning "-Wunused-variable"
#pragma GCC diagnostic warning "-Wunused-function"
// Suppress specific warnings
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

#include "ulc_brute.h"
#include "ulc_attack.h"
#include "nfc_hardware.h"
#include "crypto.h"

// Application configuration constants
#define SCREEN_UPDATE_INTERVAL_MS    1000 // How often to refresh the display (in ms)
#define BRUTEFORCE_THREAD_STACK_SIZE 2048 // Memory allocation for bruteforce thread
#define EVENT_QUEUE_SIZE             8 // Maximum pending UI events
#define STARTING_KEY_INDEX           0 // First key index to test (0 to 2^28-1)
//#define STARTING_KEY_INDEX           67671201 - 1000 // Initial key index to start bruteforce from
#define DEFAULT_KEY_MODE             2 // Default key segment (0-3 maps to keys 1-4)

/**
 * @brief Renders the initial waiting screen before bruteforce starts
 * Shows current key segment selection and start instructions
 * @param canvas Drawing surface for the UI
 * @param app Application state and context
 */
static void render_state_waiting(Canvas* canvas, AppContext* app) {
    canvas_draw_str(canvas, 2, 24, "Press OK to begin bruteforce");
    char key_mode_str[32];
    snprintf(key_mode_str, sizeof(key_mode_str), "Key segment (DOWN): %d", app->key_mode + 1);
    canvas_draw_str(canvas, 2, 60, key_mode_str);
}

/**
 * @brief Renders the active bruteforce screen with real-time statistics
 * Displays progress percentage, key testing speed, and total keys tested
 * @param canvas Drawing surface for the UI
 * @param app Application state and context
 */
static void render_state_bruteforcing(Canvas* canvas, AppContext* app) {
    canvas_draw_str(canvas, 2, 24, "Bruteforcing...");

    // Calculate and show progress (0-100%)
    char key_str[32];
    // Convert to percentage (key space is 2^28)
    double key_index = app->current_key_index / (double)(1 << 28);
    snprintf(key_str, sizeof(key_str), "Progress: %.4f%%", key_index * 100);
    canvas_draw_str(canvas, 2, 36, key_str);

    // Calculate and show keys tested per second
    char benchmark_str[32];
    int time_elapsed = furi_hal_rtc_get_timestamp() - app->time_start;
    if(time_elapsed > 0) {
        double keys_per_sec = app->current_key_index / (double)time_elapsed;
        snprintf(benchmark_str, sizeof(benchmark_str), "Speed: %.1f keys/sec", keys_per_sec);
    } else {
        snprintf(benchmark_str, sizeof(benchmark_str), "Speed: -- keys/sec");
    }
    canvas_draw_str(canvas, 2, 48, benchmark_str);

    // Display total number of keys tested
    char total_keys_str[32];
    snprintf(total_keys_str, sizeof(total_keys_str), "Keys tested: %lu", app->current_key_index);
    canvas_draw_str(canvas, 2, 60, total_keys_str);
}

/**
 * @brief Renders the success screen when a valid key is found
 * Displays the discovered key in hexadecimal format
 * @param canvas Drawing surface for the UI
 * @param app Application state and context
 */
static void render_state_complete(Canvas* canvas, AppContext* app) {
    canvas_draw_str(canvas, 2, 24, "Bruteforce complete! Key:");

    uint8_t key[16];
    calculate_key_from_index(app->current_key_index, app->key_mode, key);

    char result_str[32];
    char result_str_2[32];
    format_key_segment(result_str, sizeof(result_str), key, 0);
    format_key_segment(result_str_2, sizeof(result_str_2), key, 8);
    canvas_draw_str(canvas, 2, 36, result_str);
    canvas_draw_str(canvas, 2, 48, result_str_2);
}

/**
 * @brief Renders the error state screen
 * @param canvas Drawing canvas
 * @param app Application context
 */
static void render_state_error(Canvas* canvas, AppContext* app) {
    canvas_draw_str(canvas, 2, 24, "Error!");
    if(app->error) {
        canvas_draw_str(canvas, 2, 36, app->error);
    }
}

/**
 * @brief Main UI render callback that handles all display states
 * Manages the header and delegates to specific render functions based on app state
 * @param canvas Drawing surface for the UI
 * @param ctx Application context (cast to AppContext*)
 */
static void render_callback(Canvas* canvas, void* ctx) {
    AppContext* app = ctx;
    canvas_clear(canvas);

    // Draw header with primary font
    canvas_set_font(canvas, FontPrimary);
    const char* mode_str = (app->mode == AppModeReady) ? "ULC Brute" : "ULC Brute: Running";
    canvas_draw_str(canvas, 2, 12, mode_str);

    // Draw content with secondary font
    canvas_set_font(canvas, FontSecondary);

    // Render appropriate screen based on current state
    switch(app->state) {
    case AppStateWaiting:
        render_state_waiting(canvas, app);
        break;
    case AppStateBruteforcing:
        render_state_bruteforcing(canvas, app);
        break;
    case AppStateBruteComplete:
        render_state_complete(canvas, app);
        break;
    case AppStateError:
        render_state_error(canvas, app);
        break;
    }
}

/**
 * @brief Input event handler for user interactions
 * Queues input events for processing in the main loop
 * @param input_event Button or input device event
 * @param ctx Application context
 */
static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    AppContext* app = ctx;
    AppEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

/**
 * @brief Worker thread that performs the key bruteforce operation
 * All loops in this function will be unrolled for maximum performance
 * @param context Application context
 * @return 0 on completion
 */
static int32_t bruteforce_worker(void* context) {
    AppContext* app = context;
    app->bruteforce_running = true;
    app->state = AppStateBruteforcing;
    app->time_start = furi_hal_rtc_get_timestamp();

// Initialize 3DES crypto context with unrolled memset
#pragma GCC unroll 16
    for(size_t i = 0; i < sizeof(mbedtls_des3_context); i++) {
        ((uint8_t*)&app->ctx)[i] = 0;
    }

    // Setup NFC hardware
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_nfc);
    nfc_init(&furi_hal_spi_bus_handle_nfc);
    app->field_active = true;

    // Main bruteforce loop - will be unrolled by compiler
    while(app->bruteforce_running && app->state == AppStateBruteforcing) {
        if(test_next_key(&furi_hal_spi_bus_handle_nfc, app)) {
            app->mode = AppModeReady;
            app->state = AppStateBruteComplete;
            view_port_update(app->view_port);
            break;
        }
    }

    // Cleanup NFC hardware
    if(app->field_active) {
        nfc_deinit(&furi_hal_spi_bus_handle_nfc);
        furi_hal_spi_release(&furi_hal_spi_bus_handle_nfc);
        app->field_active = false;
    }

    app->bruteforce_running = false;
    return 0;
}

/**
 * @brief Main application entry point
 * Initializes application state, sets up UI, and manages the main event loop
 * @param p Unused parameter required by Flipper Zero API
 * @return 0 on successful completion
 */
int32_t ulc_brute_app(void* p) {
    UNUSED(p);

    // Initialize application context
    AppContext* app = malloc(sizeof(AppContext));
    memset(app, 0, sizeof(AppContext));

    // Set initial state
    app->state = AppStateWaiting;
    app->time_start = furi_hal_rtc_get_timestamp();
    app->current_key_index = STARTING_KEY_INDEX;
    app->key_mode = DEFAULT_KEY_MODE - 1;

    // Initialize system resources
    app->event_queue = furi_message_queue_alloc(EVENT_QUEUE_SIZE, sizeof(AppEvent));
    app->nfc_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Setup GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Setup bruteforce worker thread
    app->bruteforce_thread = furi_thread_alloc();
    furi_thread_set_name(app->bruteforce_thread, "ULCBruteWorker");
    furi_thread_set_stack_size(app->bruteforce_thread, BRUTEFORCE_THREAD_STACK_SIZE);
    furi_thread_set_context(app->bruteforce_thread, app);
    furi_thread_set_callback(app->bruteforce_thread, bruteforce_worker);

    // Main event loop
    bool running = true;
    while(running) {
        AppEvent event;
        FuriStatus status =
            furi_message_queue_get(app->event_queue, &event, SCREEN_UPDATE_INTERVAL_MS);

        // Handle screen updates
        bool should_update = false;
        if(status == FuriStatusErrorTimeout) {
            should_update = app->bruteforce_running;
        }
        // Handle user input
        else if(
            status == FuriStatusOk && event.type == EventTypeKey &&
            event.input.type == InputTypeShort) {
            should_update = true;

            // Handle input when not bruteforcing
            if(!app->bruteforce_running) {
                switch(event.input.key) {
                case InputKeyUp:
                    if(app->key_mode > AppKeyModeKey1) app->key_mode--;
                    break;
                case InputKeyDown:
                    if(app->key_mode < AppKeyModeKey4) app->key_mode++;
                    break;
                case InputKeyOk:
                    app->mode = AppModeRunning;
                    notification_message(app->notifications, &sequence_blink_start_cyan);
                    furi_thread_start(app->bruteforce_thread);
                    break;
                default:
                    break;
                }
            }

            // Handle back button (always active)
            if(event.input.key == InputKeyBack) {
                if(app->bruteforce_running) {
                    app->bruteforce_running = false;
                    furi_thread_join(app->bruteforce_thread);
                }
                notification_message(app->notifications, &sequence_blink_stop);
                running = false;
            }
        }

        if(should_update) {
            view_port_update(app->view_port);
        }
    }

    // Cleanup resources
    furi_thread_free(app->bruteforce_thread);
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_mutex_free(app->nfc_mutex);
    free(app);

    return 0;
}

#pragma GCC diagnostic pop