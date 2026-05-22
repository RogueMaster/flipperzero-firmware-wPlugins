#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "cvss31.h"

#include <stdbool.h>
#include <stdio.h>

#define TAG "PocketCVSS"

typedef enum {
    PocketCvssViewMainMenu,
    PocketCvssViewMetricMenu,
    PocketCvssViewInfo,
} PocketCvssViewId;

typedef enum {
    PocketCvssMainMenuNewScore,
    PocketCvssMainMenuAbout,
} PocketCvssMainMenuItem;

typedef enum {
    PocketCvssInfoResult,
    PocketCvssInfoExplain,
    PocketCvssInfoAbout,
} PocketCvssInfoScreen;

typedef struct {
    PocketCvssInfoScreen screen;
    Cvss31BaseVector vector;
} PocketCvssInfoModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    Submenu* main_menu;
    Submenu* metric_menu;
    View* info_view;
    Cvss31MetricId metric_id;
    PocketCvssViewId current_view;
    PocketCvssInfoScreen info_screen;
    char metric_header[32];
} PocketCvssApp;

static void pocket_cvss_metric_callback(void* context, uint32_t index);
static void pocket_cvss_main_menu_callback(void* context, uint32_t index);

static void pocket_cvss_switch_to(PocketCvssApp* app, PocketCvssViewId view_id) {
    app->current_view = view_id;
    view_dispatcher_switch_to_view(app->dispatcher, view_id);
}

static void pocket_cvss_draw_ok_icon(Canvas* canvas, uint8_t x, uint8_t y) {
    const uint8_t ok_icon[] = {0x18, 0x24, 0x42, 0x42, 0x42, 0x24, 0x18};

    canvas_draw_xbm(canvas, x, y, 7, 7, ok_icon);
}

static void pocket_cvss_draw_back_icon(Canvas* canvas, uint8_t x, uint8_t y) {
    const uint8_t back_icon[] = {0x04, 0x06, 0x7f, 0x46, 0x44, 0x40, 0x3c};

    canvas_draw_xbm(canvas, x, y, 7, 7, back_icon);
}

static void pocket_cvss_draw_footer(Canvas* canvas, const char* ok_text, const char* back_text) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, 54, 127, 54);

    if(ok_text) {
        pocket_cvss_draw_ok_icon(canvas, 5, 57);
        canvas_draw_str(canvas, 17, 63, ok_text);
    }

    if(back_text) {
        const uint8_t icon_x = ok_text ? 72 : 8;
        const uint8_t text_x = ok_text ? 84 : 20;

        pocket_cvss_draw_back_icon(canvas, icon_x, 57);
        canvas_draw_str(canvas, text_x, 63, back_text);
    }
}

static void pocket_cvss_draw_bullet(Canvas* canvas, uint8_t y, const char* text) {
    canvas_draw_disc(canvas, 3, y - 3, 1);
    canvas_draw_str(canvas, 8, y, text);
}

static void pocket_cvss_draw_severity_badge(Canvas* canvas, const char* severity) {
    canvas_set_font(canvas, FontSecondary);
    const uint8_t width = canvas_string_width(canvas, severity) + 8;
    const uint8_t x = 127 - width;

    canvas_draw_rbox(canvas, x, 14, width, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, x + (width / 2), 24, AlignCenter, AlignBottom, severity);
    canvas_set_color(canvas, ColorBlack);
}

static void pocket_cvss_draw_result(Canvas* canvas, const Cvss31BaseVector* vector) {
    char score_text[8];
    char metric_line1[40];
    char metric_line2[40];
    const Cvss31Score score = cvss31_base_score(vector);

    cvss31_format_score(score_text, sizeof(score_text), score.tenths);
    cvss31_format_metric_line(vector, metric_line1, sizeof(metric_line1), 0, 3);
    cvss31_format_metric_line(vector, metric_line2, sizeof(metric_line2), 4, 7);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 8, "CVSS v3.1 Base");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 25, score_text);
    pocket_cvss_draw_severity_badge(canvas, score.severity);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 36, "CVSS:3.1/...");
    canvas_draw_str(canvas, 0, 45, metric_line1);
    canvas_draw_str(canvas, 0, 52, metric_line2);

    pocket_cvss_draw_footer(canvas, "Explain", "Edit");
}

