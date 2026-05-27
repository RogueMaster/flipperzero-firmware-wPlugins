#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal_bt.h>

#define TAG "NestBridge"

#define CMD_IDLE      0x00
#define CMD_SET_TEMP  0xF0
#define CMD_MODE_HEAT 0x04
#define CMD_MODE_COOL 0x05
#define CMD_MODE_HC   0x06
#define CMD_MODE_OFF  0x07
#define CMD_FAN_15    0xA0
#define CMD_FAN_30    0xA1
#define CMD_FAN_45    0xA2
#define CMD_FAN_1H    0xA3
#define CMD_FAN_2H    0xA4
#define CMD_FAN_4H    0xA5
#define CMD_FAN_8H    0xA6
#define CMD_FAN_12H   0xA7
#define CMD_FAN_OFF   0xA8

typedef enum {
    MenuSetTemp = 0,
    MenuMode,
    MenuFan,
    MenuCount,
} MenuItem;

static const char* menu_labels[] = {
    "Set Temp",
    "Mode",
    "Fan Timer",
};

typedef enum {
    ModeHeat = 0,
    ModeCool,
    ModeHeatCool,
    ModeOff,
    ModeCount,
} ModeItem;

static const char* mode_labels[] = {
    "Heat",
    "Cool",
    "Heat-Cool",
    "Off",
};

static const uint8_t mode_cmds[] = {
    CMD_MODE_HEAT,
    CMD_MODE_COOL,
    CMD_MODE_HC,
    CMD_MODE_OFF,
};

typedef enum {
    Fan15min = 0,
    Fan30min,
    Fan45min,
    Fan1hr,
    Fan2hr,
    Fan4hr,
    Fan8hr,
    Fan12hr,
    FanOff,
    FanCount,
} FanItem;

static const char* fan_labels[] = {
    "15 minutes",
    "30 minutes",
    "45 minutes",
    "1 hour",
    "2 hours",
    "4 hours",
    "8 hours",
    "12 hours",
    "Off",
};

static const uint8_t fan_cmds[] = {
    CMD_FAN_15, CMD_FAN_30, CMD_FAN_45,
    CMD_FAN_1H, CMD_FAN_2H, CMD_FAN_4H,
    CMD_FAN_8H, CMD_FAN_12H, CMD_FAN_OFF,
};

static const uint8_t icon_thermo[] = {
    0b00011000, 0b00100100, 0b00100100, 0b00101100,
    0b00101100, 0b01001110, 0b01001110, 0b00111100,
};
static const uint8_t icon_flame[] = {
    0b00001000, 0b00011100, 0b00111010, 0b01110001,
    0b01100011, 0b01100110, 0b00111110, 0b00011100,
};
static const uint8_t icon_snow[] = {
    0b00011000, 0b01011010, 0b00111100, 0b11011011,
    0b11011011, 0b00111100, 0b01011010, 0b00011000,
};
static const uint8_t icon_fan[] = {
    0b01100110, 0b11110011, 0b11111111, 0b01111110,
    0b01111110, 0b11111111, 0b11110011, 0b01100110,
};
static const uint8_t icon_power[] = {
    0b00011000, 0b01111110, 0b11100111, 0b11000011,
    0b11000011, 0b11100111, 0b01111110, 0b00011000,
};
static const uint8_t icon_heatcool[] = {
    0b00001000, 0b00011100, 0b00111010, 0b01100001,
    0b10000011, 0b01011100, 0b00111000, 0b00010000,
};

static const uint8_t* main_icons[] = {
    icon_thermo,
    icon_flame,
    icon_fan,
};

static const uint8_t* mode_icons[] = {
    icon_flame,
    icon_snow,
    icon_heatcool,
    icon_power,
};

typedef enum {
    StateMenu,
    StateModeMenu,
    StateFanMenu,
    StateSetTemp,
    StateSending,
    StateSent,
} AppState;

typedef struct {
    AppState state;
    MenuItem selected;
    ModeItem mode_selected;
    FanItem fan_selected;
    uint8_t set_temp;
    uint8_t send_dots;
    FuriMutex* mutex;
    FuriMessageQueue* queue;
    ViewPort* view_port;
    Gui* gui;
    NotificationApp* notifications;
} NestApp;

