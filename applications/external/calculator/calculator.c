#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdbool.h> // Header-file for boolean data-type.
#include <string.h> // Header-file for string functions.
#include "tinyexpr.h" // Header-file for the TinyExpr library.

#include <stdio.h>
#include <stdlib.h>
#include <math.h> // isnan / isinf guards for the result

const short MAX_TEXT_LENGTH = 20;

typedef struct {
    short x;
    short y;
} selectedPosition;

typedef struct {
    FuriMutex* mutex;
    selectedPosition position;
    // Holds both the typed expression (capped at MAX_TEXT_LENGTH) and the
    // formatted result, which can be longer, so keep some headroom.
    char text[32];
    short textLength;
    char log[20];
} Calculator;

char getKeyAtPosition(short x, short y) {
    if(x == 0 && y == 0) {
        return 'C';
    }
    if(x == 1 && y == 0) {
        return '<';
    }
    if(x == 2 && y == 0) {
        return '%';
    }
    if(x == 3 && y == 0) {
        return '/';
    }
    if(x == 0 && y == 1) {
        return '1';
    }
    if(x == 1 && y == 1) {
        return '2';
    }
    if(x == 2 && y == 1) {
        return '3';
    }
    if(x == 3 && y == 1) {
        return '*';
    }
    if(x == 0 && y == 2) {
        return '4';
    }
    if(x == 1 && y == 2) {
        return '5';
    }
    if(x == 2 && y == 2) {
        return '6';
    }
    if(x == 3 && y == 2) {
        return '-';
    }
    if(x == 0 && y == 3) {
        return '7';
    }
    if(x == 1 && y == 3) {
        return '8';
    }
    if(x == 2 && y == 3) {
        return '9';
    }
    if(x == 3 && y == 3) {
        return '+';
    }
    if(x == 0 && y == 4) {
        return '(';
    }
    if(x == 1 && y == 4) {
        return '0';
    }
    if(x == 2 && y == 4) {
        return '.';
    }
    if(x == 3 && y == 4) {
        return '=';
    }
    return ' ';
}

void generate_calculator_layout(Canvas* canvas) {
    //draw dotted lines
    for(int i = 0; i <= 64; i++) {
        if(i % 2 == 0) {
            canvas_draw_dot(canvas, i, 14);
            canvas_draw_dot(canvas, i, 33);
        }
        if(i % 2 == 1) {
            canvas_draw_dot(canvas, i, 15);
            canvas_draw_dot(canvas, i, 34);
        }
    }

    //draw horizontal lines
    canvas_draw_box(canvas, 0, 41, 64, 2);
    canvas_draw_box(canvas, 0, 57, 64, 2);
    canvas_draw_box(canvas, 0, 73, 64, 2);
    canvas_draw_box(canvas, 0, 89, 64, 2);
    canvas_draw_box(canvas, 0, 105, 64, 2);
    canvas_draw_box(canvas, 0, 121, 64, 2);

    //draw vertical lines
    canvas_draw_box(canvas, 0, 43, 1, 80);
    canvas_draw_box(canvas, 15, 43, 2, 80);
    canvas_draw_box(canvas, 31, 43, 2, 80);
    canvas_draw_box(canvas, 47, 43, 2, 80);
    canvas_draw_box(canvas, 63, 43, 1, 80);

    //draw buttons
    //row 1 (C, <-, %, /)   long-press / for sqrt(
    canvas_draw_str(canvas, 5, 54, "C");
    canvas_draw_str(canvas, 19, 54, " <-");
    canvas_draw_str(canvas, 35, 54, " %");
    canvas_draw_str(canvas, 51, 54, " /");

    //row 2 (1, 2, 3, X)    long-press X (*) for ^ (power)
    canvas_draw_str(canvas, 5, 70, " 1");
    canvas_draw_str(canvas, 19, 70, " 2");
    canvas_draw_str(canvas, 35, 70, " 3");
    canvas_draw_str(canvas, 51, 70, " X");

    //row 3 (4, 5, 6, -)
    canvas_draw_str(canvas, 5, 86, " 4");
    canvas_draw_str(canvas, 19, 86, " 5");
    canvas_draw_str(canvas, 35, 86, " 6");
    canvas_draw_str(canvas, 51, 86, " -");

    //row 4 (7, 8, 9, +)
    canvas_draw_str(canvas, 5, 102, " 7");
    canvas_draw_str(canvas, 19, 102, " 8");
    canvas_draw_str(canvas, 35, 102, " 9");
    canvas_draw_str(canvas, 51, 102, " +");

    //row 5 (( ), 0, ., =)   long-press ( for )
    canvas_draw_str(canvas, 3, 118, "( )");
    canvas_draw_str(canvas, 19, 118, " 0");
    canvas_draw_str(canvas, 35, 118, "   .");
    canvas_draw_str(canvas, 51, 118, " =");
}

