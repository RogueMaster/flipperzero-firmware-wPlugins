#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define SCREEN_HEIGHT 64

typedef enum {
    SCREEN_INPUT,
    SCREEN_RESULT,
    SCREEN_ABOUT
} ScreenState;

typedef struct {
    float a;
    float b;
    float c;
    int selected;
    bool exit;
    char result[256];
    ViewPort* view_port;
    Gui* gui;
    ScreenState screen_state;
} AppState;

void format_root(char* buf, size_t size, const char* label, float x) {
    float rounded = roundf(x * 1e5f) / 1e5f;
    bool is_integer = (fabsf(rounded - (int)rounded) < 1e-6f);

    if(is_integer) {
        snprintf(buf, size, "%s = %d", label, (int)rounded);
    } else {
        snprintf(buf, size, "%s = %.5f", label, (double)rounded);
    }
}

// solves the quadratic equation
void solve_quadratic(AppState* app) {
    float a = app->a;
    float b = app->b;
    float c = app->c;
    float discriminant = b * b - 4 * a * c;

    if(fabsf(a) < 1e-6f) {
        snprintf(app->result, sizeof(app->result), "Not a quadratic equation.");
        return;
    }

    if(discriminant < 0) {
        snprintf(app->result, sizeof(app->result), "No real roots.");
    } else if(fabsf(discriminant) < 1e-6f) {
        float x = -b / (2 * a);
        char buf[64];
        format_root(buf, sizeof(buf), "x", x);
        snprintf(app->result, sizeof(app->result), "One root:\n%s", buf);
    } else {
        float sqrt_d = sqrtf(discriminant);
        float x1 = (-b + sqrt_d) / (2 * a);
        float x2 = (-b - sqrt_d) / (2 * a);
        char buf1[64], buf2[64];
        format_root(buf1, sizeof(buf1), "x1", x1);
        format_root(buf2, sizeof(buf2), "x2", x2);
        snprintf(app->result, sizeof(app->result), "Two roots:\n%s\n%s", buf1, buf2);
    }
}

void draw_input(Canvas* canvas, void* ctx) {
    AppState* app = ctx;
    char buf[32];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    canvas_draw_str(canvas, 2, 12, "a:");
    snprintf(buf, sizeof(buf), "%s%.1f", app->selected == 0 ? ">" : " ", (double)app->a);
    canvas_draw_str(canvas, 100, 12, buf);

    canvas_draw_str(canvas, 2, 24, "b:");
    snprintf(buf, sizeof(buf), "%s%.1f", app->selected == 1 ? ">" : " ", (double)app->b);
    canvas_draw_str(canvas, 100, 24, buf);

    canvas_draw_str(canvas, 2, 36, "c:");
    snprintf(buf, sizeof(buf), "%s%.1f", app->selected == 2 ? ">" : " ", (double)app->c);
    canvas_draw_str(canvas, 100, 36, buf);

    snprintf(buf, sizeof(buf), "%sAbout", app->selected == 3 ? ">" : " ");
    canvas_draw_str(canvas, 2, 48, buf);

    canvas_draw_str(canvas, 2, 62, "OK to solve");
}

// shows results
void draw_result(Canvas* canvas, void* ctx) {
    AppState* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    int y = 12;
    char* start = app->result;
    char* end = NULL;
    while((end = strchr(start, '\n')) != NULL && y < SCREEN_HEIGHT - 10) {
        char line[64];
        size_t len = end - start;
        if(len >= sizeof(line)) len = sizeof(line) - 1;
        strncpy(line, start, len);
        line[len] = '\0';

        canvas_draw_str(canvas, 2, y, line);
        y += 10;
        start = end + 1;
    }

    if(*start && y < SCREEN_HEIGHT - 10) {
        canvas_draw_str(canvas, 2, y, start);
    }

    canvas_draw_str(canvas, 2, SCREEN_HEIGHT - 2, "Back to edit");
}

// about screen
void draw_about(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    int y = 15;
    canvas_draw_str(canvas, 2, y, "Quadratic Solver");
    y += 12;
    canvas_draw_str(canvas, 2, y, "Solve ax^2 + bx + c = 0");
    y += 12;
    canvas_draw_str(canvas, 2, y, "Author: @paul-sopin");
    y += 12;
    canvas_draw_str(canvas, 2, y, "github.com/paul-sopin");
    y += 12;

    canvas_draw_str(canvas, 2, 62, "Back to main menu");
}

// handle input
void input_callback(InputEvent* event, void* ctx);
void result_input_callback(InputEvent* event, void* ctx) {
    AppState* app = ctx;
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        app->screen_state = SCREEN_INPUT;
        view_port_draw_callback_set(app->view_port, draw_input, app);
        view_port_input_callback_set(app->view_port, input_callback, app);
        view_port_update(app->view_port);
    }
}

void input_callback(InputEvent* event, void* ctx) {
    AppState* app = ctx;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
            app->selected = (app->selected + 3) % 4;
            break;
        case InputKeyDown:
            app->selected = (app->selected + 1) % 4;
            break;
        case InputKeyLeft:
            if(app->selected == 0) app->a -= 0.5f;
            if(app->selected == 1) app->b -= 0.5f;
            if(app->selected == 2) app->c -= 0.5f;
            break;
        case InputKeyRight:
            if(app->selected == 0) app->a += 0.5f;
            if(app->selected == 1) app->b += 0.5f;
            if(app->selected == 2) app->c += 0.5f;
            if(app->selected == 3) {
                app->screen_state = SCREEN_ABOUT;
                view_port_draw_callback_set(app->view_port, draw_about, app);
                view_port_input_callback_set(app->view_port, result_input_callback, app);
            }
            break;
        case InputKeyOk:
            solve_quadratic(app);
            app->screen_state = SCREEN_RESULT;
            view_port_draw_callback_set(app->view_port, draw_result, app);
            view_port_input_callback_set(app->view_port, result_input_callback, app);
            break;
        case InputKeyBack:
            app->exit = true;
            break;
        default:
            break;
        }
    }
    view_port_update(app->view_port);
}

// app entry
int32_t main_quadratic_solver_app(void* p) {
    UNUSED(p);

    AppState* app = malloc(sizeof(AppState));
    app->a = 1.0f;
    app->b = 0.0f;
    app->c = 0.0f;
    app->selected = 0;
    app->exit = false;
    app->screen_state = SCREEN_INPUT;
    app->result[0] = '\0';

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_input, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    while(!app->exit) {
        furi_delay_ms(50);
    }

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
