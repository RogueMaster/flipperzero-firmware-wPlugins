#include "zeromesh_roster.h"
#include "zeromesh_gui.h"

#include <furi.h>
#include <gui/canvas.h>
#include <stdio.h>
#include <string.h>

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
    
    while(pos < text_len && current_y < 64) {
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

void roster_add_node(ZeroMeshApp* app, uint32_t node_id, int8_t snr, int16_t rssi) {
    if(!app || node_id == 0 || node_id == 0xFFFFFFFF) return;

    furi_mutex_acquire(app->lock, FuriWaitForever);

    uint8_t target_idx = 255;
    uint32_t oldest_time = 0xFFFFFFFF;
    uint8_t oldest_idx = 0;

    for(uint8_t i = 0; i < app->roster.count; i++) {
        if(app->roster.nodes[i].node_id == node_id) {
            target_idx = i;
            break;
        }
        if(app->roster.nodes[i].last_seen < oldest_time) {
            oldest_time = app->roster.nodes[i].last_seen;
            oldest_idx = i;
        }
    }

    if(target_idx == 255) {
        if(app->roster.count < ROSTER_MAX_NODES) {
            target_idx = app->roster.count;
            app->roster.count++;
        } else {
            target_idx = oldest_idx;
        }
        app->roster.nodes[target_idx].node_id = node_id;
        app->roster.nodes[target_idx].has_telemetry = false;
        app->roster.nodes[target_idx].has_new_dm = false;
    }

    app->roster.nodes[target_idx].last_seen = furi_get_tick() / 1000;
    app->roster.nodes[target_idx].last_snr = snr;
    app->roster.nodes[target_idx].last_rssi = rssi;

    furi_mutex_release(app->lock);
}

void roster_update_telemetry(ZeroMeshApp* app, uint32_t node_id, uint8_t battery_level, float voltage) {
    if(!app || node_id == 0) return;

    furi_mutex_acquire(app->lock, FuriWaitForever);

    for(uint8_t i = 0; i < app->roster.count; i++) {
        if(app->roster.nodes[i].node_id == node_id) {
            app->roster.nodes[i].battery_level = battery_level;
            app->roster.nodes[i].voltage = voltage;
            app->roster.nodes[i].has_telemetry = true;
            break;
        }
    }

    furi_mutex_release(app->lock);
}

static void draw_roster_bubble(Canvas* canvas, int x, int y, int max_w, const char* text, bool is_tx, uint32_t phase_seed, ZeroMeshApp* app) {
    canvas_set_font(canvas, FontSecondary);

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
    
    Color bubble_bg = is_tx ? ColorBlack : ColorWhite;
    Color text_col = is_tx ? ColorWhite : ColorBlack;
    
    if(is_tx) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, bx, y, bubble_w, bubble_h, 3);
    } else {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_rbox(canvas, bx, y, bubble_w, bubble_h, 3);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, bx, y, bubble_w, bubble_h, 3);
    }
    
    int inner_x = bx + pad;
    int baseline = y + 10;
    
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
            canvas_draw_box(canvas, bx, y, pad, bubble_h);
            canvas_draw_box(canvas, bx + bubble_w - pad, y, pad, bubble_h);
        }
        
        canvas_set_color(canvas, ColorWhite);
        if(bx > 0) {
            canvas_draw_box(canvas, 0, y, bx, bubble_h);
        }
        int rx = bx + bubble_w;
        if(rx < 128) {
            canvas_draw_box(canvas, rx, y, 128 - rx, bubble_h);
        }
    }
    
    canvas_set_color(canvas, ColorBlack);
}

