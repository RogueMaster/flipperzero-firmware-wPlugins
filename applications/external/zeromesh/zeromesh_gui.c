#include "zeromesh_gui.h"
#include <furi.h>
#include <gui/canvas.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>

#include "zeromesh_notify.h"
#include "zeromesh_uart.h"
#include "zeromesh_protocol.h"
#include "zeromesh_history.h"
#include "zeromesh_roster.h"
#include "zeromesh_channel.h"
#include "zeromesh_settings.h"

static const uint32_t baud_options[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
#define BAUD_OPTIONS_COUNT (sizeof(baud_options) / sizeof(baud_options[0]))

static const char* lmh_names[] = {
    "Scroll",
    "Wrap",
};

static void get_short_node_id(uint32_t node_id, char* buf, size_t buf_size) {
    snprintf(buf, buf_size, "!%04lx", (unsigned long)(node_id & 0xFFFF));
}

static uint8_t baud_to_index(uint32_t baud) {
    for(uint8_t i = 0; i < BAUD_OPTIONS_COUNT; i++) {
        if(baud_options[i] == baud) return i;
    }
    return 4;
}

static void draw_footer(Canvas* canvas, const char* left_hint, const char* right_hint) {
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);
    if(left_hint && left_hint[0]) {
        canvas_draw_str(canvas, 2, 64, left_hint);
    }
    if(right_hint && right_hint[0]) {
        int w = canvas_string_width(canvas, right_hint);
        canvas_draw_str(canvas, 126 - w, 64, right_hint);
    }
}

static void draw_str_ellipsis(Canvas* canvas, int x, int y, int max_w, const char* s) {
    if(!s) s = "";
    if(max_w <= 0) return;

    if(canvas_string_width(canvas, s) <= max_w) {
        canvas_draw_str(canvas, x, y, s);
        return;
    }

    char buf[64];
    size_t n = strlen(s);
    if(n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';

    while(n > 0) {
        size_t keep = n;
        if(keep > sizeof(buf) - 4) keep = sizeof(buf) - 4;

        char tmp[64];
        memcpy(tmp, buf, keep);
        tmp[keep] = '.';
        tmp[keep + 1] = '.';
        tmp[keep + 2] = '.';
        tmp[keep + 3] = '\0';

        if(canvas_string_width(canvas, tmp) <= max_w) {
            canvas_draw_str(canvas, x, y, tmp);
            return;
        }
        n--;
    }

    canvas_draw_str(canvas, x, y, "...");
}

static int calculate_wrapped_lines(Canvas* canvas, const char* text, int max_w) {
    if(!text || !text[0]) return 1;
    
    char line_buf[64];
    size_t text_len = strlen(text);
    size_t pos = 0;
    int lines = 0;
    
    while(pos < text_len) {
        size_t line_len = 0;
        size_t last_space = 0;
        
        while(pos + line_len < text_len) {
            line_buf[line_len] = text[pos + line_len];
            line_buf[line_len + 1] = '\0';
            
            if(text[pos + line_len] == ' ') {
                last_space = line_len;
            }
            
            if(canvas_string_width(canvas, line_buf) > max_w) {
                if(last_space > 0) {
                    line_len = last_space;
                } else {
                    if(line_len > 0) line_len--;
                }
                break;
            }
            
            line_len++;
            if(line_len >= sizeof(line_buf) - 1) break;
        }
        
        if(line_len == 0 && pos < text_len) {
            line_len = 1;
        }
        
        pos += line_len;
        while(pos < text_len && text[pos] == ' ') pos++;
        lines++;
    }
    
    return lines > 0 ? lines : 1;
}

static void draw_wrapped_text_in_bubble(
    Canvas* canvas,
    int x,
    int y,
    int max_w,
    const char* text,
    Color text_col) {
    if(!text || !text[0]) return;
    
    canvas_set_color(canvas, text_col);
    
    char line_buf[64];
    size_t text_len = strlen(text);
    size_t pos = 0;
    int current_y = y;
    
    while(pos < text_len) {
        size_t line_len = 0;
        size_t last_space = 0;
        
        while(pos + line_len < text_len) {
            line_buf[line_len] = text[pos + line_len];
            line_buf[line_len + 1] = '\0';
            
            if(text[pos + line_len] == ' ') {
                last_space = line_len;
            }
            
            if(canvas_string_width(canvas, line_buf) > max_w) {
                if(last_space > 0) {
                    line_len = last_space;
                    line_buf[line_len] = '\0';
                } else {
                    if(line_len > 0) line_len--;
                    line_buf[line_len] = '\0';
                }
                break;
            }
            
            line_len++;
            if(line_len >= sizeof(line_buf) - 1) break;
        }
        
        if(line_len == 0 && pos < text_len) {
            line_buf[0] = text[pos];
            line_buf[1] = '\0';
            line_len = 1;
        }
        
        canvas_draw_str(canvas, x, current_y, line_buf);
        pos += line_len;
        while(pos < text_len && text[pos] == ' ') pos++;
        current_y += 9;
    }
}

void draw_header(Canvas* canvas, ZeroMeshApp* app, const char* title) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 14);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);

    int dot_x = 76;
    int title_x = 4;
    int title_max = (dot_x - 6) - title_x;
    draw_str_ellipsis(canvas, title_x, 11, title_max, title);

    if(app->serial && app->rx_bytes > 0) {
        canvas_draw_disc(canvas, 120, 7, 3);
    } else {
        canvas_draw_circle(canvas, 120, 7, 3);
    }

    for(int i = 0; i < PAGE_COUNT; i++) {
        if(i == (int)app->ui_mode) {
            canvas_draw_disc(canvas, dot_x + i * 7, 7, 2);
        } else {
            canvas_draw_circle(canvas, dot_x + i * 7, 7, 1);
        }
    }

    canvas_set_color(canvas, ColorBlack);
}

