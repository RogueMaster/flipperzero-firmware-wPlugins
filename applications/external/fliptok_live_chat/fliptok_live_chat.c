#include "fliptok_live_chat.h"
#include "helpers/ble_serial.h"

static const uint8_t tiktok_logo_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0xfc, 0x3f, 0x00, 0x00, 0xfe, 0x7f, 0x00,
    0x00, 0x0e, 0x70, 0x00, 0x00, 0x0e, 0x70, 0x00, 0x00, 0x0e, 0x70, 0x00, 0xfc, 0xff, 0xff, 0x3f,
    0xfc, 0xff, 0xff, 0x3f, 0xfc, 0xff, 0xff, 0x3f, 0x00, 0x0e, 0x70, 0x00, 0x00, 0x0e, 0x70, 0x00,
    0x00, 0x0e, 0x70, 0x00, 0x00, 0xfe, 0x7f, 0x00, 0x00, 0xfc, 0x3f, 0x00, 0x00, 0xf8, 0x1f, 0x00,
    0x00, 0xf8, 0x1f, 0x00, 0x00, 0xfc, 0x3f, 0x00, 0x00, 0xfe, 0x7f, 0x00, 0x00, 0x0e, 0x70, 0x00,
    0x00, 0x0e, 0x70, 0x00, 0x00, 0x0e, 0x70, 0x00, 0xfc, 0xff, 0xff, 0x3f, 0xfc, 0xff, 0xff, 0x3f,
    0xfc, 0xff, 0xff, 0x3f, 0x00, 0x0e, 0x70, 0x00, 0x00, 0x0e, 0x70, 0x00, 0x00, 0x0e, 0x70, 0x00,
    0x00, 0xfe, 0x7f, 0x00, 0x00, 0xfc, 0x3f, 0x00, 0x00, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const NotificationSequence sequence_new_message = {
    &message_green_255,
    &message_note_a4,
    &message_delay_50,
    &message_sound_off,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_new_gift = {
    &message_red_255,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_new_follow = {
    &message_blue_255,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_new_like = {
    &message_blue_255,
    &message_do_not_reset,
    NULL,
};

static void msg_buffer_push(MessageBuffer* buf, const TikTokMessage* msg) {
    buf->items[buf->head] = *msg;
    buf->head = (buf->head + 1) % MAX_MESSAGES;
    if(buf->count < MAX_MESSAGES) {
        buf->count++;
    }
    buf->scroll = 0;
}

static const TikTokMessage* msg_buffer_get(const MessageBuffer* buf, uint8_t from_newest) {
    if(from_newest >= buf->count) return NULL;
    int8_t idx = (int8_t)buf->head - 1 - (int8_t)from_newest;
    while(idx < 0)
        idx += MAX_MESSAGES;
    return &buf->items[idx % MAX_MESSAGES];
}

static const char* msg_type_prefix(uint8_t type) {
    switch(type) {
    case TikTokMsgTypeChat:
        return "";
    case TikTokMsgTypeLike:
        return "[L] ";
    case TikTokMsgTypeGift:
        return "[G] ";
    case TikTokMsgTypeFollow:
        return "[F] ";
    default:
        return "";
    }
}

static void draw_splash_screen(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_bitmap_mode(canvas, true);

    canvas_draw_xbm(canvas, 8, 16, 32, 32, tiktok_logo_bits);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 50, 24, "FlipTok");
    canvas_draw_str(canvas, 50, 36, "Live Chat");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 52, 52, "Dr.Mosfet");
}

static void draw_waiting_screen(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "FlipTok Live");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Waiting for");
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "connection...");
}

static void draw_lost_screen(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Connection lost");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Reconnecting...");
}

#define SCREEN_W       128
#define SCREEN_H       64
#define HEADER_H       10
#define CHAT_TOP       11
#define FONT_KB_W      6
#define FONT_KB_H      8
#define CHAT_LEFT      2
#define SCROLLBAR_W    3
#define CHAT_USABLE    (SCREEN_W - CHAT_LEFT - SCROLLBAR_W)
#define CHARS_LINE     (CHAT_USABLE / FONT_KB_W)
#define MAX_DRAW_LINES ((SCREEN_H - CHAT_TOP) / FONT_KB_H)

static uint8_t wrap_count(const char* src, uint8_t cpl) {
    size_t len = strlen(src);
    if(len == 0) return 1;
    uint8_t lines = 0;
    size_t pos = 0;
    while(pos < len) {
        lines++;
        if(pos + cpl >= len) break;
        int sp = -1;
        for(int i = (int)cpl; i >= 0; i--) {
            if(src[pos + i] == ' ') {
                sp = i;
                break;
            }
        }
        if(sp <= 0) sp = (int)cpl;
        pos += (size_t)sp;
        if(pos < len && src[pos] == ' ') pos++;
    }
    return lines ? lines : 1;
}

