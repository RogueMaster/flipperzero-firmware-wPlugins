#include "midi_ui.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MidiPlayerUi {
    ViewPort* view_port;
    FuriMessageQueue* events;
    FuriMutex* mutex;
    MidiPlayerUiState state;
};

static void midi_ui_draw_callback(Canvas* canvas, void* context) {
    MidiPlayerUi* ui = context;
    MidiPlayerUiState state;

    if(furi_mutex_acquire(ui->mutex, FuriWaitForever) == FuriStatusOk) {
        state = ui->state;
        furi_mutex_release(ui->mutex);
    } else {
        memset(&state, 0, sizeof(state));
        snprintf(state.message, sizeof(state.message), "%s", "UI lock error");
    }

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "MIDI Player");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, state.filename);

    char line[40];
    snprintf(
        line,
        sizeof(line),
        "%s  Vol %u%%",
        midi_player_status_string(state.status),
        state.volume_percent);
    canvas_draw_str(canvas, 2, 36, line);

    if(state.has_current_note) {
        snprintf(line, sizeof(line), "Tick %lu  Note %u", state.tick, state.current_note);
    } else {
        snprintf(line, sizeof(line), "Tick %lu", state.tick);
    }
    canvas_draw_str(canvas, 2, 48, line);

    canvas_draw_str(
        canvas, 2, 61, state.status == MidiPlayerStatusPaused ? "OK Play" : "OK Pause");
    canvas_draw_str_aligned(canvas, 86, 61, AlignRight, AlignBottom, "Vol");
    canvas_draw_str_aligned(canvas, 126, 61, AlignRight, AlignBottom, "Back");
}

static void midi_ui_input_callback(InputEvent* event, void* context) {
    MidiPlayerUi* ui = context;
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        MidiPlayerUiEvent ui_event = {.type = MidiPlayerUiEventBack};
        furi_message_queue_put(ui->events, &ui_event, 0U);
    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        MidiPlayerUiEvent ui_event = {.type = MidiPlayerUiEventPlayPause};
        furi_message_queue_put(ui->events, &ui_event, 0U);
    } else if(
        event->key == InputKeyUp &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        MidiPlayerUiEvent ui_event = {.type = MidiPlayerUiEventVolumeUp};
        furi_message_queue_put(ui->events, &ui_event, 0U);
    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
        MidiPlayerUiEvent ui_event = {.type = MidiPlayerUiEventVolumeDown};
        furi_message_queue_put(ui->events, &ui_event, 0U);
    }
}

MidiPlayerUi* midi_ui_alloc(FuriMessageQueue* events) {
    if(!events) {
        return NULL;
    }

    MidiPlayerUi* ui = malloc(sizeof(MidiPlayerUi));
    if(!ui) {
        return NULL;
    }

    memset(ui, 0, sizeof(*ui));
    ui->events = events;
    ui->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    ui->view_port = view_port_alloc();

    if(!ui->mutex || !ui->view_port) {
        midi_ui_free(ui, NULL);
        return NULL;
    }

    ui->state.status = MidiPlayerStatusIdle;
    ui->state.volume_percent = 50U;
    snprintf(ui->state.filename, sizeof(ui->state.filename), "%s", "example.mid");
    snprintf(ui->state.message, sizeof(ui->state.message), "%s", "Loading");

    view_port_draw_callback_set(ui->view_port, midi_ui_draw_callback, ui);
    view_port_input_callback_set(ui->view_port, midi_ui_input_callback, ui);

    return ui;
}

void midi_ui_attach(MidiPlayerUi* ui, Gui* gui) {
    if(ui && gui) {
        gui_add_view_port(gui, ui->view_port, GuiLayerFullscreen);
    }
}

void midi_ui_update(MidiPlayerUi* ui, const MidiPlayerUiState* state) {
    if(!ui || !state) {
        return;
    }

    if(furi_mutex_acquire(ui->mutex, FuriWaitForever) == FuriStatusOk) {
        ui->state = *state;
        furi_mutex_release(ui->mutex);
    }
    view_port_update(ui->view_port);
}

void midi_ui_free(MidiPlayerUi* ui, Gui* gui) {
    if(!ui) {
        return;
    }

    if(ui->view_port) {
        if(gui) {
            gui_remove_view_port(gui, ui->view_port);
        }
        view_port_free(ui->view_port);
    }
    if(ui->mutex) {
        furi_mutex_free(ui->mutex);
    }
    free(ui);
}