static void draw_message_bubble(Canvas* canvas, int x, int y, int max_w, const char* text, bool is_tx, uint32_t from_id, uint32_t phase_seed, ZeroMeshApp* app) {
    canvas_set_font(canvas, FontSecondary);

    char sender[10];
    get_short_node_id(from_id, sender, sizeof(sender));
    
    int sender_w = canvas_string_width(canvas, sender);
    int name_y = y;
    int bubble_y = y + 10;
    
    const char* s = text ? text : "";
    uint16_t text_w = canvas_string_width(canvas, s);
    
    int pad = 4;
    int inner_w = max_w - (pad * 2);
    
    int bubble_h = 12;
    int bubble_w = text_w + (pad * 2);
    
    if(app->lmh_mode == LMH_Wrap && text_w > inner_w) {
        int lines = calculate_wrapped_lines(canvas, s, inner_w);
        bubble_h = 2 + (lines * 9) + 2;
        bubble_w = max_w;
    } else if(bubble_w > max_w) {
        bubble_w = max_w;
    }
    
    if(bubble_w < 20) bubble_w = 20;
    
    int bx = is_tx ? (x + max_w - bubble_w) : x;
    int name_x = is_tx ? (bx + bubble_w - sender_w) : bx;
    
    Color bubble_bg = is_tx ? ColorBlack : ColorWhite;
    Color text_col = is_tx ? ColorWhite : ColorBlack;
    Color name_col = is_tx ? ColorWhite : ColorBlack;
    
    canvas_set_color(canvas, name_col);
    canvas_draw_str(canvas, name_x, name_y + 7, sender);
    
    if(is_tx) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, bx, bubble_y, bubble_w, bubble_h, 3);
    } else {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_rbox(canvas, bx, bubble_y, bubble_w, bubble_h, 3);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, bx, bubble_y, bubble_w, bubble_h, 3);
    }
    
    int inner_x = bx + pad;
    int baseline = bubble_y + 10;
    
    if(text_w <= (uint16_t)inner_w) {
        canvas_set_color(canvas, text_col);
        canvas_draw_str(canvas, inner_x, baseline, s);
    } else if(app->lmh_mode == LMH_Wrap) {
        draw_wrapped_text_in_bubble(canvas, inner_x, baseline, inner_w, s, text_col);
    } else {
        uint32_t t = furi_get_tick() + phase_seed;
        uint32_t speed_delay = 24 * (11 - app->scroll_speed);
        uint32_t step = t / speed_delay;
        uint16_t gap = 14;
        uint16_t cycle = text_w + gap;
        uint16_t off = (uint16_t)(step % cycle);
        
        int32_t x1 = (int32_t)inner_x - (int32_t)off;
        int32_t x2 = x1 + (int32_t)cycle;
        
        canvas_set_color(canvas, text_col);
        canvas_draw_str(canvas, (int)x1, baseline, s);
        canvas_draw_str(canvas, (int)x2, baseline, s);
        
        canvas_set_color(canvas, bubble_bg);
        if(pad > 0) {
            canvas_draw_box(canvas, bx, bubble_y, pad, bubble_h);
            canvas_draw_box(canvas, bx + bubble_w - pad, bubble_y, pad, bubble_h);
        }
        
        canvas_set_color(canvas, ColorWhite);
        if(bx > 0) {
            canvas_draw_box(canvas, 0, bubble_y, bx, bubble_h);
        }
        int rx = bx + bubble_w;
        if(rx < 128) {
            canvas_draw_box(canvas, rx, bubble_y, 128 - rx, bubble_h);
        }
    }
    
    canvas_set_color(canvas, ColorBlack);
}

