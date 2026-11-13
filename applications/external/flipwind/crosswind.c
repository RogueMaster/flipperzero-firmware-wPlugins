
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/canvas.h>
#include <input/input.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CW_PI 3.14159265358979323846f

typedef enum {
    ScreenMain,
    ScreenCalculator,
    ScreenAbout,
    ScreenSafety,
} FlipWindScreen;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    bool running;

    FlipWindScreen screen;
    uint8_t menu_selected;
    uint8_t selected;
    uint8_t safety_scroll;

    int runway;
    int wind_dir;
    int wind_speed;
} FlipWindApp;

/* Disegna una freccia */
static void cw_draw_arrow(Canvas* canvas, int x1, int y1, int x2, int y2) {
    canvas_draw_line(canvas, x1, y1, x2, y2);

    float angle = atan2f((float)(y2 - y1), (float)(x2 - x1));
    float head_angle = 0.65f;
    float head_length = 4.0f;

    int hx1 = (int)(x2 - head_length * cosf(angle - head_angle));
    int hy1 = (int)(y2 - head_length * sinf(angle - head_angle));
    int hx2 = (int)(x2 - head_length * cosf(angle + head_angle));
    int hy2 = (int)(y2 - head_length * sinf(angle + head_angle));

    canvas_draw_line(canvas, x2, y2, hx1, hy1);
    canvas_draw_line(canvas, x2, y2, hx2, hy2);
}

/* Diagramma con pista e una sola freccia del vento */
static void draw_diagram(Canvas* canvas, int runway, int wind_dir) {
    const int cx = 99;
    const int cy = 29;

    /* Pista: 000 = Nord, 090 = Est */
    float r = (float)runway * CW_PI / 180.0f;
    float rx = sinf(r);
    float ry = -cosf(r);

    int x1 = (int)(cx - rx * 17.0f);
    int y1 = (int)(cy - ry * 17.0f);
    int x2 = (int)(cx + rx * 17.0f);
    int y2 = (int)(cy + ry * 17.0f);

    float px = -ry;
    float py = rx;
    int half_width = 4;

    int a1x = (int)(x1 + px * half_width);
    int a1y = (int)(y1 + py * half_width);
    int a2x = (int)(x1 - px * half_width);
    int a2y = (int)(y1 - py * half_width);

    int b1x = (int)(x2 + px * half_width);
    int b1y = (int)(y2 + py * half_width);
    int b2x = (int)(x2 - px * half_width);
    int b2y = (int)(y2 - py * half_width);

    canvas_draw_line(canvas, a1x, a1y, b1x, b1y);
    canvas_draw_line(canvas, a2x, a2y, b2x, b2y);
    canvas_draw_line(canvas, a1x, a1y, a2x, a2y);
    canvas_draw_line(canvas, b1x, b1y, b2x, b2y);

    /* Linea centrale tratteggiata */
    for(int i = 0; i < 3; i++) {
        float start = -12.0f + (float)i * 10.0f;
        float end = start + 4.0f;

        canvas_draw_line(
            canvas,
            (int)(cx + rx * start),
            (int)(cy + ry * start),
            (int)(cx + rx * end),
            (int)(cy + ry * end));
    }

    /* wind_dir è la direzione DA CUI proviene il vento.
       La freccia punta nella direzione verso cui soffia. */
    float w = (float)wind_dir * CW_PI / 180.0f;
    float wind_to_x = -sinf(w);
    float wind_to_y = cosf(w);

    int wx1 = (int)(cx - wind_to_x * 17.0f);
    int wy1 = (int)(cy - wind_to_y * 17.0f);
    int wx2 = (int)(cx - wind_to_x * 4.0f);
    int wy2 = (int)(cy - wind_to_y * 4.0f);

    cw_draw_arrow(canvas, wx1, wy1, wx2, wy2);

    char heading[8];
    snprintf(heading, sizeof(heading), "%03d", runway);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 84, 49, heading);
}

/* Menu principale */
static void draw_main_menu(Canvas* canvas, FlipWindApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 30, 13, "FlipWind");

    canvas_set_font(canvas, FontSecondary);

    if(app->menu_selected == 0) {
        canvas_draw_str(canvas, 23, 28, ">");
    }
    canvas_draw_str(canvas, 34, 28, "Go fly!");

    if(app->menu_selected == 1) {
        canvas_draw_str(canvas, 23, 41, ">");
    }
    canvas_draw_str(canvas, 34, 41, "About");

    if(app->menu_selected == 2) {
        canvas_draw_str(canvas, 23, 54, ">");
    }
    canvas_draw_str(canvas, 34, 54, "Safety notice");
}

