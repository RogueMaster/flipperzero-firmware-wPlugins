#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/gui.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "tunings.h"
#include "instruments.h"

#define VOLUME_STEP             0.1f
#define DEFAULT_VOLUME          1.0f
#define QUEUE_SIZE              8
#define SPEAKER_ACQUIRE_TIMEOUT 1000

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

enum Page {
    InstrumentsPage,
    VariationsPage,
    TuningsPage,
    NotesPage
};

typedef struct {
    FuriMutex* mutex;
    bool playing;
    enum Page page;
    float volume;

    int current_instrument_index;
    int current_variation_index;
    int current_tuning_index;
    int current_tuning_note_index;

    INSTRUMENT instrument;
    VARIATION variation;
    TUNING tuning;
} TuningForkState;

// Getters
static INSTRUMENT current_instrument(TuningForkState* tuningForkState) {
    return tuningForkState->instrument;
}

static VARIATION current_variation(TuningForkState* tuningForkState) {
    return tuningForkState->variation;
}

static TUNING current_tuning(TuningForkState* tuningForkState) {
    return tuningForkState->tuning;
}

static NOTE current_tuning_note(TuningForkState* tuningForkState) {
    return current_tuning(tuningForkState).notes[tuningForkState->current_tuning_note_index];
}

static float current_tuning_note_freq(TuningForkState* tuningForkState) {
    return current_tuning_note(tuningForkState).frequency;
}

// String helper
static void safe_string_copy(char* dest, const char* src, size_t size) {
    if(dest && src) {
        strncpy(dest, src, size - 1);
        dest[size - 1] = '\0';
    }
}

// Labels
static void current_instrument_label(TuningForkState* tuningForkState, char* outCategoryLabel) {
    if(outCategoryLabel) {
        safe_string_copy(
            outCategoryLabel, current_instrument(tuningForkState).label, MAX_LABEL_LENGTH);
    }
}

static void current_variation_label(TuningForkState* tuningForkState, char* outCategoryLabel) {
    if(outCategoryLabel) {
        safe_string_copy(
            outCategoryLabel, current_variation(tuningForkState).label, MAX_LABEL_LENGTH);
    }
}

static void current_tuning_label(TuningForkState* tuningForkState, char* outTuningLabel) {
    if(outTuningLabel) {
        safe_string_copy(outTuningLabel, current_tuning(tuningForkState).label, MAX_LABEL_LENGTH);
    }
}

static void current_tuning_note_label(TuningForkState* tuningForkState, char* outNoteLabel) {
    if(outNoteLabel) {
        safe_string_copy(
            outNoteLabel, current_tuning_note(tuningForkState).label, MAX_LABEL_LENGTH);
    }
}

// Update references
static void updateTuning(TuningForkState* tuning_fork_state) {
    tuning_fork_state->instrument = Instruments[tuning_fork_state->current_instrument_index];
    tuning_fork_state->variation =
        tuning_fork_state->instrument.variations[tuning_fork_state->current_variation_index];
    tuning_fork_state->tuning =
        tuning_fork_state->variation.tunings[tuning_fork_state->current_tuning_index];
    tuning_fork_state->current_tuning_note_index = 0;
}

// Instruments navigation
static void next_instrument(TuningForkState* tuning_fork_state) {
    if(tuning_fork_state->current_instrument_index == INSTRUMENTS_COUNT - 1) {
        tuning_fork_state->current_instrument_index = 0;
    } else {
        tuning_fork_state->current_instrument_index += 1;
    }
    tuning_fork_state->current_variation_index = 0;
    tuning_fork_state->current_tuning_index = 0;
    updateTuning(tuning_fork_state);
}

static void prev_instrument(TuningForkState* tuning_fork_state) {
    if(tuning_fork_state->current_instrument_index == 0) {
        tuning_fork_state->current_instrument_index = INSTRUMENTS_COUNT - 1;
    } else {
        tuning_fork_state->current_instrument_index -= 1;
    }
    tuning_fork_state->current_variation_index = 0;
    tuning_fork_state->current_tuning_index = 0;
    updateTuning(tuning_fork_state);
}