static void draw_xbm(Canvas* canvas, int x, int y, const uint8_t* bmp) {
    for(int r = 0; r < 8; r++)
        for(int c = 0; c < 8; c++)
            if(bmp[r] & (0x80 >> c))
                canvas_draw_dot(canvas, x + c, y + r);
}

static void draw_header(Canvas* canvas, const char* title) {
    // 13px tall header
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    draw_xbm(canvas, 2, 3, icon_thermo);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 13, 11, title);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, 13, 128, 13);
}

static void draw_list(
    Canvas* canvas,
    const char** labels,
    const uint8_t** icons,
    int count,
    int selected) {
    // Start items below 13px header
    int start = selected - 1;
    if(start < 0) start = 0;
    if(start + 4 > count) start = count - 4;
    if(start < 0) start = 0;

    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < 4 && (start + i) < count; i++) {
        int item = start + i;
        int y = 25 + i * 13;
        if(item == selected) {
            canvas_draw_rbox(canvas, 0, y - 9, 128, 11, 3);
            canvas_set_color(canvas, ColorWhite);
            if(icons) draw_xbm(canvas, 3, y - 8, icons[item]);
            canvas_draw_str(canvas, icons ? 14 : 4, y, labels[item]);
            canvas_draw_str(canvas, 119, y, ">");
            canvas_set_color(canvas, ColorBlack);
        } else {
            if(icons) draw_xbm(canvas, 3, y - 8, icons[item]);
            canvas_draw_str(canvas, icons ? 14 : 4, y, labels[item]);
        }
    }
    if(start > 0)
        canvas_draw_triangle(canvas, 124, 17, 4, 4, CanvasDirectionTopToBottom);
    if(start + 4 < count)
        canvas_draw_triangle(canvas, 124, 62, 4, 4, CanvasDirectionBottomToTop);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    NestApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);

    if(app->state == StateMenu) {
        draw_header(canvas, "NestBridge");
        draw_list(canvas, menu_labels, main_icons, MenuCount, (int)app->selected);

    } else if(app->state == StateModeMenu) {
        draw_header(canvas, "Mode");
        draw_list(canvas, mode_labels, mode_icons, ModeCount, (int)app->mode_selected);

    } else if(app->state == StateFanMenu) {
        draw_header(canvas, "Fan Timer");
        draw_list(canvas, fan_labels, NULL, FanCount, (int)app->fan_selected);

    } else if(app->state == StateSetTemp) {
        draw_header(canvas, "Set Temperature");
        canvas_set_font(canvas, FontBigNumbers);
        char tmp[8];
        snprintf(tmp, sizeof(tmp), "%d", app->set_temp);
        canvas_draw_str_aligned(canvas, 55, 42, AlignCenter, AlignCenter, tmp);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 73, 36, "o");
        canvas_draw_str(canvas, 79, 42, "F");
        draw_xbm(canvas, 10, 22, icon_flame);
        draw_xbm(canvas, 10, 50, icon_snow);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 63, "Ok: send   Back: cancel");

    } else if(app->state == StateSending) {
        draw_header(canvas, "NestBridge");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Sending");
        char dots[5] = "    ";
        for(uint8_t d = 0; d < (app->send_dots % 4); d++) dots[d] = '.';
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, dots);

    } else if(app->state == StateSent) {
        draw_header(canvas, "NestBridge");
        canvas_draw_rbox(canvas, 24, 20, 80, 26, 5);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 33, AlignCenter, AlignCenter, "Done!");
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "Ok/Back: return");
    }

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    furi_message_queue_put(((NestApp*)ctx)->queue, event, 0);
}

static void set_adv(uint8_t cmd, uint8_t temp) {
    uint8_t adv[] = {0x07, 0xFF, 0xE5, 0x02, 'N', 'B', cmd, temp};
    GapExtraBeaconConfig cfg = {
        .min_adv_interval_ms = 100,
        .max_adv_interval_ms = 100,
        .adv_channel_map = GapAdvChannelMapAll,
        .adv_power_level = GapAdvPowerLevel_0dBm,
    };
    furi_hal_bt_extra_beacon_set_config(&cfg);
    furi_hal_bt_extra_beacon_set_data(adv, sizeof(adv));
    furi_hal_bt_extra_beacon_start();
}

