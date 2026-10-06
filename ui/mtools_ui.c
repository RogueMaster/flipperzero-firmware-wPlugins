#include "mtools_ui.h"
#include "nfc_scan_art.h"
#include "../mtools_app.h"
#include "../nfc/card_info.h"
#include <mtools_icons.h>
#include <furi.h>
#include <stdio.h>

static void mtools_draw_home_icon(Canvas* canvas, int x, int y, size_t index) {
    switch(index) {
    case 0: // NFC field
        canvas_draw_circle(canvas, x + 5, y + 5, 4);
        canvas_draw_circle(canvas, x + 5, y + 5, 2);
        canvas_draw_dot(canvas, x + 5, y + 5);
        break;
    case 1: // UID card
        canvas_draw_rframe(canvas, x, y + 1, 11, 8, 1);
        canvas_draw_line(canvas, x + 2, y + 4, x + 4, y + 4);
        canvas_draw_line(canvas, x + 6, y + 4, x + 8, y + 4);
        canvas_draw_line(canvas, x + 2, y + 6, x + 7, y + 6);
        break;
    case 2: // Information
        canvas_draw_circle(canvas, x + 5, y + 5, 4);
        canvas_draw_dot(canvas, x + 5, y + 3);
        canvas_draw_line(canvas, x + 5, y + 5, x + 5, y + 7);
        break;
    }
}

static void mtools_draw_home(Canvas* canvas, MToolsApp* app) {
    static const char* const labels[] = {"Magic Check", "UID Changer", "About MTools"};
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "MTools");
    for(size_t i = 0; i < COUNT_OF(labels); ++i) {
        int x = 4;
        int y = 16 + i * 16;
        if(app->selected_tool == i) {
            canvas_draw_rbox(canvas, x, y, 120, 15, 3);
            canvas_set_color(canvas, ColorWhite);
        }
        mtools_draw_home_icon(canvas, x + 5, y + 2, i);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, x + 21, y + 11, labels[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void mtools_draw_about(Canvas* canvas, MToolsApp* app) {
    if(app->about_page == 1) {
        canvas_draw_icon(canvas, 0, 0, &I_NFC_dolphin_emulation_51x64);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 55, 14, "Shop URL");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 55, 30, app->about_listener ? "NFC active" : "NFC error");
        canvas_draw_str(canvas, 55, 44, "Tap phone");
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 54, 128, 10);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "shop.mtoolstec.com");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 3, 11, "About MTools");
        canvas_draw_line(canvas, 0, 14, 127, 14);
        canvas_set_font(canvas, FontSecondary);
        /* Header shortcut leaves the full list height available. */
        canvas_draw_rbox(canvas, 90, 0, 38, 14, 3);
        canvas_draw_box(canvas, 90, 3, 38, 11);
        canvas_draw_box(canvas, 93, 0, 35, 14);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 98, 11, "URL >");
        canvas_set_color(canvas, ColorBlack);
        if(app->about_list_page == 0) {
            canvas_draw_str(canvas, 4, 24, "MTools NFC by MTCK");
            canvas_draw_str(canvas, 4, 34, "Supported magic cards:");
            canvas_draw_str(canvas, 7, 44, "MFC GEN1 - UID");
            canvas_draw_str(canvas, 7, 53, "MFC GEN2 - CUID");
            canvas_draw_str(canvas, 7, 62, "MFC GEN3 - APDU");
        } else {
            canvas_draw_str(canvas, 7, 23, "MFC GEN4 - UMC");
            canvas_draw_str(canvas, 7, 33, "MFC GDM - USCUID");
            canvas_draw_str(canvas, 7, 43, "ISO15 GEN1");
            canvas_draw_str(canvas, 7, 53, "ISO15 GEN2");
            canvas_draw_str(canvas, 7, 63, "ISO15 GEN3");
        }
        canvas_draw_line(canvas, 126, 19, 126, 62);
        canvas_draw_box(canvas, 124, app->about_list_page ? 43 : 19, 4, 20);
    }
}

