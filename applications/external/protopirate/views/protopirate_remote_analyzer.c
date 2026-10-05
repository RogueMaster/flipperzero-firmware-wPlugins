#ifdef PROTOPIRATE_REMOTE_ANALYZER_PLUGIN_BUILD
#include "protopirate_remote_analyzer.h"

#include <furi.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <gui/elements.h>
#include "../helpers/RemoteAnalyzer/protopirate_remote_analyzer_worker.h"
#include "../helpers/protopirate_txrx.h"

#include <assets_icons.h>
#include <float_tools.h>

#define TAG "PPRemoteAnalyzer"

#define RSSI_MIN     (-97.0f)
#define RSSI_MAX     (-60.0f)
#define RSSI_SCALE   2.3f
#define TRIGGER_STEP 1
#define MAX_HISTORY  4
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

typedef enum {
    ProtoPirateRemoteAnalyzerStatusIDLE,
} ProtoPirateRemoteAnalyzerStatus;

struct ProtoPirateRemoteAnalyzer {
    View* view;
    ProtoPirateRemoteAnalyzerWorker* worker;
    ProtoPirateRemoteAnalyzerCallback callback;
    void* context;
    ProtoPirateTxRx* txrx;
    ProtoPirateRemoteAnalyzerFeedbackLevel
        feedback_level; // 0 - no feedback, 1 - vibro only, 2 - vibro and sound
    float rssi_last;
    uint8_t selected_index;
    uint8_t max_index;
    bool show_frame;
    bool locked;
};

typedef struct {
    float rssi;
    float rssi_last;
    float trigger;
    uint32_t history_frequency[MAX_HISTORY];
    uint32_t frequency;
    uint32_t frequency_to_save;
    ProtoPirateRemoteAnalyzerFeedbackLevel feedback_level;
    uint8_t selected_index;
    uint8_t max_index;
    uint8_t history_frequency_rx_count[MAX_HISTORY];
    bool history_am[MAX_HISTORY];
    bool is_am;
    bool signal;
    bool show_frame;
    //bool is_ext_radio;
} ProtoPirateRemoteAnalyzerModel;

void protopirate_remote_analyzer_set_callback(
    ProtoPirateRemoteAnalyzer* protopirate_remote_analyzer,
    ProtoPirateRemoteAnalyzerCallback callback,
    void* context) {
    protopirate_remote_analyzer->callback = callback;
    protopirate_remote_analyzer->context = context;
}

void protopirate_remote_analyzer_draw_rssi(
    Canvas* canvas,
    float rssi,
    float rssi_last,
    float trigger,
    uint8_t x,
    uint8_t y) {
    // Current RSSI
    if(!float_is_equal(rssi, 0.f)) {
        if(rssi > RSSI_MAX) {
            rssi = RSSI_MAX;
        }
        rssi = (rssi - RSSI_MIN) / RSSI_SCALE;
        uint8_t column_number = 0;
        for(size_t i = 0; i <= (uint8_t)rssi; i++) {
            if((i + 1) % 4) {
                column_number++;
                canvas_draw_box(canvas, x + 2 * i, (y + 4) - column_number, 2, column_number);
            }
        }
    }

    // Last RSSI
    if(!float_is_equal(rssi_last, 0.f)) {
        if(rssi_last > RSSI_MAX) {
            rssi_last = RSSI_MAX;
        }
        int max_x = (int)((rssi_last - RSSI_MIN) / RSSI_SCALE) * 2;
        //if(!(max_x % 8)) max_x -= 2;
        int max_h = (int)((rssi_last - RSSI_MIN) / RSSI_SCALE) + 1;
        max_h -= (max_h / 4) + 3;
        canvas_draw_line(canvas, x + max_x + 1, y - max_h, x + max_x + 1, y + 3);
    }

    // Trigger cursor
    trigger = (trigger - RSSI_MIN) / RSSI_SCALE;
    uint8_t tr_x = (uint8_t)((float)x + (2 * trigger));
    canvas_draw_dot(canvas, tr_x, y + 4);
    canvas_draw_line(canvas, tr_x - 1, y + 5, tr_x + 1, y + 5);

    canvas_draw_line(canvas, x, y + 3, x + (RSSI_MAX - RSSI_MIN) * 2 / RSSI_SCALE, y + 3);
}