static void pocket_cvss_draw_explain(Canvas* canvas, const Cvss31BaseVector* vector) {
    char title[24];
    char metric_line[40];
    const Cvss31Score score = cvss31_base_score(vector);

    snprintf(title, sizeof(title), "Why %s?", score.severity);
    cvss31_format_metric_line(vector, metric_line, sizeof(metric_line), 0, 3);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, title);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 23, metric_line);
    pocket_cvss_draw_bullet(canvas, 31, cvss31_explain_attack_vector(vector));
    pocket_cvss_draw_bullet(canvas, 38, cvss31_explain_attack_complexity(vector));
    pocket_cvss_draw_bullet(canvas, 45, cvss31_explain_privileges_required(vector));
    pocket_cvss_draw_bullet(canvas, 52, cvss31_explain_impact(vector));
    pocket_cvss_draw_footer(canvas, NULL, "Result");
}

static void pocket_cvss_draw_about(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "PocketCVSS");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 24, "v0.1");
    canvas_draw_str(canvas, 0, 35, "CVSS v3.1 Base");
    canvas_draw_str(canvas, 0, 46, "Repo:");
    canvas_draw_str(canvas, 28, 46, "github.com/vavkamil");
    canvas_draw_str(canvas, 28, 53, "/pocket-cvss");

    pocket_cvss_draw_footer(canvas, NULL, "Menu");
}

static void pocket_cvss_info_draw(Canvas* canvas, void* model_context) {
    const PocketCvssInfoModel* model = model_context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->screen == PocketCvssInfoResult) {
        pocket_cvss_draw_result(canvas, &model->vector);
    } else if(model->screen == PocketCvssInfoExplain) {
        pocket_cvss_draw_explain(canvas, &model->vector);
    } else {
        pocket_cvss_draw_about(canvas);
    }
}

static void pocket_cvss_show_result(PocketCvssApp* app) {
    app->info_screen = PocketCvssInfoResult;

    with_view_model(
        app->info_view,
        PocketCvssInfoModel * model,
        { model->screen = PocketCvssInfoResult; },
        true);

    pocket_cvss_switch_to(app, PocketCvssViewInfo);
}

static void pocket_cvss_show_explain(PocketCvssApp* app) {
    app->info_screen = PocketCvssInfoExplain;

    with_view_model(
        app->info_view,
        PocketCvssInfoModel * model,
        { model->screen = PocketCvssInfoExplain; },
        true);

    pocket_cvss_switch_to(app, PocketCvssViewInfo);
}

static void pocket_cvss_show_about(PocketCvssApp* app) {
    app->info_screen = PocketCvssInfoAbout;

    with_view_model(
        app->info_view,
        PocketCvssInfoModel * model,
        { model->screen = PocketCvssInfoAbout; },
        true);

    pocket_cvss_switch_to(app, PocketCvssViewInfo);
}

static bool pocket_cvss_metric_previous(Cvss31MetricId metric_id, Cvss31MetricId* previous) {
    if(metric_id == Cvss31MetricAttackVector || metric_id >= Cvss31MetricCount) {
        return false;
    }

    *previous = (Cvss31MetricId)(metric_id - 1);
    return true;
}

static bool pocket_cvss_metric_next(Cvss31MetricId metric_id, Cvss31MetricId* next) {
    if(metric_id >= Cvss31MetricAvailability) {
        return false;
    }

    *next = (Cvss31MetricId)(metric_id + 1);
    return true;
}