void calculator_draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    const Calculator* calculator_state = ctx;
    furi_mutex_acquire(calculator_state->mutex, FuriWaitForever);

    canvas_clear(canvas);

    //show selected button
    short startX = 1;
    short startY = 43;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(
        canvas,
        startX + (calculator_state->position.x) * 16,
        (startY) + (calculator_state->position.y) * 16,
        16,
        16);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(
        canvas,
        startX + (calculator_state->position.x) * 16 + 2,
        (startY) + (calculator_state->position.y) * 16 + 2,
        10,
        10);

    canvas_set_color(canvas, ColorBlack);
    generate_calculator_layout(canvas);

    //draw text (measure with the active font so the cursor/scroll track exactly)
    short stringWidth = (short)canvas_string_width(canvas, calculator_state->text);
    short startingPosition = 5;
    if(stringWidth > 60) {
        startingPosition += 60 - (stringWidth + 5);
    }
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, startingPosition, 28, calculator_state->text);
    //canvas_draw_str(canvas, 10, 10, calculator_state->log);

    //draw cursor at the end of the text (follows the scroll offset)
    canvas_draw_box(canvas, startingPosition + stringWidth, 29, 5, 1);

    furi_mutex_release(calculator_state->mutex);
}

void calculator_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// Number of fractional digits kept in the displayed result.
#define CALC_DECIMALS   6
// Index of the last byte of `text`, reserved for the terminating '\0';
// character writes stay strictly below it (textLength < CALC_RESULT_MAX).
#define CALC_RESULT_MAX ((short)(sizeof(((Calculator*)0)->text) - 1))

void calculate(Calculator* calculator_state) {
    double result;
    result = te_interp(calculator_state->text, 0);

    calculator_state->textLength = 0;
    calculator_state->text[0] = '\0';

    // te_interp returns NaN on a malformed expression and +/-inf on things
    // like division by zero. Also reject finite results too large for the
    // integer-based formatter below (now reachable via the '^' key), which
    // would otherwise overflow (long long) and print a wrong number. The
    // limit ~9.0e18 stays safely under LLONG_MAX (~9.22e18); the literal is
    // an integer cast to double so it isn't demoted to float by the toolchain.
    const double CALC_MAX = (double)9000000000000000000LL;
    if(isnan(result) || isinf(result) || result > CALC_MAX || result < -CALC_MAX) {
        const char* err = "Error";
        while(*err && calculator_state->textLength < CALC_RESULT_MAX) {
            calculator_state->text[calculator_state->textLength++] = *err++;
        }
        calculator_state->text[calculator_state->textLength] = '\0';
        return;
    }

    // remember the sign; the '-' is only emitted once we know the rounded
    // value is non-zero, so a tiny negative that rounds to 0 shows "0" not "-0"
    bool negative = result < 0;
    if(negative) {
        result = -result;
    }

    // Split into integer and fractional parts. The fraction is rounded to
    // CALC_DECIMALS places, carrying into the integer part when it rounds up
    // (e.g. 0.9999995 -> 1). scale is 10^CALC_DECIMALS.
    long long scale = 1;
    for(int d = 0; d < CALC_DECIMALS; d++) {
        scale *= 10;
    }
    long long beforeDecimal = (long long)result;
    // llround avoids a floating-point literal, which the firmware would treat
    // as a float (-fsingle-precision-constant) and reject (-Wdouble-promotion).
    long long afterDecimal = llround((result - (double)beforeDecimal) * (double)scale);
    if(afterDecimal >= scale) {
        beforeDecimal += 1;
        afterDecimal -= scale;
    }

    if(negative && (beforeDecimal != 0 || afterDecimal != 0)) {
        calculator_state->text[calculator_state->textLength++] = '-';
    }

    char beforeDecimalString[24];
    char afterDecimalString[CALC_DECIMALS];
    int i = 0;
    //parse the integer part to a string (digits come out least-significant
    //first; always produce at least a single '0')
    if(beforeDecimal == 0) {
        beforeDecimalString[i++] = '0';
    } else {
        while(beforeDecimal > 0 && i < (int)sizeof(beforeDecimalString)) {
            beforeDecimalString[i++] = beforeDecimal % 10 + '0';
            beforeDecimal /= 10;
        }
    }
    //copy it to the answer most-significant digit first (no separate reversal)
    for(int j = i - 1; j >= 0 && calculator_state->textLength < CALC_RESULT_MAX; j--) {
        calculator_state->text[calculator_state->textLength++] = beforeDecimalString[j];
    }

    if(afterDecimal > 0) {
        // Write the fraction as a fixed-width field so leading zeros survive
        // (0.083 must not collapse to .83), then trim trailing zeros.
        for(int j = CALC_DECIMALS - 1; j >= 0; j--) {
            afterDecimalString[j] = afterDecimal % 10 + '0';
            afterDecimal /= 10;
        }
        // afterDecimal > 0 guarantees a non-zero digit survives the trim,
        // so frLen is always >= 1 here.
        int frLen = CALC_DECIMALS;
        while(frLen > 0 && afterDecimalString[frLen - 1] == '0') {
            frLen--;
        }

        if(calculator_state->textLength < CALC_RESULT_MAX) {
            //add decimal point
            calculator_state->text[calculator_state->textLength++] = '.';

            //add numbers after decimal
            for(int j = 0; j < frLen && calculator_state->textLength < CALC_RESULT_MAX; j++) {
                calculator_state->text[calculator_state->textLength++] = afterDecimalString[j];
            }
        }
    }
    calculator_state->text[calculator_state->textLength] = '\0';
}