static void send_cmd(NestApp* app, uint8_t cmd, uint8_t temp) {
    app->state = StateSending;
    app->send_dots = 0;
    view_port_update(app->view_port);
    set_adv(cmd, temp);
    for(int i = 0; i < 12; i++) {
        app->send_dots = i;
        view_port_update(app->view_port);
        furi_delay_ms(250);
    }
    set_adv(CMD_IDLE, 0);
    notification_message(app->notifications, &sequence_single_vibro);
    app->state = StateSent;
    view_port_update(app->view_port);
}

static NestApp* app_alloc() {
    NestApp* app = malloc(sizeof(NestApp));
    app->state = StateMenu;
    app->selected = MenuSetTemp;
    app->mode_selected = ModeHeat;
    app->fan_selected = Fan15min;
    app->set_temp = 72;
    app->send_dots = 0;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    return app;
}

static void app_free(NestApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t nest_bridge_app(void* p) {
    UNUSED(p);
    NestApp* app = app_alloc();
    notification_message(app->notifications, &sequence_display_backlight_on);
    set_adv(CMD_IDLE, 0);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(app->queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);

                if(app->state == StateMenu) {
                    switch(event.key) {
                    case InputKeyUp:
                        if(app->selected > 0) app->selected--;
                        break;
                    case InputKeyDown:
                        if(app->selected < MenuCount - 1) app->selected++;
                        break;
                    case InputKeyOk:
                        if(app->selected == MenuSetTemp)
                            app->state = StateSetTemp;
                        else if(app->selected == MenuMode)
                            app->state = StateModeMenu;
                        else if(app->selected == MenuFan)
                            app->state = StateFanMenu;
                        break;
                    case InputKeyBack:
                        running = false;
                        break;
                    default: break;
                    }

                } else if(app->state == StateModeMenu) {
                    switch(event.key) {
                    case InputKeyUp:
                        if(app->mode_selected > 0) app->mode_selected--;
                        break;
                    case InputKeyDown:
                        if(app->mode_selected < ModeCount - 1) app->mode_selected++;
                        break;
                    case InputKeyOk: {
                        uint8_t cmd = mode_cmds[app->mode_selected];
                        furi_mutex_release(app->mutex);
                        send_cmd(app, cmd, 0);
                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        break;
                    }
                    case InputKeyBack:
                        app->state = StateMenu;
                        break;
                    default: break;
                    }

                } else if(app->state == StateFanMenu) {
                    switch(event.key) {
                    case InputKeyUp:
                        if(app->fan_selected > 0) app->fan_selected--;
                        break;
                    case InputKeyDown:
                        if(app->fan_selected < FanCount - 1) app->fan_selected++;
                        break;
                    case InputKeyOk: {
                        uint8_t cmd = fan_cmds[app->fan_selected];
                        furi_mutex_release(app->mutex);
                        send_cmd(app, cmd, 0);
                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        break;
                    }
                    case InputKeyBack:
                        app->state = StateMenu;
                        break;
                    default: break;
                    }

                } else if(app->state == StateSetTemp) {
                    switch(event.key) {
                    case InputKeyUp:
                        if(app->set_temp < 90) app->set_temp++;
                        break;
                    case InputKeyDown:
                        if(app->set_temp > 60) app->set_temp--;
                        break;
                    case InputKeyOk: {
                        uint8_t t = app->set_temp;
                        furi_mutex_release(app->mutex);
                        send_cmd(app, CMD_SET_TEMP, t);
                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        break;
                    }
                    case InputKeyBack:
                        app->state = StateMenu;
                        break;
                    default: break;
                    }

                } else if(app->state == StateSent) {
                    if(event.key == InputKeyBack || event.key == InputKeyOk)
                        app->state = StateMenu;
                }

                furi_mutex_release(app->mutex);
                view_port_update(app->view_port);
            }
        }
    }

    furi_hal_bt_extra_beacon_stop();
    app_free(app);
    return 0;
}
