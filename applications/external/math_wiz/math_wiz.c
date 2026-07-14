#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <math.h>

#define MAX_DEGREE  6
#define SCROLL_STEP 1

typedef enum {
    ModeInputCoefficients,
    ModeViewPolynomial,
    ModeEditCalculus,
    ModeInstructions,
    ModeMainMenu,
    ModeSetX
} AppMode;

typedef struct {
    double coefficients[MAX_DEGREE + 1];
    int degree;
    double x_value;
    uint8_t vertical_scroll_offset;
    uint8_t horizontal_scroll_offset;
    uint8_t scroll_pos_total;
    AppMode mode; // Current mode of the app
    int current_input_index; // To track which coefficient we are editing
    uint32_t back_button_press_time; // Track back button press duration
    bool running; // Flag to control the main loop
} MathWizApp;

static double evaluate_polynomial(double x, int degree, const double coefficients[]) {
    double result = 0.0;
    for(int i = 0; i <= degree; i++) {
        result += coefficients[i] * pow(x, i);
    }
    return result;
}

static void display_polynomial(MathWizApp* app, FuriString* output) {
    furi_string_cat_str(output, "Polynomial: ");
    for(int i = app->degree; i >= 0; i--) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%+lf*x^%d ", app->coefficients[i], i);
        furi_string_cat_str(output, buffer);
    }
    app->scroll_pos_total = furi_string_size(output);
}

static void take_derivative(MathWizApp* app) {
    for(int i = 1; i <= app->degree; i++) {
        app->coefficients[i - 1] = app->coefficients[i] * i;
    }
    app->coefficients[app->degree] = 0;
    if(app->degree > 0) {
        app->degree--;
    }
}

static void take_integral(MathWizApp* app) {
    for(int i = app->degree; i >= 0; i--) {
        app->coefficients[i + 1] = app->coefficients[i] / (i + 1);
    }
    app->coefficients[0] = 0;
    app->degree++;
}

static void math_wiz_input_callback(InputEvent* input_event, void* context) {
    MathWizApp* app = context;

    if(input_event->type == InputTypePress) {
        switch(app->mode) {
        case ModeMainMenu:
            switch(input_event->key) {
            case InputKeyUp:
                app->mode = ModeInstructions;
                break;
            case InputKeyOk:
                app->mode = ModeSetX; // Start by setting X
                break;
            case InputKeyBack:
                app->running = false; // Exit the app
                break;
            default:
                break;
            }
            break;

        case ModeSetX: // Adjust the x value
            switch(input_event->key) {
            case InputKeyUp:
                app->x_value += (double)0.1; // Increment x
                break;
            case InputKeyDown:
                app->x_value -= (double)0.1; // Decrement x
                break;
            case InputKeyOk:
                app->mode = ModeInputCoefficients; // Transition to coefficient input
                app->current_input_index = 0; // Reset input index
                break;
            case InputKeyBack:
                app->mode = ModeMainMenu; // Cancel and return to main menu
                break;
            default:
                break;
            }
            break;

        case ModeInputCoefficients:
            switch(input_event->key) {
            case InputKeyUp:
                app->coefficients[app->current_input_index] += (double)0.1;
                break;
            case InputKeyDown:
                app->coefficients[app->current_input_index] -= (double)0.1;
                break;
            case InputKeyOk:
                app->current_input_index++;
                if(app->current_input_index > app->degree) {
                    app->mode = ModeViewPolynomial;
                }
                break;
            case InputKeyBack:
                app->mode = ModeViewPolynomial;
                break;
            default:
                break;
            }
            break;

        case ModeViewPolynomial:
            switch(input_event->key) {
            case InputKeyLeft:
                if(app->horizontal_scroll_offset > 0) {
                    app->horizontal_scroll_offset -= SCROLL_STEP;
                }
                break;
            case InputKeyRight:
                if(app->horizontal_scroll_offset < app->scroll_pos_total) {
                    app->horizontal_scroll_offset += SCROLL_STEP;
                }
                break;
            case InputKeyOk:
                app->mode = ModeEditCalculus;
                break;
            case InputKeyBack:
                app->mode = ModeMainMenu;
                break;
            default:
                break;
            }
            break;

        case ModeEditCalculus:
            switch(input_event->key) {
            case InputKeyUp:
                take_derivative(app);
                app->mode = ModeViewPolynomial;
                break;
            case InputKeyDown:
                take_integral(app);
                app->mode = ModeViewPolynomial;
                break;
            case InputKeyBack:
                app->mode = ModeViewPolynomial;
                break;
            default:
                break;
            }
            break;

        case ModeInstructions:
            switch(input_event->key) {
            case InputKeyBack:
                app->mode = ModeMainMenu;
                break;
            default:
                break;
            }
            break;

        default:
            break;
        }
    }
}

