#include "ir_learn.h"
#include <gui/elements.h>
#include <furi.h>
#include <furi_hal.h>

struct IrLearnArgs {
    View* view;
    InfraredWorker* infrared_worker;
    IrLearnSignalLearnedCallback signal_learned_callback;
    IrLearnBackCallback back_callback;
    void* context;
    IrLearnResult result;
    bool should_stop_worker;
    volatile bool alive;
};

typedef struct {
    bool receiving;
    bool signal_received;
    bool is_decoded;
    char message[64];
} IrLearnModel;

static void ir_learn_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    IrLearnModel* model = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 64, 10, AlignCenter, AlignTop, "IR Learning");

    canvas_set_font(canvas, FontSecondary);

    if(model->receiving) {
        elements_multiline_text_aligned(
            canvas, 64, 25, AlignCenter, AlignTop, "Waiting for IR signal...");
        elements_multiline_text_aligned(
            canvas, 64, 45, AlignCenter, AlignTop, "Point remote at Flipper");
    } else if(model->signal_received) {
        elements_multiline_text_aligned(canvas, 64, 25, AlignCenter, AlignTop, "Signal Learned!");
        elements_multiline_text_aligned(canvas, 64, 37, AlignCenter, AlignTop, model->message);
        elements_multiline_text_aligned(
            canvas, 64, 55, AlignCenter, AlignBottom, "Press OK to continue");
    } else {
        furi_crash("Reached impossible state");
    }

    elements_multiline_text_aligned(
        canvas, 64, 63, AlignCenter, AlignBottom, "Press Back to cancel");
}

static void ir_learn_worker_rx_callback(void* context, InfraredWorkerSignal* received_signal) {
    furi_assert(context);
    IrLearnArgs* ir_learn = context;

    if(!ir_learn->alive) {
        return;
    }

    bool signal_received = false;
    bool is_decoded = false;
    InfraredMessage decoded_message = {0};
    uint32_t* raw_timings = NULL;
    size_t raw_timings_size = 0;
    char display_message[64] = {0};

    if(infrared_worker_signal_is_decoded(received_signal)) {
        const InfraredMessage* message = infrared_worker_get_decoded_signal(received_signal);
        if(message) {
            signal_received = true;
            is_decoded = true;
            decoded_message = *message;

            const char* protocol_name = infrared_get_protocol_name(message->protocol);
            if(protocol_name) {
                snprintf(
                    display_message,
                    sizeof(display_message),
                    "%s: 0x%lX 0x%lX",
                    protocol_name,
                    message->address,
                    message->command);
            } else {
                snprintf(
                    display_message, sizeof(display_message), "Protocol: %d", message->protocol);
            }
        }
    } else {
        // Handle raw signal
        const uint32_t* timings;
        size_t timings_size;
        infrared_worker_get_raw_signal(received_signal, &timings, &timings_size);

        if(timings && timings_size > 0) {
            // copy timings
            raw_timings = malloc(timings_size * sizeof(uint32_t));
            if(raw_timings) {
                memcpy(raw_timings, timings, timings_size * sizeof(uint32_t));
                raw_timings_size = timings_size;
                signal_received = true;
                is_decoded = false;

                snprintf(
                    display_message, sizeof(display_message), "Raw: %d timings", (int)timings_size);
            }
        }
    }

    if(!signal_received) {
        snprintf(display_message, sizeof(display_message), "No signal received");
    }

    with_view_model(
        ir_learn->view,
        IrLearnModel * model,
        {
            model->receiving = false;
            model->signal_received = signal_received;
            model->is_decoded = is_decoded;
            strncpy(model->message, display_message, sizeof(model->message) - 1);
            model->message[sizeof(model->message) - 1] = '\0';
        },
        true);

    // Store the raw timings in the result if we have them
    if(signal_received) {
        // Clear previous result
        if(ir_learn->result.raw_timings) {
            free(ir_learn->result.raw_timings);
            ir_learn->result.raw_timings = NULL;
            ir_learn->result.raw_timings_size = 0;
        }

        ir_learn->result.has_signal = true;
        ir_learn->result.is_decoded = is_decoded;

        if(is_decoded) {
            ir_learn->result.decoded_message = decoded_message;
        } else {
            ir_learn->result.raw_timings = raw_timings;
            ir_learn->result.raw_timings_size = raw_timings_size;
            ir_learn->result.frequency = 38000;
            ir_learn->result.duty_cycle = 0.33f;
        }

        // Set a flag to indicate we should stop the worker
        ir_learn->should_stop_worker = true;
    } else {
        if(raw_timings) {
            free(raw_timings);
        }
        ir_learn->should_stop_worker = true;
    }

    // Vibrate quick to show we grabbed a signal
    furi_hal_vibro_on(true);
    furi_delay_ms(100);
    furi_hal_vibro_on(false);
}

