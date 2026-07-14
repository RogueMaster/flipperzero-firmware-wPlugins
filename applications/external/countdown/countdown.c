#include <furi.h>
#include <gui/gui.h>
#include <gui/canvas.h>
#include <gui/view_port.h>
#include <gui/icon_i.h>
#include <input/input.h>
#include <string.h>
#include <stdlib.h>

#include "countdown_icons.h" // from icons/delete_10x10.png
#include "solver.h"

#define NUM_SELECTION_COUNT 14
#define GRID_COLS           5

// Phase 1: 14 numbers + delete icon
#define ITEM_COUNT_SELECTION   15
#define DELETE_INDEX_SELECTION 14
#define MAX_SELECTED           6

// Phase 2: digits 0–9 + delete icon
#define DIGIT_COUNT         10
#define ITEM_COUNT_TARGET   11
#define DELETE_INDEX_TARGET 10
#define MAX_TARGET_DIGITS   3

static const int numbers[NUM_SELECTION_COUNT] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    25,
    50,
    75,
    100,
};

typedef enum {
    ModeSelectNumbers, // Fase 1
    ModeSelectTarget, // Fase 2
    ModeResultMenu, // Fase 3: Target + Numbers + Solve
    ModeSolving, // Fase intermedia: "Solving..."
    ModeResultSolved, // Fase 4: solo pasos + texto
} GameMode;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    uint8_t cursor_index;
    bool running;

    // Phase 1
    int selected_values[MAX_SELECTED];
    uint8_t selected_count;

    // Phase 2
    uint8_t target_digits[MAX_TARGET_DIGITS];
    uint8_t target_digit_count;
    int target_value;

    // Solver output (phases 3–4)
    bool solution_exact;
    int solution_result;
    int solution_diff;
    SolveStep solution_steps[MAX_SOLVE_STEPS];
    uint8_t solution_step_count;

    // Solve timing
    uint32_t solve_time_ms;

    // Flag para que el bucle principal sepa que debe resolver
    bool solve_requested;

    GameMode mode;

} CountdownApp;

// ---------- Delete icon from PNG ----------

static void draw_delete_icon(Canvas* canvas, int x, int y) {
    canvas_draw_icon(canvas, x + 4, y - 9, &I_delete_10x10);
}

// ---------- Helper: reset everything ----------

static void reset_game(CountdownApp* app) {
    app->cursor_index = 0;
    app->selected_count = 0;
    app->target_digit_count = 0;
    app->target_value = 0;

    app->solution_exact = false;
    app->solution_result = 0;
    app->solution_diff = 0;
    app->solution_step_count = 0;
    app->solve_time_ms = 0;
    app->solve_requested = false;

    memset(app->selected_values, 0, sizeof(app->selected_values));
    memset(app->target_digits, 0, sizeof(app->target_digits));
    memset(app->solution_steps, 0, sizeof(app->solution_steps));

    app->mode = ModeSelectNumbers;
}

// ---------- PHASE 1 — Select numbers ----------

static void draw_selection_grid(Canvas* canvas, CountdownApp* app) {
    canvas_set_font(canvas, FontSecondary);

    const int start_x = 4;
    const int start_y = 14;
    const int cell_w = 22;
    const int cell_h = 12;

    for(uint8_t i = 0; i < ITEM_COUNT_SELECTION; i++) {
        int row = i / GRID_COLS;
        int col = i % GRID_COLS;

        int x = start_x + col * cell_w;
        int y = start_y + row * cell_h;

        if(i == app->cursor_index) {
            canvas_draw_frame(canvas, x - 3, y - 9, cell_w, cell_h);
        }

        if(i == DELETE_INDEX_SELECTION) {
            draw_delete_icon(canvas, x, y);
        } else {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", numbers[i]);
            canvas_draw_str(canvas, x, y, buf);
        }
    }

    canvas_set_font(canvas, FontPrimary);

    char line[64];
    int pos = snprintf(line, sizeof(line), "Sel:");
    for(uint8_t i = 0; i < app->selected_count; i++) {
        pos += snprintf(line + pos, sizeof(line) - pos, " %d", app->selected_values[i]);
    }

    if(app->selected_count >= MAX_SELECTED) {
        snprintf(line + pos, sizeof(line) - pos, " MAX");
    }

    canvas_draw_str(canvas, 2, 62, line);
}