static void render_messages(Canvas* canvas, ZeroMeshApp* app) {
    char title[32];
    if(app->num_channels > 1) {
        snprintf(title, sizeof(title), "Msg - Ch%d", app->current_channel);
    } else {
        snprintf(title, sizeof(title), "Messages");
    }
    draw_header(canvas, app, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);

    uint8_t broadcast_indices[MSG_HISTORY];
    uint8_t broadcast_count = 0;

    for(uint8_t i = 0; i < app->history.count; i++) {
        uint8_t idx = (app->history.head + MSG_HISTORY - app->history.count + i) % MSG_HISTORY;
        if(app->history.msgs[idx].to == 0xFFFFFFFF) {
            broadcast_indices[broadcast_count++] = idx;
        }
    }

    if(broadcast_count == 0) {
        canvas_draw_str(canvas, 16, 34, "No mesh traffic yet");
        canvas_draw_str(canvas, 10, 46, "Press OK to broadcast");
        draw_footer(canvas, "", "");
        return;
    }

    int available_height = 46;
    int y = 18;
    int visible_count = 0;
    
    for(int i = broadcast_count - 1; i >= 0; i--) {
        uint8_t idx = broadcast_indices[i];
        Message* msg = &app->history.msgs[idx];
        
        int msg_height = 16;
        if(app->lmh_mode == LMH_Wrap) {
            int text_w = canvas_string_width(canvas, msg->text);
            int inner_w = 116;
            if(text_w > inner_w) {
                int lines = calculate_wrapped_lines(canvas, msg->text, inner_w);
                msg_height = 8 + (lines * 9) + 2;
            }
        }
        
        if(y + msg_height <= available_height + 18) {
            visible_count++;
            y += msg_height;
        } else {
            break;
        }
    }
    
    if(visible_count == 0) visible_count = 1;
    
    if(broadcast_count <= visible_count) {
        app->msg_scroll_offset = 0;
    } else if(app->msg_scroll_offset > broadcast_count - visible_count) {
        app->msg_scroll_offset = broadcast_count - visible_count;
    }

    uint8_t start_idx = broadcast_count - visible_count - app->msg_scroll_offset;
    if(start_idx > broadcast_count) start_idx = 0;

    y = 18;
    for(int i = 0; i < visible_count && (start_idx + i) < broadcast_count; i++) {
        uint8_t history_idx = broadcast_indices[start_idx + i];
        Message* msg = &app->history.msgs[history_idx];
        
        draw_message_bubble(canvas, 2, y, 124, msg->text, msg->is_tx, msg->from, (uint32_t)history_idx * 977u, app);
        
        if(app->lmh_mode == LMH_Wrap) {
            int text_w = canvas_string_width(canvas, msg->text);
            int inner_w = 116;
            if(text_w > inner_w) {
                int lines = calculate_wrapped_lines(canvas, msg->text, inner_w);
                y += 8 + (lines * 9) + 2;
            } else {
                y += 16;
            }
        } else {
            y += 16;
        }
    }

    if(app->msg_scroll_offset > 0) {
        canvas_draw_str(canvas, 60, 62, "v");
    }
    if(broadcast_count > visible_count && app->msg_scroll_offset < broadcast_count - visible_count) {
        canvas_draw_str(canvas, 60, 17, "^");
    }

    draw_footer(canvas, "", "");
}