static void protopirate_remote_analyzer_history_frequency_draw(
    Canvas* canvas,
    ProtoPirateRemoteAnalyzerModel* model) {
    char buffer[64] = {0};
    const uint8_t x1 = 2;
    const uint8_t x2 = 66;
    const uint8_t y = 37;

    canvas_set_font(canvas, FontSecondary);
    uint8_t line = 0;
    bool show_frame = model->show_frame && model->max_index > 0;
    for(uint8_t i = 0; i < MAX_HISTORY; i++) {
        uint8_t current_x;
        uint8_t current_y = y + line * 11;

        if(i % 2 == 0) {
            current_x = x1;
        } else {
            current_x = x2;
            line++;
        }
        if(model->history_frequency[i]) {
            snprintf(
                buffer,
                sizeof(buffer),
                "%03ld.%03ld",
                model->history_frequency[i] / 1000000 % 1000,
                model->history_frequency[i] / 1000 % 1000);
            canvas_draw_str(canvas, current_x, current_y, buffer);
        } else {
            canvas_draw_str(canvas, current_x, current_y, "---.---");
        }
        if(model->history_frequency_rx_count[i] > 0) {
            snprintf(buffer, sizeof(buffer), "x%d", model->history_frequency_rx_count[i]);
            canvas_draw_str(canvas, current_x + 41, current_y, buffer);
        } else {
            canvas_draw_str(
                canvas,
                current_x + 41,
                current_y,
                (model->history_frequency[i]) ? model->history_am[i] ? "AM" : "FM" : "MHz");
        }

        if(show_frame && i == model->selected_index) {
            elements_frame(canvas, current_x - 2, current_y - 9, 63, 11);
        }
    }
}

void protopirate_remote_analyzer_draw(Canvas* canvas, ProtoPirateRemoteAnalyzerModel* model) {
    char buffer[20] = {0};

    // Title
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    //canvas_draw_str(canvas, 0, 7, model->is_ext_radio ? "Ext" : "Int");
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignCenter, "Remote Analyzer");
    canvas_set_font(canvas, FontSecondary);

    if((model->frequency) || (model->max_index)) {
        // Last detected frequency
        protopirate_remote_analyzer_history_frequency_draw(canvas, model);

        // Frequency
        canvas_set_font(canvas, FontPrimary);
        if(model->frequency) {
            snprintf(
                buffer,
                sizeof(buffer),
                "%03ld.%03ld Mhz %s",
                model->frequency / 1000000 % 1000,
                model->frequency / 1000 % 1000,
                model->is_am ? "AM" : "FM");

            if(model->signal) {
                canvas_draw_box(canvas, 4, 14, 121, 12);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }

            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, buffer);
        } else if(model->max_index)
            canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Hold OK to Clone");
    } else {
        canvas_draw_icon(canvas, 10, 11, &I_WarningDolphin_45x42);
        elements_multiline_text_aligned(
            canvas, 65, 32, AlignLeft, AlignCenter, "Press your\nRemote\nto Analyze....");
    }

    // RSSI
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 33, 62, "RSSI");
    protopirate_remote_analyzer_draw_rssi(
        canvas, model->rssi, model->rssi_last, model->trigger, 56, 57);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    const uint8_t icon_x = 119;
    switch(model->feedback_level) {
    case ProtoPirateRemoteAnalyzerFeedbackLevelAll:
        canvas_draw_icon(canvas, icon_x, 1, &I_Volup_8x6);
        break;
    case ProtoPirateRemoteAnalyzerFeedbackLevelVibro:
        canvas_draw_icon(canvas, icon_x, 1, &I_Voldwn_6x6);
        break;
    case ProtoPirateRemoteAnalyzerFeedbackLevelMute:
        canvas_draw_icon(canvas, icon_x, 1, &I_Voldwn_6x6);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 123, 1, 2, 6);
        canvas_set_color(canvas, ColorBlack);
        break;
    }

    // Buttons hint
    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, " -");
    elements_button_right(canvas, "+ ");
}