static void handle_input_select_numbers(InputEvent* event, CountdownApp* app) {
    uint8_t idx = app->cursor_index;
    int col = idx % GRID_COLS;

    switch(event->key) {
    case InputKeyBack:
        // En fase 1, Back sale de la app
        app->running = false;
        break;

    case InputKeyLeft:
        if(col > 0) {
            app->cursor_index = idx - 1;
        } else if(idx >= GRID_COLS) {
            // Salto al último elemento de la fila anterior
            app->cursor_index = idx - 1;
        }
        break;

    case InputKeyRight: {
        if(col < GRID_COLS - 1 && idx + 1 < ITEM_COUNT_SELECTION) {
            app->cursor_index = idx + 1;
        } else {
            // Salto al primer elemento de la fila siguiente
            uint8_t next_row_start = idx + (GRID_COLS - col);
            if(next_row_start < ITEM_COUNT_SELECTION) {
                app->cursor_index = next_row_start;
            }
        }
        break;
    }

    case InputKeyUp:
        if(idx >= GRID_COLS) {
            app->cursor_index = idx - GRID_COLS;
        }
        break;

    case InputKeyDown:
        if(idx + GRID_COLS < ITEM_COUNT_SELECTION) {
            app->cursor_index = idx + GRID_COLS;
        }
        break;

    case InputKeyOk:
        if(idx == DELETE_INDEX_SELECTION) {
            if(app->selected_count > 0) app->selected_count--;
        } else if(idx < NUM_SELECTION_COUNT) {
            if(app->selected_count < MAX_SELECTED) {
                app->selected_values[app->selected_count++] = numbers[idx];
                if(app->selected_count == MAX_SELECTED) {
                    app->mode = ModeSelectTarget;
                    app->cursor_index = 0;
                    app->target_digit_count = 0;
                }
            }
        }
        break;

    case InputKeyMAX:
        break;
    }
}

// ---------- PHASE 2 — Select 3-digit target ----------

static void draw_target_grid(Canvas* canvas, CountdownApp* app) {
    canvas_set_font(canvas, FontSecondary);

    const int start_x = 8;
    const int start_y = 14;
    const int cell_w = 14;
    const int cell_h = 12;

    for(uint8_t i = 0; i < ITEM_COUNT_TARGET; i++) {
        int row = i / GRID_COLS;
        int col = i % GRID_COLS;

        int x = start_x + col * cell_w;
        int y = start_y + row * cell_h;

        if(i == app->cursor_index) {
            canvas_draw_frame(canvas, x - 2, y - 9, cell_w, cell_h);
        }

        if(i == DELETE_INDEX_TARGET) {
            draw_delete_icon(canvas, x, y);
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", i);
            canvas_draw_str(canvas, x + 2, y, buf);
        }
    }

    canvas_set_font(canvas, FontPrimary);

    char line[32];
    int pos = snprintf(line, sizeof(line), "Target:");
    for(uint8_t i = 0; i < app->target_digit_count; i++) {
        pos += snprintf(line + pos, sizeof(line) - pos, " %d", app->target_digits[i]);
    }
    for(uint8_t i = app->target_digit_count; i < MAX_TARGET_DIGITS; i++) {
        pos += snprintf(line + pos, sizeof(line) - pos, " _");
    }

    canvas_draw_str(canvas, 2, 62, line);
}

static void handle_input_select_target(InputEvent* event, CountdownApp* app) {
    uint8_t idx = app->cursor_index;
    int col = idx % GRID_COLS;

    switch(event->key) {
    case InputKeyBack:
        // Volver a fase 1 y borrar los números elegidos
        app->mode = ModeSelectNumbers;
        app->cursor_index = 0;
        app->selected_count = 0;
        memset(app->selected_values, 0, sizeof(app->selected_values));
        app->target_digit_count = 0;
        app->target_value = 0;
        break;

    case InputKeyLeft:
        if(col > 0) {
            app->cursor_index = idx - 1;
        } else if(idx >= GRID_COLS) {
            app->cursor_index = idx - 1;
        }
        break;

    case InputKeyRight: {
        if(col < GRID_COLS - 1 && idx + 1 < ITEM_COUNT_TARGET) {
            app->cursor_index = idx + 1;
        } else {
            uint8_t next_row_start = idx + (GRID_COLS - col);
            if(next_row_start < ITEM_COUNT_TARGET) {
                app->cursor_index = next_row_start;
            }
        }
        break;
    }

    case InputKeyUp:
        if(idx >= GRID_COLS) {
            app->cursor_index = idx - GRID_COLS;
        }
        break;

    case InputKeyDown:
        if(idx + GRID_COLS < ITEM_COUNT_TARGET) {
            app->cursor_index = idx + GRID_COLS;
        }
        break;

    case InputKeyOk:
        if(idx == DELETE_INDEX_TARGET) {
            if(app->target_digit_count > 0) app->target_digit_count--;
        } else if(idx < DIGIT_COUNT) {
            if(app->target_digit_count == 0 && idx == 0) break;

            if(app->target_digit_count < MAX_TARGET_DIGITS) {
                app->target_digits[app->target_digit_count++] = idx;

                if(app->target_digit_count == MAX_TARGET_DIGITS) {
                    app->target_value = app->target_digits[0] * 100 + app->target_digits[1] * 10 +
                                        app->target_digits[2];

                    app->mode = ModeResultMenu;
                }
            }
        }
        break;

    case InputKeyMAX:
        break;
    }
}

