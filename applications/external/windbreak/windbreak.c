// Fart Sound Generator for Flipper Zero
// Application ID: mitzi_windbreak
// Entry point: fart_main

#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h> // GUI system
#include <input/input.h> // Input handling (buttons)
#include <notification/notification_messages.h>
#include <furi_hal_speaker.h> // sound output
#include <gui/elements.h> // to access button drawing functions
#include "fart_icons.h" // Custom icon definitions

#define TAG "Windbreak"

// Custom character mixing "/" and "|"
const uint8_t slash_pipe_char[7] = {
    0b00011, // Row 1: top right
    0b00110, // Row 2: moving left
    0b01100, // Row 3: center
    0b01100, // Row 4: vertical hold
    0b11000, // Row 5: moving left
    0b10000, // Row 6: bottom left
    0b10000 // Row 7: bottom left
};

// Event types for the message queue
typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

// Event structure passed through the queue
typedef struct {
    EventType type;
    InputEvent input;
} FartEvent;

// Application state holding all sound parameters
typedef struct {
    uint8_t wet_dry; // 1-5: 1=wet, 5=dry
    uint8_t length; // 1-5: 1=short, 5=long
    uint8_t pitch; // 1-5: 1=low, 5=high
    uint8_t bubbliness; // 1-5: 1=smooth, 5=bubbly
    uint8_t selected; // Currently selected parameter (0-3)
    bool playing; // Is sound currently playing
} FartState;

// Main application context
typedef struct {
    FartState* state;
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    NotificationApp* notifications;
} FartApp;

// Convert parameter value to frequency using logarithmic scale
// Returns frequency range from 50 Hz to 800 Hz
static float get_base_frequency(uint8_t pitch) {
    // Logarithmic scale from 50 Hz to 800 Hz
    // pitch 1->50Hz, 2->94Hz, 3->178Hz, 4->336Hz, 5->800Hz (approx)
    return 50.0f * powf(2.0f, (pitch - 1) * 1.0f);
}

// Get duration in milliseconds based on length parameter
static uint32_t get_duration_ms(uint8_t length) {
    return 100 + (length - 1) * 200; // 100ms to 900ms
}

// Play a fart sound with given parameters
static void play_fart(FartState* state, NotificationApp* notifications) {
    if(!furi_hal_speaker_acquire(1000)) {
        FURI_LOG_E(TAG, "Could not acquire speaker");
        return;
    }

    state->playing = true;

    // Set LED to red during playback (closest to brown available)
    notification_message(notifications, &sequence_set_only_red_255);

    float base_freq = get_base_frequency(state->pitch);
    uint32_t duration = get_duration_ms(state->length);
    float volume = 0.8f; // Fixed volume at 80%

    // Wetness affects frequency variation (wet = more wobble)
    float wet_variation = (6 - state->wet_dry) * 10.0f;

    // Bubbliness affects how often we change frequency
    uint32_t bubble_interval = 50 - (state->bubbliness - 1) * 8; // 50ms to 18ms

    uint32_t start_time = furi_get_tick();
    uint32_t elapsed = 0;
    uint32_t last_bubble = 0;

    float current_freq = base_freq;
    // Main synthesis loop
    while(elapsed < duration) {
        elapsed = furi_get_tick() - start_time;

        // Apply envelope (fade in/out) for natural sound
        float envelope = 1.0f;
        if(elapsed < 50) {
            envelope = (float)elapsed / 50.0f; // Attack
        } else if(elapsed > duration - 100) {
            envelope = (float)(duration - elapsed) / 100.0f; // Release
        }

        // Change frequency for bubbliness effect
        if(elapsed - last_bubble > bubble_interval) {
            // Random frequency variation (increased for more erratic wobble)
            uint32_t random_val = random() % 100;
            float variation = ((float)random_val / 100.0f - 0.5f) * 4.0f;
            current_freq = base_freq + (variation * wet_variation);

            // Add non-linear pitch drift downward (faster at the end)
            float drift_factor = (float)elapsed / (float)duration;
            float drift = drift_factor * drift_factor * 40.0f;
            current_freq -= drift;

            // Occasional squeaks
            if(random() % 100 < 5) {
                current_freq += 200.0f;
            }

            // Clamp frequency to reasonable range
            if(current_freq < 50.0f) current_freq = 50.0f;
            if(current_freq > 2500.0f) current_freq = 2500.0f;

            last_bubble = elapsed;
        }

        // Add sputtering effect - randomly skip some tone bursts
        if(random() % 100 > 20) {
            // Play main frequency with harmonic complexity
            float rumble_freq = current_freq * 0.5f;

            // Play fundamental frequency
            furi_hal_speaker_start(current_freq, volume * envelope * 0.6f);
            furi_delay_ms(2);

            // Play lower rumble frequency for richness
            furi_hal_speaker_start(rumble_freq, volume * envelope * 0.4f);
            furi_delay_ms(3);
        } else {
            // Silence for sputtering effect
            furi_delay_ms(5);
        }
    }

    furi_hal_speaker_stop();
    furi_hal_speaker_release();
    notification_message(notifications, &sequence_reset_rgb); // Turn off LED
    state->playing = false;
}