static void pocket_cvss_prepare_metric_menu(PocketCvssApp* app) {
    const Cvss31Metric* metric = cvss31_metric_get(app->metric_id);

    if(!metric) {
        pocket_cvss_switch_to(app, PocketCvssViewMainMenu);
        return;
    }

    snprintf(
        app->metric_header,
        sizeof(app->metric_header),
        "%u/%u %s",
        (unsigned)app->metric_id + 1,
        (unsigned)cvss31_metric_count(),
        metric->title);

    submenu_reset(app->metric_menu);
    submenu_set_header(app->metric_menu, app->metric_header);

    for(uint8_t i = 0; i < metric->option_count; i++) {
        submenu_add_item(
            app->metric_menu, metric->options[i].label, i, pocket_cvss_metric_callback, app);
    }

    with_view_model(
        app->info_view,
        PocketCvssInfoModel * model,
        {
            submenu_set_selected_item(
                app->metric_menu, cvss31_base_vector_get(&model->vector, app->metric_id));
        },
        false);
}

static void pocket_cvss_start_score(PocketCvssApp* app) {
    with_view_model(
        app->info_view,
        PocketCvssInfoModel * model,
        { cvss31_base_vector_reset(&model->vector); },
        true);

    app->metric_id = Cvss31MetricAttackVector;
    pocket_cvss_prepare_metric_menu(app);
    pocket_cvss_switch_to(app, PocketCvssViewMetricMenu);
}

static void pocket_cvss_metric_callback(void* context, uint32_t index) {
    PocketCvssApp* app = context;
    Cvss31MetricId next_metric;

    with_view_model(
        app->info_view,
        PocketCvssInfoModel * model,
        {
            if(index <= UINT8_MAX) {
                cvss31_base_vector_set(&model->vector, app->metric_id, (uint8_t)index);
            }
        },
        true);

    if(!pocket_cvss_metric_next(app->metric_id, &next_metric)) {
        pocket_cvss_show_result(app);
    } else {
        app->metric_id = next_metric;
        pocket_cvss_prepare_metric_menu(app);
        pocket_cvss_switch_to(app, PocketCvssViewMetricMenu);
    }
}

static void pocket_cvss_main_menu_callback(void* context, uint32_t index) {
    PocketCvssApp* app = context;

    if(index == PocketCvssMainMenuNewScore) {
        pocket_cvss_start_score(app);
    } else if(index == PocketCvssMainMenuAbout) {
        pocket_cvss_show_about(app);
    }
}

static bool pocket_cvss_info_input(InputEvent* event, void* context) {
    PocketCvssApp* app = context;

    if(event->type != InputTypeShort) {
        return false;
    }

    if(app->current_view == PocketCvssViewInfo && app->info_screen == PocketCvssInfoResult) {
        if(event->key == InputKeyOk || event->key == InputKeyRight) {
            pocket_cvss_show_explain(app);
            return true;
        }

        if(event->key == InputKeyBack || event->key == InputKeyLeft) {
            app->metric_id = Cvss31MetricAvailability;
            pocket_cvss_prepare_metric_menu(app);
            pocket_cvss_switch_to(app, PocketCvssViewMetricMenu);
            return true;
        }
    } else if(app->current_view == PocketCvssViewInfo && app->info_screen == PocketCvssInfoExplain) {
        if(event->key == InputKeyBack || event->key == InputKeyLeft || event->key == InputKeyOk) {
            pocket_cvss_show_result(app);
            return true;
        }
    } else if(app->current_view == PocketCvssViewInfo && app->info_screen == PocketCvssInfoAbout) {
        if(event->key == InputKeyBack || event->key == InputKeyLeft || event->key == InputKeyOk) {
            pocket_cvss_switch_to(app, PocketCvssViewMainMenu);
            return true;
        }
    }

    return false;
}

