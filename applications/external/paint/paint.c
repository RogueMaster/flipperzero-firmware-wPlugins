#include <furi.h>
#include <dialogs/dialogs.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>
#include <string.h>

#define BOARD_WIDTH  32
#define BOARD_HEIGHT 16
#define CELL_SIZE    4

#define PAINT_SAVE_PATH     APP_DATA_PATH("canvas.bin")
#define PAINT_SAVE_TMP_PATH APP_DATA_PATH("canvas.bin.tmp")

//saved_struct tags the file with this magic + version and a checksum, so a
//foreign, truncated or corrupt save is rejected on load
#define PAINT_SAVE_MAGIC   0x50
#define PAINT_SAVE_VERSION 1

typedef enum {
    PaintModeMove, //cursor moves without touching the board
    PaintModeDraw, //cursor paints every cell it visits
    PaintModeErase, //cursor clears every cell it visits
    PaintModeCount,
} PaintMode;

typedef struct selected_position {
    int x;
    int y;
} selected_position;

typedef struct {
    FuriMutex* mutex;
    selected_position selected;
    bool board[BOARD_WIDTH][BOARD_HEIGHT];
    PaintMode mode;
} PaintData;

void paint_draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    const PaintData* paint_state = ctx;
    furi_mutex_acquire(paint_state->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    //draw the board(32x16) on screen(128x64) using 4x4 tiles
    for(int y = 0; y < BOARD_HEIGHT; y++) {
        for(int x = 0; x < BOARD_WIDTH; x++) {
            if(paint_state->board[x][y]) {
                canvas_draw_box(canvas, x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
            }
        }
    }

    //the cursor is drawn in XOR so it stays visible on both painted and
    //empty cells; its shape shows the active mode
    int cursor_x = paint_state->selected.x * CELL_SIZE;
    int cursor_y = paint_state->selected.y * CELL_SIZE;
    canvas_set_color(canvas, ColorXOR);
    switch(paint_state->mode) {
    case PaintModeMove: //small dot: the pen is up
        canvas_draw_box(canvas, cursor_x + 1, cursor_y + 1, 2, 2);
        break;
    case PaintModeDraw: //full cell: the pen is down
        canvas_draw_box(canvas, cursor_x, cursor_y, CELL_SIZE, CELL_SIZE);
        break;
    case PaintModeErase: //hollow frame: the eraser is down
        canvas_draw_frame(canvas, cursor_x, cursor_y, CELL_SIZE, CELL_SIZE);
        break;
    default:
        break;
    }

    furi_mutex_release(paint_state->mutex);
}

void paint_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

//paint or erase the cell under the cursor according to the active mode
static void paint_apply_mode(PaintData* paint_state) {
    if(paint_state->mode == PaintModeDraw) {
        paint_state->board[paint_state->selected.x][paint_state->selected.y] = true;
    } else if(paint_state->mode == PaintModeErase) {
        paint_state->board[paint_state->selected.x][paint_state->selected.y] = false;
    }
}

static void paint_move_cursor(PaintData* paint_state, int dx, int dy) {
    //apply the mode to the cell being left as well, so a stroke
    //includes the cell it started on
    paint_apply_mode(paint_state);
    paint_state->selected.x = CLAMP(paint_state->selected.x + dx, BOARD_WIDTH - 1, 0);
    paint_state->selected.y = CLAMP(paint_state->selected.y + dy, BOARD_HEIGHT - 1, 0);
    paint_apply_mode(paint_state);
}

//react to the initial press and keep moving while the key is held;
//InputTypeLong is included so movement does not stall between the
//press and the first repeat
static bool paint_is_move_event(InputType type) {
    return type == InputTypePress || type == InputTypeLong || type == InputTypeRepeat;
}

static void paint_load(PaintData* paint_state) {
    //a missing or corrupt save just leaves the initialized empty canvas
    if(!saved_struct_load(
           PAINT_SAVE_PATH,
           paint_state->board,
           sizeof(paint_state->board),
           PAINT_SAVE_MAGIC,
           PAINT_SAVE_VERSION)) {
        memset(paint_state->board, 0, sizeof(paint_state->board));
    }
}

static bool paint_save(const PaintData* paint_state) {
    //write to a temporary file first, then rename it over the real save, so a
    //failed write cannot destroy the previously saved canvas
    if(!saved_struct_save(
           PAINT_SAVE_TMP_PATH,
           paint_state->board,
           sizeof(paint_state->board),
           PAINT_SAVE_MAGIC,
           PAINT_SAVE_VERSION)) {
        FURI_LOG_E("paint", "cannot write the canvas");
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FS_Error error = storage_common_rename(storage, PAINT_SAVE_TMP_PATH, PAINT_SAVE_PATH);
    furi_record_close(RECORD_STORAGE);
    if(error != FSE_OK) {
        FURI_LOG_E("paint", "cannot save the canvas: %s", storage_error_get_desc(error));
        return false;
    }
    return true;
}

int32_t paint_app(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    PaintData* paint_state = malloc(sizeof(PaintData));
    memset(paint_state->board, 0, sizeof(paint_state->board));
    paint_state->selected.x = BOARD_WIDTH / 2;
    paint_state->selected.y = BOARD_HEIGHT / 2;
    paint_state->mode = PaintModeMove;
    paint_load(paint_state);

    paint_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!paint_state->mutex) {
        FURI_LOG_E("paint", "cannot create mutex\r\n");
        free(paint_state);
        furi_message_queue_free(event_queue);
        return -1;
    }

    // Configure view port
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, paint_draw_callback, paint_state);
    view_port_input_callback_set(view_port, paint_input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running &&
          furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
        furi_mutex_acquire(paint_state->mutex, FuriWaitForever);

        switch(event.key) {
        case InputKeyUp:
            if(paint_is_move_event(event.type)) paint_move_cursor(paint_state, 0, -1);
            break;
        case InputKeyDown:
            if(paint_is_move_event(event.type)) paint_move_cursor(paint_state, 0, 1);
            break;
        case InputKeyLeft:
            if(paint_is_move_event(event.type)) paint_move_cursor(paint_state, -1, 0);
            break;
        case InputKeyRight:
            if(paint_is_move_event(event.type)) paint_move_cursor(paint_state, 1, 0);
            break;
        case InputKeyOk:
            if(event.type == InputTypeShort) {
                if(paint_state->mode == PaintModeMove) {
                    //toggle the cell under the cursor
                    paint_state->board[paint_state->selected.x][paint_state->selected.y] =
                        !paint_state->board[paint_state->selected.x][paint_state->selected.y];
                } else {
                    //a short press lifts the pen or eraser
                    paint_state->mode = PaintModeMove;
                }
            } else if(event.type == InputTypeLong) {
                paint_state->mode = (PaintMode)((paint_state->mode + 1) % PaintModeCount);
            }
            break;
        case InputKeyBack:
            if(event.type == InputTypeLong) {
                //start over with an empty canvas and the pen lifted
                memset(paint_state->board, 0, sizeof(paint_state->board));
                paint_state->mode = PaintModeMove;
            } else if(event.type == InputTypeShort) {
                running = false;
            }
            break;
        default:
            break;
        }

        furi_mutex_release(paint_state->mutex);
        view_port_update(view_port);
    }

    //tear down the viewport before the (potentially slow) save so input
    //events can no longer pile up in a queue nobody drains anymore
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);

    if(!paint_save(paint_state)) {
        //the drawing could not be written out - tell the user instead of
        //silently losing it
        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
        dialog_message_show_storage_error(dialogs, "Cannot save\nthe canvas");
        furi_record_close(RECORD_DIALOGS);
    }

    furi_mutex_free(paint_state->mutex);
    free(paint_state);

    return 0;
}