// Render callback for GUI - draws the interface
static void fart_render_callback(Canvas* canvas, void* ctx) {
    FartApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);

    // Draw header with icon and title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
    canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Windbreak");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 110, 1, AlignLeft, AlignTop, "v0.2");

    // Draw date rotated 90 degrees on right edge
    canvas_set_font_direction(canvas, CanvasDirectionBottomToTop);
    canvas_draw_str(canvas, 128, 47, "2025-12");
    canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);

    // Draw footer text and button
    canvas_draw_str_aligned(canvas, 1, 57, AlignLeft, AlignTop, "Back: exit");
    // canvas_draw_str_aligned(canvas, 127, 50, AlignRight, AlignTop, "f418.eu/");
    canvas_draw_str_aligned(canvas, 127, 57, AlignRight, AlignTop, "f418.eu");
    elements_button_center(canvas, "Fart");

    char str_buffer[64];
    uint8_t y = 19;

    // Draw parameters with selection indicator and bar graphs
    const char* params[] = {"Wet/Dry", "Length", "Pitch", "Bubbliness"};
    const Icon* icons[] = {&I_humidity_10x10, &I_length_10x10, &I_pitch_10x10, &I_bubbles_10x10};

    uint8_t values[] = {
        app->state->wet_dry, app->state->length, app->state->pitch, app->state->bubbliness};
    uint8_t increment_y = 10;
    uint8_t increment_x = 8;
    uint8_t box_width = increment_x * 5 + 2;
    for(uint8_t i = 0; i < 4; i++) {
        snprintf(str_buffer, sizeof(str_buffer), "%s %d", params[i], values[i]);
        if(app->state->selected == i) canvas_draw_str(canvas, 0, y, ">"); // Selection indicator
        canvas_draw_str(canvas, 6, y, str_buffer);

        // Draw bar indicator i to display the value
        uint8_t bar_x = 75;
        uint8_t bar_y = 7;

        canvas_draw_icon(canvas, bar_x - 11, y - bar_y - 1, icons[i]);
        uint8_t bar_fill = values[i] * increment_x;
        canvas_draw_frame(canvas, bar_x, y - bar_y - 1, box_width, 8); // outer box
        canvas_draw_box(canvas, bar_x + 1, y - bar_y, bar_fill, 6); // actual value

        y += increment_y;
    }

    furi_mutex_release(app->mutex);
}

// Input callback - queues input events for processing
static void fart_input_callback(InputEvent* input_event, void* ctx) {
    FartApp* app = ctx;
    FartEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(app->event_queue, &event, FuriWaitForever);
}

// Main application entry point
int32_t fart_main(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "Windbreak fart generator starting...");

    // Allocate app structure and resources
    FartApp* app = malloc(sizeof(FartApp));
    app->state = malloc(sizeof(FartState));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->event_queue = furi_message_queue_alloc(8, sizeof(FartEvent));
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Initialize default state
    app->state->wet_dry = 3;
    app->state->length = 4;
    app->state->pitch = 1;
    app->state->bubbliness = 3;
    app->state->selected = 0;
    app->state->playing = false;

    // Setup GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, fart_render_callback, app);
    view_port_input_callback_set(view_port, fart_input_callback, app);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    FURI_LOG_I(TAG, "GUI initialized, entering main loop");

    // Main event loop
    FartEvent event;
    bool running = true;

    while(running) {
        // Wait for events with 100ms timeout
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress || event.input.type == InputTypeRepeat) {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);

                    switch(event.input.key) {
                    case InputKeyUp:
                        // Navigate up through parameters
                        if(app->state->selected > 0) {
                            app->state->selected--;
                        }
                        break;

                    case InputKeyDown:
                        // Navigate down through parameters
                        if(app->state->selected < 3) {
                            app->state->selected++;
                        }
                        break;

                    case InputKeyRight:
                        // Increase selected parameter value
                        if(app->state->selected < 4) {
                            uint8_t* param = NULL;
                            switch(app->state->selected) {
                            case 0:
                                param = &app->state->wet_dry;
                                break;
                            case 1:
                                param = &app->state->length;
                                break;
                            case 2:
                                param = &app->state->pitch;
                                break;
                            case 3:
                                param = &app->state->bubbliness;
                                break;
                            }
                            if(param && *param < 5) (*param)++;
                        }
                        break;

                    case InputKeyLeft:
                        // Decrease selected parameter value
                        if(app->state->selected < 4) {
                            uint8_t* param = NULL;
                            switch(app->state->selected) {
                            case 0:
                                param = &app->state->wet_dry;
                                break;
                            case 1:
                                param = &app->state->length;
                                break;
                            case 2:
                                param = &app->state->pitch;
                                break;
                            case 3:
                                param = &app->state->bubbliness;
                                break;
                            }
                            if(param && *param > 1) (*param)--;
                        }
                        break;

                    case InputKeyOk:
                        // Play the fart sound (if not already playing)
                        if(!app->state->playing) {
                            FURI_LOG_I(
                                TAG,
                                "Playing fart: W/D=%d L=%d P=%d B=%d",
                                app->state->wet_dry,
                                app->state->length,
                                app->state->pitch,
                                app->state->bubbliness);
                            // Play fart in separate thread to not block UI
                            furi_mutex_release(app->mutex);
                            play_fart(app->state, app->notifications);
                            furi_mutex_acquire(app->mutex, FuriWaitForever);
                        }
                        break;

                    case InputKeyBack:
                        // Exit the application
                        FURI_LOG_I(TAG, "Exit requested");
                        running = false;
                        break;

                    default:
                        break;
                    }

                    furi_mutex_release(app->mutex);
                    view_port_update(view_port);
                }
            }
        }
    }

    FURI_LOG_I(TAG, "Cleaning up...");

    // Cleanup all resources
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app->state);
    free(app);

    FURI_LOG_I(TAG, "Windbreak fart generator terminated");

    return 0;
}