int32_t calculator_app(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    Calculator* calculator_state = malloc(sizeof(Calculator));
    // malloc does not zero memory: clear position, textLength and the text
    // buffer so the first draw shows an empty display instead of garbage.
    memset(calculator_state, 0, sizeof(Calculator));
    calculator_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!calculator_state->mutex) {
        //FURI_LOG_E("calculator", "cannot create mutex\r\n");
        free(calculator_state);
        return -1;
    }

    // Configure view port
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, calculator_draw_callback, calculator_state);
    view_port_input_callback_set(view_port, calculator_input_callback, event_queue);
    view_port_set_orientation(view_port, ViewPortOrientationVertical);

    // Register view port in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    //NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    InputEvent event;

    while(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
        //break out of the loop if the back key is pressed
        if(event.type == InputTypeShort && event.key == InputKeyBack) {
            break;
        }

        if(event.type == InputTypeShort) {
            switch(event.key) {
            case InputKeyUp:
                if(calculator_state->position.y > 0) {
                    calculator_state->position.y--;
                }
                break;
            case InputKeyDown:
                if(calculator_state->position.y < 4) {
                    calculator_state->position.y++;
                }
                break;
            case InputKeyLeft:
                if(calculator_state->position.x > 0) {
                    calculator_state->position.x--;
                }
                break;
            case InputKeyRight:
                if(calculator_state->position.x < 3) {
                    calculator_state->position.x++;
                }
                break;
            case InputKeyOk: {
                //add the selected button to the text
                //char* text = calculator_state->text;
                // short* textLength = &calculator_state->textLength;

                char key =
                    getKeyAtPosition(calculator_state->position.x, calculator_state->position.y);

                switch(key) {
                case 'C':
                    while(calculator_state->textLength > 0) {
                        calculator_state->text[calculator_state->textLength--] = '\0';
                    }
                    calculator_state->text[0] = '\0';
                    calculator_state->log[2] = key;
                    break;
                case '<':
                    calculator_state->log[2] = key;
                    if(calculator_state->textLength > 0) {
                        calculator_state->text[--calculator_state->textLength] = '\0';
                    } else {
                        calculator_state->text[0] = '\0';
                    }
                    break;
                case '=':
                    calculator_state->log[2] = key;
                    calculate(calculator_state);
                    break;
                case '%':
                case '/':
                case '*':
                case '-':
                case '+':
                case '.':
                case '(':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                case '0':
                    if(calculator_state->textLength < MAX_TEXT_LENGTH) {
                        calculator_state->text[calculator_state->textLength++] = key;
                        calculator_state->text[calculator_state->textLength] = '\0';
                    }
                    //calculator_state->log[1] = calculator_state->text[*textLength];
                    break;
                default:
                    break;
                }
            }
            default:
                break;
            }

            view_port_update(view_port);
        }

        if(event.type == InputTypeLong) {
            if(event.key == InputKeyOk) {
                // Long-press inserts the "secondary" symbol of the focused key:
                //   (  ->  )        *  ->  ^ (power)        /  ->  sqrt(
                char key =
                    getKeyAtPosition(calculator_state->position.x, calculator_state->position.y);
                const char* insert = NULL;
                if(key == '(') {
                    insert = ")";
                } else if(key == '*') {
                    insert = "^";
                } else if(key == '/') {
                    insert = "sqrt(";
                }
                if(insert != NULL) {
                    // insert the whole token or nothing, so no partial "sqrt(" is left
                    short len = (short)strlen(insert);
                    if(calculator_state->textLength + len <= MAX_TEXT_LENGTH) {
                        memcpy(&calculator_state->text[calculator_state->textLength], insert, len);
                        calculator_state->textLength += len;
                        calculator_state->text[calculator_state->textLength] = '\0';
                    }
                    view_port_update(view_port);
                }
            }
        }
    }
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_mutex_free(calculator_state->mutex);
    furi_message_queue_free(event_queue);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(calculator_state);

    return 0;
}
