#include "zero_buster_view.h"
#include "zero_buster.h"
#include "helpers/zero_buster_hid.h"

/* UI Draw Callback */
void zero_buster_view_draw_callback(Canvas* canvas, void* context) {
    ZeroBusterApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);

    // Top Header Bar
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
            canvas, 125, 9, AlignRight, AlignBottom, app->layout == Layout_ES ? "ES" : "US");
    }

    canvas_set_color(canvas, ColorBlack);

    switch(app->state) {
    case State_Idle: {
        canvas_set_font(canvas, FontSecondary);
        bool is_custom = (app->spoof_profile == Spoof_Custom);

        if(is_custom) {
            // Row 0: Payload
            if(app->active_row == Setting_Payload) {
                canvas_draw_rbox(canvas, 2, 12, 124, 9, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char p_buf[48];
            const char* p_name = app->payloads.count > 0 ?
                                     app->payloads.names[app->payloads.selected_index] :
                                     "No Payloads";
            snprintf(p_buf, sizeof(p_buf), "PID: < %s >", p_name);
            canvas_draw_str(canvas, 4, 19, p_buf);
            canvas_set_color(canvas, ColorBlack);

            // Row 1: Layout
            if(app->active_row == Setting_Layout) {
                canvas_draw_rbox(canvas, 2, 21, 124, 9, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char l_buf[32];
            snprintf(l_buf, sizeof(l_buf), "KEY: < %s >", layout_names[app->layout]);
            canvas_draw_str(canvas, 4, 28, l_buf);
            canvas_set_color(canvas, ColorBlack);

            // Row 2: Spoof Profile
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

            // Row 3: Edit Custom HID
            if(app->active_row == Setting_Edit_HID) {
                canvas_draw_rbox(canvas, 2, 39, 124, 9, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 4, 46, "Cfg: [ Edit Custom HID > ]");
            canvas_set_color(canvas, ColorBlack);

            // Row 4: About / Credits
            if(app->active_row == Setting_About) {
                canvas_draw_rbox(canvas, 2, 48, 124, 9, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 4, 55, "Info: [ Creditos / About > ]");
            canvas_set_color(canvas, ColorBlack);

            // Footer (Informative only, not selectable)
            if(app->active_row == Setting_Edit_HID) {
                canvas_draw_str_aligned(
                    canvas, 64, 63, AlignCenter, AlignBottom, "[ OK = Editar Custom HID ]");
            } else if(app->active_row == Setting_About) {
                canvas_draw_str_aligned(
                    canvas, 64, 63, AlignCenter, AlignBottom, "[ OK = Ver Creditos ]");
            } else {
                canvas_draw_str_aligned(
                    canvas, 64, 63, AlignCenter, AlignBottom, "[ OK = Jack In! ]");
            }
        } else {
            // Row 0: Payload
            if(app->active_row == Setting_Payload) {
                canvas_draw_rbox(canvas, 2, 13, 124, 10, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char p_buf[48];
            const char* p_name = app->payloads.count > 0 ?
                                     app->payloads.names[app->payloads.selected_index] :
                                     "No Payloads";
            snprintf(p_buf, sizeof(p_buf), "PID: < %s >", p_name);
            canvas_draw_str(canvas, 4, 21, p_buf);
            canvas_set_color(canvas, ColorBlack);

            // Row 1: Layout
            if(app->active_row == Setting_Layout) {
                canvas_draw_rbox(canvas, 2, 23, 124, 10, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char l_buf[32];
            snprintf(l_buf, sizeof(l_buf), "KEY: < %s >", layout_names[app->layout]);
            canvas_draw_str(canvas, 4, 31, l_buf);
            canvas_set_color(canvas, ColorBlack);

            // Row 2: Spoof Profile
            if(app->active_row == Setting_Spoof) {
                canvas_draw_rbox(canvas, 2, 33, 124, 10, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            char s_buf[32];
            snprintf(s_buf, sizeof(s_buf), "HID: < %s >", spoof_names[app->spoof_profile]);
            canvas_draw_str(canvas, 4, 41, s_buf);
            canvas_set_color(canvas, ColorBlack);

            // Row 3: About / Credits
            if(app->active_row == Setting_About) {
                canvas_draw_rbox(canvas, 2, 43, 124, 10, 1);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 4, 51, "Info: [ Creditos / About > ]");
            canvas_set_color(canvas, ColorBlack);

            // Footer (Informative only, not selectable)
            if(app->active_row == Setting_About) {
                canvas_draw_str_aligned(
                    canvas, 64, 62, AlignCenter, AlignBottom, "[ OK = Ver Creditos ]");
            } else {
                canvas_draw_str_aligned(
                    canvas, 64, 62, AlignCenter, AlignBottom, "[ OK = Jack In! ]");
            }
        }
        break;
    }

    case State_About:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 23, "Zero Buster v1.1");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 34, "Autor: Javier Julve");
        canvas_draw_str(canvas, 4, 44, "GH: github.com/DonJulve");
        canvas_draw_str(canvas, 4, 53, "LI: in/javier-julve-yubero");
        canvas_draw_str_aligned(
            canvas, 64, 63, AlignCenter, AlignBottom, "[ OK / Back = Volver ]");
        break;

    case State_Edit_HID: {
        canvas_set_font(canvas, FontSecondary);

        // VID Row  (y baseline=23, box top=14)
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

        // PID Row  (y baseline=33, box top=24)
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

        // Manufacturer Row (y baseline=43, box top=34)
        if(app->edit_cursor == 8) {
            canvas_draw_rbox(canvas, 2, 34, 124, 10, 1);
            canvas_set_color(canvas, ColorWhite);
        }
        char m_buf[48];
        snprintf(m_buf, sizeof(m_buf), "Mfg: < %.18s >", app->custom_hid_cfg.manuf);
        canvas_draw_str(canvas, 4, 43, m_buf);
        canvas_set_color(canvas, ColorBlack);

        // Product Row (y baseline=53, box top=44)
        if(app->edit_cursor == 9) {
            canvas_draw_rbox(canvas, 2, 44, 124, 10, 1);
            canvas_set_color(canvas, ColorWhite);
        }
        char prd_buf[48];
        snprintf(prd_buf, sizeof(prd_buf), "Prd: < %.18s >", app->custom_hid_cfg.product);
        canvas_draw_str(canvas, 4, 53, prd_buf);
        canvas_set_color(canvas, ColorBlack);

        // Bottom bar: Save & Back
        if(app->edit_cursor == 10) {
            canvas_draw_rbox(canvas, 16, 54, 96, 10, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ SAVE & EXIT ]");
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str_aligned(
                canvas, 64, 62, AlignCenter, AlignBottom, "[ Back = Guardar ]");
        }
        break;
    }

    case State_HID_Connecting:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 26, "Charging NetLink...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 40, "Spoofing Hardware ID:");
        if(app->spoof_profile == Spoof_Custom && strlen(app->custom_hid_cfg.product) > 0) {
            canvas_draw_str(canvas, 4, 50, app->custom_hid_cfg.product);
        } else {
            canvas_draw_str(canvas, 4, 50, spoof_names[app->spoof_profile]);
        }
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ Back = Cancelar ]");
        break;

    case State_Injecting:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 26, "Jack In! Transmitting...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 40, "Layout Active:");
        canvas_draw_str(canvas, 4, 50, layout_names[app->layout]);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ Back = Cancelar ]");
        break;

    case State_Switching_CDC:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 26, "Switching to CDC...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 42, "Awaiting host serial COM");
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ Back = Cancelar ]");
        break;

    case State_Receiving: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 26, "Exfiltrating Data...");

        char stats_buf[32];
        snprintf(stats_buf, sizeof(stats_buf), "Bytes: %lu B", (unsigned long)app->bytes_received);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 38, stats_buf);

        canvas_draw_rframe(canvas, 4, 41, 120, 9, 2);
        uint8_t progress = (app->bytes_received / 8) % 116;
        canvas_draw_box(canvas, 6, 43, progress, 5);

        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ Back = Cancelar ]");
        break;
    }

    case State_Completed: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 24, "Mission Complete!");

        canvas_set_font(canvas, FontSecondary);
        char summary[48];
        snprintf(summary, sizeof(summary), "Total: %lu bytes", (unsigned long)app->bytes_received);
        canvas_draw_str(canvas, 4, 36, summary);

        const char* fname = strrchr(app->current_file, '/');
        if(fname)
            fname++;
        else
            fname = app->current_file;
        char file_lbl[80];
        snprintf(file_lbl, sizeof(file_lbl), "File: %s", fname);
        canvas_draw_str(canvas, 4, 48, file_lbl);

        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ OK / Back = Salir ]");
        break;
    }

    case State_Error:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 24, "Operation Failed!");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 38, app->error_msg);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "[ OK / Back = Salir ]");
        break;
    }

    furi_mutex_release(app->mutex);
}

/* UI Input Callback */
void zero_buster_view_input_callback(InputEvent* input_event, void* context) {
    ZeroBusterApp* app = context;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}