bool protopirate_remote_analyzer_input(InputEvent* event, void* context) {
    furi_assert(context);
    ProtoPirateRemoteAnalyzer* instance = (ProtoPirateRemoteAnalyzer*)context;

    bool need_redraw = false;
    if(event->key == InputKeyBack) {
        return need_redraw;
    }

    bool is_press_or_repeat = (event->type == InputTypePress) || (event->type == InputTypeRepeat);
    if(is_press_or_repeat && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        // Trigger setup
        float trigger_level =
            protopirate_remote_analyzer_worker_get_trigger_level(instance->worker);
        if(event->key == InputKeyLeft) {
            trigger_level -= TRIGGER_STEP;
            if(trigger_level < RSSI_MIN) {
                trigger_level = RSSI_MIN;
            }
        } else {
            trigger_level += TRIGGER_STEP;
            if(trigger_level > RSSI_MAX) {
                trigger_level = RSSI_MAX;
            }
        }
        protopirate_remote_analyzer_worker_set_trigger_level(instance->worker, trigger_level);
        FURI_LOG_D(TAG, "trigger = %.1f", (double)trigger_level);
        need_redraw = true;
    } else if(event->type == InputTypePress && event->key == InputKeyUp) {
        if(instance->feedback_level == ProtoPirateRemoteAnalyzerFeedbackLevelAll) {
            instance->feedback_level = ProtoPirateRemoteAnalyzerFeedbackLevelMute;
        } else {
            instance->feedback_level--;
        }

        need_redraw = true;
    } else if(is_press_or_repeat && event->key == InputKeyDown) {
        instance->show_frame = instance->max_index > 0;
        if(instance->show_frame) {
            instance->selected_index = (instance->selected_index + 1) % instance->max_index;
            need_redraw = true;
        }
    } else if(event->key == InputKeyOk) {
        need_redraw = false;
        //bool updated = false;
        uint32_t frequency_to_save;
        //uint32_t is_am_to_save;
        with_view_model(
            instance->view,
            ProtoPirateRemoteAnalyzerModel * model,
            {
                //       frequency_to_save = model->frequency_to_save;
                uint32_t prev_freq_to_save = model->frequency_to_save;
                uint32_t frequency_candidate = 0;
                uint32_t is_am_candidate = 0;

                if(model->show_frame && !model->signal) {
                    frequency_candidate = model->history_frequency[model->selected_index];
                    is_am_candidate = model->history_am[model->selected_index];
                } else if(
                    (model->show_frame && model->signal) ||
                    (!model->show_frame && model->signal)) {
                    frequency_candidate = protopirate_remote_analyzer_get_nearest_frequency(
                        instance->worker, model->frequency);
                    is_am_candidate = model->is_am;
                }

                frequency_candidate =
                    (frequency_candidate == 0 || prev_freq_to_save == frequency_candidate ?
                         0 :
                         protopirate_remote_analyzer_get_nearest_frequency(
                             instance->worker, frequency_candidate));

                if(frequency_candidate > 0 && frequency_candidate != model->frequency_to_save) {
                    need_redraw = true;
                    model->frequency_to_save = frequency_candidate;
                    model->is_am = is_am_candidate;
                    frequency_to_save = frequency_candidate;
                    //is_am_to_save = is_am_candidate;
                    //           updated = true;
                }
            },
            false);
        if((event->type == InputTypeLong) && (frequency_to_save > 0)) {
            instance->callback(ProtoPirateCustomEventViewReceiverOK, instance->context);
        }
    }

    if(need_redraw) {
        with_view_model(
            instance->view,
            ProtoPirateRemoteAnalyzerModel * model,
            {
                model->rssi_last = instance->rssi_last;
                model->trigger =
                    protopirate_remote_analyzer_worker_get_trigger_level(instance->worker);
                model->feedback_level = instance->feedback_level;
                model->max_index = instance->max_index;
                model->show_frame = instance->show_frame;
                model->selected_index = instance->selected_index;
            },
            true);
    }

    return true;
}

uint32_t round_int(uint32_t value, uint8_t n) {
    // Round value
    uint8_t on = n;
    while(n--) {
        uint8_t i = value % 10;
        value /= 10;
        if(i >= 5) value++;
    }
    while(on--)
        value *= 10;
    return value;
}

