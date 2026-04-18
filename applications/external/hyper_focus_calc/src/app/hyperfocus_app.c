#include "include/app/hyperfocus_app.h"
#include "include/domain/hyperfocal.h"
#include "include/domain/sensor_data.h"
#include "include/persistence/sensors_store.h"
#include "include/ui/calc_scene.h"
#include "include/version.h"
#include <furi.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SENSOR_EDIT_NEW 0xFFFFFFFFu

#define HFC_W_H_MIN 0.01f
#define HFC_W_H_MAX 999.99f
#define HFC_COC_MIN 0.0001f
#define HFC_COC_MAX 1.0f

static float hfc_round2(float x) {
    return roundf(x * 100.0f) / 100.0f;
}

static float hfc_round3(float x) {
    return roundf(x * 1000.0f) / 1000.0f;
}

static float hfc_clamp_wh(float x) {
    if (x < HFC_W_H_MIN) {
        return HFC_W_H_MIN;
    }
    if (x > HFC_W_H_MAX) {
        return HFC_W_H_MAX;
    }
    return x;
}

static float hfc_clamp_coc(float x) {
    if (x < HFC_COC_MIN) {
        return HFC_COC_MIN;
    }
    if (x > HFC_COC_MAX) {
        return HFC_COC_MAX;
    }
    return x;
}

typedef enum {
    ViewMenu = 0,
    ViewCalc,
    ViewSensorList,
    ViewSensorEdit,
    ViewTextInput,
    ViewCredits,
} AppViewId;

typedef struct {
    ViewDispatcher *vd;
    Gui *gui;
    Submenu *menu;
    Widget *credits;
    View *calc;
    View *sensor_list;
    View *sensor_edit;
    TextInput *text_input;
    SensorsStore store;
    CalcSceneModel calc_model;
    uint16_t focal_mm;
    size_t fstop_idx;
    uint32_t list_sel;
    uint32_t list_scroll;
    SensorData edit_buf;
    uint32_t edit_index;
    uint8_t edit_row;
    bool edit_is_new;
    char text_store[SENSOR_NAME_MAX];
    char credits_buf[384];
    AppViewId current_view;
} HyperFocusApp;

/** Flipper passes view model to draw_cb, not view_set_context (see view_draw in view.c). */
typedef struct {
    HyperFocusApp *app;
} HfcViewCtx;

static void app_calc_refresh(HyperFocusApp *app);
static void app_switch(HyperFocusApp *app, AppViewId v);
static void app_build_credits(HyperFocusApp *app);
static bool app_navigation(void *context);

static void app_switch(HyperFocusApp *app, AppViewId v) {
    app->current_view = v;
    view_dispatcher_switch_to_view(app->vd, v);
}

static void app_build_credits(HyperFocusApp *app) {
    furi_assert(app && app->credits);
    widget_reset(app->credits);
    snprintf(app->credits_buf, sizeof(app->credits_buf),
             "\e#%s\e#\n\nVersion: %s\n\nAuthor: %s\n\n%s", APP_NAME, APP_VERSION, APP_AUTHOR,
             APP_REPO_URL);
    widget_add_text_scroll_element(app->credits, 0, 0, 128, 64, app->credits_buf);
}

static void app_calc_refresh(HyperFocusApp *app) {
    const SensorData *s = sensors_store_active(&app->store);
    if (!s) {
        return;
    }
    app->calc_model.focal_mm = app->focal_mm;
    app->calc_model.fstop_idx = app->fstop_idx;
    app->calc_model.coc_mm = s->coc;
    const float h_m = hyperfocal_distance_m((float)app->focal_mm,
                                            hyperfocal_fstop_at_index(app->fstop_idx), s->coc);
    app->calc_model.h_meters = h_m;
    snprintf(app->calc_model.result_text, sizeof(app->calc_model.result_text), "H: %.2f m",
             (double)h_m);
}

static void calc_draw_cb(Canvas *canvas, void *model) {
    const HfcViewCtx *ctx = model;
    furi_assert(ctx && ctx->app);
    calc_scene_draw(canvas, &ctx->app->calc_model);
}

