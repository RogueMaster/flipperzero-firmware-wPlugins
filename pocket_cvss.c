/*
 * Pocket CVSS Flipper application UI.
 *
 * Owns the app lifecycle, menu navigation, custom result/vector/about views,
 * and user input flow for selecting CVSS v3.1 base metrics on-device. The
 * scoring and vector formatting rules live in cvss31.c; this file focuses on
 * presenting those rules through Flipper's GUI and ViewDispatcher APIs.
 */
#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "cvss31.h"

#include <stdbool.h>
#include <stdio.h>

#define TAG                             "PocketCVSS"
#define POCKET_CVSS_RESULT_VISIBLE_ROWS 4
#define POCKET_CVSS_RESULT_ROW_COUNT    CVSS31_METRIC_COUNT

typedef enum {
    PocketCvssViewMainMenu,
    PocketCvssViewMetricMenu,
    PocketCvssViewExamplesMenu,
    PocketCvssViewInfo,
} PocketCvssViewId;

typedef enum {
    PocketCvssMainMenuNewScore,
    PocketCvssMainMenuExamples,
    PocketCvssMainMenuAbout,
} PocketCvssMainMenuItem;

typedef enum {
    PocketCvssInfoResult,
    PocketCvssInfoVector,
    PocketCvssInfoAbout,
} PocketCvssInfoScreen;

typedef struct {
    PocketCvssInfoScreen screen;
    Cvss31BaseVector vector;
    uint8_t result_scroll;
} PocketCvssInfoModel;

typedef struct {
    const char* label;
    uint8_t values[CVSS31_METRIC_COUNT];
} PocketCvssExample;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    Submenu* main_menu;
    Submenu* metric_menu;
    Submenu* examples_menu;
    View* info_view;
    Cvss31MetricId metric_id;
    PocketCvssViewId current_view;
    PocketCvssInfoScreen info_screen;
    char metric_header[32];
} PocketCvssApp;

static void pocket_cvss_metric_callback(void* context, uint32_t index);
static void pocket_cvss_example_callback(void* context, uint32_t index);
static void pocket_cvss_main_menu_callback(void* context, uint32_t index);

static const PocketCvssExample pocket_cvss_examples[] = {
    {
        .label = "10.0 RCE",
        .values = {0, 0, 0, 0, 1, 2, 2, 2},
    },
    {
        .label = "9.8 Auth Bypass",
        .values = {0, 0, 0, 0, 0, 2, 2, 2},
    },
    {
        .label = "6.5 DLS Bypass",
        .values = {0, 0, 1, 0, 0, 2, 0, 0},
    },
};

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
        const uint8_t text_gap = 12;
        const uint8_t icon_x =
            ok_text ? 72 : (uint8_t)(127 - text_gap - canvas_string_width(canvas, back_text));
        const uint8_t text_x = icon_x + text_gap;

        pocket_cvss_draw_back_icon(canvas, icon_x, 57);
        canvas_draw_str(canvas, text_x, 63, back_text);
    }
}

static void pocket_cvss_draw_summary(Canvas* canvas, const Cvss31Score* score) {
    char score_text[8];
    char summary_text[20];

    cvss31_format_score(score_text, sizeof(score_text), score->tenths);
    snprintf(summary_text, sizeof(summary_text), "%s %s", score_text, score->severity);

    canvas_draw_rbox(canvas, 0, 0, 128, 16, 2);
    canvas_set_color(canvas, ColorWhite);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 5, 13, summary_text);

    canvas_set_color(canvas, ColorBlack);
}

static const Cvss31Option* opt(const Cvss31BaseVector* vector, Cvss31MetricId metric_id) {
    const Cvss31Metric* metric = cvss31_metric_get(metric_id);
    return &metric->options[cvss31_base_vector_get(vector, metric_id)];
}

static void pocket_cvss_set_vector(Cvss31BaseVector* vector, const uint8_t* values) {
    for(uint8_t i = 0; i < CVSS31_METRIC_COUNT; i++) {
        cvss31_base_vector_set(vector, (Cvss31MetricId)i, values[i]);
    }
}

static const char* pocket_cvss_row_label(Cvss31MetricId metric_id) {
    if(metric_id == Cvss31MetricAttackVector) return "Attack vector";
    if(metric_id == Cvss31MetricAttackComplexity) return "Attack complexity";
    if(metric_id == Cvss31MetricPrivilegesRequired) return "Privileges required";
    if(metric_id == Cvss31MetricUserInteraction) return "User interaction";
    if(metric_id == Cvss31MetricScope) return "Scope";
    if(metric_id == Cvss31MetricConfidentiality) return "Confidentiality";
    if(metric_id == Cvss31MetricIntegrity) return "Integrity";
    return "Availability";
}