static void math_wiz_render_callback(Canvas* canvas, void* context) {
    MathWizApp* app = context;
    FuriString* output = furi_string_alloc();
    char buffer[64]; // Declare buffer once here and reuse

    switch(app->mode) {
    case ModeMainMenu:
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "Main Menu:");
        canvas_draw_str_aligned(canvas, 0, 10, AlignLeft, AlignTop, "UP: Instructions");
        canvas_draw_str_aligned(canvas, 0, 30, AlignLeft, AlignTop, "OK: Set X Value");
        canvas_draw_str_aligned(canvas, 0, 40, AlignLeft, AlignTop, "BACK: Exit App");
        break;

    case ModeSetX:
        snprintf(buffer, sizeof(buffer), "Set X Value: %.2lf", app->x_value);
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, buffer);
        canvas_draw_str_aligned(canvas, 0, 10, AlignLeft, AlignTop, "UP: Increase");
        canvas_draw_str_aligned(canvas, 0, 30, AlignLeft, AlignTop, "DOWN: Decrease");
        canvas_draw_str_aligned(canvas, 0, 40, AlignLeft, AlignTop, "OK: Confirm BACK: Cancel");
        break;

    case ModeInputCoefficients:
        snprintf(
            buffer,
            sizeof(buffer),
            "Set coefficient for x^%d: %.2lf",
            app->current_input_index,
            app->coefficients[app->current_input_index]);
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, buffer);
        canvas_draw_str_aligned(canvas, 0, 10, AlignLeft, AlignTop, "UP/DOWN: Adjust");
        canvas_draw_str_aligned(canvas, 0, 30, AlignLeft, AlignTop, "OK: Next Coefficient");
        canvas_draw_str_aligned(canvas, 0, 40, AlignLeft, AlignTop, "BACK: Return to Polynomial");
        break;

    case ModeViewPolynomial:
        display_polynomial(app, output);
        const char* output_text = furi_string_get_cstr(output) + app->horizontal_scroll_offset;
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, output_text);

        snprintf(
            buffer,
            sizeof(buffer),
            "f(%lf) = %lf",
            app->x_value,
            evaluate_polynomial(app->x_value, app->degree, app->coefficients));
        canvas_draw_str_aligned(canvas, 0, 10, AlignLeft, AlignTop, buffer);

        canvas_draw_str_aligned(canvas, 0, 30, AlignLeft, AlignTop, "LEFT/RIGHT: Scroll");
        canvas_draw_str_aligned(canvas, 0, 40, AlignLeft, AlignTop, "BACK: Return to Menu");
        break;

    case ModeEditCalculus:
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "Calculus Options:");
        canvas_draw_str_aligned(canvas, 0, 10, AlignLeft, AlignTop, "UP: Derivative");
        canvas_draw_str_aligned(canvas, 0, 30, AlignLeft, AlignTop, "DOWN: Integral");
        canvas_draw_str_aligned(canvas, 0, 40, AlignLeft, AlignTop, "BACK: Return to Polynomial");
        break;

    case ModeInstructions:
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "Instructions:");
        canvas_draw_str_aligned(canvas, 0, 10, AlignLeft, AlignTop, "Navigate the app:");
        canvas_draw_str_aligned(canvas, 0, 30, AlignLeft, AlignTop, "- UP/DOWN/LEFT/RIGHT");
        canvas_draw_str_aligned(canvas, 0, 40, AlignLeft, AlignTop, "BACK: Return to Menu");
        break;

    default:
        canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, "Unexpected Mode");
        break;
    }
}

int32_t math_wiz_main(void* p) {
    (void)p;
    MathWizApp* app = malloc(sizeof(MathWizApp));
    app->degree = MAX_DEGREE;
    app->running = true; // Initialize the running flag
    app->mode = ModeMainMenu; // Start in the main menu
    app->x_value = 0.0; // Set initial x_value to 0

    // Initialize coefficients to 0
    for(int i = 0; i <= app->degree; i++) {
        app->coefficients[i] = 0.0;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, math_wiz_render_callback, app);
    view_port_input_callback_set(view_port, math_wiz_input_callback, app);

    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Main loop
    while(app->running) {
        furi_delay_ms(100);
    }

    // Cleanup
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close("gui");
    free(app);

    return 0;
}