static void render_stats(Canvas* canvas, ZeroMeshApp* app) {
    draw_header(canvas, app, "Statistics");
    canvas_set_font(canvas, FontSecondary);

    char buf[64];
    snprintf(
        buf,
        sizeof(buf),
        "Port: %s @ %lu",
        (app->uart_id == FuriHalSerialIdUsart) ? "USART" : "LPUART",
        (unsigned long)app->baud);
    canvas_draw_str(canvas, 2, 24, buf);

    snprintf(buf, sizeof(buf), "RX: %lu bytes", (unsigned long)app->rx_bytes);
    canvas_draw_str(canvas, 2, 34, buf);

    snprintf(
        buf,
        sizeof(buf),
        "Frames: %lu OK / %lu bad",
        (unsigned long)app->rx_frames_ok,
        (unsigned long)(app->rx_bad_magic + app->rx_bad_len + app->rx_decode_fail));
    canvas_draw_str(canvas, 2, 44, buf);

    snprintf(buf, sizeof(buf), "TX: %lu frames", (unsigned long)app->tx_frames);
    canvas_draw_str(canvas, 2, 54, buf);

    draw_footer(canvas, "", "");
}

static void render_signal(Canvas* canvas, ZeroMeshApp* app) {
    draw_header(canvas, app, "Signal Info");
    canvas_set_font(canvas, FontSecondary);

    char buf[64];

    if(app->my_node_num != 0) {
        snprintf(buf, sizeof(buf), "My Node: %08lX", (unsigned long)app->my_node_num);
        canvas_draw_str(canvas, 2, 24, buf);
    }

    if(app->last_rx_from != 0) {
        snprintf(buf, sizeof(buf), "Last From: %08lX", (unsigned long)app->last_rx_from);
        canvas_draw_str(canvas, 2, 34, buf);

        if(app->has_rx_signal_data) {
            snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)app->last_rx_rssi);
            canvas_draw_str(canvas, 2, 44, buf);

            int snr_tenths = ((int)app->last_rx_snr * 10) / 4;
            snprintf(buf, sizeof(buf), "SNR: %d.%d dB", snr_tenths / 10, snr_tenths % 10);
            canvas_draw_str(canvas, 2, 54, buf);
        }
    } else {
        canvas_draw_str(canvas, 2, 34, "No messages yet");
    }

    draw_footer(canvas, "", "");
}