static void pocket_cvss_draw_scrollbar(Canvas* canvas, uint8_t scroll) {
    const uint8_t thumb_y = 18 + (scroll * 5);

    canvas_draw_line(canvas, 126, 18, 126, 53);
    canvas_draw_box(canvas, 125, thumb_y, 3, 16);
}

static void pocket_cvss_draw_result(Canvas* canvas, const PocketCvssInfoModel* model) {
    const Cvss31BaseVector* vector = &model->vector;
    const Cvss31Score score = cvss31_base_score(vector);

    pocket_cvss_draw_summary(canvas, &score);

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < POCKET_CVSS_RESULT_VISIBLE_ROWS; i++) {
        const Cvss31MetricId metric_id = (Cvss31MetricId)(model->result_scroll + i);
        const uint8_t row_y = 26 + (i * 9);

        canvas_draw_str(canvas, 0, row_y, pocket_cvss_row_label(metric_id));
        canvas_draw_str_aligned(
            canvas, 122, row_y, AlignRight, AlignBottom, opt(vector, metric_id)->label);
    }

    pocket_cvss_draw_scrollbar(canvas, model->result_scroll);

    pocket_cvss_draw_footer(canvas, "Vector", "Edit");
}

static void pocket_cvss_draw_vector(Canvas* canvas, const Cvss31BaseVector* vector) {
    const Cvss31Score score = cvss31_base_score(vector);

    pocket_cvss_draw_summary(canvas, &score);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 27, "CVSS:3.1");

    for(uint8_t i = 0; i < CVSS31_METRIC_COUNT; i++) {
        const Cvss31MetricId metric_id = (Cvss31MetricId)i;
        const Cvss31Metric* metric = cvss31_metric_get(metric_id);
        const Cvss31Option* option = opt(vector, metric_id);
        char token[8];

        snprintf(token, sizeof(token), "%s:%s", metric->metric_code, option->code);
        canvas_draw_str(canvas, (i % 4) * 32, 40 + ((i / 4) * 11), token);
    }

    pocket_cvss_draw_footer(canvas, "Exit", "Result");
}

static void pocket_cvss_draw_about(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 9, "Pocket CVSS");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 22, "Offline CVSS trainer");
    canvas_draw_str_aligned(canvas, 127, 22, AlignRight, AlignBottom, "v0.1");
    canvas_draw_line(canvas, 0, 27, 127, 27);
    canvas_draw_str(canvas, 0, 39, "github.com");
    canvas_draw_str(canvas, 0, 48, "vavkamil/pocket-cvss");

    pocket_cvss_draw_footer(canvas, NULL, "Menu");
}