static bool calc_input_cb(InputEvent *event, void *context) {
    HyperFocusApp *app = context;
    if (event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }
    if (event->key == InputKeyBack) {
        app_switch(app, ViewMenu);
        return true;
    }
    if (event->key == InputKeyUp) {
        if (app->focal_mm < (uint16_t)HFC_FOCAL_MM_MAX) {
            app->focal_mm++;
        }
    } else if (event->key == InputKeyDown) {
        if (app->focal_mm > (uint16_t)HFC_FOCAL_MM_MIN) {
            app->focal_mm--;
        }
    } else if (event->key == InputKeyRight) {
        hyperfocal_fstop_step(&app->fstop_idx, 1);
    } else if (event->key == InputKeyLeft) {
        hyperfocal_fstop_step(&app->fstop_idx, -1);
    } else {
        return false;
    }
    app_calc_refresh(app);
    return true;
}

static uint32_t sensor_list_total(const HyperFocusApp *app) {
    return app->store.count + 1u;
}

static void list_clamp_scroll(HyperFocusApp *app) {
    const uint32_t total = sensor_list_total(app);
    if (total == 0) {
        return;
    }
    if (app->list_sel >= total) {
        app->list_sel = total - 1u;
    }
    const uint32_t vis = 4u;
    if (app->list_sel < app->list_scroll) {
        app->list_scroll = app->list_sel;
    }
    if (app->list_sel >= app->list_scroll + vis) {
        app->list_scroll = app->list_sel - (vis - 1u);
    }
}

static void sensor_list_draw_cb(Canvas *canvas, void *model) {
    const HfcViewCtx *ctx = model;
    furi_assert(ctx && ctx->app);
    HyperFocusApp *app = ctx->app;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "My sensors");
    list_clamp_scroll(app);
    const uint32_t total = sensor_list_total(app);
    const uint32_t vis = 4u;
    uint32_t y = 18;
    for (uint32_t row = 0; row < vis && app->list_scroll + row < total; row++) {
        const uint32_t i = app->list_scroll + row;
        char line[40];
        if (i < app->store.count) {
            const bool sel = (i == app->list_sel);
            snprintf(line, sizeof(line), "%s%s", sel ? "> " : "  ", app->store.sensors[i].name);
        } else {
            const bool sel = (i == app->list_sel);
            snprintf(line, sizeof(line), "%s%s", sel ? "> " : "  ", "Add sensor");
        }
        canvas_draw_str(canvas, 2, y, line);
        y += 10;
    }
    canvas_draw_str(canvas, 2, 58, "OK: use  Long OK: edit");
}

static bool sensor_list_input_cb(InputEvent *event, void *context) {
    HyperFocusApp *app = context;
    const uint32_t total = sensor_list_total(app);

    if (event->key == InputKeyBack &&
        (event->type == InputTypeShort || event->type == InputTypeLong)) {
        app_switch(app, ViewMenu);
        return true;
    }

    if (event->type != InputTypeShort && event->type != InputTypeRepeat &&
        event->type != InputTypeLong) {
        return false;
    }

    if (event->key == InputKeyUp) {
        if (app->list_sel > 0) {
            app->list_sel--;
        }
        list_clamp_scroll(app);
        return true;
    }
    if (event->key == InputKeyDown) {
        if (app->list_sel + 1 < total) {
            app->list_sel++;
        }
        list_clamp_scroll(app);
        return true;
    }

    if (event->key == InputKeyOk) {
        if (event->type == InputTypeLong) {
            if (app->list_sel < app->store.count) {
                app->edit_buf = app->store.sensors[app->list_sel];
                app->edit_index = app->list_sel;
                app->edit_is_new = false;
                app->edit_row = 0;
                app_switch(app, ViewSensorEdit);
            }
            return true;
        }
        if (event->type == InputTypeShort) {
            if (app->list_sel < app->store.count) {
                sensors_store_set_active(&app->store, app->list_sel);
                app_calc_refresh(app);
                app_switch(app, ViewCalc);
            } else {
                sensor_data_set_defaults(&app->edit_buf);
                app->edit_buf.name[0] = '\0';
                app->edit_index = SENSOR_EDIT_NEW;
                app->edit_is_new = true;
                app->edit_row = 0;
                app_switch(app, ViewSensorEdit);
            }
            return true;
        }
    }

    return false;
}

static uint8_t edit_row_max(bool is_new) {
    return is_new ? (uint8_t)4u : (uint8_t)5u;
}