// ---------- PHASE 3 — Result menu (Target + Numbers + Solve) ----------

static void draw_result_menu(Canvas* canvas, CountdownApp* app) {
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    char target_buf[32];
    snprintf(target_buf, sizeof(target_buf), "Target: %d", app->target_value);
    canvas_draw_str(canvas, 2, 10, target_buf);

    canvas_set_font(canvas, FontSecondary);
    char nums_line[64];
    int pos = snprintf(nums_line, sizeof(nums_line), "Numbers:");
    for(uint8_t i = 0; i < app->selected_count; i++) {
        pos += snprintf(nums_line + pos, sizeof(nums_line) - pos, " %d", app->selected_values[i]);
    }
    canvas_draw_str(canvas, 2, 22, nums_line);

    // Un único botón Solve centrado
    canvas_set_font(canvas, FontSecondary);
    int btn_y = 62;
    int btn_w = 40;
    int btn_h = 10;
    int btn_x = 44; // centrado aproximado

    canvas_draw_frame(canvas, btn_x - 2, btn_y - 9, btn_w, btn_h);
    canvas_draw_str(canvas, btn_x, btn_y, "Solve");
}

static void handle_input_result_menu(InputEvent* event, CountdownApp* app) {
    switch(event->key) {
    case InputKeyBack:
        // Volver a fase 2 borrando el objetivo
        app->mode = ModeSelectTarget;
        app->cursor_index = 0;
        app->target_digit_count = 0;
        app->target_value = 0;
        break;

    case InputKeyLeft:
    case InputKeyRight:
    case InputKeyUp:
    case InputKeyDown:
        // No hay navegación aquí, solo botón Solve fijo
        break;

    case InputKeyOk:
        // Cambiamos a modo "Solving..." y pedimos resolver en el bucle principal
        app->mode = ModeSolving;
        app->solve_requested = true;
        break;

    case InputKeyMAX:
        break;
    }
}

// ---------- PHASE Solving — mostrar "Solving..." mientras se calcula ----------

static void draw_solving(Canvas* canvas, CountdownApp* app) {
    (void)app;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 20, 30, "Solving...");
}

// ---------- Helper para truncar texto con ... ----------

static void truncate_to_fit(Canvas* canvas, char* text, uint8_t max_width) {
    uint8_t width = canvas_string_width(canvas, text);
    if(width <= max_width) return;

    size_t len = strlen(text);
    if(len <= 3) {
        strcpy(text, "...");
        return;
    }

    // Reservamos 3 caracteres para "..."
    while(len > 3) {
        text[len - 4] = '.';
        text[len - 3] = '.';
        text[len - 2] = '.';
        text[len - 1] = '\0';
        len--;

        width = canvas_string_width(canvas, text);
        if(width <= max_width) break;
    }
}

// ---------- PHASE 4 — Result solved (first line: op left, time right) ----------