void render_roster(Canvas* canvas, ZeroMeshApp* app) {
    if(app->roster.count == 0) {
        draw_header(canvas, app, "Node Roster");
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 16, 34, "No nodes discovered");
        return;
    }

    NodeEntry* selected = &app->roster.nodes[app->roster.selected_idx];
    char title_buf[32];

    if(app->roster.state == RosterStateList) {
        draw_header(canvas, app, "Node Roster");
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);

        int y = 24;
        for(uint8_t i = 0; i < 4 && i < app->roster.count; i++) {
            uint8_t idx = (app->roster.selected_idx / 4) * 4 + i;
            if(idx >= app->roster.count) break;

            if(idx == app->roster.selected_idx) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_box(canvas, 0, y - 8, 128, 11);
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }

            char line_buf[64];
            uint32_t now = furi_get_tick() / 1000;
            uint32_t diff = now - app->roster.nodes[idx].last_seen;
            const char* alert = app->roster.nodes[idx].has_new_dm ? "(!)" : " ";
            snprintf(
                line_buf,
                sizeof(line_buf),
                "%s %08lX %lus ago",
                alert,
                (unsigned long)app->roster.nodes[idx].node_id,
                (unsigned long)diff);
            canvas_draw_str(canvas, 4, y, line_buf);
            y += 12;
        }

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 2, 64, "Hold OK: Info");
        canvas_draw_str(canvas, 92, 64, "OK: Chat");
        return;
    }

    if(app->roster.state == RosterStateChat) {
        snprintf(title_buf, sizeof(title_buf), "Chat: %08lX", (unsigned long)selected->node_id);
        draw_header(canvas, app, title_buf);
        canvas_set_color(canvas, ColorBlack);

        uint8_t chat_msgs[MSG_HISTORY];
        uint8_t chat_count = 0;

        for(uint8_t i = 0; i < app->history.count; i++) {
            uint8_t idx = (app->history.head + MSG_HISTORY - app->history.count + i) % MSG_HISTORY;
            Message* m = &app->history.msgs[idx];

            bool is_dm_from_them = (m->from == selected->node_id && m->to == app->my_node_num);
            bool is_dm_from_us = (m->from == app->my_node_num && m->to == selected->node_id);

            if(is_dm_from_them || is_dm_from_us) {
                chat_msgs[chat_count++] = idx;
            }
        }

        if(chat_count == 0) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 20, 34, "No direct messages");
        } else {
            int available_height = 46;
            int y = 18;
            int visible_count = 0;
            
            for(int i = chat_count - 1; i >= 0; i--) {
                uint8_t idx = chat_msgs[i];
                Message* msg = &app->history.msgs[idx];
                
                int msg_height = 14;
                if(app->lmh_mode == LMH_Wrap) {
                    int text_w = canvas_string_width(canvas, msg->text);
                    int inner_w = 116;
                    if(text_w > inner_w) {
                        int lines = calculate_wrapped_lines(canvas, msg->text, inner_w);
                        msg_height = 4 + (lines * 9) + 2;
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
            
            if(chat_count <= visible_count) {
                app->roster.chat_scroll = 0;
            } else if(app->roster.chat_scroll > chat_count - visible_count) {
                app->roster.chat_scroll = chat_count - visible_count;
            }

            uint8_t start_idx = chat_count - visible_count - app->roster.chat_scroll;
            if(start_idx > chat_count) start_idx = 0;

            y = 18;
            for(int i = 0; i < visible_count && (start_idx + i) < chat_count; i++) {
                uint8_t idx = chat_msgs[start_idx + i];
                Message* msg = &app->history.msgs[idx];
                draw_roster_bubble(canvas, 2, y, 124, msg->text, msg->is_tx, (uint32_t)idx * 977u, app);
                
                if(app->lmh_mode == LMH_Wrap) {
                    int text_w = canvas_string_width(canvas, msg->text);
                    int inner_w = 116;
                    if(text_w > inner_w) {
                        int lines = calculate_wrapped_lines(canvas, msg->text, inner_w);
                        y += 4 + (lines * 9) + 2;
                    } else {
                        y += 14;
                    }
                } else {
                    y += 14;
                }
            }
        }

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 64, "< Back");
        canvas_draw_str(canvas, 98, 64, "OK: TX");
        return;
    }

    if(app->roster.state == RosterStateDetails) {
        snprintf(title_buf, sizeof(title_buf), "Node: %08lX", (unsigned long)selected->node_id);
        draw_header(canvas, app, title_buf);

        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);

        char buf[64];
        uint32_t now = furi_get_tick() / 1000;
        uint32_t diff = now - selected->last_seen;

        snprintf(buf, sizeof(buf), "Last Seen: %lus ago", (unsigned long)diff);
        canvas_draw_str(canvas, 4, 24, buf);

        snprintf(buf, sizeof(buf), "Signal: SNR %d / RSSI %d", selected->last_snr, selected->last_rssi);
        canvas_draw_str(canvas, 4, 36, buf);

        if(selected->has_telemetry) {
            snprintf(buf, sizeof(buf), "Battery: %u%%", selected->battery_level);
            canvas_draw_str(canvas, 4, 48, buf);

            int v_int = (int)selected->voltage;
            int v_dec = (int)(selected->voltage * 100) % 100;
            snprintf(buf, sizeof(buf), "Voltage: %d.%02dV", v_int, v_dec);
            canvas_draw_str(canvas, 4, 60, buf);
        } else {
            canvas_draw_str(canvas, 4, 48, "No telemetry data yet");
        }
        return;
    }
}

void input_roster(InputEvent* e, ZeroMeshApp* app) {
    if(!app || app->roster.count == 0) return;

    if(app->roster.state == RosterStateList) {
        if(e->key == InputKeyUp && (e->type == InputTypeShort || e->type == InputTypeRepeat)) {
            if(app->roster.selected_idx > 0)
                app->roster.selected_idx--;
            else
                app->roster.selected_idx = app->roster.count - 1;
            view_port_update(app->vp);
        } else if(e->key == InputKeyDown && (e->type == InputTypeShort || e->type == InputTypeRepeat)) {
            if(app->roster.selected_idx < app->roster.count - 1)
                app->roster.selected_idx++;
            else
                app->roster.selected_idx = 0;
            view_port_update(app->vp);
        } else if(e->key == InputKeyOk) {
            if(e->type == InputTypeShort) {
                app->roster.nodes[app->roster.selected_idx].has_new_dm = false;
                app->roster.state = RosterStateChat;
                app->roster.chat_scroll = 0;
                view_port_update(app->vp);
            } else if(e->type == InputTypeLong) {
                app->roster.state = RosterStateDetails;
                view_port_update(app->vp);
            }
        }
        return;
    }

    if(app->roster.state == RosterStateChat) {
        if(e->key == InputKeyUp && (e->type == InputTypeShort || e->type == InputTypeRepeat)) {
            app->roster.chat_scroll++;
            view_port_update(app->vp);
        } else if(e->key == InputKeyDown && (e->type == InputTypeShort || e->type == InputTypeRepeat)) {
            if(app->roster.chat_scroll > 0) app->roster.chat_scroll--;
            view_port_update(app->vp);
        } else if(e->key == InputKeyOk && e->type == InputTypeShort) {
            app->show_keyboard = true;
        } else if(e->key == InputKeyBack && e->type == InputTypeShort) {
            app->roster.state = RosterStateList;
            view_port_update(app->vp);
        }
        return;
    }

    if(app->roster.state == RosterStateDetails) {
        if(e->key == InputKeyBack && e->type == InputTypeShort) {
            app->roster.state = RosterStateList;
            view_port_update(app->vp);
        }
        return;
    }
}