static void sensor_edit_draw_cb(Canvas *canvas, void *model) {
    const HfcViewCtx *ctx = model;
    furi_assert(ctx && ctx->app);
    HyperFocusApp *app = ctx->app;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 6, "Edit sensor");
    char line[56];
    snprintf(line, sizeof(line), "%s%s", (app->edit_row == 0) ? "> " : "  ",
             app->edit_buf.name[0] ? app->edit_buf.name : "(name)");
    canvas_draw_str(canvas, 2, 14, line);
    snprintf(line, sizeof(line), "%sW: %6.2f mm", (app->edit_row == 1) ? "> " : "  ",
             (double)app->edit_buf.w);
    canvas_draw_str(canvas, 2, 22, line);
    snprintf(line, sizeof(line), "%sH: %6.2f mm", (app->edit_row == 2) ? "> " : "  ",
             (double)app->edit_buf.h);
    canvas_draw_str(canvas, 2, 30, line);
    const float ref_coc = sensor_compute_coc_mm(app->edit_buf.w, app->edit_buf.h);
    snprintf(line, sizeof(line), "%sCoC: %6.3f  d/1500=%.3f", (app->edit_row == 3) ? "> " : "  ",
             (double)app->edit_buf.coc, (double)ref_coc);
    canvas_draw_str(canvas, 2, 38, line);
    snprintf(line, sizeof(line), "%sSave", (app->edit_row == 4) ? "> " : "  ");
    canvas_draw_str(canvas, 2, 46, line);
    if (!app->edit_is_new) {
        snprintf(line, sizeof(line), "%sDelete", (app->edit_row == 5) ? "> " : "  ");
        canvas_draw_str(canvas, 2, 54, line);
    }
}