static void pocket_cvss_info_draw(Canvas* canvas, void* model_context) {
    const PocketCvssInfoModel* model = model_context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->screen == PocketCvssInfoResult) {
        pocket_cvss_draw_result(canvas, model);
    } else if(model->screen == PocketCvssInfoVector) {
        pocket_cvss_draw_vector(canvas, &model->vector);
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

static void pocket_cvss_show_vector(PocketCvssApp* app) {
    app->info_screen = PocketCvssInfoVector;

    with_view_model(
        app->info_view,
        PocketCvssInfoModel * model,
        { model->screen = PocketCvssInfoVector; },
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

static uint8_t pocket_cvss_result_max_scroll(void) {
    return POCKET_CVSS_RESULT_ROW_COUNT - POCKET_CVSS_RESULT_VISIBLE_ROWS;
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
        {
            cvss31_base_vector_reset(&model->vector);
            model->result_scroll = 0;
        },
        true);

    app->metric_id = Cvss31MetricAttackVector;
    pocket_cvss_prepare_metric_menu(app);
    pocket_cvss_switch_to(app, PocketCvssViewMetricMenu);
}

static void pocket_cvss_show_examples(PocketCvssApp* app) {
    submenu_set_selected_item(app->examples_menu, 0);
    pocket_cvss_switch_to(app, PocketCvssViewExamplesMenu);
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

static void pocket_cvss_example_callback(void* context, uint32_t index) {
    PocketCvssApp* app = context;

    if(index >= sizeof(pocket_cvss_examples) / sizeof(pocket_cvss_examples[0])) {
        pocket_cvss_switch_to(app, PocketCvssViewMainMenu);
        return;
    }

    with_view_model(
        app->info_view,
        PocketCvssInfoModel * model,
        {
            pocket_cvss_set_vector(&model->vector, pocket_cvss_examples[index].values);
            model->result_scroll = 0;
        },
        true);

    pocket_cvss_show_result(app);
}

static void pocket_cvss_main_menu_callback(void* context, uint32_t index) {
    PocketCvssApp* app = context;

    if(index == PocketCvssMainMenuNewScore) {
        pocket_cvss_start_score(app);
    } else if(index == PocketCvssMainMenuExamples) {
        pocket_cvss_show_examples(app);
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
        if(event->key == InputKeyUp || event->key == InputKeyDown) {
            with_view_model(
                app->info_view,
                PocketCvssInfoModel * model,
                {
                    if(event->key == InputKeyUp && model->result_scroll > 0) {
                        model->result_scroll--;
                    } else if(
                        event->key == InputKeyDown &&
                        model->result_scroll < pocket_cvss_result_max_scroll()) {
                        model->result_scroll++;
                    }
                },
                true);
            return true;
        }

        if(event->key == InputKeyOk || event->key == InputKeyRight) {
            pocket_cvss_show_vector(app);
            return true;
        }

        if(event->key == InputKeyBack || event->key == InputKeyLeft) {
            app->metric_id = Cvss31MetricAvailability;
            pocket_cvss_prepare_metric_menu(app);
            pocket_cvss_switch_to(app, PocketCvssViewMetricMenu);
            return true;
        }
    } else if(app->current_view == PocketCvssViewInfo && app->info_screen == PocketCvssInfoVector) {
        if(event->key == InputKeyOk) {
            view_dispatcher_stop(app->dispatcher);
            return true;
        }

        if(event->key == InputKeyBack || event->key == InputKeyLeft) {
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

    if(app->current_view == PocketCvssViewExamplesMenu) {
        pocket_cvss_switch_to(app, PocketCvssViewMainMenu);
        return true;
    }

    if(app->current_view == PocketCvssViewInfo) {
        if(app->info_screen == PocketCvssInfoResult) {
            app->metric_id = Cvss31MetricAvailability;
            pocket_cvss_prepare_metric_menu(app);
            pocket_cvss_switch_to(app, PocketCvssViewMetricMenu);
        } else if(app->info_screen == PocketCvssInfoVector) {
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
    app->examples_menu = submenu_alloc();
    app->info_view = view_alloc();
    furi_check(app->gui);
    furi_check(app->dispatcher);
    furi_check(app->main_menu);
    furi_check(app->metric_menu);
    furi_check(app->examples_menu);
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
            model->result_scroll = 0;
        },
        false);

    submenu_set_header(app->main_menu, "Pocket CVSS");
    submenu_add_item(
        app->main_menu,
        "New Score",
        PocketCvssMainMenuNewScore,
        pocket_cvss_main_menu_callback,
        app);
    submenu_add_item(
        app->main_menu,
        "Examples",
        PocketCvssMainMenuExamples,
        pocket_cvss_main_menu_callback,
        app);
    submenu_add_item(
        app->main_menu, "About", PocketCvssMainMenuAbout, pocket_cvss_main_menu_callback, app);

    submenu_set_header(app->examples_menu, "Examples");
    for(uint8_t i = 0; i < sizeof(pocket_cvss_examples) / sizeof(pocket_cvss_examples[0]); i++) {
        submenu_add_item(
            app->examples_menu,
            pocket_cvss_examples[i].label,
            i,
            pocket_cvss_example_callback,
            app);
    }

    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->dispatcher, pocket_cvss_navigation_callback);
    view_dispatcher_add_view(
        app->dispatcher, PocketCvssViewMainMenu, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(
        app->dispatcher, PocketCvssViewMetricMenu, submenu_get_view(app->metric_menu));
    view_dispatcher_add_view(
        app->dispatcher, PocketCvssViewExamplesMenu, submenu_get_view(app->examples_menu));
    view_dispatcher_add_view(app->dispatcher, PocketCvssViewInfo, app->info_view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    pocket_cvss_switch_to(app, PocketCvssViewMainMenu);

    return app;
}

static void pocket_cvss_app_free(PocketCvssApp* app) {
    view_dispatcher_remove_view(app->dispatcher, PocketCvssViewMainMenu);
    view_dispatcher_remove_view(app->dispatcher, PocketCvssViewMetricMenu);
    view_dispatcher_remove_view(app->dispatcher, PocketCvssViewExamplesMenu);
    view_dispatcher_remove_view(app->dispatcher, PocketCvssViewInfo);
    view_free_model(app->info_view);
    view_free(app->info_view);
    submenu_free(app->examples_menu);
    submenu_free(app->metric_menu);
    submenu_free(app->main_menu);
    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t pocket_cvss_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "Starting Pocket CVSS");
    PocketCvssApp* app = pocket_cvss_app_alloc();
    view_dispatcher_run(app->dispatcher);
    pocket_cvss_app_free(app);
    FURI_LOG_I(TAG, "Stopped Pocket CVSS");

    return 0;
}