/* Schermata About */
static void draw_about(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "About");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 25, "FlipWind v1.0");
    canvas_draw_str(canvas, 2, 37, "created by marcopit");
    canvas_draw_str(canvas, 2, 55, "HAVE A SAFE FLIGHT!");
}

/* Avviso di sicurezza in inglese.
   Su/Giu scorrono il testo. */
static void draw_safety(Canvas* canvas, FlipWindApp* app) {
    static const char* safety_lines[] = {
        "SAFETY NOTICE",
        "Planning aid only.",
        "Verify wind and",
        "runway data using",
        "official sources.",
        "Follow your AFM/",
        "POH and SOPs.",
        "Observe aircraft",
        "crosswind limits.",
        "Consult your",
        "instructor/ATC.",
        "The pilot in",
        "command is always",
        "responsible for",
        "flight decisions."};

    const uint8_t line_count = sizeof(safety_lines) / sizeof(safety_lines[0]);
    const uint8_t visible_lines = 6;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    for(uint8_t i = 0; i < visible_lines; i++) {
        uint8_t index = app->safety_scroll + i;

        if(index < line_count) {
            canvas_draw_str(canvas, 2, 9 + (int)i * 10, safety_lines[index]);
        }
    }
}

/* Schermata calcolatore */
static void draw_calculator(Canvas* canvas, FlipWindApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "FLIPWIND");

    canvas_set_font(canvas, FontSecondary);

    if(app->selected == 0) {
        canvas_draw_str(canvas, 2, 23, ">");
    }
    canvas_draw_str(canvas, 10, 23, "RWY");

    char runway_text[8];
    snprintf(runway_text, sizeof(runway_text), "%03d", app->runway);
    canvas_draw_str(canvas, 38, 23, runway_text);

    if(app->selected == 1) {
        canvas_draw_str(canvas, 2, 34, ">");
    }
    canvas_draw_str(canvas, 10, 34, "WIND");

    char wind_text[8];
    snprintf(wind_text, sizeof(wind_text), "%03d", app->wind_dir);
    canvas_draw_str(canvas, 38, 34, wind_text);

    if(app->selected == 2) {
        canvas_draw_str(canvas, 2, 45, ">");
    }
    canvas_draw_str(canvas, 10, 45, "SPD");

    char speed_text[8];
    snprintf(speed_text, sizeof(speed_text), "%03d", app->wind_speed);
    canvas_draw_str(canvas, 38, 45, speed_text);

    canvas_draw_line(canvas, 70, 13, 70, 48);

    draw_diagram(canvas, app->runway, app->wind_dir);

    canvas_draw_line(canvas, 0, 51, 127, 51);

    float angle_deg = (float)(app->wind_dir - app->runway);
    float angle_rad = angle_deg * CW_PI / 180.0f;

    float crosswind = (float)app->wind_speed * sinf(angle_rad);
    float headwind = (float)app->wind_speed * cosf(angle_rad);

    int crosswind_abs = (int)(fabsf(crosswind) + 0.5f);
    int headwind_abs = (int)(fabsf(headwind) + 0.5f);

    char crosswind_text[24];

    if(crosswind_abs == 0) {
        snprintf(crosswind_text, sizeof(crosswind_text), "XW 0 kt");
    } else if(crosswind > 0.0f) {
        snprintf(crosswind_text, sizeof(crosswind_text), "XW %d kt RIGHT", crosswind_abs);
    } else {
        snprintf(crosswind_text, sizeof(crosswind_text), "XW %d kt LEFT", crosswind_abs);
    }

    char headwind_text[24];

    if(headwind_abs == 0) {
        snprintf(headwind_text, sizeof(headwind_text), "WIND 0 kt");
    } else if(headwind > 0.0f) {
        snprintf(headwind_text, sizeof(headwind_text), "HEAD %d kt", headwind_abs);
    } else {
        snprintf(headwind_text, sizeof(headwind_text), "TAIL %d kt", headwind_abs);
    }

    canvas_draw_str(canvas, 2, 60, crosswind_text);
    canvas_draw_str(canvas, 82, 60, headwind_text);
}

