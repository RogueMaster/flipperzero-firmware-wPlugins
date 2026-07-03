#include "ui_draw.h"
#include "ui_feedback.h"
#include "ui_input.h"
#include "score_store.h"

static void input_callback(InputEvent* input_event, void* ctx) {
    AppContext* app = ctx;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

int32_t fliprogue_app(void* p) {
    UNUSED(p);

    AppContext* app = malloc(sizeof(AppContext));
    if(!app) return -1;
    memset(app, 0, sizeof(AppContext));
    app->game = malloc(sizeof(FrGame));
    if(!app->game) {
        free(app);
        return -1;
    }
    memset(app->game, 0, sizeof(FrGame));
    app->screen = UI_TITLE;
    app->sound_enabled = true;
    load_score_store(app);

    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InputEvent event;
    while(app->game->mode != FR_MODE_QUIT) {
        if(furi_message_queue_get(app->input_queue, &event, 100) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            handle_input(app, &event);
            record_score_if_done(app);
            feedback_update_led(app, false);
            furi_mutex_release(app->mutex);
            view_port_update(app->view_port);
        } else {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool dirty = tick_visual_fx(app);
            feedback_update_led(app, false);
            furi_mutex_release(app->mutex);
            if(dirty) view_port_update(app->view_port);
        }
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    if(app->notifications) {
        notification_message(app->notifications, &sequence_reset_rgb);
        furi_record_close(RECORD_NOTIFICATION);
    }
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app->game);
    free(app);
    return 0;
}
