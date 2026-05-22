#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include <stdio.h>
#include <string.h>

#define TAG "PocketCVSS"

typedef enum {
    PocketCvssScreenMenu,
    PocketCvssScreenMetric,
    PocketCvssScreenResult,
    PocketCvssScreenExplain,
    PocketCvssScreenSettings,
    PocketCvssScreenAbout,
} PocketCvssScreen;

typedef struct {
    const char* label;
    const char* code;
} Cvss31Option;

typedef struct {
    const char* title;
    const char* metric_code;
    uint8_t option_count;
    Cvss31Option options[4];
} Cvss31Metric;

typedef struct {
    PocketCvssScreen screen;
    uint8_t menu_index;
    uint8_t step;
    uint8_t current_option;
    uint8_t values[8];
} PocketCvssModel;

typedef struct {
    Gui* gui;
    ViewDispatcher* dispatcher;
    View* view;
} PocketCvssApp;

static const Cvss31Metric cvss31_metrics[] = {
    {
        .title = "Attack Vector",
        .metric_code = "AV",
        .option_count = 4,
        .options = {{"Network", "N"}, {"Adjacent", "A"}, {"Local", "L"}, {"Physical", "P"}},
    },
    {
        .title = "Attack Complexity",
        .metric_code = "AC",
        .option_count = 2,
        .options = {{"Low", "L"}, {"High", "H"}},
    },
    {
        .title = "Privileges Required",
        .metric_code = "PR",
        .option_count = 3,
        .options = {{"None", "N"}, {"Low", "L"}, {"High", "H"}},
    },
    {
        .title = "User Interaction",
        .metric_code = "UI",
        .option_count = 2,
        .options = {{"None", "N"}, {"Required", "R"}},
    },
    {
        .title = "Scope",
        .metric_code = "S",
        .option_count = 2,
        .options = {{"Unchanged", "U"}, {"Changed", "C"}},
    },
    {
        .title = "Confidentiality",
        .metric_code = "C",
        .option_count = 3,
        .options = {{"None", "N"}, {"Low", "L"}, {"High", "H"}},
    },
    {
        .title = "Integrity",
        .metric_code = "I",
        .option_count = 3,
        .options = {{"None", "N"}, {"Low", "L"}, {"High", "H"}},
    },
    {
        .title = "Availability",
        .metric_code = "A",
        .option_count = 3,
        .options = {{"None", "N"}, {"Low", "L"}, {"High", "H"}},
    },
};

static const char* pocket_cvss_menu_items[] = {
    "New v3.1 Score",
    "Settings",
    "About",
};

static void pocket_cvss_reset_score(PocketCvssModel* model) {
    model->step = 0;
    model->current_option = 0;

    model->values[0] = 0; /* AV:N */
    model->values[1] = 0; /* AC:L */
    model->values[2] = 1; /* PR:L */
    model->values[3] = 0; /* UI:N */
    model->values[4] = 0; /* S:U */
    model->values[5] = 2; /* C:H */
    model->values[6] = 2; /* I:H */
    model->values[7] = 2; /* A:H */
}

static float pocket_cvss_pow15(float value) {
    float result = 1.0f;

    for(uint8_t i = 0; i < 15; i++) {
        result *= value;
    }

    return result;
}

static float pocket_cvss_min(float a, float b) {
    return a < b ? a : b;
}

static uint8_t pocket_cvss_roundup_tenths(float input) {
    uint32_t scaled = (uint32_t)(input * 100000.0f + 0.5f);

    if((scaled % 10000) == 0) {
        return scaled / 10000;
    }

    return (scaled / 10000) + 1;
}

