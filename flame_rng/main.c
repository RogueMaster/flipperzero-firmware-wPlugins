#include <furi.h>
#include <furi_hal.h>
#include <infrared.h>
#include <infrared_worker.h>
#include <furi_hal_infrared.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <time.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/name_generator.h>

#define TAG             "flame_rng"
#define HISTORY_SIZE    5
#define MAX_PATH_LENGTH 256
#define MAX_NAME_LENGTH 128

typedef struct {
    uint32_t rng_value;
    uint32_t seed;
    uint32_t history[HISTORY_SIZE];
    uint8_t history_index;
    bool new_value;
    FuriMutex* mutex;
    uint32_t message_timestamp;
} FlameRngState;

static uint32_t generate_rng_from_ir(InfraredWorkerSignal* signal) {
    const uint32_t* timings;
    size_t timings_cnt;
    infrared_worker_get_raw_signal(signal, &timings, &timings_cnt);

    uint32_t seed = furi_hal_random_get(); // Start with hardware RNG
    uint32_t cpu_ticks = DWT->CYCCNT; // CPU cycle counter

    seed ^= seed >> 16;
    seed *= 0x7feb352d;
    seed ^= seed >> 15;
    seed *= 0x846ca68b;
    seed ^= seed >> 16;

    // More thorough mixing of entropy sources
    for(size_t i = 0; i < timings_cnt; i++) {
        seed = (seed << 7) ^ (seed >> 25) ^ timings[i];
        seed += cpu_ticks ^ (i * 0x9E3779B9);
        cpu_ticks = DWT->CYCCNT; // Refresh CPU ticks
    }

    return seed;
}

static void update_history(FlameRngState* state, uint32_t value) {
    state->history[state->history_index] = value;
    state->history_index = (state->history_index + 1) % HISTORY_SIZE;
}

static void ir_callback(void* ctx, InfraredWorkerSignal* signal) {
    FlameRngState* state = ctx;
    uint32_t seed = generate_rng_from_ir(signal);

    furi_mutex_acquire(state->mutex, FuriWaitForever);

    // Combine multiple entropy sources
    uint32_t rng_number = seed ^ furi_hal_random_get();
    rng_number ^= (uint32_t)furi_get_tick(); // System ticks

    // Improved mixing function (based on xxHash)
    rng_number ^= rng_number >> 15;
    rng_number *= 0x85EBCA77;
    rng_number ^= rng_number >> 13;
    rng_number *= 0xC2B2AE3D;
    rng_number ^= rng_number >> 16;

    // Better range reduction without bias
    uint64_t scaled = (uint64_t)rng_number * 1000000ULL;
    state->rng_value = (scaled >> 32); // Take high bits

    state->seed = seed;
    update_history(state, state->rng_value);
    state->new_value = true;
    state->message_timestamp = furi_get_tick();

    furi_mutex_release(state->mutex);
    FURI_LOG_I(TAG, "Generated RNG: %lu (seed: %lu)", state->rng_value, state->seed);
}

static bool save_random_numbers(FlameRngState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* file = NULL;
    bool success = false;

    // Create directory if it doesn't exist
    if(!storage_simply_mkdir(storage, "/ext/random_gen")) {
        FURI_LOG_E(TAG, "Failed to create directory");
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Generate random filename
    char filename[MAX_PATH_LENGTH];
    char name_buffer[MAX_NAME_LENGTH]; // You might need to define MAX_NAME_LENGTH
    name_generator_make_auto(name_buffer, sizeof(name_buffer), "RANDOM");
    snprintf(filename, sizeof(filename), "/ext/random_gen/%s.rng", name_buffer);

    file = file_stream_alloc(storage);
    if(!file_stream_open(file, filename, FSAM_WRITE, FSOM_CREATE_NEW)) {
        FURI_LOG_E(TAG, "Failed to open file: %s", filename);
        goto cleanup;
    }

    // Write all history numbers
    for(uint8_t i = 0; i < HISTORY_SIZE; i++) {
        uint8_t idx = (state->history_index + i) % HISTORY_SIZE;
        if(stream_write_format(file, "%lu\n", state->history[idx]) == 0) {
            FURI_LOG_E(TAG, "Failed to write to file");
            goto cleanup;
        }
    }

    success = true;
    FURI_LOG_I(TAG, "Saved random numbers to %s", filename);

cleanup:
    if(file) {
        file_stream_close(file);
        stream_free(file);
    }
    furi_record_close(RECORD_STORAGE);
    return success;
}

static void render_callback(Canvas* canvas, void* ctx) {
    FlameRngState* state = ctx;
    furi_mutex_acquire(state->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Flame RNG");

    // Main random number display (always updates)
    canvas_set_font(canvas, FontBigNumbers);
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%06lu", state->rng_value);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, value_str);

    // Status message (disappears after some time)
    if(state->new_value) {
        uint32_t current_time = furi_get_tick();
        uint32_t elapsed_time = current_time - state->message_timestamp;

        if(elapsed_time < 1000) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 55, "New value! Press OK to save");
        } else {
            state->new_value = false; // Clear the message after delay
        }
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 55, "Waiting for IR signal...");
    }

    furi_mutex_release(state->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t flame_rng(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Starting Flame RNG");

    // Initialize state
    FlameRngState* state = malloc(sizeof(FlameRngState));
    state->rng_value = 0;
    state->seed = 0;
    state->history_index = 0;
    state->new_value = false;
    memset(state->history, 0, sizeof(state->history));
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    // Set up message queue
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Set up ViewPort
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Register viewport in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Set up infrared
    InfraredWorker* worker = infrared_worker_alloc();
    infrared_worker_rx_enable_signal_decoding(worker, false);
    infrared_worker_rx_set_received_signal_callback(worker, ir_callback, state);
    infrared_worker_rx_start(worker);

    // Main loop
    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                if(event.key == InputKeyBack) {
                    running = false;
                } else if(event.key == InputKeyOk) {
                    furi_mutex_acquire(state->mutex, FuriWaitForever);
                    bool saved = save_random_numbers(state);
                    furi_mutex_release(state->mutex);

                    if(saved) {
                        // Show save confirmation
                        view_port_update(view_port);
                        furi_delay_ms(500);
                    }
                }
            }
        }
        view_port_update(view_port);
    }

    // Cleanup
    infrared_worker_rx_stop(worker);
    infrared_worker_free(worker);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(state->mutex);
    free(state);
    furi_record_close(RECORD_GUI);

    FURI_LOG_I(TAG, "Stopping Flame RNG");
    return 0;
}