static bool ir_learn_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    IrLearnArgs* ir_learn = context;
    bool consumed = false;

    // Check if we need to stop the worker (from callback)
    if(ir_learn->should_stop_worker) {
        infrared_worker_rx_stop(ir_learn->infrared_worker);
        ir_learn->should_stop_worker = false;
    }

    with_view_model(
        ir_learn->view,
        IrLearnModel * model,
        {
            if(event->type == InputTypeShort) {
                if(event->key == InputKeyOk) {
                    if(model->signal_received) {
                        // Signal learned, return to main
                        if(ir_learn->signal_learned_callback) {
                            ir_learn->signal_learned_callback(ir_learn->context);
                        }
                        consumed = true;
                    }
                    // If no signal received yet, do nothing (still receiving)
                } else if(event->key == InputKeyBack) {
                    // Stop receiving if active
                    if(model->receiving) {
                        infrared_worker_rx_stop(ir_learn->infrared_worker);
                    }

                    // Call the back button callback
                    if(ir_learn->back_callback) {
                        ir_learn->back_callback(ir_learn->context);
                    }
                    consumed = true;
                }
            }
        },
        true);

    return consumed;
}

void ir_learn_start_receiving(IrLearnArgs* ir_learn) {
    furi_assert(ir_learn);

    with_view_model(
        ir_learn->view,
        IrLearnModel * model,
        {
            model->receiving = true;
            model->signal_received = false;
            memset(model->message, 0, sizeof(model->message));
        },
        true);

    infrared_worker_rx_enable_blink_on_receiving(ir_learn->infrared_worker, true);
    infrared_worker_rx_enable_signal_decoding(ir_learn->infrared_worker, true);
    infrared_worker_rx_set_received_signal_callback(
        ir_learn->infrared_worker, ir_learn_worker_rx_callback, ir_learn);
    infrared_worker_rx_start(ir_learn->infrared_worker);
}

IrLearnArgs* ir_learn_alloc() {
    IrLearnArgs* ir_learn = malloc(sizeof(IrLearnArgs));

    ir_learn->view = view_alloc();
    view_set_context(ir_learn->view, ir_learn);
    view_allocate_model(ir_learn->view, ViewModelTypeLocking, sizeof(IrLearnModel));
    view_set_draw_callback(ir_learn->view, ir_learn_draw_callback);
    view_set_input_callback(ir_learn->view, ir_learn_input_callback);

    ir_learn->infrared_worker = infrared_worker_alloc();

    ir_learn->signal_learned_callback = NULL;
    ir_learn->back_callback = NULL;
    ir_learn->context = NULL;
    ir_learn->should_stop_worker = false;

    ir_learn->result.has_signal = false;
    ir_learn->result.is_decoded = false;
    ir_learn->result.raw_timings = NULL;
    ir_learn->result.raw_timings_size = 0;
    ir_learn->result.frequency = 38000;
    ir_learn->result.duty_cycle = 0.33f;

    // Start in receiving state
    with_view_model(
        ir_learn->view,
        IrLearnModel * model,
        {
            model->receiving = true;
            model->signal_received = false;
            model->is_decoded = false;
            memset(model->message, 0, sizeof(model->message));
        },
        true);

    ir_learn->alive = true;

    return ir_learn;
}

void ir_learn_free(IrLearnArgs* ir_learn) {
    furi_assert(ir_learn);
    ir_learn->alive = false;

    infrared_worker_rx_set_received_signal_callback(ir_learn->infrared_worker, NULL, NULL);

    // Trying to stop this causes the app to crash but we probably should stop it hmm
    // infrared_worker_rx_stop(ir_learn->infrared_worker);

    infrared_worker_free(ir_learn->infrared_worker);

    if(ir_learn->result.raw_timings) {
        free(ir_learn->result.raw_timings);
    }

    view_free(ir_learn->view);
    free(ir_learn);
}

View* ir_learn_get_view(IrLearnArgs* ir_learn) {
    furi_assert(ir_learn);
    return ir_learn->view;
}

void ir_learn_set_callbacks(
    IrLearnArgs* ir_learn,
    IrLearnSignalLearnedCallback signal_learned_callback,
    IrLearnBackCallback back_callback,
    void* context) {
    furi_assert(ir_learn);
    ir_learn->signal_learned_callback = signal_learned_callback;
    ir_learn->back_callback = back_callback;
    ir_learn->context = context;
}

IrLearnResult ir_learn_get_result(IrLearnArgs* ir_learn) {
    furi_assert(ir_learn);
    return ir_learn->result;
}