static uint8_t pocket_cvss_score_tenths(const PocketCvssModel* model) {
    static const float attack_vector_weights[] = {0.85f, 0.62f, 0.55f, 0.20f};
    static const float attack_complexity_weights[] = {0.77f, 0.44f};
    static const float user_interaction_weights[] = {0.85f, 0.62f};
    static const float impact_weights[] = {0.0f, 0.22f, 0.56f};

    const uint8_t scope = model->values[4];
    const float privilege_required_weights_unchanged[] = {0.85f, 0.62f, 0.27f};
    const float privilege_required_weights_changed[] = {0.85f, 0.68f, 0.50f};

    const float av = attack_vector_weights[model->values[0]];
    const float ac = attack_complexity_weights[model->values[1]];
    const float pr = scope == 0 ? privilege_required_weights_unchanged[model->values[2]] :
                                  privilege_required_weights_changed[model->values[2]];
    const float ui = user_interaction_weights[model->values[3]];
    const float c = impact_weights[model->values[5]];
    const float i = impact_weights[model->values[6]];
    const float a = impact_weights[model->values[7]];

    const float iss = 1.0f - ((1.0f - c) * (1.0f - i) * (1.0f - a));
    const float impact =
        scope == 0 ? 6.42f * iss : 7.52f * (iss - 0.029f) - 3.25f * pocket_cvss_pow15(iss - 0.02f);

    if(impact <= 0.0f) {
        return 0;
    }

    const float exploitability = 8.22f * av * ac * pr * ui;
    const float raw_score = scope == 0 ? impact + exploitability :
                                         1.08f * (impact + exploitability);

    return pocket_cvss_roundup_tenths(pocket_cvss_min(raw_score, 10.0f));
}

static const char* pocket_cvss_severity(uint8_t score_tenths) {
    if(score_tenths == 0) return "NONE";
    if(score_tenths < 40) return "LOW";
    if(score_tenths < 70) return "MEDIUM";
    if(score_tenths < 90) return "HIGH";
    return "CRITICAL";
}

static void pocket_cvss_format_metric_line(
    const PocketCvssModel* model,
    char* buffer,
    size_t buffer_size,
    uint8_t first,
    uint8_t last) {
    buffer[0] = '\0';

    for(uint8_t i = first; i <= last; i++) {
        const Cvss31Metric* metric = &cvss31_metrics[i];
        const Cvss31Option* option = &metric->options[model->values[i]];

        char token[12];
        snprintf(token, sizeof(token), "%s:%s", metric->metric_code, option->code);

        if(i != first) {
            strlcat(buffer, " ", buffer_size);
        }

        strlcat(buffer, token, buffer_size);
    }
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

static void pocket_cvss_draw_severity_badge(Canvas* canvas, const char* severity) {
    canvas_set_font(canvas, FontSecondary);
    const uint8_t width = canvas_string_width(canvas, severity) + 8;
    const uint8_t x = 127 - width;

    canvas_draw_rbox(canvas, x, 14, width, 12, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str_aligned(canvas, x + (width / 2), 24, AlignCenter, AlignBottom, severity);
    canvas_set_color(canvas, ColorBlack);
}

static void pocket_cvss_draw_bullet(Canvas* canvas, uint8_t y, const char* text) {
    canvas_draw_disc(canvas, 3, y - 3, 1);
    canvas_draw_str(canvas, 8, y, text);
}

static void pocket_cvss_draw_menu(Canvas* canvas, const PocketCvssModel* model) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "PocketCVSS");

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < COUNT_OF(pocket_cvss_menu_items); i++) {
        const uint8_t y = 26 + (i * 11);
        const bool selected = i == model->menu_index;

        if(selected) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_str(canvas, 4, y, pocket_cvss_menu_items[i]);

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
        }
    }

    pocket_cvss_draw_footer(canvas, "Select", "Exit");
}

static void pocket_cvss_draw_metric(Canvas* canvas, const PocketCvssModel* model) {
    const Cvss31Metric* metric = &cvss31_metrics[model->step];
    char progress[8];
    snprintf(progress, sizeof(progress), "%u/8", model->step + 1);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 8, "PocketCVSS");
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, progress);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 21, metric->title);

    canvas_set_font(canvas, FontSecondary);
    const uint8_t first_option_y = metric->option_count > 3 ? 31 : 33;
    const uint8_t option_step_y = metric->option_count > 3 ? 9 : 10;

    for(uint8_t i = 0; i < metric->option_count; i++) {
        const uint8_t y = first_option_y + (i * option_step_y);
        const bool selected = i == model->current_option;

        if(selected) {
            canvas_draw_box(canvas, 0, y - 8, 128, option_step_y);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_str(canvas, 4, y, metric->options[i].label);

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
        }
    }

    if(metric->option_count <= 3) {
        pocket_cvss_draw_footer(canvas, "Next", "Prev");
    }
}

