#include "flizzer_tracker.h"
#include "diskop.h"
#include "init_deinit.h"
#include "input_event.h"
#include "util.h"
#include "view/instrument_editor.h"
#include "view/pattern_editor.h"

#include "font.h"
#include <flizzer_tracker_icons.h>

void draw_callback(Canvas* canvas, void* ctx) {
    TrackerViewModel* model = (TrackerViewModel*)ctx;
    FlizzerTrackerApp* tracker = (FlizzerTrackerApp*)(model->tracker);

    canvas_set_color(canvas, ColorXOR);

    if(tracker->is_loading || tracker->is_loading_instrument) {
        canvas_draw_str(canvas, 10, 10, "Loading...");
        return;
    }

    if(tracker->is_saving || tracker->is_saving_instrument) {
        canvas_draw_str(canvas, 10, 10, "Saving...");
        return;
    }

    if(tracker->showing_help) {
        canvas_draw_icon(canvas, 0, 0, &I_help);
        return;
    }

    canvas_set_custom_u8g2_font(canvas, u8g2_font_tom_thumb_4x6_tr);

    switch(tracker->mode) {
    case PATTERN_VIEW: {
        if(tracker->tracker_engine.song == NULL) {
            stop();
            tracker_engine_set_song(&tracker->tracker_engine, &tracker->song);
        }

        if(tracker->focus != EDIT_PATTERN) {
            draw_songinfo_view(canvas, tracker);
        }

        if(tracker->focus != EDIT_PATTERN) {
            draw_sequence_view(canvas, tracker);
        }

        draw_pattern_view(canvas, tracker);
        break;
    }

    case INST_EDITOR_VIEW: {
        draw_instrument_view(canvas, tracker);
        draw_instrument_program_view(canvas, tracker);
        break;
    }

    default:
        break;
    }
}

bool input_callback(InputEvent* input_event, void* ctx) {
    // Проверяем, что контекст не нулевой
    furi_assert(ctx);
    TrackerView* tracker_view = (TrackerView*)ctx;
    FlizzerTrackerApp* tracker = (FlizzerTrackerApp*)(tracker_view->context);

    bool consumed = false;

    if(input_event->key == InputKeyBack && input_event->type == InputTypeShort) {
        tracker->period = furi_get_tick() - tracker->current_time;
        tracker->current_time = furi_get_tick();

        tracker->was_it_back_keypress = true;
    }

    else if(input_event->type == InputTypeShort || input_event->type == InputTypeLong) {
        tracker->was_it_back_keypress = false;
        tracker->period = 0;
    }

    uint32_t final_period = (tracker->was_it_back_keypress ? tracker->period : 0);

    FlizzerTrackerEvent event = {
        .type = EventTypeInput, .input = *input_event, .period = final_period};

    if(!(tracker->is_loading) && !(tracker->is_saving)) {
        if(event.type == EventTypeInput) {
            process_input_event(tracker, &event);
        }
    }

    consumed = true;
    return consumed;
}

int32_t flizzer_tracker_app(void* p) {
    UNUSED(p);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool st = storage_simply_mkdir(storage, APPSDATA_FOLDER);
    st = storage_simply_mkdir(storage, FLIZZER_TRACKER_FOLDER);
    st = storage_simply_mkdir(storage, FLIZZER_TRACKER_INSTRUMENTS_FOLDER);
    UNUSED(st);
    furi_record_close(RECORD_STORAGE);

    FlizzerTrackerApp* tracker = init_tracker(44100, 50, true, 1024);

    view_dispatcher_switch_to_view(tracker->view_dispatcher, VIEW_TRACKER);
    view_dispatcher_run(tracker->view_dispatcher);

    //here program hangs until view_dispatcher_stop() is called!

    stop();

    save_config(tracker);

    deinit_tracker(tracker);

    return 0;
}