static bool pocket_cvss_navigation_callback(void* context) {
    PocketCvssApp* app = context;

    if(app->current_view == PocketCvssViewMainMenu) {
        view_dispatcher_stop(app->dispatcher);
        return true;
    }

    if(app->current_view == PocketCvssViewMetricMenu) {
        Cvss31MetricId previous_metric;

        if(!pocket_cvss_metric_previous(app->metric_id, &previous_metric)) {
            pocket_cvss_switch_to(app, PocketCvssViewMainMenu);
        } else {
            app->metric_id = previous_metric;
            pocket_cvss_prepare_metric_menu(app);
            pocket_cvss_switch_to(app, PocketCvssViewMetricMenu);
        }

        return true;
    }

    if(app->current_view == PocketCvssViewInfo) {
        if(app->info_screen == PocketCvssInfoResult) {
            app->metric_id = Cvss31MetricAvailability;
            pocket_cvss_prepare_metric_menu(app);
            pocket_cvss_switch_to(app, PocketCvssViewMetricMenu);
        } else if(app->info_screen == PocketCvssInfoExplain) {
            pocket_cvss_show_result(app);
        } else {
            pocket_cvss_switch_to(app, PocketCvssViewMainMenu);
        }

        return true;
    }

    pocket_cvss_switch_to(app, PocketCvssViewMainMenu);
    return true;
}

static PocketCvssApp* pocket_cvss_app_alloc(void) {
    PocketCvssApp* app = malloc(sizeof(PocketCvssApp));
    furi_check(app);

    app->gui = furi_record_open(RECORD_GUI);
    app->dispatcher = view_dispatcher_alloc();
    app->main_menu = submenu_alloc();
    app->metric_menu = submenu_alloc();
    app->info_view = view_alloc();
    furi_check(app->gui);
    furi_check(app->dispatcher);
    furi_check(app->main_menu);
    furi_check(app->metric_menu);
    furi_check(app->info_view);

    app->metric_id = Cvss31MetricAttackVector;
    app->current_view = PocketCvssViewMainMenu;
    app->info_screen = PocketCvssInfoResult;
    app->metric_header[0] = '\0';

    view_allocate_model(app->info_view, ViewModelTypeLocking, sizeof(PocketCvssInfoModel));
    view_set_draw_callback(app->info_view, pocket_cvss_info_draw);
    view_set_input_callback(app->info_view, pocket_cvss_info_input);
    view_set_context(app->info_view, app);

    with_view_model(
        app->info_view,
        PocketCvssInfoModel * model,
        {
            model->screen = PocketCvssInfoResult;
            cvss31_base_vector_reset(&model->vector);
        },
        false);

    submenu_set_header(app->main_menu, "PocketCVSS");
    submenu_add_item(
        app->main_menu,
        "New v3.1 Score",
        PocketCvssMainMenuNewScore,
        pocket_cvss_main_menu_callback,
        app);
    submenu_add_item(
        app->main_menu, "About", PocketCvssMainMenuAbout, pocket_cvss_main_menu_callback, app);

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->dispatcher, pocket_cvss_navigation_callback);
    view_dispatcher_add_view(
        app->dispatcher, PocketCvssViewMainMenu, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(
        app->dispatcher, PocketCvssViewMetricMenu, submenu_get_view(app->metric_menu));
    view_dispatcher_add_view(app->dispatcher, PocketCvssViewInfo, app->info_view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    pocket_cvss_switch_to(app, PocketCvssViewMainMenu);

    return app;
}

static void pocket_cvss_app_free(PocketCvssApp* app) {
    view_dispatcher_remove_view(app->dispatcher, PocketCvssViewMainMenu);
    view_dispatcher_remove_view(app->dispatcher, PocketCvssViewMetricMenu);
    view_dispatcher_remove_view(app->dispatcher, PocketCvssViewInfo);
    view_free_model(app->info_view);
    view_free(app->info_view);
    submenu_free(app->metric_menu);
    submenu_free(app->main_menu);
    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t pocket_cvss_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "Starting PocketCVSS");
    PocketCvssApp* app = pocket_cvss_app_alloc();
    view_dispatcher_run(app->dispatcher);
    pocket_cvss_app_free(app);
    FURI_LOG_I(TAG, "Stopped PocketCVSS");

    return 0;
}