static void pocket_cvss_draw_result(Canvas* canvas, const PocketCvssModel* model) {
    char score[8];
    char line1[40];
    char line2[40];
    const uint8_t score_tenths = pocket_cvss_score_tenths(model);
    const char* severity = pocket_cvss_severity(score_tenths);

    snprintf(score, sizeof(score), "%u.%u", score_tenths / 10, score_tenths % 10);

    pocket_cvss_format_metric_line(model, line1, sizeof(line1), 0, 3);
    pocket_cvss_format_metric_line(model, line2, sizeof(line2), 4, 7);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 8, "CVSS v3.1 Base");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 25, score);
    pocket_cvss_draw_severity_badge(canvas, severity);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 36, "CVSS:3.1");
    canvas_draw_str(canvas, 0, 45, line1);
    canvas_draw_str(canvas, 0, 52, line2);

    pocket_cvss_draw_footer(canvas, "Explain", "Edit");
}

static void pocket_cvss_draw_explain(Canvas* canvas, const PocketCvssModel* model) {
    char title[24];
    char metric_line[40];
    const uint8_t score_tenths = pocket_cvss_score_tenths(model);

    snprintf(title, sizeof(title), "Why %s?", pocket_cvss_severity(score_tenths));
    pocket_cvss_format_metric_line(model, metric_line, sizeof(metric_line), 0, 3);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, title);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 23, metric_line);

    if(model->values[0] == 0) {
        pocket_cvss_draw_bullet(canvas, 34, "Network reachable");
    } else {
        pocket_cvss_draw_bullet(canvas, 34, "Limited attack vector");
    }

    if(model->values[1] == 0) {
        pocket_cvss_draw_bullet(canvas, 43, "Low complexity");
    } else {
        pocket_cvss_draw_bullet(canvas, 43, "High complexity");
    }

    if(model->values[5] == 2 || model->values[6] == 2 || model->values[7] == 2) {
        pocket_cvss_draw_bullet(canvas, 52, "High C/I/A impact");
    } else if(model->values[5] || model->values[6] || model->values[7]) {
        pocket_cvss_draw_bullet(canvas, 52, "Partial C/I/A impact");
    } else {
        pocket_cvss_draw_bullet(canvas, 52, "No C/I/A impact");
    }

    pocket_cvss_draw_footer(canvas, NULL, "Result");
}

static void pocket_cvss_draw_settings(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "Settings");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 27, "TODO");
    canvas_draw_str(canvas, 0, 38, "No settings yet.");

    pocket_cvss_draw_footer(canvas, NULL, "Menu");
}

static void pocket_cvss_draw_about(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "PocketCVSS");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 25, "v0.1");
    canvas_draw_str(canvas, 0, 36, "CVSS v3.1 Base");
    canvas_draw_str(canvas, 0, 47, "More soon.");

    pocket_cvss_draw_footer(canvas, NULL, "Menu");
}

static void pocket_cvss_draw(Canvas* canvas, void* model_context) {
    const PocketCvssModel* model = model_context;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->screen == PocketCvssScreenMenu) {
        pocket_cvss_draw_menu(canvas, model);
    } else if(model->screen == PocketCvssScreenMetric) {
        pocket_cvss_draw_metric(canvas, model);
    } else if(model->screen == PocketCvssScreenResult) {
        pocket_cvss_draw_result(canvas, model);
    } else if(model->screen == PocketCvssScreenExplain) {
        pocket_cvss_draw_explain(canvas, model);
    } else if(model->screen == PocketCvssScreenSettings) {
        pocket_cvss_draw_settings(canvas);
    } else {
        pocket_cvss_draw_about(canvas);
    }
}