static void wrap_line(const char* src, uint8_t idx, char* out, uint8_t cpl) {
    size_t len = strlen(src);
    size_t pos = 0;
    uint8_t cur = 0;
    while(pos < len) {
        if(pos + cpl >= len) {
            if(cur == idx) {
                size_t n = len - pos;
                memcpy(out, src + pos, n);
                out[n] = '\0';
            } else {
                out[0] = '\0';
            }
            return;
        }
        int sp = -1;
        for(int i = (int)cpl; i >= 0; i--) {
            if(src[pos + i] == ' ') {
                sp = i;
                break;
            }
        }
        if(sp <= 0) sp = (int)cpl;
        if(cur == idx) {
            memcpy(out, src + pos, (size_t)sp);
            out[sp] = '\0';
            return;
        }
        cur++;
        pos += (size_t)sp;
        if(pos < len && src[pos] == ' ') pos++;
    }
    out[0] = '\0';
}

static void draw_chat_view(Canvas* canvas, TikTokApp* app) {
    canvas_clear(canvas);

    canvas_draw_box(canvas, 0, 0, SCREEN_W, HEADER_H);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "FlipTok Live Chat");
    canvas_set_color(canvas, ColorBlack);

    if(app->msgs.count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "No messages yet...");
        return;
    }

    uint8_t total = app->msgs.count;
    int8_t scroll = app->msgs.scroll;
    if(scroll < 0) scroll = 0;
    if(scroll >= (int8_t)total) scroll = (int8_t)(total - 1);

#define MAX_VIS_MSGS 8
    uint8_t vis_fn[MAX_VIS_MSGS];
    uint8_t vis_lc[MAX_VIS_MSGS];
    uint8_t nvis = 0;
    uint8_t total_lines = 0;

    for(uint8_t i = 0; i < MAX_VIS_MSGS; i++) {
        uint8_t fn = (uint8_t)scroll + i;
        if(fn >= total) break;
        const TikTokMessage* msg = msg_buffer_get(&app->msgs, fn);
        if(!msg) break;
        uint8_t lc = 1 + wrap_count(msg->message, CHARS_LINE);
        vis_fn[nvis] = fn;
        vis_lc[nvis] = lc;
        nvis++;
        total_lines += lc;
        if(total_lines >= MAX_DRAW_LINES) break;
    }

    int cur_slot = (int)MAX_DRAW_LINES - (int)total_lines;

    canvas_set_font(canvas, FontKeyboard);

    for(int mi = (int)nvis - 1; mi >= 0; mi--) {
        const TikTokMessage* msg = msg_buffer_get(&app->msgs, vis_fn[mi]);
        if(!msg) {
            cur_slot += (int)vis_lc[mi];
            continue;
        }

        uint8_t text_lc = wrap_count(msg->message, CHARS_LINE);

        int sl = cur_slot;
        if(sl >= 0 && sl < MAX_DRAW_LINES) {
            char ubuf[USERNAME_LEN + 6];
            snprintf(ubuf, sizeof(ubuf), "%s%s:", msg_type_prefix(msg->type), msg->username);
            uint8_t y = (uint8_t)(CHAT_TOP + sl * FONT_KB_H + (FONT_KB_H - 1));
            canvas_draw_str(canvas, CHAT_LEFT, y, ubuf);
        }

        char tbuf[CHARS_LINE + 1];
        for(uint8_t ti = 0; ti < text_lc; ti++) {
            sl = cur_slot + 1 + (int)ti;
            if(sl >= 0 && sl < MAX_DRAW_LINES) {
                wrap_line(msg->message, ti, tbuf, CHARS_LINE);
                uint8_t y = (uint8_t)(CHAT_TOP + sl * FONT_KB_H + (FONT_KB_H - 1));
                canvas_draw_str(canvas, CHAT_LEFT, y, tbuf);
            }
        }

        cur_slot += (int)vis_lc[mi];

        if(mi > 0 && cur_slot > 0 && cur_slot < MAX_DRAW_LINES) {
            uint8_t sep_y = (uint8_t)(CHAT_TOP + cur_slot * FONT_KB_H - 1);
            canvas_draw_line(canvas, 0, sep_y, SCREEN_W - SCROLLBAR_W - 1, sep_y);
        }
    }

    if(total > 1) {
        uint8_t max_scroll = total - 1;
        uint8_t bar_h = 40;
        uint8_t bar_y = 12;
        uint8_t slider_h = bar_h / (max_scroll + 1);
        if(slider_h < 3) slider_h = 3;
        uint8_t slider_y = bar_y + (uint8_t)(scroll * (bar_h - slider_h) / max_scroll);
        canvas_draw_frame(canvas, 125, bar_y, 3, bar_h);
        canvas_draw_box(canvas, 126, slider_y, 1, slider_h);
    }
}

