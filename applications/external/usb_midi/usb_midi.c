#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <math.h>

#include "usb/usb_midi_driver.h"
#include "midi/parser.h"
#include "midi/usb_message.h"

#include "usb_midi.h"

static void draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    UsbMidiApp* app = (UsbMidiApp*)ctx;

    char* note_line = malloc(32);
    if(app->current_note.note >= 0) {
        int note = app->current_note.note % 12;
        int octave = app->current_note.note / 12 + 1;
        snprintf(note_line, 32, "Playing note: %s%d", NOTES[note], octave);
    } else {
        strcpy(note_line, "No note");
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 60, 28, AlignCenter, AlignCenter, "USB-MIDI");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 60, 42, AlignCenter, AlignCenter, note_line);
}

static void input_callback(InputEvent* input, void* ctx) {
    furi_assert(ctx);
    UsbMidiApp* app = (UsbMidiApp*)ctx;
    UsbMidiEvent event;

    if((input->key == InputKeyBack) && (input->type == InputTypeShort)) {
        event.type = UsbMidiEventTypeExit;
        furi_message_queue_put(app->events, &event, FuriWaitForever);
        return;
    }

    if(input->key == InputKeyUp)
        event.note.note = 45;
    else if(input->key == InputKeyDown)
        event.note.note = 42;
    else if(input->key == InputKeyLeft)
        event.note.note = 47;
    else if(input->key == InputKeyRight)
        event.note.note = 49;
    else
        return;

    event.note.velocity = 70;

    if(input->type == InputTypePress)
        event.type = UsbMidiEventTypeNoteOn;
    else if(input->type == InputTypeRelease)
        event.type = UsbMidiEventTypeNoteOff;
    else
        return;

    furi_message_queue_put(app->events, &event, FuriWaitForever);
}

static void midi_rx_callback(void* ctx) {
    furi_assert(ctx);
    FuriThreadId thread_id = (FuriThreadId)ctx;
    furi_thread_flags_set(thread_id, 1);
}

int32_t usb_midi_app_reader(void* ctx) {
    furi_assert(ctx);
    UsbMidiApp* app = (UsbMidiApp*)ctx;

    midi_usb_set_context(furi_thread_get_id(furi_thread_get_current()));
    midi_usb_set_rx_callback(midi_rx_callback);

    app->usb_config_restore = furi_hal_usb_get_config();
    furi_hal_usb_set_config(&midi_usb_interface, NULL);

    app->parser = midi_parser_alloc();

    while(app->running) {
        int status = furi_thread_flags_wait(1, FuriFlagWaitAny, 100);
        if(status & FuriFlagError) continue;
        if((status & 1) == 0) continue;

        uint8_t buffer[32];
        size_t size = midi_usb_rx(buffer, sizeof(buffer));
        size_t start = 0;

        while(start < size) {
            CodeIndex code_index = code_index_from_data(buffer[start]);
            uint8_t data_size = usb_message_data_size(code_index);
            if(data_size == 0) break;
            start += 1;
            for(size_t j = 0; j < data_size; j++) {
                if(midi_parser_parse(app->parser, buffer[start + j])) {
                    MidiEvent* midi = midi_parser_get_message(app->parser);
                    UsbMidiEvent event;
                    if(midi->type == NoteOn) {
                        event.type = UsbMidiEventTypeNoteOn;
                        event.note.note = midi->data[0];
                        event.note.velocity = midi->data[1];
                    } else if(midi->type == NoteOff) {
                        event.type = UsbMidiEventTypeNoteOff;
                        event.note.note = midi->data[0];
                    } else
                        continue;
                    furi_message_queue_put(app->events, &event, FuriWaitForever);
                }
            }
            start += data_size;
        }
    }

    midi_parser_free(app->parser);
    furi_hal_usb_set_config(app->usb_config_restore, NULL);
    return 0;
}

UsbMidiApp* usb_midi_app_alloc() {
    UsbMidiApp* app = malloc(sizeof(UsbMidiApp));

    app->events = furi_message_queue_alloc(8, sizeof(UsbMidiEvent));
    app->running = 1;
    app->reader = furi_thread_alloc_ex("Reader", 4 * 1024, &usb_midi_app_reader, app);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->current_note.note = -1;
    if(!furi_hal_speaker_acquire(1000)) {
        furi_assert(NULL);
    }

    return app;
}

void usb_midi_app_process_note_on(UsbMidiApp* app, Note note) {
    app->current_note = note;
    float freq = (440 / 32) * powf(2, ((note.note - 9) / 12.0));
    float volume = note.velocity / 100.0f;
    if(furi_hal_speaker_is_mine()) furi_hal_speaker_start(freq, volume);
}

void usb_midi_app_process_note_off(UsbMidiApp* app, Note note) {
    if(app->current_note.note != note.note) return;
    app->current_note.note = -1;
    if(furi_hal_speaker_is_mine()) furi_hal_speaker_stop();
}

int usb_midi_app_run(UsbMidiApp* app) {
    furi_thread_start(app->reader);

    UsbMidiEvent event;
    while(1) {
        furi_check(furi_message_queue_get(app->events, &event, FuriWaitForever) == FuriStatusOk);
        if(event.type == UsbMidiEventTypeExit) {
            break;
        } else if(event.type == UsbMidiEventTypeNoteOn) {
            usb_midi_app_process_note_on(app, event.note);
        } else if(event.type == UsbMidiEventTypeNoteOff) {
            usb_midi_app_process_note_off(app, event.note);
        }
    }

    app->running = 0;
    furi_thread_join(app->reader);
    return 0;
}

void usb_midi_app_free(UsbMidiApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->events);
    furi_hal_speaker_release();
}

int32_t usb_midi_app(void* p) {
    UNUSED(p);
    UsbMidiApp* app = usb_midi_app_alloc();
    int ret_code = usb_midi_app_run(app);
    usb_midi_app_free(app);
    return ret_code;
}