void protopirate_remote_analyzer_pair_callback(
    void* context,
    uint32_t frequency,
    float rssi,
    bool is_am,
    bool signal) {
    ProtoPirateRemoteAnalyzer* instance = (ProtoPirateRemoteAnalyzer*)context;
    bool is_new = false;
    if(float_is_equal(rssi, 0.f) && instance->locked) {
        if(instance->callback) {
            instance->callback(ProtoPirateCustomEventViewReceiverUnlock, instance->context);
        }
        //update history
        instance->show_frame = true;
        uint8_t max_index = instance->max_index;
        with_view_model(
            instance->view,
            ProtoPirateRemoteAnalyzerModel * model,
            {
                bool in_array = false;
                uint32_t normal_frequency = protopirate_remote_analyzer_get_nearest_frequency(
                    instance->worker, model->frequency);
                for(size_t i = 0; i < MAX_HISTORY; i++) {
                    if(model->history_frequency[i] == normal_frequency &&
                       model->history_am[i] == (model->is_am || is_am)) {
                        in_array = true;
                        if(model->history_frequency[i] > 0) {
                            if(model->history_frequency_rx_count[i] == 0) {
                                model->history_frequency_rx_count[i]++;
                            }
                            model->history_frequency_rx_count[i]++;
                        }
                        if(i > 0) {
                            size_t offset = 0;
                            uint8_t temp_rx_count = model->history_frequency_rx_count[i];

                            for(size_t j = MAX_HISTORY - 1; j > 0; j--) {
                                if(j == i) {
                                    offset++;
                                }
                                model->history_am[j] = model->history_am[j - offset];
                                model->history_frequency[j] = model->history_frequency[j - offset];
                                model->history_frequency_rx_count[j] =
                                    model->history_frequency_rx_count[j - offset];
                            }
                            model->history_frequency[0] = normal_frequency;
                            FURI_LOG_D(
                                "Test",
                                "Updating Model Hitsory %s and %s",
                                (is_am) ? "AM" : "FM",
                                (model->history_am[0]) ? "AM" : "FM");

                            //if(!model->history_am[0])
                            //    model->history_am[0] = (model->is_am || is_am);
                            //if(model->history_am[0]) is_am = true;

                            FURI_LOG_D(
                                "Test",
                                "Updated Model Hitsory %s",
                                model->history_am[0] ? "AM" : "FM");
                            model->history_frequency_rx_count[0] = temp_rx_count;
                        }

                        break;
                    }
                }

                if(!in_array) {
                    model->history_am[3] = model->history_am[2];
                    model->history_am[2] = model->history_am[1];
                    model->history_am[1] = model->history_am[0];
                    model->history_am[0] = model->is_am || is_am;
                    FURI_LOG_D(
                        "Test", "Added to Model History %s", model->is_am || is_am ? "AM" : "FM");
                    model->is_am = is_am;

                    model->history_frequency[3] = model->history_frequency[2];
                    model->history_frequency[2] = model->history_frequency[1];
                    model->history_frequency[1] = model->history_frequency[0];
                    model->history_frequency[0] = normal_frequency;

                    model->history_frequency_rx_count[3] = model->history_frequency_rx_count[2];
                    model->history_frequency_rx_count[2] = model->history_frequency_rx_count[1];
                    model->history_frequency_rx_count[1] = model->history_frequency_rx_count[0];
                    model->history_frequency_rx_count[0] = 0;
                }

                if(max_index < MAX_HISTORY) {
                    for(size_t i = 0; i < MAX_HISTORY; i++) {
                        if(model->history_frequency[i] > 0) {
                            max_index = i + 1;
                        }
                    }
                }
            },
            false);
        instance->max_index = max_index;
    } else if(!float_is_equal(rssi, 0.f) && !instance->locked) {
        // There is some signal
        FURI_LOG_I(
            TAG,
            "rssi = %.2f, frequency = %ld Hz %s",
            (double)rssi,
            frequency,
            (is_am ? "AM" : "FM"));
        frequency = round_int(frequency, 3); // Round 299999990Hz to 300000000Hz
        is_new = true;

        // Triggered!
        instance->rssi_last = rssi;
        if(instance->callback) {
            instance->callback(ProtoPirateCustomEventSceneSettingLock, instance->context);
        }
    }

    // Update values
    if(rssi >= instance->rssi_last && frequency != 0) {
        instance->rssi_last = rssi;
    }

    instance->locked = !float_is_equal(rssi, 0.f);
    with_view_model(
        instance->view,
        ProtoPirateRemoteAnalyzerModel * model,
        {
            model->rssi = rssi;
            model->rssi_last = instance->rssi_last;
            model->frequency = frequency;
            if(is_new) model->is_am = is_am;
            model->signal = signal;
            model->trigger =
                protopirate_remote_analyzer_worker_get_trigger_level(instance->worker);
            model->feedback_level = instance->feedback_level;
            model->max_index = instance->max_index;
            model->show_frame = instance->show_frame;
            model->selected_index = instance->selected_index;
        },
        true);
}

