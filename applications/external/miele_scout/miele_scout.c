#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <infrared/worker/infrared_transmit.h>

/* ── NEC protocol constants ─────────────────────────────────────────── */

#define MIELE_IR_ADDRESS 0x01
#define MIELE_IR_REPEATS 1

/* ── IR command table ───────────────────────────────────────────────── */

typedef struct {
    const char* name;
    uint32_t command;
} MieleCommand;

/* Drive-mode commands */
static const uint32_t cmd_up    = 0x08;
static const uint32_t cmd_down  = 0x09;
static const uint32_t cmd_left  = 0x0B;
static const uint32_t cmd_right = 0x0A;
static const uint32_t cmd_start = 0x06; /* PLAY/PAUSE — starts cleaning */

/* Menu-mode commands */
static const MieleCommand menu_commands[] = {
    {"POWER",      0x07},
    {"BASE",       0x04},
    {"PLAY/PAUSE", 0x06},
    {"OK",         0x0C},
    {"MODE",       0x05},
    {"WIFI",       0x01},
    {"TIMER",      0x03},
    {"CLOCK",      0x02},
    {"CLIMB",      0x0D},
    {"MUTE",       0x0E},
};

#define MENU_COUNT (sizeof(menu_commands) / sizeof(menu_commands[0]))

/* ── App mode ───────────────────────────────────────────────────────── */

typedef enum {
    ModeDrive,
    ModeMenu,
} AppMode;

typedef enum {
    DirNone,
    DirUp,
    DirDown,
    DirLeft,
    DirRight,
    DirOk,
} ActiveDirection;

typedef struct {
    AppMode mode;
    int menu_index;
    ActiveDirection active_dir;    /* brief highlight on TX */
    bool tx_flash;                 /* brief flash on any TX */
    bool running;
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notif;
} MieleScoutApp;

/* ── IR transmit helper ─────────────────────────────────────────────── */

static void miele_send(MieleScoutApp* app, uint32_t command) {
    InfraredMessage msg = {
        .protocol = InfraredProtocolNEC,
        .address = MIELE_IR_ADDRESS,
        .command = command,
        .repeat = false,
    };

    infrared_send(&msg, MIELE_IR_REPEATS);

    /* Haptic + LED feedback */
    notification_message(app->notif, &sequence_blink_magenta_10);

    /* Visual feedback flag */
    app->tx_flash = true;
}

/* ── Draw callback ──────────────────────────────────────────────────── */

static void draw_callback(Canvas* canvas, void* ctx) {
    MieleScoutApp* app = ctx;
    canvas_clear(canvas);

    /* Portrait: 64w x 128h */

    /* ── Header bar ── */
    canvas_set_font(canvas, FontPrimary);
    if(app->mode == ModeDrive) {
        canvas_draw_str(canvas, 2, 12, "DRIVE");
    } else {
        canvas_draw_str(canvas, 2, 12, "MENU");
    }

    /* TX indicator */
    if(app->tx_flash) {
        canvas_draw_str_aligned(canvas, 62, 12, AlignRight, AlignBottom, "[TX]");
        app->tx_flash = false;
    } else {
        canvas_draw_str_aligned(canvas, 62, 12, AlignRight, AlignBottom, "[IR]");
    }

    /* Separator line */
    canvas_draw_line(canvas, 0, 15, 64, 15);

    if(app->mode == ModeDrive) {
        /* ── Drive mode UI ── */

        /* Center of d-pad graphic — centered in the available area */
        const int cx = 32;
        const int cy = 60;
        const int arrow_len = 18;
        const int head = 5;

        /* Up arrow */
        canvas_draw_line(canvas, cx, cy - arrow_len, cx, cy - 5);
        canvas_draw_line(canvas, cx - head, cy - arrow_len + head, cx, cy - arrow_len);
        canvas_draw_line(canvas, cx + head, cy - arrow_len + head, cx, cy - arrow_len);
        if(app->active_dir == DirUp) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, cx, cy - arrow_len - 3, AlignCenter, AlignBottom, "UP");
        }

        /* Down arrow */
        canvas_draw_line(canvas, cx, cy + 5, cx, cy + arrow_len);
        canvas_draw_line(canvas, cx - head, cy + arrow_len - head, cx, cy + arrow_len);
        canvas_draw_line(canvas, cx + head, cy + arrow_len - head, cx, cy + arrow_len);
        if(app->active_dir == DirDown) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, cx, cy + arrow_len + 9, AlignCenter, AlignBottom, "DN");
        }

        /* Left arrow */
        canvas_draw_line(canvas, cx - arrow_len, cy, cx - 5, cy);
        canvas_draw_line(canvas, cx - arrow_len + head, cy - head, cx - arrow_len, cy);
        canvas_draw_line(canvas, cx - arrow_len + head, cy + head, cx - arrow_len, cy);
        if(app->active_dir == DirLeft) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, cx - arrow_len - 2, cy + 3, AlignRight, AlignBottom, "LT");
        }

        /* Right arrow */
        canvas_draw_line(canvas, cx + 5, cy, cx + arrow_len, cy);
        canvas_draw_line(canvas, cx + arrow_len - head, cy - head, cx + arrow_len, cy);
        canvas_draw_line(canvas, cx + arrow_len - head, cy + head, cx + arrow_len, cy);
        if(app->active_dir == DirRight) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, cx + arrow_len + 2, cy + 3, AlignLeft, AlignBottom, "RT");
        }

        /* Center dot / OK */
        if(app->active_dir == DirOk) {
            canvas_draw_disc(canvas, cx, cy, 5);
        } else {
            canvas_draw_circle(canvas, cx, cy, 4);
        }

        /* Bottom hints */
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 32, 110, AlignCenter, AlignBottom, "OK = Start");
        canvas_draw_str_aligned(canvas, 32, 122, AlignCenter, AlignBottom, "Back = Menu");

        /* Reset active direction after drawing */
        app->active_dir = DirNone;

    } else {
        /* ── Menu mode UI ── */

        canvas_set_font(canvas, FontSecondary);
        const int item_h = 11;
        const int visible = 8;
        const int list_y = 18;

        /* Calculate scroll offset to keep selection visible */
        int scroll = 0;
        if(app->menu_index >= visible) {
            scroll = app->menu_index - visible + 1;
        }

        for(int i = 0; i < visible && (scroll + i) < (int)MENU_COUNT; i++) {
            int idx = scroll + i;
            int y = list_y + i * item_h;

            if(idx == app->menu_index) {
                /* Highlighted selection */
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_box(canvas, 0, y, 64, item_h);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str(canvas, 6, y + 9, menu_commands[idx].name);
                canvas_draw_str(canvas, 1, y + 9, ">");
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str(canvas, 6, y + 9, menu_commands[idx].name);
            }
        }

        /* Scroll indicators */
        canvas_set_color(canvas, ColorBlack);
        if(scroll > 0) {
            canvas_draw_str_aligned(canvas, 58, 19, AlignCenter, AlignBottom, "^");
        }
        if(scroll + visible < (int)MENU_COUNT) {
            canvas_draw_str_aligned(
                canvas, 58, list_y + visible * item_h + 2, AlignCenter, AlignBottom, "v");
        }

        /* Bottom hints */
        canvas_draw_str_aligned(canvas, 32, 110, AlignCenter, AlignBottom, "OK = Send");
        canvas_draw_str_aligned(canvas, 32, 122, AlignCenter, AlignBottom, "Back = Drive");
    }
}