static void draw_result_solved(Canvas* canvas, CountdownApp* app) {
    canvas_clear(canvas);

    uint8_t screen_w = canvas_width(canvas);

    // Texto de tiempo en la esquina superior derecha con FontPrimary
    char time_buf[32];
    snprintf(time_buf, sizeof(time_buf), "%lu ms", (unsigned long)app->solve_time_ms);

    canvas_set_font(canvas, FontPrimary);
    uint8_t time_w = canvas_string_width(canvas, time_buf);
    int time_x = (int)screen_w - (int)time_w - 2;
    int header_y = 10;

    // Construir la primera línea (primera operación o "No solution") con FontSecondary
    canvas_set_font(canvas, FontSecondary);
    char header[80];

    if(app->solution_step_count == 0) {
        snprintf(header, sizeof(header), "No solution");
    } else {
        uint8_t steps_to_show = app->solution_step_count;
        if(steps_to_show > MAX_SOLVE_STEPS) steps_to_show = MAX_SOLVE_STEPS;

        SolveStep* s0 = &app->solution_steps[0];
        bool is_last = (steps_to_show == 1);

        if(is_last) {
            if(app->solution_exact) {
                snprintf(
                    header,
                    sizeof(header),
                    "%d %c %d = %d (exact)",
                    s0->a,
                    s0->op,
                    s0->b,
                    s0->result);
            } else {
                snprintf(
                    header,
                    sizeof(header),
                    "%d %c %d = %d (diff %d)",
                    s0->a,
                    s0->op,
                    s0->b,
                    s0->result,
                    app->solution_diff);
            }
        } else {
            snprintf(header, sizeof(header), "%d %c %d = %d", s0->a, s0->op, s0->b, s0->result);
        }
    }

    // Ajustar la cabecera para que quepa con el tiempo
    uint8_t max_header_width = time_x - 2; // margen izquierdo 2
    truncate_to_fit(canvas, header, max_header_width);

    // Dibujar header (FontSecondary) a la izquierda
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, header_y, header);

    // Dibujar tiempo (FontPrimary) a la derecha
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, time_x, header_y, time_buf);

    // Ahora dibujar el resto de pasos (si los hay) debajo
    canvas_set_font(canvas, FontSecondary);
    int y = 20;

    if(app->solution_step_count > 0) {
        uint8_t steps_to_show = app->solution_step_count;
        if(steps_to_show > MAX_SOLVE_STEPS) steps_to_show = MAX_SOLVE_STEPS;

        for(uint8_t i = 1; i < steps_to_show; i++) {
            SolveStep* s = &app->solution_steps[i];
            bool is_last = (i == steps_to_show - 1);

            char step_buf[80];
            if(is_last) {
                if(app->solution_exact) {
                    snprintf(
                        step_buf,
                        sizeof(step_buf),
                        "%d %c %d = %d (exact)",
                        s->a,
                        s->op,
                        s->b,
                        s->result);
                } else {
                    snprintf(
                        step_buf,
                        sizeof(step_buf),
                        "%d %c %d = %d (diff %d)",
                        s->a,
                        s->op,
                        s->b,
                        s->result,
                        app->solution_diff);
                }
            } else {
                snprintf(
                    step_buf, sizeof(step_buf), "%d %c %d = %d", s->a, s->op, s->b, s->result);
            }

            canvas_draw_str(canvas, 2, y, step_buf);
            y += 9;
            if(y > 52) break;
        }
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, "Back: previous   OK: restart");
}

static void handle_input_result_solved(InputEvent* event, CountdownApp* app) {
    switch(event->key) {
    case InputKeyBack:
        // Volver a fase 3 (menu Solve), manteniendo target y numeros
        app->mode = ModeResultMenu;
        break;

    case InputKeyLeft:
    case InputKeyRight:
    case InputKeyUp:
    case InputKeyDown:
        break;

    case InputKeyOk:
        // Restart completo: volver a fase 1, todo limpio
        reset_game(app);
        break;

    case InputKeyMAX:
        break;
    }
}

// ---------- Dispatchers ----------

static void countdown_draw(Canvas* canvas, void* ctx) {
    CountdownApp* app = ctx;

    switch(app->mode) {
    case ModeSelectNumbers:
        draw_selection_grid(canvas, app);
        break;
    case ModeSelectTarget:
        draw_target_grid(canvas, app);
        break;
    case ModeResultMenu:
        draw_result_menu(canvas, app);
        break;
    case ModeSolving:
        draw_solving(canvas, app);
        break;
    case ModeResultSolved:
        draw_result_solved(canvas, app);
        break;
    }
}

static void countdown_input(InputEvent* event, void* ctx) {
    CountdownApp* app = ctx;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;

    switch(app->mode) {
    case ModeSelectNumbers:
        handle_input_select_numbers(event, app);
        break;
    case ModeSelectTarget:
        handle_input_select_target(event, app);
        break;
    case ModeResultMenu:
        handle_input_result_menu(event, app);
        break;
    case ModeSolving:
        // Durante "Solving..." ignoramos entradas (es muy rápido)
        break;
    case ModeResultSolved:
        handle_input_result_solved(event, app);
        break;
    }

    view_port_update(app->view_port);
}

// ---------- Entry point ----------

int32_t countdown_app(void* p) {
    UNUSED(p);

    CountdownApp app;
    memset(&app, 0, sizeof(app));

    app.running = true;
    reset_game(&app);

    app.gui = furi_record_open("gui");
    app.view_port = view_port_alloc();

    view_port_draw_callback_set(app.view_port, countdown_draw, &app);
    view_port_input_callback_set(app.view_port, countdown_input, &app);

    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    while(app.running) {
        if(app.solve_requested) {
            app.solve_requested = false;

            uint32_t start = furi_get_tick();
            if(app.selected_count == MAX_SELECTED) {
                countdown_solve(
                    app.selected_values,
                    MAX_SELECTED,
                    app.target_value,
                    &app.solution_exact,
                    &app.solution_result,
                    &app.solution_diff,
                    app.solution_steps,
                    &app.solution_step_count);
            } else {
                app.solution_exact = false;
                app.solution_result = 0;
                app.solution_diff = -1;
                app.solution_step_count = 0;
            }
            uint32_t end = furi_get_tick();
            app.solve_time_ms = end - start;

            app.mode = ModeResultSolved;
            view_port_update(app.view_port);
        }

        furi_delay_ms(30);
    }

    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_record_close("gui");

    return 0;
}
