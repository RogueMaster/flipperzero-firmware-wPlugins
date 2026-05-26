#include "include/app/trivia_zero_app.h"
#include "include/domain/anti_repeat.h"
#include "include/domain/category.h"
#include "include/domain/history_buffer.h"
#include "include/domain/question_pool.h"
#include "include/i18n/strings.h"
#include "include/infrastructure/pack_reader.h"
#include "include/infrastructure/settings_storage.h"
#include "include/platform/random_port.h"
#include "include/ui/question_view.h"
#include "include/version.h"

#include <stdio.h>

#ifdef __has_include
#if __has_include(<furi.h>)
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/view_dispatcher.h>
#include <stdlib.h>
#define TZ_HAVE_FURI 1
#endif
#endif

#ifdef TZ_HAVE_FURI

typedef enum {
    AppViewLangSelect = 0,
    AppViewQuestion,
    AppViewMenu,
    AppViewCredits,
} AppView;

typedef enum {
    LangSelectIdEs = 0,
    LangSelectIdEn = 1,
} LangSelectId;

typedef enum {
    MenuIdChangeLang = 0,
    MenuIdCredits = 1,
    MenuIdExit = 2,
} MenuId;

#define CREDITS_BUF_SIZE 128u

typedef struct {
    Gui *gui;
    ViewDispatcher *vd;

    Submenu *lang_menu;
    Submenu *back_menu;
    Widget *credits;
    QuestionView *qview;

    Question current;
    AntiRepeat seen;
    HistoryBuffer history;
    Settings settings;
    AppView current_view;
    char credits_buf[CREDITS_BUF_SIZE];
    bool exit_requested;
} App;

static void on_lang_pick(void *ctx, uint32_t selected);
static void on_menu_pick(void *ctx, uint32_t selected);

static void app_switch(App *app, AppView v) {
    app->current_view = v;
    view_dispatcher_switch_to_view(app->vd, (uint32_t)v);
}

static void app_show_question_for_id(App *app, uint32_t id) {
    if (pack_get_by_id(id, &app->current)) {
        anti_repeat_mark(&app->seen, id);
        history_buffer_push(&app->history, id);
        question_view_set_question(app->qview, &app->current);
        app->settings.last_id = id;
        app->settings.last_id_valid = true;
    }
}

static void app_show_random(App *app) {
    uint32_t id;
    bool reset_happened;
    if (question_pool_next(pack_count(), &app->seen, tz_rng_u32, NULL, &id, &reset_happened)) {
        app_show_question_for_id(app, id);
    }
}

static void app_open_pack_for_current_lang(App *app) {
    /* The pack is embedded in the FAP binary as `const` arrays, so opening
     * is essentially free and cannot fail under normal operation. If it
     * does fail (corrupt/missing magic — would mean a build error), we let
     * the UI come up anyway with an empty pool rather than killing the app
     * silently. The user sees no questions instead of a sudden close. */
    (void)pack_open(app->settings.lang);
}

static void app_rebuild_back_menu(App *app) {
    submenu_reset(app->back_menu);
    submenu_add_item(app->back_menu, tz_str(TzStrMenuChangeLang), MenuIdChangeLang, on_menu_pick,
                     app);
    submenu_add_item(app->back_menu, tz_str(TzStrMenuCredits), MenuIdCredits, on_menu_pick, app);
    submenu_add_item(app->back_menu, tz_str(TzStrMenuExit), MenuIdExit, on_menu_pick, app);
}

static void app_rebuild_lang_menu(App *app) {
    submenu_reset(app->lang_menu);
    submenu_set_header(app->lang_menu, tz_str(TzStrLangPickHeader));
    submenu_add_item(app->lang_menu, tz_str(TzStrLangSpanish), LangSelectIdEs, on_lang_pick, app);
    submenu_add_item(app->lang_menu, tz_str(TzStrLangEnglish), LangSelectIdEn, on_lang_pick, app);
}

static void app_rebuild_credits(App *app) {
    widget_reset(app->credits);
    snprintf(app->credits_buf, sizeof(app->credits_buf), "%s%s", tz_str(TzStrCreditsBody),
             APP_VERSION);
    widget_add_text_scroll_element(app->credits, 0, 0, 128, 46, app->credits_buf);
    widget_add_string_element(app->credits, 64, 48, AlignCenter, AlignTop, FontSecondary,
                              tz_str(TzStrCreditsRepoLine1));
    widget_add_string_element(app->credits, 64, 56, AlignCenter, AlignTop, FontSecondary,
                              tz_str(TzStrCreditsRepoLine2));
}