static void render_logs(Canvas* canvas, ZeroMeshApp* app) {
    draw_header(canvas, app, "Debug Logs");
    canvas_set_font(canvas, FontSecondary);

    if(app->rx_bytes == 0) {
        canvas_draw_str(canvas, 2, 26, "No data received!");
        canvas_draw_str(canvas, 2, 36, "Check:");
        canvas_draw_str(canvas, 2, 46, "- Serial mode PROTO");
        canvas_draw_str(canvas, 2, 56, "- Serial echo true");
        draw_footer(canvas, "", "");
        return;
    }

    if(app->log_paused) {
        canvas_draw_str(canvas, 96, 24, "PAUSE");
    }

    int visible_lines = 5;
    uint8_t start_idx = app->log_paused
                            ? (app->line_head + LOG_LINES - app->log_scroll_offset - visible_lines) % LOG_LINES
                            : (app->line_head + LOG_LINES - visible_lines) % LOG_LINES;

    int y = 24;
    for(int i = 0; i < visible_lines; i++) {
        uint8_t idx = (start_idx + i) % LOG_LINES;
        const char* line = app->lines[idx];
        if(line[0] != '\0') {
            canvas_draw_str(canvas, 2, y, line);
        }
        y += 8;
    }

    draw_footer(canvas, "OK: Pause", "Hold OK: Info");
}

static void render_settings(Canvas* canvas, ZeroMeshApp* app) {
    draw_header(canvas, app, "Settings");
    canvas_set_font(canvas, FontSecondary);
    canvas_set_color(canvas, ColorBlack);

    const int start_y = 24;
    const int row_h = 12;
    const int visible_items = 4;
    const int label_x = 4;
    const int value_x = 92;

    uint8_t start_idx = 0;
    if(app->settings_cursor >= visible_items) {
        start_idx = app->settings_cursor - visible_items + 1;
    }

    for(uint8_t i = start_idx; i < start_idx + visible_items && i < SETTING_COUNT; i++) {
        int display_idx = i - start_idx;
        int y = start_y + display_idx * row_h;

        if(i == app->settings_cursor) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 8, 128, row_h);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        const char* label = "";
        char val_buf[20];

        switch(i) {
        case SettingUart:
            label = "UART Port";
            snprintf(
                val_buf,
                sizeof(val_buf),
                "%s",
                (app->uart_id == FuriHalSerialIdUsart) ? "USART" : "LPUART");
            break;
        case SettingBaud:
            label = "Baud Rate";
            snprintf(val_buf, sizeof(val_buf), "%lu", (unsigned long)app->baud);
            break;
        case SettingVibro:
            label = "Vibration";
            snprintf(val_buf, sizeof(val_buf), "%s", app->notify_vibro ? "ON" : "OFF");
            break;
        case SettingLed:
            label = "LED Flash";
            snprintf(val_buf, sizeof(val_buf), "%s", app->notify_led ? "ON" : "OFF");
            break;
        case SettingRingtone:
            label = "Ringtone";
            snprintf(val_buf, sizeof(val_buf), "%s", ringtone_names[app->notify_ringtone]);
            break;
        case SettingScrollSpeed:
            label = "Scroll Speed";
            snprintf(val_buf, sizeof(val_buf), "%u", app->scroll_speed);
            break;
        case SettingScrollFramerate:
            label = "Scroll FPS";
            snprintf(val_buf, sizeof(val_buf), "%u", app->scroll_framerate);
            break;
        case SettingLMH:
            label = "Long Msg";
            snprintf(val_buf, sizeof(val_buf), "%s", lmh_names[app->lmh_mode]);
            break;
        default:
            val_buf[0] = '\0';
            break;
        }

        canvas_draw_str(canvas, label_x, y, label);

        if(i == app->settings_cursor && app->settings_editing) {
            char arrow_buf[28];
            snprintf(arrow_buf, sizeof(arrow_buf), "< %s >", val_buf);
            canvas_draw_str(canvas, value_x - 10, y, arrow_buf);
        } else {
            canvas_draw_str(canvas, value_x, y, val_buf);
        }

        canvas_set_color(canvas, ColorBlack);
    }
}

