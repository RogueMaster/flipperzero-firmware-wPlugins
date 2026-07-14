#include <furi.h>
#include <storage/storage.h>

#include <py/mperrno.h>

#include <mp_flipper_runtime.h>
#include <mp_flipper_modflipperzero.h>

#include "mp_flipper_context.h"

static void on_input_callback(const InputEvent* event, void* ctx) {
    uint16_t button = 1 << event->key;
    uint16_t type = 1 << (InputKeyMAX + event->type);

    mp_flipper_on_input(button, type);
}

void mp_flipper_save_file(const char* file_path, const char* data, size_t size) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    File* file = storage_file_alloc(ctx->storage);

    do {
        if(!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_free(file);

            mp_flipper_raise_os_error_with_filename(MP_ENOENT, file_path);

            break;
        }

        storage_file_write(file, data, size);
    } while(false);

    storage_file_free(file);
}

inline void mp_flipper_nlr_jump_fail(void* val) {
    furi_crash();
}

inline void mp_flipper_assert(const char* file, int line, const char* func, const char* expr) {
}

inline void mp_flipper_fatal_error(const char* msg) {
    furi_crash(msg);
}

const char* mp_flipper_print_get_data(void* data) {
    return furi_string_get_cstr(data);
}

size_t mp_flipper_print_get_data_length(void* data) {
    return furi_string_size(data);
}

void* mp_flipper_print_data_alloc() {
    return furi_string_alloc();
}

void mp_flipper_print_strn(void* data, const char* str, size_t length) {
    for(size_t i = 0; i < length; i++) {
        furi_string_push_back(data, str[i]);
    }
}

void mp_flipper_print_data_free(void* data) {
    furi_string_free(data);
}

void* mp_flipper_context_alloc() {
    mp_flipper_context_t* ctx = malloc(sizeof(mp_flipper_context_t));

    ctx->gui = furi_record_open(RECORD_GUI);
    ctx->view_port = view_port_alloc();

    ctx->input_event_queue = furi_record_open(RECORD_INPUT_EVENTS);
    ctx->input_event = furi_pubsub_subscribe(ctx->input_event_queue, on_input_callback, NULL);

    gui_add_view_port(ctx->gui, ctx->view_port, GuiLayerFullscreen);

    ctx->canvas = gui_direct_draw_acquire(ctx->gui);

    ctx->dialog_message = dialog_message_alloc();
    ctx->dialog_message_button_left = NULL;
    ctx->dialog_message_button_center = NULL;
    ctx->dialog_message_button_right = NULL;

    ctx->storage = furi_record_open(RECORD_STORAGE);

    ctx->adc_handle = NULL;

    // GPIO
    ctx->gpio_pins = malloc(MP_FLIPPER_GPIO_PINS * sizeof(mp_flipper_gpio_pin_t));

    for(uint8_t pin = 0; pin < MP_FLIPPER_GPIO_PINS; pin++) {
        ctx->gpio_pins[pin] = MP_FLIPPER_GPIO_PIN_OFF;
    }

    // infrared rx
    ctx->infrared_rx = malloc(sizeof(mp_flipper_infrared_rx_t));

    ctx->infrared_rx->size = MP_FLIPPER_INFRARED_RX_BUFFER_SIZE;
    ctx->infrared_rx->buffer = calloc(ctx->infrared_rx->size, sizeof(uint16_t));
    ctx->infrared_rx->pointer = 0;
    ctx->infrared_rx->running = true;

    // infrared tx
    ctx->infrared_tx = malloc(sizeof(mp_flipper_infrared_tx_t));
    ctx->infrared_tx->index = 0;
    ctx->infrared_tx->provider = NULL;
    ctx->infrared_tx->repeat = 0;
    ctx->infrared_tx->signal = NULL;
    ctx->infrared_tx->size = 0;
    ctx->infrared_tx->level = false;

    return ctx;
}

void mp_flipper_context_free(void* context) {
    mp_flipper_context_t* ctx = context;

    gui_direct_draw_release(ctx->gui);

    furi_pubsub_unsubscribe(ctx->input_event_queue, ctx->input_event);

    gui_remove_view_port(ctx->gui, ctx->view_port);

    view_port_free(ctx->view_port);

    dialog_message_free(ctx->dialog_message);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_INPUT_EVENTS);

    furi_record_close(RECORD_STORAGE);

    // disable ADC handle
    if(ctx->adc_handle) {
        furi_hal_adc_release(ctx->adc_handle);
    }

    // de-initialize all GPIO pins
    for(uint8_t pin = 0; pin < MP_FLIPPER_GPIO_PINS; pin++) {
        mp_flipper_gpio_deinit_pin(pin);
    }

    // stop running PWM output
    mp_flipper_pwm_stop(MP_FLIPPER_GPIO_PIN_PA4);
    mp_flipper_pwm_stop(MP_FLIPPER_GPIO_PIN_PA7);

    free(ctx->gpio_pins);

    // stop infrared
    free(ctx->infrared_rx->buffer);
    free(ctx->infrared_rx);
    free(ctx->infrared_tx);

    free(ctx);
}
