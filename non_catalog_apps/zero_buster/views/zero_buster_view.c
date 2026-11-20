#include "zero_buster_view.h"
#include "zero_buster.h"
#include "helpers/zero_buster_hid.h"
#include "helpers/zero_buster_storage.h"

void zero_buster_view_draw_callback(Canvas* canvas, void* context) {
    ZeroBusterApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 12);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);

    if(app->state == State_Edit_HID) {
        canvas_draw_str(canvas, 3, 9, "HID Editor");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 125, 9, AlignRight, AlignBottom, "CUSTOM");
    } else if(app->state == State_About) {
        canvas_draw_str(canvas, 3, 9, "Zero Buster");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 125, 9, AlignRight, AlignBottom, "ABOUT");
    } else {
        canvas_draw_str(canvas, 3, 9, "Zero Buster");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 125, 9, AlignRight, AlignBottom, current_layout_name(app));
    }

    canvas_set_color(canvas, ColorBlack);

    switch(app->state) {
    case State_Idle: {
        canvas_set_font(canvas, FontSecondary);
        bool is_custom = (app->spoof_profile == Spoof_Custom);

        if(is_custom) {

            if(app->active_row == Setting_Payload) {
                canvas_draw_rbox(canvas, 2, 12, 124, 9, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char p_buf[48];
            const char* p_name = app->payloads.count > 0 ?
                app->payloads.names[app->payloads.selected_index] : "No Scripts";
            snprintf(p_buf, sizeof(p_buf), "PID: < %s >", p_name);
            canvas_draw_str(canvas, 4, 19, p_buf);
            canvas_set_color(canvas, ColorBlack);

            if(app->active_row == Setting_Layout) {
                canvas_draw_rbox(canvas, 2, 21, 124, 9, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char l_buf[40];
            snprintf(l_buf, sizeof(l_buf), "KEY: < %s >", current_layout_name(app));
            canvas_draw_str(canvas, 4, 28, l_buf);
            canvas_set_color(canvas, ColorBlack);

            if(app->active_row == Setting_Spoof) {
                canvas_draw_rbox(canvas, 2, 30, 124, 9, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char s_buf[32];
            snprintf(
                s_buf,
                sizeof(s_buf),
                "HID: < Custom %04lX >",
                (unsigned long)app->custom_hid_cfg.vid);
            canvas_draw_str(canvas, 4, 37, s_buf);
            canvas_set_color(canvas, ColorBlack);

            if(app->active_row == Setting_Edit_HID) {
                canvas_draw_rbox(canvas, 2, 39, 124, 9, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 4, 46, "Cfg: [ Edit Custom HID > ]");
            canvas_set_color(canvas, ColorBlack);

            if(app->active_row == Setting_About) {
                canvas_draw_rbox(canvas, 2, 48, 124, 9, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 4, 55, "Info: [ About / Credits > ]");
            canvas_set_color(canvas, ColorBlack);

            if(app->active_row == Setting_Edit_HID) {
                canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "[ OK = Edit Custom HID ]");
            } else if(app->active_row == Setting_About) {
                canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "[ OK = View Credits ]");
            } else {
                canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "[ OK = Start ]");
            }
        } else {

            if(app->active_row == Setting_Payload) {
                canvas_draw_rbox(canvas, 2, 13, 124, 10, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char p_buf[48];
            const char* p_name = app->payloads.count > 0 ?
                app->payloads.names[app->payloads.selected_index] : "No Scripts";
            snprintf(p_buf, sizeof(p_buf), "PID: < %s >", p_name);
            canvas_draw_str(canvas, 4, 21, p_buf);
            canvas_set_color(canvas, ColorBlack);

            if(app->active_row == Setting_Layout) {
                canvas_draw_rbox(canvas, 2, 23, 124, 10, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char l_buf[40];
            snprintf(l_buf, sizeof(l_buf), "KEY: < %s >", current_layout_name(app));
            canvas_draw_str(canvas, 4, 31, l_buf);
            canvas_set_color(canvas, ColorBlack);

            if(app->active_row == Setting_Spoof) {
                canvas_draw_rbox(canvas, 2, 33, 124, 10, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char s_buf[32];
            snprintf(s_buf, sizeof(s_buf), "HID: < %s >", spoof_names[app->spoof_profile]);
            canvas_draw_str(canvas, 4, 41, s_buf);
            canvas_set_color(canvas, ColorBlack);

            if(app->active_row == Setting_About) {
                canvas_draw_rbox(canvas, 2, 43, 124, 10, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 4, 51, "Info: [ About / Credits > ]");
            canvas_set_color(canvas, ColorBlack);

            if(app->active_row == Setting_About) {
                canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ OK = View Credits ]");
            } else {
                canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ OK = Start ]");
            }
        }
        break;
    }

    case State_About:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 23, "Zero Buster v1.3");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 34, "Author: Javier Julve");
        canvas_draw_str(canvas, 4, 44, "GH: github.com/DonJulve");
        canvas_draw_str(canvas, 4, 53, "LI: in/javier-julve-yubero");
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "[ OK / Back = Back ]");
        break;

    case State_Edit_HID: {
        canvas_set_font(canvas, FontSecondary);

        char vid_str[16];
        snprintf(vid_str, sizeof(vid_str), "VID: 0x%04lX", (unsigned long)app->custom_hid_cfg.vid);
        if(app->edit_cursor < 4) {
            canvas_draw_str(canvas, 4, 23, "VID: 0x");
            for(uint8_t i = 0; i < 4; i++) {
                uint8_t n = get_hex_nibble(app->custom_hid_cfg.vid, i);
                char c = (n < 10) ? ('0' + n) : ('A' + n - 10);
                int x = 46 + i * 7;
                if(app->edit_cursor == i) {
                    canvas_draw_box(canvas, x - 1, 14, 8, 10);
                    canvas_set_color(canvas, ColorWhite);
                }
                char s[2] = {c, '\0'};
                canvas_draw_str(canvas, x, 23, s);
                canvas_set_color(canvas, ColorBlack);
            }
        } else {
            canvas_draw_str(canvas, 4, 23, vid_str);
        }

        char pid_str[16];
        snprintf(pid_str, sizeof(pid_str), "PID: 0x%04lX", (unsigned long)app->custom_hid_cfg.pid);
        if(app->edit_cursor >= 4 && app->edit_cursor < 8) {
            canvas_draw_str(canvas, 4, 33, "PID: 0x");
            for(uint8_t i = 0; i < 4; i++) {
                uint8_t n = get_hex_nibble(app->custom_hid_cfg.pid, i);
                char c = (n < 10) ? ('0' + n) : ('A' + n - 10);
                int x = 46 + i * 7;
                if(app->edit_cursor == (4 + i)) {
                    canvas_draw_box(canvas, x - 1, 24, 8, 10);
                    canvas_set_color(canvas, ColorWhite);
                }
                char s[2] = {c, '\0'};
                canvas_draw_str(canvas, x, 33, s);
                canvas_set_color(canvas, ColorBlack);
            }
        } else {
            canvas_draw_str(canvas, 4, 33, pid_str);
        }

        if(app->edit_cursor == 8) {
            canvas_draw_rbox(canvas, 2, 34, 124, 10, 1);
            canvas_set_color(canvas, ColorWhite);
        }
        char m_buf[48];
        snprintf(m_buf, sizeof(m_buf), "Mfg: < %.18s >", app->custom_hid_cfg.manuf);
        canvas_draw_str(canvas, 4, 43, m_buf);
        canvas_set_color(canvas, ColorBlack);

        if(app->edit_cursor == 9) {
            canvas_draw_rbox(canvas, 2, 44, 124, 10, 1);
            canvas_set_color(canvas, ColorWhite);
        }
        char prd_buf[48];
        snprintf(prd_buf, sizeof(prd_buf), "Prd: < %.18s >", app->custom_hid_cfg.product);
        canvas_draw_str(canvas, 4, 53, prd_buf);
        canvas_set_color(canvas, ColorBlack);

        if(app->edit_cursor == 10) {
            canvas_draw_rbox(canvas, 16, 54, 96, 10, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ SAVE & EXIT ]");
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ Back = Save ]");
        }
        break;
    }

    case State_HID_Connecting:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 26, "Starting NetLink...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 40, "USB Device Profile:");
        if(app->spoof_profile == Spoof_Custom && strlen(app->custom_hid_cfg.product) > 0) {
            canvas_draw_str(canvas, 4, 50, app->custom_hid_cfg.product);
        } else {
            canvas_draw_str(canvas, 4, 50, spoof_names[app->spoof_profile]);
        }
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ Back = Cancel ]");
        break;

    case State_Injecting:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 26, "Transmitting Script...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 40, "Layout Active:");
        canvas_draw_str(canvas, 4, 50, current_layout_name(app));
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ Back = Cancel ]");
        break;

    case State_Switching_CDC:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 26, "Switching to CDC...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 42, "Awaiting host serial COM");
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ Back = Cancel ]");
        break;

    case State_Receiving: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 26, "Receiving Data...");

        char stats_buf[32];
        snprintf(stats_buf, sizeof(stats_buf), "Bytes: %lu B", (unsigned long)app->bytes_received);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 38, stats_buf);

        canvas_draw_rframe(canvas, 4, 41, 120, 9, 2);
        uint8_t progress = (app->bytes_received / 8) % 116;
        canvas_draw_box(canvas, 6, 43, progress, 5);

        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ Back = Cancel ]");
        break;
    }

    case State_Completed: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 24, "Transfer Complete!");

        canvas_set_font(canvas, FontSecondary);
        char summary[48];
        snprintf(summary, sizeof(summary), "Total: %lu bytes", (unsigned long)app->bytes_received);
        canvas_draw_str(canvas, 4, 36, summary);

        const char* fname = strrchr(app->current_file, '/');
        if(fname) fname++;
        else fname = app->current_file;
        canvas_draw_str(canvas, 4, 46, "Saved log:");
        canvas_draw_str(canvas, 4, 55, fname);

        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ OK / Back = Exit ]");
        break;
    }

    case State_Error:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 24, "Operation Failed!");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 38, app->error_msg);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ OK / Back = Exit ]");
        break;
    }

    furi_mutex_release(app->mutex);
}

void zero_buster_view_input_callback(InputEvent* input_event, void* context) {
    ZeroBusterApp* app = context;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}