static bool sensor_edit_input_cb(InputEvent *event, void *context) {
    HyperFocusApp *app = context;
    const uint8_t rmax = edit_row_max(app->edit_is_new);

    if (event->key == InputKeyBack &&
        (event->type == InputTypeShort || event->type == InputTypeLong)) {
        app_switch(app, ViewSensorList);
        return true;
    }

    if (event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    if (event->key == InputKeyUp) {
        if (app->edit_row > 0) {
            app->edit_row--;
        }
        return true;
    }
    if (event->key == InputKeyDown) {
        if (app->edit_row < rmax) {
            app->edit_row++;
        }
        return true;
    }

    if (event->key == InputKeyLeft || event->key == InputKeyRight) {
        const int sign = (event->key == InputKeyRight) ? 1 : -1;
        if (app->edit_row == 1) {
            const float step = 0.01f;
            app->edit_buf.w = hfc_round2(hfc_clamp_wh(app->edit_buf.w + (float)sign * step));
            app->edit_buf.coc = hfc_round3(sensor_compute_coc_mm(app->edit_buf.w, app->edit_buf.h));
        } else if (app->edit_row == 2) {
            const float step = 0.01f;
            app->edit_buf.h = hfc_round2(hfc_clamp_wh(app->edit_buf.h + (float)sign * step));
            app->edit_buf.coc = hfc_round3(sensor_compute_coc_mm(app->edit_buf.w, app->edit_buf.h));
        } else if (app->edit_row == 3) {
            const float step = 0.001f;
            app->edit_buf.coc = hfc_round3(hfc_clamp_coc(app->edit_buf.coc + (float)sign * step));
        }
        return true;
    }

    if (event->key == InputKeyOk) {
        if (app->edit_row == 0) {
            snprintf(app->text_store, sizeof(app->text_store), "%s", app->edit_buf.name);
            app_switch(app, ViewTextInput);
            return true;
        }
        if (app->edit_row == 4) {
            if (strlen(app->edit_buf.name) == 0) {
                snprintf(app->edit_buf.name, sizeof(app->edit_buf.name), "Sensor");
            }
            app->edit_buf.w = hfc_round2(app->edit_buf.w);
            app->edit_buf.h = hfc_round2(app->edit_buf.h);
            app->edit_buf.coc = hfc_round3(app->edit_buf.coc);
            if (!sensor_data_valid(&app->edit_buf)) {
                return true;
            }
            if (app->edit_is_new) {
                sensors_store_add(&app->store, &app->edit_buf);
            } else {
                sensors_store_replace_at(&app->store, app->edit_index, &app->edit_buf);
            }
            app_calc_refresh(app);
            app_switch(app, ViewSensorList);
            return true;
        }
        if (app->edit_row == 5 && !app->edit_is_new) {
            sensors_store_delete_at(&app->store, app->edit_index);
            app_calc_refresh(app);
            app_switch(app, ViewSensorList);
            return true;
        }
    }

    return false;
}

static void text_input_ok_cb(void *context) {
    HyperFocusApp *app = context;
    snprintf(app->edit_buf.name, sizeof(app->edit_buf.name), "%s", app->text_store);
    app_switch(app, ViewSensorEdit);
}

static void menu_cb(void *context, uint32_t index) {
    HyperFocusApp *app = context;
    if (index == 0u) {
        app_calc_refresh(app);
        app_switch(app, ViewCalc);
    } else if (index == 1u) {
        app->list_sel = 0;
        app->list_scroll = 0;
        list_clamp_scroll(app);
        app_switch(app, ViewSensorList);
    } else if (index == 2u) {
        app_build_credits(app);
        app_switch(app, ViewCredits);
    }
}

static bool app_navigation(void *context) {
    HyperFocusApp *app = context;
    if (app->current_view == ViewTextInput) {
        app_switch(app, ViewSensorEdit);
        return true;
    }
    if (app->current_view == ViewCredits) {
        app_switch(app, ViewMenu);
        return true;
    }
    if (app->current_view == ViewMenu) {
        view_dispatcher_stop(app->vd);
        return true;
    }
    return true;
}

int32_t hyperfocus_app_run(void) {
    HyperFocusApp *app = malloc(sizeof(HyperFocusApp));
    if (!app) {
        return -1;
    }
    memset(app, 0, sizeof(*app));

    sensors_store_load(&app->store);

    app->focal_mm = 50;
    app->fstop_idx = hyperfocal_fstop_index_of(2.8f);

    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_navigation_event_callback(app->vd, app_navigation);

    app->menu = submenu_alloc();
    submenu_add_item(app->menu, "Calculate", 0, menu_cb, app);
    submenu_add_item(app->menu, "My sensors", 1, menu_cb, app);
    submenu_add_item(app->menu, "Credits", 2, menu_cb, app);
    view_dispatcher_add_view(app->vd, ViewMenu, submenu_get_view(app->menu));

    app->credits = widget_alloc();
    view_dispatcher_add_view(app->vd, ViewCredits, widget_get_view(app->credits));

    app->calc = view_alloc();
    view_allocate_model(app->calc, ViewModelTypeLocking, sizeof(HfcViewCtx));
    view_set_context(app->calc, app);
    view_set_draw_callback(app->calc, calc_draw_cb);
    view_set_input_callback(app->calc, calc_input_cb);
    with_view_model(app->calc, HfcViewCtx * cm, { cm->app = app; }, true);
    view_dispatcher_add_view(app->vd, ViewCalc, app->calc);

    app->sensor_list = view_alloc();
    view_allocate_model(app->sensor_list, ViewModelTypeLocking, sizeof(HfcViewCtx));
    view_set_context(app->sensor_list, app);
    view_set_draw_callback(app->sensor_list, sensor_list_draw_cb);
    view_set_input_callback(app->sensor_list, sensor_list_input_cb);
    with_view_model(app->sensor_list, HfcViewCtx * lm, { lm->app = app; }, true);
    view_dispatcher_add_view(app->vd, ViewSensorList, app->sensor_list);

    app->sensor_edit = view_alloc();
    view_allocate_model(app->sensor_edit, ViewModelTypeLocking, sizeof(HfcViewCtx));
    view_set_context(app->sensor_edit, app);
    view_set_draw_callback(app->sensor_edit, sensor_edit_draw_cb);
    view_set_input_callback(app->sensor_edit, sensor_edit_input_cb);
    with_view_model(app->sensor_edit, HfcViewCtx * em, { em->app = app; }, true);
    view_dispatcher_add_view(app->vd, ViewSensorEdit, app->sensor_edit);

    app->text_input = text_input_alloc();
    text_input_set_header_text(app->text_input, "Name");
    text_input_set_result_callback(app->text_input, text_input_ok_cb, app, app->text_store,
                                   sizeof(app->text_store), false);
    view_dispatcher_add_view(app->vd, ViewTextInput, text_input_get_view(app->text_input));

    app_calc_refresh(app);
    app_switch(app, ViewCalc);

    view_dispatcher_run(app->vd);

    view_dispatcher_remove_view(app->vd, ViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->vd, ViewCredits);
    widget_free(app->credits);

    view_dispatcher_remove_view(app->vd, ViewSensorEdit);
    view_free(app->sensor_edit);

    view_dispatcher_remove_view(app->vd, ViewSensorList);
    view_free(app->sensor_list);

    view_dispatcher_remove_view(app->vd, ViewCalc);
    view_free(app->calc);

    view_dispatcher_remove_view(app->vd, ViewMenu);
    submenu_free(app->menu);

    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);

    free(app);
    return 0;
}