void protopirate_remote_analyzer_enter(void* context) {
    ProtoPirateRemoteAnalyzer* instance = (ProtoPirateRemoteAnalyzer*)context;

    //Start worker
    instance->worker = protopirate_remote_analyzer_worker_alloc(instance->context);

    protopirate_remote_analyzer_worker_set_pair_callback(
        instance->worker,
        (ProtoPirateRemoteAnalyzerWorkerPairCallback)protopirate_remote_analyzer_pair_callback,
        instance);

    protopirate_remote_analyzer_worker_start(instance->worker);

    instance->rssi_last = 0;
    instance->selected_index = 0;
    instance->max_index = 0;
    instance->show_frame = false;
    protopirate_remote_analyzer_worker_set_trigger_level(instance->worker, RSSI_MIN);
    with_view_model(
        instance->view,
        ProtoPirateRemoteAnalyzerModel * model,
        {
            model->selected_index = 0;
            model->max_index = 0;
            model->show_frame = false;
            model->rssi = 0;
            model->rssi_last = 0;
            model->frequency = 0;
            model->is_am = false;
            model->history_frequency[3] = 0;
            model->history_frequency[2] = 0;
            model->history_frequency[1] = 0;
            model->history_frequency[0] = 0;
            model->history_frequency_rx_count[3] = 0;
            model->history_frequency_rx_count[2] = 0;
            model->history_frequency_rx_count[1] = 0;
            model->history_frequency_rx_count[0] = 0;
            model->frequency_to_save = 0;
            model->trigger = RSSI_MIN;
            //model->is_ext_radio = app->exe;
        },
        true);
}

void protopirate_remote_analyzer_exit(void* context) {
    furi_assert(context);
    ProtoPirateRemoteAnalyzer* instance = (ProtoPirateRemoteAnalyzer*)context;

    // Stop worker
    if(protopirate_remote_analyzer_worker_is_running(instance->worker)) {
        protopirate_remote_analyzer_worker_stop(instance->worker);
    }
    protopirate_remote_analyzer_worker_free(instance->worker);

    furi_record_close(RECORD_NOTIFICATION);
}

ProtoPirateRemoteAnalyzer* protopirate_remote_analyzer_alloc(ProtoPirateTxRx* txrx) {
    ProtoPirateRemoteAnalyzer* instance = malloc(sizeof(ProtoPirateRemoteAnalyzer));

    // View allocation and configuration
    instance->view = view_alloc();
    view_allocate_model(
        instance->view, ViewModelTypeLocking, sizeof(ProtoPirateRemoteAnalyzerModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)protopirate_remote_analyzer_draw);
    view_set_input_callback(instance->view, protopirate_remote_analyzer_input);
    view_set_enter_callback(instance->view, protopirate_remote_analyzer_enter);
    view_set_exit_callback(instance->view, protopirate_remote_analyzer_exit);
    instance->txrx = txrx;

    return instance;
}

void protopirate_remote_analyzer_free(ProtoPirateRemoteAnalyzer* instance) {
    furi_assert(instance);

    view_free(instance->view);
    free(instance);
}

View* protopirate_remote_analyzer_get_view(ProtoPirateRemoteAnalyzer* instance) {
    furi_assert(instance);
    return instance->view;
}

uint32_t protopirate_remote_analyzer_get_frequency_to_save(ProtoPirateRemoteAnalyzer* instance) {
    furi_assert(instance);
    uint32_t frequency;
    with_view_model(
        instance->view,
        ProtoPirateRemoteAnalyzerModel * model,
        { frequency = model->frequency_to_save; },
        false);

    return frequency;
}

bool protopirate_remote_analyzer_get_is_am_to_save(ProtoPirateRemoteAnalyzer* instance) {
    furi_assert(instance);
    bool is_am;
    with_view_model(
        instance->view, ProtoPirateRemoteAnalyzerModel * model, { is_am = model->is_am; }, false);

    return is_am;
}

ProtoPirateRemoteAnalyzerFeedbackLevel protopirate_remote_analyzer_feedback_level(
    ProtoPirateRemoteAnalyzer* instance,
    ProtoPirateRemoteAnalyzerFeedbackLevel level,
    bool update) {
    if(update) {
        instance->feedback_level = level;
        with_view_model(
            instance->view,
            ProtoPirateRemoteAnalyzerModel * model,
            { model->feedback_level = instance->feedback_level; },
            true);
    }

    return instance->feedback_level;
}

float protopirate_remote_analyzer_get_trigger_level(ProtoPirateRemoteAnalyzer* instance) {
    furi_assert(instance);
    return protopirate_remote_analyzer_worker_get_trigger_level(instance->worker);
}
#endif