static void on_qview_action(void *ctx, QViewAction action) {
    App *app = ctx;
    switch (action) {
        case QViewActionReveal:
            question_view_set_mode(app->qview, QViewModeAnswer);
            break;
        case QViewActionNext:
            app_show_random(app);
            question_view_set_mode(app->qview, QViewModeQuestion);
            break;
        case QViewActionPrev: {
            uint32_t prev_id;
            if (history_buffer_peek_back(&app->history, 1u, &prev_id)) {
                if (pack_get_by_id(prev_id, &app->current)) {
                    question_view_set_question(app->qview, &app->current);
                }
            }
            break;
        }
        case QViewActionScrollUp:
        case QViewActionScrollDown:
            /* Scroll is currently view-local state; expansion deferred. */
            break;
        case QViewActionMenu:
            app_rebuild_back_menu(app);
            app_switch(app, AppViewMenu);
            break;
    }
}

static void on_lang_pick(void *ctx, uint32_t selected) {
    App *app = ctx;
    app->settings.lang = (selected == LangSelectIdEs) ? LangEs : LangEn;
    tz_locale_set(app->settings.lang);
    settings_save(&app->settings);
    pack_close();
    app_open_pack_for_current_lang(app);
    anti_repeat_reset(&app->seen);
    history_buffer_clear(&app->history);
    app_show_random(app);
    app_switch(app, AppViewQuestion);
}

static void on_menu_pick(void *ctx, uint32_t selected) {
    App *app = ctx;
    switch (selected) {
        case MenuIdChangeLang:
            app_rebuild_lang_menu(app);
            app_switch(app, AppViewLangSelect);
            break;
        case MenuIdCredits:
            app_rebuild_credits(app);
            app_switch(app, AppViewCredits);
            break;
        default:
            app->exit_requested = true;
            view_dispatcher_stop(app->vd);
            break;
    }
}

static bool app_nav(void *context) {
    App *app = context;
    switch (app->current_view) {
        case AppViewMenu:
            app_switch(app, AppViewQuestion);
            return true;
        case AppViewCredits:
            app_rebuild_back_menu(app);
            app_switch(app, AppViewMenu);
            return true;
        case AppViewLangSelect:
            if (app->settings.last_id_valid) {
                app_switch(app, AppViewQuestion);
                return true;
            }
            return false;
        default:
            return false;
    }
}

int32_t trivia_zero_app_run(void) {
    App *app = malloc(sizeof(App));
    if (!app)
        return -1;
    *app = (App){0};

    app->settings = settings_default();
    const bool settings_ok = settings_load(&app->settings);
    tz_locale_set(app->settings.lang);
    anti_repeat_init(&app->seen);
    history_buffer_init(&app->history);

    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_navigation_event_callback(app->vd, app_nav);

    /* Submenu callbacks are attached per-item by app_rebuild_*_menu — there
     * is no submenu-wide context setter on this Furi version. */
    app->lang_menu = submenu_alloc();
    app_rebuild_lang_menu(app);
    view_dispatcher_add_view(app->vd, AppViewLangSelect, submenu_get_view(app->lang_menu));

    app->qview = question_view_alloc();
    question_view_set_callback(app->qview, app, on_qview_action);
    view_dispatcher_add_view(app->vd, AppViewQuestion, question_view_get_view(app->qview));

    app->back_menu = submenu_alloc();
    app_rebuild_back_menu(app);
    view_dispatcher_add_view(app->vd, AppViewMenu, submenu_get_view(app->back_menu));

    app->credits = widget_alloc();
    view_dispatcher_add_view(app->vd, AppViewCredits, widget_get_view(app->credits));

    if (!settings_ok) {
        /* First run (settings file missing/corrupt) → show language picker. */
        app_switch(app, AppViewLangSelect);
    } else {
        app_open_pack_for_current_lang(app);
        if (app->settings.last_id_valid && app->settings.last_id < pack_count()) {
            app_show_question_for_id(app, app->settings.last_id);
        } else {
            app_show_random(app);
        }
        app_switch(app, AppViewQuestion);
    }

    view_dispatcher_run(app->vd);

    settings_save(&app->settings);
    pack_close();

    view_dispatcher_remove_view(app->vd, AppViewLangSelect);
    view_dispatcher_remove_view(app->vd, AppViewQuestion);
    view_dispatcher_remove_view(app->vd, AppViewMenu);
    view_dispatcher_remove_view(app->vd, AppViewCredits);
    submenu_free(app->lang_menu);
    submenu_free(app->back_menu);
    widget_free(app->credits);
    question_view_free(app->qview);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}

#else /* !TZ_HAVE_FURI — host build */

int32_t trivia_zero_app_run(void) {
    /* Host build: composition root cannot run without Furi. Returning 0 keeps
     * the symbol resolvable for cppcheck; real execution happens on device. */
    return 0;
}

#endif