static void mtools_draw_detail(Canvas* canvas, MToolsApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 11, "Magic Check");
    canvas_draw_line(canvas, 0, 14, 127, 14);
    canvas_set_font(canvas, FontSecondary);
    if(app->scan_status == 0 || app->scan_status == 6) {
        const bool reading = app->scan_status == 6;
        mtools_scan_draw_flipper(canvas, 0, 17);
        mtools_scan_draw_fob(canvas, 5, 51);
        mtools_scan_draw_card(canvas, 35, 51);
        mtools_scan_draw_arrow(canvas, 26, 42 + app->magic_anim_phase % 3, false);
        canvas_draw_line(canvas, 62, 18, 62, 60);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 66, 31, reading ? "READING" : "SCANNING");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 66, 44, reading ? "Reading UID" : "Hold card");
        canvas_draw_str(canvas, 66, 54, reading ? "Please wait" : "to back");
    } else {
        const bool long_uid = app->magic_uid_len > 8;
        char line[32];
        if(app->magic_protocol == 1) {
            snprintf(line, sizeof(line), "%s", mtools_iso15_chip_name(app->magic_uid, app->magic_uid_len));
        } else if(app->magic_type2) {
            snprintf(line, sizeof(line), "Ultralight / NTAG");
        } else if(app->magic_sak == 0x08) {
            snprintf(line, sizeof(line), "MFC 1K / %uB", app->magic_uid_len);
        } else if(app->magic_sak == 0x18) {
            snprintf(line, sizeof(line), "MFC 4K / %uB", app->magic_uid_len);
        } else {
            snprintf(line, sizeof(line), "ISO14443-A / %uB", app->magic_uid_len);
        }
        canvas_draw_str(canvas, 3, long_uid ? 22 : 24, line);
        canvas_set_font(canvas, FontKeyboard);
        const size_t first = MIN(app->magic_uid_len, 8);
        size_t used = snprintf(line, sizeof(line), "UID: ");
        mtools_uid_format_hex(line + used, sizeof(line) - used, app->magic_uid, first);
        canvas_draw_str(canvas, 3, long_uid ? 30 : 34, line);
        if(app->magic_uid_len > first) {
            size_t count = MIN((size_t)app->magic_uid_len - first, (size_t)8);
            used = snprintf(line, sizeof(line), "     ");
            mtools_uid_format_hex(line + used, sizeof(line) - used, app->magic_uid + first, count);
            canvas_draw_str(canvas, 3, 38, line);
        }
        if(app->magic_protocol != 1) {
            snprintf(line, sizeof(line), "SAK: %02X ATQA: %02X%02X", app->magic_sak,
                     app->magic_atqa[0], app->magic_atqa[1]);
            canvas_draw_str(canvas, 3, long_uid ? 46 : 44, line);
        } else {
            snprintf(
                line,
                sizeof(line),
                "%u blk x %uB IC:%02X",
                app->magic_iso_blocks,
                app->magic_iso_block_size,
                app->magic_iso_ic_ref);
            canvas_draw_str(canvas, 3, 44, line);
        }
        canvas_set_font(canvas, FontSecondary);
        const char* magic_name = "Unconfirmed";
        switch(app->scan_status) {
        case 3: magic_name = "ISO15 GEN2"; break;
        case 4: magic_name = "ISO15 GEN1"; break;
        case 5: magic_name = "ISO15 GEN3"; break;
        case 7: magic_name = "MFC GEN1 - UID"; break;
        case 8: magic_name = "MFC GEN2 - CUID"; break;
        case 9: magic_name = "MFC GEN3 - APDU"; break;
        case 10: magic_name = "MFC GEN4 - UMC"; break;
        case 11: magic_name = "MFC GDM - USCUID"; break;
        }
        if(app->magic_detecting) {
            canvas_draw_rframe(canvas, 2, 49, 124, 14, 3);
            /* Move top-right to bottom-left stripes through the badge outline. */
            const int offset = (app->magic_anim_phase * 2) % 12;
            for(int base = -18; base < 144; base += 12) {
                for(int y = 51; y <= 60; y++) {
                    for(int width = 0; width < 3; width++) {
                        const int x = base - (y - 51) + offset + width;
                        if(x >= 5 && x <= 122) canvas_draw_dot(canvas, x, y);
                    }
                }
            }
        } else {
            canvas_draw_rbox(canvas, 2, 49, 124, 14, 3);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_line(canvas, 7, 54, 7, 58);
            canvas_draw_line(canvas, 120, 54, 120, 58);
            canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, magic_name);
            canvas_set_color(canvas, ColorBlack);
        }
    }
}

void mtools_ui_draw(Canvas* canvas, void* model) {
    MToolsApp* app = *(MToolsApp**)model;
    if(app->active_scene == MToolsSceneHome) {
        mtools_draw_home(canvas, app);
    } else if(app->active_scene == MToolsSceneAbout) {
        mtools_draw_about(canvas, app);
    } else {
        mtools_draw_detail(canvas, app);
    }
}
