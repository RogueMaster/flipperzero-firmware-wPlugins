#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_random.h>
#include <furi_hal_rtc.h>
#include <furi_hal_power.h>
#include "tiv_led_hodiny_icons.h"

typedef struct {
    ViewPort* vp;
    FuriMessageQueue* q;
    uint8_t led[3];
    uint32_t cas; // od 1.1.1970
    uint8_t format;
    uint8_t baterie[2];
    bool running;
} AppState;

static void draw_callback(Canvas* canvas, void* ctx) {
    AppState* app = ctx;

    char line[32], linee[32], lineee[32], lin[32];
    snprintf(line, sizeof(line), "LED:  #%02X%02X%02X", app->led[0], app->led[1], app->led[2]);
    snprintf(linee, sizeof(linee), "RTC data: %010lu", app->cas);
    if(app->format == 24) {
        snprintf(
            lineee,
            sizeof(lineee),
            "%02d:%02d:%02d",
            (int)(app->cas % (app->format * 3600)) / 3600,
            (int)(app->cas % 3600) / 60,
            (int)app->cas % 60);
    } else {
        if((app->cas % 86400) / 3600 <= 12) {
            snprintf(
                lineee,
                sizeof(lineee),
                "AM %02d:%02d:%02d",
                (int)(app->cas % (app->format * 3600)) / 3600,
                (int)(app->cas % 3600) / 60,
                (int)app->cas % 60);
        } else {
            snprintf(
                lineee,
                sizeof(lineee),
                "PM %02d:%02d:%02d",
                (int)(app->cas % (app->format * 3600)) / 3600,
                (int)(app->cas % 3600) / 60,
                (int)app->cas % 60);
        }
    }

    snprintf(lin, sizeof(lin), "%03d%%", app->baterie[1]);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    switch(app->baterie[0]) {
    case 0:
        canvas_draw_icon(canvas, 128 - canvas_string_width(canvas, lin) - 12, 0, &I_baterie);
        break;
    case 1:
        canvas_draw_icon(canvas, 128 - canvas_string_width(canvas, lin) - 12, 0, &I_nabijeni);
        break;
    case 2:
        canvas_draw_icon(canvas, 128 - canvas_string_width(canvas, lin) - 22, 0, &I_nabito);
        break;
    }
    canvas_draw_str(canvas, 128 - canvas_string_width(canvas, lin) - 2, 8, lin);
    canvas_draw_str(canvas, 2, 15, "BACK = Exit");
    canvas_draw_str(canvas, 2, 25, "OK = change hour format");
    canvas_draw_line(canvas, 0, 30, 128, 30);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 42, lineee);
    canvas_draw_str(canvas, 2, 52, line);
    canvas_draw_str(canvas, 2, 62, linee);
}

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* q = ctx;
    // callback nesmi blokovat
    furi_message_queue_put(q, event, 0);
}

int32_t main_app(void* p) {
    UNUSED(p);
    AppState app = {0};
    app.running = true;
    app.led[0] = 0;
    app.led[1] = 0;
    app.led[2] = 0;
    if(furi_hal_rtc_get_locale_timeformat() == FuriHalRtcLocaleTimeFormat24h) {
        app.format = 24;
    } else {
        app.format = 12;
    }

    app.q = furi_message_queue_alloc(8, sizeof(InputEvent));

    Gui* gui = furi_record_open(RECORD_GUI);

    app.vp = view_port_alloc();
    view_port_draw_callback_set(app.vp, draw_callback, &app);
    view_port_input_callback_set(app.vp, input_callback, app.q);

    gui_add_view_port(gui, app.vp, GuiLayerFullscreen);

    // prvni vykresleni
    view_port_update(app.vp);

    InputEvent ev;
    while(app.running) {
        app.baterie[1] = furi_hal_power_get_pct();
        if(furi_hal_power_is_charging()) {
            app.baterie[0] = 1;
        } else {
            app.baterie[0] = 0;
        }
        if(furi_hal_power_is_charging_done()) {
            app.baterie[0] = 2;
        }

        if(furi_message_queue_get(app.q, &ev, 500) == FuriStatusOk) {
            if(ev.type == InputTypeShort) {
                if(ev.key == InputKeyBack) {
                    app.running = false;
                } else if(ev.key == InputKeyOk) {
                    if(app.format == 24) {
                        app.format = 12;
                    } else {
                        app.format = 24;
                    }
                }
            }
        }
        app.cas = furi_hal_rtc_get_timestamp();
        app.led[0] = (uint8_t)(app.cas % 60) * 4.25;
        app.led[1] = (uint8_t)((app.cas % 3600) / 60) * 4.25;
        app.led[2] = (uint8_t)(((app.cas % (app.format * 3600)) / 3600) * 10.625) - 1;

        furi_hal_light_set(LightRed, app.led[0]);
        furi_hal_light_set(LightGreen, app.led[1]);
        furi_hal_light_set(LightBlue, app.led[2]);
        view_port_update(app.vp);
    }

    // zhasni
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);

    gui_remove_view_port(gui, app.vp);
    view_port_free(app.vp);

    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app.q);

    return 0;
}
