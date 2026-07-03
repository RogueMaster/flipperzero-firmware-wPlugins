/*
 * Startup warning scene
 */

#include "../tagtinker_app.h"
#include <gui/elements.h>

enum {
    TagTinkerStartupWarningContinue,
};

typedef struct {
    uint8_t page;
} WarningViewModel;

typedef struct {
    const char* title;
    const char* lines[2];
} WarningPage;

static const WarningPage startup_warning_pages[] = {
    {
        .title = "RESEARCH TOOL:",
        .lines =
            {
                "Educational tool for",
                "infrared ESL study.",
            },
    },
    {
        .title = "PERMISSION:",
        .lines =
            {
                "Use only on tags",
                "you own or may test.",
            },
    },
    {
        .title = "CAUTION:",
        .lines =
            {
                "Unauthorized use",
                "may be illegal.",
            },
    },
    {
        .title = "RESPONSIBILITY:",
        .lines =
            {
                "You are responsible",
                "for your actions.",
            },
    },
};

static const uint8_t startup_warning_page_count =
    sizeof(startup_warning_pages) / sizeof(startup_warning_pages[0]);

static bool warning_is_unlocked(const WarningViewModel* model) {
    return model->page >= (startup_warning_page_count - 1);
}

static void warning_draw_cb(Canvas* canvas, void* _model) {
    WarningViewModel* model = _model;
    bool unlocked = warning_is_unlocked(model);
    const WarningPage* page = &startup_warning_pages[model->page];

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 13, AlignCenter, AlignCenter, page->title);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 29, AlignCenter, AlignCenter, page->lines[0]);
    canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignCenter, page->lines[1]);

    if(model->page > 0) {
        elements_button_left(canvas, "Prev");
    }

    elements_button_right(canvas, unlocked ? "OK" : "Next");
}

static bool warning_input_cb(InputEvent* event, void* context) {
    TagTinkerApp* app = context;
    bool handled = false;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    WarningViewModel* model = view_get_model(app->warning_view);

    switch(event->key) {
    case InputKeyDown:
    case InputKeyRight:
        if(model->page + 1 < startup_warning_page_count) {
            model->page++;
            handled = true;
        }
        break;
    case InputKeyUp:
    case InputKeyLeft:
        if(model->page > 0) {
            model->page--;
            handled = true;
        }
        break;
    case InputKeyOk:
        if(event->type == InputTypeShort) {
            if(warning_is_unlocked(model)) {
                handled = true;
                view_commit_model(app->warning_view, false);
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, TagTinkerStartupWarningContinue);
                return true;
            } else if(model->page + 1 < startup_warning_page_count) {
                model->page++;
                handled = true;
            }
        }
        break;
    default:
        break;
    }

    view_commit_model(app->warning_view, handled);
    return handled;
}

void tagtinker_scene_warning_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    if(!app->warning_view_allocated) {
        view_allocate_model(app->warning_view, ViewModelTypeLockFree, sizeof(WarningViewModel));
        view_set_context(app->warning_view, app);
        view_set_draw_callback(app->warning_view, warning_draw_cb);
        view_set_input_callback(app->warning_view, warning_input_cb);
        app->warning_view_allocated = true;
    }

    WarningViewModel* model = view_get_model(app->warning_view);
    model->page = 0;
    view_commit_model(app->warning_view, true);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewWarning);
}

bool tagtinker_scene_warning_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == TagTinkerStartupWarningContinue) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, TagTinkerSceneMainMenu);
        return true;
    }

    return false;
}

void tagtinker_scene_warning_on_exit(void* ctx) {
    UNUSED(ctx);
}