void render_cb(Canvas* canvas, void* ctx) {
    ZeroMeshApp* app = (ZeroMeshApp*)ctx;
    if(!app) return;

    canvas_clear(canvas);

    furi_mutex_acquire(app->lock, FuriWaitForever);

    switch(app->ui_mode) {
    case PAGE_MESSAGES:
        render_messages(canvas, app);
        break;
    case PAGE_ROSTER:
        render_roster(canvas, app);
        break;
    case PAGE_STATS:
        render_stats(canvas, app);
        break;
    case PAGE_SIGNAL:
        render_signal(canvas, app);
        break;
    case PAGE_LOGS:
        render_logs(canvas, app);
        break;
    case PAGE_SETTINGS:
        render_settings(canvas, app);
        break;
    default:
        app->ui_mode = PAGE_MESSAGES;
        render_messages(canvas, app);
        break;
    }

    furi_mutex_release(app->lock);
}

static void setting_change(ZeroMeshApp* app, int direction) {
    switch(app->settings_cursor) {
    case SettingUart: {
        FuriHalSerialId nid =
            (app->uart_id == FuriHalSerialIdUsart) ? FuriHalSerialIdLpuart : FuriHalSerialIdUsart;
        uart_reopen(app, nid, app->baud);
        break;
    }
    case SettingBaud: {
        uint8_t idx = baud_to_index(app->baud);
        if(direction > 0 && idx < BAUD_OPTIONS_COUNT - 1)
            idx++;
        else if(direction < 0 && idx > 0)
            idx--;
        uart_reopen(app, app->uart_id, baud_options[idx]);
        break;
    }
    case SettingVibro:
        app->notify_vibro = !app->notify_vibro;
        break;
    case SettingLed:
        app->notify_led = !app->notify_led;
        break;
    case SettingRingtone: {
        int r = (int)app->notify_ringtone + direction;
        if(r < 0) r = RINGTONE_COUNT - 1;
        if(r >= RINGTONE_COUNT) r = 0;
        app->notify_ringtone = (RingtoneType)r;
        if(app->notify_ringtone != RingtoneNone) {
            play_ringtone(app);
        }
        break;
    }
    case SettingScrollSpeed: {
        int new_speed = (int)app->scroll_speed + direction;
        if(new_speed < 1) new_speed = 10;
        if(new_speed > 10) new_speed = 1;
        app->scroll_speed = (uint8_t)new_speed;
        break;
    }
    case SettingScrollFramerate: {
        int new_fps = (int)app->scroll_framerate + direction;
        if(new_fps < 1) new_fps = 10;
        if(new_fps > 10) new_fps = 1;
        app->scroll_framerate = (uint8_t)new_fps;
        break;
    }
    case SettingLMH: {
        app->lmh_mode = (app->lmh_mode == LMH_Scroll) ? LMH_Wrap : LMH_Scroll;
        break;
    }
    default:
        break;
    }
    
    settings_save(app);
}

void text_input_callback(void* ctx) {
    ZeroMeshApp* app = ctx;
    if(strlen(app->text_buffer) > 0) {
        if(app->ui_mode == PAGE_ROSTER && app->roster.state == RosterStateChat) {
            uint32_t to_node = app->roster.nodes[app->roster.selected_idx].node_id;
            send_text_message(app, app->text_buffer, to_node);
        } else {
            send_text_message(app, app->text_buffer, 0xFFFFFFFF);
        }
        app->text_buffer[0] = '\0';
    }
    view_dispatcher_stop(app->kb_dispatcher);
}