static void render_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    TikTokApp* app = ctx;

    if(app->show_splash) {
        draw_splash_screen(canvas);
        return;
    }

    switch(app->bt_state) {
    case BtStateWaiting:
        draw_waiting_screen(canvas);
        break;
    case BtStateLost:
        draw_lost_screen(canvas);
        break;
    case BtStateReceiving:
        draw_chat_view(canvas, app);
        break;
    default:
        draw_waiting_screen(canvas);
        break;
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static uint16_t bt_serial_callback(SerialServiceEvent event, void* ctx) {
    furi_assert(ctx);
    TikTokApp* app = ctx;

    if(event.event == SerialServiceEventTypeDataReceived) {
        FURI_LOG_D(
            TAG, "Data received. Size: %u (expected: %u)", event.data.size, sizeof(TikTokMessage));

        if(event.data.size == sizeof(TikTokMessage)) {
            TikTokMessage msg;
            memcpy(&msg, event.data.buffer, sizeof(TikTokMessage));

            msg.username[USERNAME_LEN - 1] = '\0';
            msg.message[MESSAGE_LEN - 1] = '\0';

            for(char* p = msg.username; *p; p++) {
                if((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7E) *p = '?';
            }
            for(char* p = msg.message; *p; p++) {
                if((unsigned char)*p < 0x20 || (unsigned char)*p > 0x7E) *p = '?';
            }

            app->bt_state = BtStateReceiving;
            app->last_packet = furi_hal_rtc_get_timestamp();

            notification_message(app->notification, &sequence_display_backlight_on);

            switch(msg.type) {
            case TikTokMsgTypeGift:
                notification_message(app->notification, &sequence_new_gift);
                break;
            case TikTokMsgTypeLike:
                notification_message(app->notification, &sequence_new_like);
                break;
            case TikTokMsgTypeFollow:
                notification_message(app->notification, &sequence_new_follow);
                msg_buffer_push(&app->msgs, &msg);
                break;
            default:
            case TikTokMsgTypeChat:
                notification_message(app->notification, &sequence_new_message);
                msg_buffer_push(&app->msgs, &msg);
                break;
            }

            FURI_LOG_I(TAG, "Msg [%u] %s: %s", msg.type, msg.username, msg.message);
        }
    }

    return 0;
}

static TikTokApp* fliptok_live_chat_alloc() {
    TikTokApp* app = malloc(sizeof(TikTokApp));

    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->gui = furi_record_open(RECORD_GUI);
    app->bt = furi_record_open(RECORD_BT);

    memset(&app->msgs, 0, sizeof(MessageBuffer));

    app->bt_state = BtStateWaiting;
    app->scene = SceneSplash;
    app->show_splash = true;
    app->last_packet = 0;
    app->ble_serial_profile = NULL;

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);

    return app;
}

static void fliptok_live_chat_free(TikTokApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_BT);
    free(app);
}

int32_t fliptok_live_chat_app(void* p) {
    UNUSED(p);
    TikTokApp* app = fliptok_live_chat_alloc();

    view_port_update(app->view_port);
    furi_delay_ms(2500);
    app->show_splash = false;
    app->scene = SceneChat;

    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_storage_path(app->bt, APP_DATA_PATH(".bt_fliptok.keys"));

    BleProfileSerialParams params = {
        .device_name_prefix = "FlipTok",
        .mac_xor = 0x0007,
    };

    app->ble_serial_profile = bt_profile_start(app->bt, ble_profile_serial, &params);
    furi_check(app->ble_serial_profile);

    ble_profile_serial_set_event_callback(
        app->ble_serial_profile, BT_SERIAL_BUFFER_SIZE, bt_serial_callback, app);

    furi_hal_bt_start_advertising();
    app->bt_state = BtStateWaiting;

    FURI_LOG_I(TAG, "BLE advertising started, device name prefix: FlipTok");

    InputEvent event;
    uint32_t last_render = 0;

    while(true) {
        if(furi_message_queue_get(app->event_queue, &event, 20) == FuriStatusOk) {
            if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                if(event.key == InputKeyBack) {
                    break;
                }

                if(app->bt_state == BtStateReceiving) {
                    if(event.key == InputKeyUp) {
                        if(app->msgs.scroll < (int8_t)(app->msgs.count - 1)) {
                            app->msgs.scroll++;
                        }
                    }
                    if(event.key == InputKeyDown) {
                        if(app->msgs.scroll > 0) {
                            app->msgs.scroll--;
                        }
                    }
                    if(event.key == InputKeyOk) {
                        app->msgs.scroll = 0;
                    }
                }
            }
        }

        if(app->bt_state == BtStateReceiving) {
            if((furi_hal_rtc_get_timestamp() - app->last_packet) > 90) {
                app->bt_state = BtStateLost;
                FURI_LOG_W(TAG, "No data for 90s, connection probably lost");
            }
        }

        uint32_t now = furi_get_tick();
        if(now - last_render >= 100) {
            view_port_update(app->view_port);
            last_render = now;
        }
    }

    ble_profile_serial_set_event_callback(app->ble_serial_profile, 0, NULL, NULL);
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(app->bt);
    furi_check(bt_profile_restore_default(app->bt));

    fliptok_live_chat_free(app);

    FURI_LOG_I(TAG, "FlipTok Live closed");
    return 0;
}