// Variations navigation
static void next_variation(TuningForkState* tuning_fork_state) {
    if(tuning_fork_state->current_variation_index ==
       tuning_fork_state->instrument.variations_count - 1) {
        tuning_fork_state->current_variation_index = 0;
    } else {
        tuning_fork_state->current_variation_index += 1;
    }
    updateTuning(tuning_fork_state);
}

static void prev_variation(TuningForkState* tuning_fork_state) {
    if(tuning_fork_state->current_variation_index == 0) {
        tuning_fork_state->current_variation_index =
            tuning_fork_state->instrument.variations_count - 1;
    } else {
        tuning_fork_state->current_variation_index -= 1;
    }
    updateTuning(tuning_fork_state);
}

// Tunings navigation
static void next_tuning(TuningForkState* tuning_fork_state) {
    if(tuning_fork_state->current_tuning_index == tuning_fork_state->variation.tunings_count - 1) {
        tuning_fork_state->current_tuning_index = 0;
    } else {
        tuning_fork_state->current_tuning_index += 1;
    }
    updateTuning(tuning_fork_state);
}

static void prev_tuning(TuningForkState* tuning_fork_state) {
    if(tuning_fork_state->current_tuning_index == 0) {
        tuning_fork_state->current_tuning_index = tuning_fork_state->variation.tunings_count - 1;
    } else {
        tuning_fork_state->current_tuning_index -= 1;
    }
    updateTuning(tuning_fork_state);
}

// Notes navigation
static void next_note(TuningForkState* tuning_fork_state) {
    if(tuning_fork_state->current_tuning_note_index ==
       current_tuning(tuning_fork_state).notes_count - 1) {
        tuning_fork_state->current_tuning_note_index = 0;
    } else {
        tuning_fork_state->current_tuning_note_index += 1;
    }
}

static void prev_note(TuningForkState* tuning_fork_state) {
    if(tuning_fork_state->current_tuning_note_index == 0) {
        tuning_fork_state->current_tuning_note_index =
            current_tuning(tuning_fork_state).notes_count - 1;
    } else {
        tuning_fork_state->current_tuning_note_index -= 1;
    }
}

// Volume adjustments
static void increase_volume(TuningForkState* tuning_fork_state) {
    if(tuning_fork_state->volume <= (1.0f - VOLUME_STEP)) {
        tuning_fork_state->volume += VOLUME_STEP;
    }
}

static void decrease_volume(TuningForkState* tuning_fork_state) {
    if(tuning_fork_state->volume >= VOLUME_STEP) {
        tuning_fork_state->volume -= VOLUME_STEP;
    }
}

// Player
static void play(TuningForkState* tuning_fork_state) {
    if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_start(
            current_tuning_note_freq(tuning_fork_state), tuning_fork_state->volume);
    }
}

