#include "include/app/avocado_session.h"
#include "include/domain/avocado_state.h"
#include "include/platform/feedback_helper.h"
#include "include/platform/storage_helper.h"
#include "include/ui/play_screen.h"
#include "include/ui/welcome_screen.h"
#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <string.h>

typedef enum {
    AvocadoViewWelcome = 0,
    AvocadoViewPlay,
} AvocadoView;

typedef struct {
    ViewDispatcher *vd;
    Gui *gui;
    WelcomeScreen *welcome;
    PlayScreen *play;
    AvocadoFeedback feedback;
    AvocadoData data;
} AvocadoZeroApp;

static bool avocado_navigation_stop(void *context) {
    AvocadoZeroApp *app = context;
    furi_assert(app && app->vd);
    view_dispatcher_stop(app->vd);
    return true;
}

int32_t avocado_zero_app(void *p) {
    UNUSED(p);

    AvocadoZeroApp *app = malloc(sizeof(AvocadoZeroApp));
    if (!app) {
        return -1;
    }
    memset(app, 0, sizeof(*app));

    avocado_session_bootstrap(&app->data, furi_hal_rtc_get_timestamp());

    avocado_feedback_init(&app->feedback);

    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_navigation_event_callback(app->vd, avocado_navigation_stop);

    const bool show_welcome = avocado_onboarding_should_show();

    app->play = play_screen_alloc(&app->data, &app->feedback);
    if (!app->play) {
        view_dispatcher_free(app->vd);
        furi_record_close(RECORD_GUI);
        avocado_feedback_deinit(&app->feedback);
        free(app);
        return -1;
    }
    view_dispatcher_add_view(app->vd, AvocadoViewPlay, play_screen_get_view(app->play));

    if (show_welcome) {
        WelcomeScreenContext wctx = {
            .vd = app->vd,
            .view_game_index = AvocadoViewPlay,
            .feedback = &app->feedback,
        };
        app->welcome = welcome_screen_alloc(&wctx);
        if (!app->welcome) {
            view_dispatcher_remove_view(app->vd, AvocadoViewPlay);
            play_screen_free(app->play);
            view_dispatcher_free(app->vd);
            furi_record_close(RECORD_GUI);
            avocado_feedback_deinit(&app->feedback);
            free(app);
            return -1;
        }
        view_dispatcher_add_view(app->vd, AvocadoViewWelcome,
                                 welcome_screen_get_view(app->welcome));
        view_dispatcher_switch_to_view(app->vd, AvocadoViewWelcome);
    } else {
        view_dispatcher_switch_to_view(app->vd, AvocadoViewPlay);
    }

    view_dispatcher_run(app->vd);

    if (app->welcome) {
        view_dispatcher_remove_view(app->vd, AvocadoViewWelcome);
    }
    view_dispatcher_remove_view(app->vd, AvocadoViewPlay);

    welcome_screen_free(app->welcome);
    play_screen_free(app->play);

    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);

    avocado_feedback_deinit(&app->feedback);
    free(app);

    return 0;
}