static bool pocket_cvss_input(InputEvent* event, void* context) {
    PocketCvssApp* app = context;
    bool handled = false;
    bool should_stop = false;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    with_view_model(
        app->view,
        PocketCvssModel * model,
        {
            if(model->screen == PocketCvssScreenMenu) {
                if(event->key == InputKeyUp) {
                    model->menu_index = model->menu_index == 0 ?
                                            (uint8_t)(COUNT_OF(pocket_cvss_menu_items) - 1) :
                                            model->menu_index - 1;
                    handled = true;
                } else if(event->key == InputKeyDown) {
                    model->menu_index = (model->menu_index + 1) % COUNT_OF(pocket_cvss_menu_items);
                    handled = true;
                } else if(
                    event->type == InputTypeShort &&
                    (event->key == InputKeyOk || event->key == InputKeyRight)) {
                    if(model->menu_index == 0) {
                        pocket_cvss_reset_score(model);
                        model->screen = PocketCvssScreenMetric;
                    } else if(model->menu_index == 1) {
                        model->screen = PocketCvssScreenSettings;
                    } else {
                        model->screen = PocketCvssScreenAbout;
                    }

                    handled = true;
                } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
                    should_stop = true;
                    handled = true;
                }
            } else if(model->screen == PocketCvssScreenMetric) {
                const Cvss31Metric* metric = &cvss31_metrics[model->step];

                if(event->key == InputKeyUp) {
                    model->current_option = model->current_option == 0 ? metric->option_count - 1 :
                                                                         model->current_option - 1;
                    handled = true;
                } else if(event->key == InputKeyDown) {
                    model->current_option = (model->current_option + 1) % metric->option_count;
                    handled = true;
                } else if(
                    event->type == InputTypeShort &&
                    (event->key == InputKeyOk || event->key == InputKeyRight)) {
                    model->values[model->step] = model->current_option;

                    if(model->step == COUNT_OF(cvss31_metrics) - 1) {
                        model->screen = PocketCvssScreenResult;
                    } else {
                        model->step++;
                        model->current_option = model->values[model->step];
                    }

                    handled = true;
                } else if(
                    event->type == InputTypeShort &&
                    (event->key == InputKeyBack || event->key == InputKeyLeft)) {
                    model->values[model->step] = model->current_option;

                    if(model->step == 0) {
                        model->screen = PocketCvssScreenMenu;
                    } else {
                        model->step--;
                        model->current_option = model->values[model->step];
                    }

                    handled = true;
                }
            } else if(model->screen == PocketCvssScreenResult) {
                if(event->type == InputTypeShort &&
                   (event->key == InputKeyOk || event->key == InputKeyRight)) {
                    model->screen = PocketCvssScreenExplain;
                    handled = true;
                } else if(
                    event->type == InputTypeShort &&
                    (event->key == InputKeyBack || event->key == InputKeyLeft)) {
                    model->screen = PocketCvssScreenMetric;
                    model->step = COUNT_OF(cvss31_metrics) - 1;
                    model->current_option = model->values[model->step];
                    handled = true;
                }
            } else if(model->screen == PocketCvssScreenExplain) {
                if(event->type == InputTypeShort) {
                    model->screen = PocketCvssScreenResult;
                    handled = true;
                }
            } else if(
                model->screen == PocketCvssScreenSettings ||
                model->screen == PocketCvssScreenAbout) {
                if(event->type == InputTypeShort &&
                   (event->key == InputKeyBack || event->key == InputKeyLeft ||
                    event->key == InputKeyOk)) {
                    model->screen = PocketCvssScreenMenu;
                    handled = true;
                }
            }
        },
        handled);

    if(should_stop) {
        view_dispatcher_stop(app->dispatcher);
    }

    return handled;
}

static PocketCvssApp* pocket_cvss_app_alloc(void) {
    PocketCvssApp* app = malloc(sizeof(PocketCvssApp));
    app->gui = furi_record_open(RECORD_GUI);
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();

    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(PocketCvssModel));
    view_set_draw_callback(app->view, pocket_cvss_draw);
    view_set_input_callback(app->view, pocket_cvss_input);
    view_set_context(app->view, app);

    with_view_model(
        app->view,
        PocketCvssModel * model,
        {
            memset(model, 0, sizeof(PocketCvssModel));
            model->screen = PocketCvssScreenMenu;
            model->menu_index = 0;
            pocket_cvss_reset_score(model);
        },
        false);

    view_dispatcher_add_view(app->dispatcher, 0, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->dispatcher, 0);

    return app;
}

static void pocket_cvss_app_free(PocketCvssApp* app) {
    view_dispatcher_remove_view(app->dispatcher, 0);
    view_free_model(app->view);
    view_free(app->view);
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