void input_cb(InputEvent* e, void* ctx) {
    ZeroMeshApp* app = (ZeroMeshApp*)ctx;
    if(!app || !e) return;
    if(e->type != InputTypeShort && e->type != InputTypeLong && e->type != InputTypeRepeat) return;

    if(app->ui_mode == PAGE_ROSTER) {
        bool deep_in_roster = (app->roster.state != RosterStateList);
        input_roster(e, app);
        if(deep_in_roster) return;
        if(e->key == InputKeyUp || e->key == InputKeyDown || e->key == InputKeyOk) return;
    }

    switch(e->key) {
    case InputKeyLeft:
        if(app->ui_mode == PAGE_SETTINGS && app->settings_editing) {
            if(e->type == InputTypeShort || e->type == InputTypeRepeat) {
                setting_change(app, -1);
                view_port_update(app->vp);
            }
            break;
        }
        if(e->type != InputTypeShort) break;
        if(app->ui_mode == 0)
            app->ui_mode = PAGE_COUNT - 1;
        else
            app->ui_mode--;
        view_port_update(app->vp);
        break;

    case InputKeyRight:
        if(app->ui_mode == PAGE_SETTINGS && app->settings_editing) {
            if(e->type == InputTypeShort || e->type == InputTypeRepeat) {
                setting_change(app, 1);
                view_port_update(app->vp);
            }
            break;
        }
        if(e->type != InputTypeShort) break;
        app->ui_mode = (app->ui_mode + 1) % PAGE_COUNT;
        view_port_update(app->vp);
        break;

    case InputKeyUp:
        if(e->type != InputTypeShort && e->type != InputTypeRepeat) break;
        if(app->ui_mode == PAGE_MESSAGES) {
            if(app->msg_scroll_offset < app->history.count) {
                app->msg_scroll_offset++;
            }
            view_port_update(app->vp);
        } else if(app->ui_mode == PAGE_SIGNAL) {
            // Allow up/down scrolling in signal view if needed
            view_port_update(app->vp);
        } else if(app->ui_mode == PAGE_LOGS) {
            if(app->log_paused && app->log_scroll_offset < LOG_LINES - 5) {
                app->log_scroll_offset++;
            }
            view_port_update(app->vp);
        } else if(app->ui_mode == PAGE_SETTINGS) {
            if(!app->settings_editing) {
                if(app->settings_cursor > 0)
                    app->settings_cursor--;
                else
                    app->settings_cursor = SETTING_COUNT - 1;
                view_port_update(app->vp);
            }
        }
        break;

    case InputKeyDown:
        if(e->type != InputTypeShort && e->type != InputTypeRepeat) break;
        if(app->ui_mode == PAGE_MESSAGES) {
            if(app->msg_scroll_offset > 0) {
                app->msg_scroll_offset--;
            }
            view_port_update(app->vp);
        } else if(app->ui_mode == PAGE_SIGNAL) {
            // Allow up/down scrolling in signal view if needed
            view_port_update(app->vp);
        } else if(app->ui_mode == PAGE_LOGS) {
            if(app->log_paused && app->log_scroll_offset > 0) {
                app->log_scroll_offset--;
            }
            view_port_update(app->vp);
        } else if(app->ui_mode == PAGE_SETTINGS) {
            if(!app->settings_editing) {
                app->settings_cursor = (app->settings_cursor + 1) % SETTING_COUNT;
                view_port_update(app->vp);
            }
        }
        break;

    case InputKeyOk:
        if(e->type == InputTypeShort) {
            if(app->ui_mode == PAGE_SETTINGS) {
                app->settings_editing = !app->settings_editing;
                view_port_update(app->vp);
            } else if(app->ui_mode == PAGE_LOGS) {
                app->log_paused = !app->log_paused;
                if(!app->log_paused) app->log_scroll_offset = 0;
                view_port_update(app->vp);
            } else if(app->ui_mode == PAGE_MESSAGES) {
                app->show_keyboard = true;
            }
        } else if(e->type == InputTypeLong) {
            if(app->ui_mode == PAGE_MESSAGES && app->num_channels > 1) {
                channel_next(app);
                view_port_update(app->vp);
            } else {
                request_info(app);
                set_status(app, "Info requested");
            }
        }
        break;

    case InputKeyBack:
        if(e->type != InputTypeShort) break;
        if(app->ui_mode == PAGE_SETTINGS && app->settings_editing) {
            app->settings_editing = false;
            view_port_update(app->vp);
        } else {
            app->stop_thread = true;
        }
        break;

    default:
        break;
    }
}

uint32_t kb_back_callback(void* ctx) {
    (void)ctx;
    return VIEW_NONE;
}