/* ── Input callback ─────────────────────────────────────────────────── */

static void input_callback(InputEvent* event, void* ctx) {
    MieleScoutApp* app = ctx;
    furi_message_queue_put(app->queue, event, FuriWaitForever);
}

/* ── Main application ───────────────────────────────────────────────── */

int32_t miele_scout_app(void* p) {
    UNUSED(p);

    /* Allocate app state */
    MieleScoutApp* app = malloc(sizeof(MieleScoutApp));
    app->mode = ModeDrive;
    app->menu_index = 0;
    app->active_dir = DirNone;
    app->tx_flash = false;
    app->running = true;

    /* Open system services */
    app->gui = furi_record_open(RECORD_GUI);
    app->notif = furi_record_open(RECORD_NOTIFICATION);

    /* Event queue */
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    /* ViewPort — portrait orientation (IR points up when held vertical) */
    app->view_port = view_port_alloc();
    view_port_set_orientation(app->view_port, ViewPortOrientationVertical);
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    /* ── Main event loop ── */
    InputEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->queue, &event, 100) == FuriStatusOk) {

            /* Back button: short = toggle mode, long = exit */
            if(event.key == InputKeyBack) {
                if(event.type == InputTypeLong) {
                    app->running = false;
                    continue;
                }
                if(event.type == InputTypeShort) {
                    app->mode = (app->mode == ModeDrive) ? ModeMenu : ModeDrive;
                    view_port_update(app->view_port);
                    continue;
                }
            }

            if(app->mode == ModeDrive) {
                /* Drive mode: d-pad sends IR directly.
                   On release, flush the queue so queued repeats don't overcorrect. */
                if(event.type == InputTypeRelease) {
                    /* Drain any pending repeat events from the queue */
                    InputEvent discard;
                    while(furi_message_queue_get(app->queue, &discard, 0) == FuriStatusOk) {
                        /* discard */
                    }
                    app->active_dir = DirNone;
                    view_port_update(app->view_port);
                } else if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                    switch(event.key) {
                    case InputKeyUp:
                        app->active_dir = DirUp;
                        miele_send(app, cmd_up);
                        break;
                    case InputKeyDown:
                        app->active_dir = DirDown;
                        miele_send(app, cmd_down);
                        break;
                    case InputKeyLeft:
                        app->active_dir = DirLeft;
                        miele_send(app, cmd_left);
                        break;
                    case InputKeyRight:
                        app->active_dir = DirRight;
                        miele_send(app, cmd_right);
                        break;
                    case InputKeyOk:
                        app->active_dir = DirOk;
                        miele_send(app, cmd_start);
                        break;
                    default:
                        break;
                    }
                    view_port_update(app->view_port);
                }

            } else {
                /* Menu mode: d-pad navigates list, OK sends selected */
                if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                    switch(event.key) {
                    case InputKeyUp:
                        if(app->menu_index > 0) {
                            app->menu_index--;
                        } else {
                            app->menu_index = MENU_COUNT - 1;
                        }
                        break;
                    case InputKeyDown:
                        if(app->menu_index < (int)MENU_COUNT - 1) {
                            app->menu_index++;
                        } else {
                            app->menu_index = 0;
                        }
                        break;
                    case InputKeyOk:
                        miele_send(app, menu_commands[app->menu_index].command);
                        break;
                    default:
                        break;
                    }
                    view_port_update(app->view_port);
                }
            }

        } else {
            /* Timeout with no event — just refresh display */
            view_port_update(app->view_port);
        }
    }

    /* ── Cleanup ── */
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);

    return 0;
}