/* Disegno in base alla schermata corrente */
static void draw_callback(Canvas* canvas, void* context) {
    FlipWindApp* app = context;

    switch(app->screen) {
    case ScreenMain:
        draw_main_menu(canvas, app);
        break;

    case ScreenCalculator:
        draw_calculator(canvas, app);
        break;

    case ScreenAbout:
        draw_about(canvas);
        break;

    case ScreenSafety:
        draw_safety(canvas, app);
        break;

    default:
        canvas_clear(canvas);
        break;
    }
}

/* Callback input */
static void input_callback(InputEvent* event, void* context) {
    FlipWindApp* app = context;
    furi_message_queue_put(app->queue, event, FuriWaitForever);
}

/* Gestione input */
static void handle_input(FlipWindApp* app, InputEvent* event) {
    if(event->type != InputTypePress && event->type != InputTypeRepeat) {
        return;
    }

    bool changed = false;

    switch(app->screen) {
    case ScreenMain:
        switch(event->key) {
        case InputKeyUp:
            app->menu_selected = (app->menu_selected + 2) % 3;
            changed = true;
            break;

        case InputKeyDown:
            app->menu_selected = (app->menu_selected + 1) % 3;
            changed = true;
            break;

        case InputKeyOk:
            if(event->type == InputTypePress) {
                if(app->menu_selected == 0) {
                    app->screen = ScreenCalculator;
                    app->selected = 0;
                } else if(app->menu_selected == 1) {
                    app->screen = ScreenAbout;
                } else {
                    app->screen = ScreenSafety;
                    app->safety_scroll = 0;
                }
                changed = true;
            }
            break;

        case InputKeyBack:
            if(event->type == InputTypePress) {
                app->running = false;
            }
            break;

        default:
            break;
        }
        break;

    case ScreenCalculator:
        switch(event->key) {
        case InputKeyBack:
            if(event->type == InputTypePress) {
                app->screen = ScreenMain;
                changed = true;
            }
            break;

        case InputKeyUp:
            app->selected = (app->selected + 2) % 3;
            changed = true;
            break;

        case InputKeyDown:
            app->selected = (app->selected + 1) % 3;
            changed = true;
            break;

        case InputKeyRight:
            if(app->selected == 0) {
                app->runway = (app->runway + 1) % 360;
            } else if(app->selected == 1) {
                app->wind_dir = (app->wind_dir + 1) % 360;
            } else if(app->wind_speed < 200) {
                app->wind_speed++;
            }
            changed = true;
            break;

        case InputKeyLeft:
            if(app->selected == 0) {
                app->runway = (app->runway + 359) % 360;
            } else if(app->selected == 1) {
                app->wind_dir = (app->wind_dir + 359) % 360;
            } else if(app->wind_speed > 0) {
                app->wind_speed--;
            }
            changed = true;
            break;

        default:
            break;
        }
        break;

    case ScreenAbout:
        if(event->key == InputKeyBack && event->type == InputTypePress) {
            app->screen = ScreenMain;
            changed = true;
        }
        break;

    case ScreenSafety:
        if(event->key == InputKeyBack && event->type == InputTypePress) {
            app->screen = ScreenMain;
            changed = true;
        } else if(event->key == InputKeyDown) {
            if(app->safety_scroll < 9) {
                app->safety_scroll++;
                changed = true;
            }
        } else if(event->key == InputKeyUp) {
            if(app->safety_scroll > 0) {
                app->safety_scroll--;
                changed = true;
            }
        }
        break;

    default:
        break;
    }

    if(changed) {
        view_port_update(app->view_port);
    }
}

/* Entry point della FAP */
int32_t flipwind_app(void* p) {
    UNUSED(p);

    FlipWindApp* app = malloc(sizeof(FlipWindApp));
    furi_check(app);

    app->running = true;
    app->screen = ScreenMain;
    app->menu_selected = 0;
    app->selected = 0;
    app->safety_scroll = 0;

    app->runway = 270;
    app->wind_dir = 320;
    app->wind_speed = 20;

    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();

    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    view_port_update(app->view_port);

    InputEvent event;

    while(app->running) {
        if(furi_message_queue_get(app->queue, &event, FuriWaitForever) == FuriStatusOk) {
            handle_input(app, &event);
        }
    }

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);

    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    free(app);

    return 0;
}