static void stop() {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

static void replay(TuningForkState* tuning_fork_state) {
    stop();
    play(tuning_fork_state);
}

// Renderer
static void render_callback(Canvas* const canvas, void* ctx) {
    furi_assert(ctx);
    TuningForkState* tuning_fork_state = ctx;
    furi_mutex_acquire(tuning_fork_state->mutex, FuriWaitForever);

    FuriString* tempStr = furi_string_alloc();

    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontPrimary);

    if(tuning_fork_state->page == InstrumentsPage) {
        char instrumentLabel[MAX_LABEL_LENGTH];
        current_instrument_label(tuning_fork_state, instrumentLabel);
        furi_string_printf(tempStr, "< %s >", instrumentLabel);
        canvas_draw_str_aligned(
            canvas, 64, 28, AlignCenter, AlignCenter, furi_string_get_cstr(tempStr));
    } else if(tuning_fork_state->page == VariationsPage) {
        char instrumentLabel[MAX_LABEL_LENGTH];
        char variationLabel[MAX_LABEL_LENGTH];

        current_instrument_label(tuning_fork_state, instrumentLabel);
        current_variation_label(tuning_fork_state, variationLabel);

        furi_string_printf(tempStr, "%s", instrumentLabel);
        canvas_draw_str_aligned(canvas, 4, 4, AlignLeft, AlignTop, furi_string_get_cstr(tempStr));

        furi_string_reset(tempStr);

        furi_string_printf(tempStr, "< %s >", variationLabel);
        canvas_draw_str_aligned(
            canvas, 64, 28, AlignCenter, AlignCenter, furi_string_get_cstr(tempStr));

    } else if(tuning_fork_state->page == TuningsPage) {
        char instrumentLabel[MAX_LABEL_LENGTH];
        char variationLabel[MAX_LABEL_LENGTH];
        char tuningLabel[MAX_LABEL_LENGTH];

        current_instrument_label(tuning_fork_state, instrumentLabel);
        current_variation_label(tuning_fork_state, variationLabel);
        current_tuning_label(tuning_fork_state, tuningLabel);

        furi_string_printf(tempStr, "%s", instrumentLabel);
        canvas_draw_str_aligned(canvas, 4, 4, AlignLeft, AlignTop, furi_string_get_cstr(tempStr));

        furi_string_reset(tempStr);

        furi_string_printf(tempStr, "%s", variationLabel);
        canvas_draw_str_aligned(
            canvas, 124, 4, AlignRight, AlignTop, furi_string_get_cstr(tempStr));

        furi_string_reset(tempStr);

        furi_string_printf(tempStr, "< %s >", tuningLabel);
        canvas_draw_str_aligned(
            canvas, 64, 28, AlignCenter, AlignCenter, furi_string_get_cstr(tempStr));

    } else {
        char instrumentLabel[MAX_LABEL_LENGTH];
        char variationLabel[MAX_LABEL_LENGTH];
        char tuningLabel[MAX_LABEL_LENGTH];

        current_instrument_label(tuning_fork_state, instrumentLabel);
        current_variation_label(tuning_fork_state, variationLabel);
        current_tuning_label(tuning_fork_state, tuningLabel);

        furi_string_printf(tempStr, "%s", instrumentLabel);
        canvas_draw_str_aligned(canvas, 4, 4, AlignLeft, AlignTop, furi_string_get_cstr(tempStr));

        furi_string_reset(tempStr);

        furi_string_printf(tempStr, "%s", variationLabel);
        canvas_draw_str_aligned(
            canvas, 124, 4, AlignRight, AlignTop, furi_string_get_cstr(tempStr));

        furi_string_reset(tempStr);

        furi_string_printf(tempStr, "%s", tuningLabel);
        canvas_draw_str_aligned(
            canvas, 64, 20, AlignCenter, AlignCenter, furi_string_get_cstr(tempStr));

        furi_string_reset(tempStr);

        char tuningNoteLabel[MAX_LABEL_LENGTH];
        current_tuning_note_label(tuning_fork_state, tuningNoteLabel);
        furi_string_printf(tempStr, "< %s >", tuningNoteLabel);
        canvas_draw_str_aligned(
            canvas, 64, 32, AlignCenter, AlignCenter, furi_string_get_cstr(tempStr));
        furi_string_reset(tempStr);
    }

    canvas_set_font(canvas, FontSecondary);
    elements_button_left(canvas, "Prev");
    elements_button_right(canvas, "Next");

    if(tuning_fork_state->page == NotesPage) {
        if(tuning_fork_state->playing) {
            elements_button_center(canvas, "Stop");
        } else {
            elements_button_center(canvas, "Play");
        }
    } else {
        elements_button_center(canvas, "Select");
    }

    if(tuning_fork_state->page == NotesPage) {
        elements_progress_bar(canvas, 8, 40, 112, tuning_fork_state->volume);
    }

    furi_string_free(tempStr);
    furi_mutex_release(tuning_fork_state->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void tuning_fork_state_init(TuningForkState* const tuning_fork_state) {
    if(tuning_fork_state) {
        tuning_fork_state->playing = false;
        tuning_fork_state->page = InstrumentsPage;
        tuning_fork_state->volume = DEFAULT_VOLUME;
        tuning_fork_state->current_instrument_index = 0;
        tuning_fork_state->current_variation_index = 0;
        tuning_fork_state->current_tuning_index = 0;
        tuning_fork_state->current_tuning_note_index = 0;
        updateTuning(tuning_fork_state);
    }
}

int32_t tuning_fork_app() {
    FuriMessageQueue* event_queue = furi_message_queue_alloc(QUEUE_SIZE, sizeof(PluginEvent));
    if(!event_queue) {
        FURI_LOG_E("TuningFork", "Cannot create message queue\r\n");
        return 255;
    }

    TuningForkState* tuning_fork_state = malloc(sizeof(TuningForkState));
    if(!tuning_fork_state) {
        FURI_LOG_E("TuningFork", "Cannot allocate state\r\n");
        furi_message_queue_free(event_queue);
        return 255;
    }

    tuning_fork_state_init(tuning_fork_state);

    tuning_fork_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!tuning_fork_state->mutex) {
        FURI_LOG_E("TuningFork", "cannot create mutex\r\n");
        free(tuning_fork_state);
        furi_message_queue_free(event_queue);
        return 255;
    }

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, tuning_fork_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    PluginEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        furi_mutex_acquire(tuning_fork_state->mutex, FuriWaitForever);

        if(event_status == FuriStatusOk) {
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypeShort) {
                    // push events
                    switch(event.input.key) {
                    case InputKeyUp:
                        if(tuning_fork_state->page == NotesPage) {
                            increase_volume(tuning_fork_state);
                            if(tuning_fork_state->playing) {
                                replay(tuning_fork_state);
                            }
                        }
                        break;
                    case InputKeyDown:
                        if(tuning_fork_state->page == NotesPage) {
                            decrease_volume(tuning_fork_state);
                            if(tuning_fork_state->playing) {
                                replay(tuning_fork_state);
                            }
                        }
                        break;
                    case InputKeyRight:
                        if(tuning_fork_state->page == InstrumentsPage) {
                            next_instrument(tuning_fork_state);
                        } else if(tuning_fork_state->page == VariationsPage) {
                            next_variation(tuning_fork_state);
                        } else if(tuning_fork_state->page == TuningsPage) {
                            next_tuning(tuning_fork_state);
                        } else {
                            next_note(tuning_fork_state);
                            if(tuning_fork_state->playing) {
                                replay(tuning_fork_state);
                            }
                        }
                        break;
                    case InputKeyLeft:
                        if(tuning_fork_state->page == InstrumentsPage) {
                            prev_instrument(tuning_fork_state);
                        } else if(tuning_fork_state->page == VariationsPage) {
                            prev_variation(tuning_fork_state);
                        } else if(tuning_fork_state->page == TuningsPage) {
                            prev_tuning(tuning_fork_state);
                        } else {
                            prev_note(tuning_fork_state);
                            if(tuning_fork_state->playing) {
                                replay(tuning_fork_state);
                            }
                        }
                        break;
                    case InputKeyOk:
                        if(tuning_fork_state->page == InstrumentsPage) {
                            tuning_fork_state->page = VariationsPage;
                        } else if(tuning_fork_state->page == VariationsPage) {
                            tuning_fork_state->page = TuningsPage;
                        } else if(tuning_fork_state->page == TuningsPage) {
                            tuning_fork_state->page = NotesPage;
                        } else {
                            tuning_fork_state->playing = !tuning_fork_state->playing;
                            if(tuning_fork_state->playing) {
                                play(tuning_fork_state);
                            } else {
                                stop();
                            }
                        }
                        break;
                    case InputKeyBack:
                        if(tuning_fork_state->page == InstrumentsPage) {
                            processing = false;
                        } else if(tuning_fork_state->page == VariationsPage) {
                            tuning_fork_state->page = InstrumentsPage;
                        } else if(tuning_fork_state->page == TuningsPage) {
                            tuning_fork_state->page = VariationsPage;
                        } else {
                            tuning_fork_state->playing = false;
                            tuning_fork_state->current_tuning_note_index = 0;
                            stop();
                            tuning_fork_state->page = TuningsPage;
                        }
                        break;
                    default:
                        break;
                    }
                } else if(event.input.type == InputTypeLong) {
                    // hold events
                    switch(event.input.key) {
                    case InputKeyUp:
                        break;
                    case InputKeyDown:
                        break;
                    case InputKeyRight:
                        if(tuning_fork_state->page == InstrumentsPage) {
                            next_instrument(tuning_fork_state);
                        } else if(tuning_fork_state->page == VariationsPage) {
                            next_variation(tuning_fork_state);
                        } else if(tuning_fork_state->page == TuningsPage) {
                            next_tuning(tuning_fork_state);
                        } else {
                            next_note(tuning_fork_state);
                            if(tuning_fork_state->playing) {
                                replay(tuning_fork_state);
                            }
                        }

                        break;
                    case InputKeyLeft:
                        if(tuning_fork_state->page == InstrumentsPage) {
                            prev_instrument(tuning_fork_state);
                        } else if(tuning_fork_state->page == VariationsPage) {
                            prev_variation(tuning_fork_state);
                        } else if(tuning_fork_state->page == TuningsPage) {
                            prev_tuning(tuning_fork_state);
                        } else {
                            prev_note(tuning_fork_state);
                            if(tuning_fork_state->playing) {
                                replay(tuning_fork_state);
                            }
                        }

                        break;
                    case InputKeyOk:
                        break;
                    case InputKeyBack:
                        if(tuning_fork_state->page == InstrumentsPage) {
                            processing = false;
                        } else if(tuning_fork_state->page == VariationsPage) {
                            tuning_fork_state->page = InstrumentsPage;
                        } else if(tuning_fork_state->page == TuningsPage) {
                            tuning_fork_state->page = VariationsPage;
                        } else {
                            tuning_fork_state->playing = false;
                            stop();
                            tuning_fork_state->page = TuningsPage;
                            tuning_fork_state->current_tuning_note_index = 0;
                        }
                        break;
                    default:
                        break;
                    }
                } else if(event.input.type == InputTypeRepeat) {
                    // repeat events
                    switch(event.input.key) {
                    case InputKeyUp:
                        break;
                    case InputKeyDown:
                        break;
                    case InputKeyRight:
                        if(tuning_fork_state->page == InstrumentsPage) {
                            next_instrument(tuning_fork_state);
                        } else if(tuning_fork_state->page == VariationsPage) {
                            next_variation(tuning_fork_state);
                        } else if(tuning_fork_state->page == TuningsPage) {
                            next_tuning(tuning_fork_state);
                        } else {
                            next_note(tuning_fork_state);
                            if(tuning_fork_state->playing) {
                                replay(tuning_fork_state);
                            }
                        }

                        break;
                    case InputKeyLeft:
                        if(tuning_fork_state->page == InstrumentsPage) {
                            prev_instrument(tuning_fork_state);
                        } else if(tuning_fork_state->page == VariationsPage) {
                            prev_variation(tuning_fork_state);
                        } else if(tuning_fork_state->page == TuningsPage) {
                            prev_tuning(tuning_fork_state);
                        } else {
                            prev_note(tuning_fork_state);
                            if(tuning_fork_state->playing) {
                                replay(tuning_fork_state);
                            }
                        }

                        break;
                    case InputKeyOk:
                        break;
                    case InputKeyBack:
                        if(tuning_fork_state->page == InstrumentsPage) {
                            processing = false;
                        } else if(tuning_fork_state->page == VariationsPage) {
                            tuning_fork_state->page = InstrumentsPage;
                        } else if(tuning_fork_state->page == TuningsPage) {
                            tuning_fork_state->page = VariationsPage;
                        } else {
                            tuning_fork_state->playing = false;
                            stop();
                            tuning_fork_state->page = TuningsPage;
                            tuning_fork_state->current_tuning_note_index = 0;
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        furi_mutex_release(tuning_fork_state->mutex);
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(tuning_fork_state->mutex);
    furi_record_close(RECORD_NOTIFICATION);
    free(tuning_fork_state);

    return 0;
}
