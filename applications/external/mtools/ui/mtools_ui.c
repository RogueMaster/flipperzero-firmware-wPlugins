#include "mtools_ui.h"
#include "nfc_scan_art.h"
#include "../mtools_app.h"
#include "../nfc/card_info.h"
#include <gui/elements.h>
#include <mtools_icons.h>
#include <furi.h>
#include <stdio.h>

#define MTOOLS_HOME_ITEMS_VISIBLE 3

static void mtools_draw_home_icon(Canvas* canvas, int x, int y, size_t index) {
    switch(index) {
    case 0: // NFC field
        canvas_draw_circle(canvas, x + 6, y + 7, 5);
        canvas_draw_circle(canvas, x + 6, y + 7, 3);
        canvas_draw_dot(canvas, x + 6, y + 7);
        break;
    case 1: // UID card
        canvas_draw_rframe(canvas, x, y + 2, 12, 10, 1);
        canvas_draw_line(canvas, x + 2, y + 5, x + 5, y + 5);
        canvas_draw_line(canvas, x + 7, y + 5, x + 10, y + 5);
        canvas_draw_line(canvas, x + 2, y + 8, x + 8, y + 8);
        break;
    case 2: // Recover cross
        canvas_draw_box(canvas, x + 5, y + 2, 3, 11);
        canvas_draw_box(canvas, x + 1, y + 6, 11, 3);
        break;
    case 3: // Information
        canvas_draw_circle(canvas, x + 6, y + 7, 5);
        canvas_draw_dot(canvas, x + 6, y + 4);
        canvas_draw_line(canvas, x + 6, y + 6, x + 6, y + 10);
        break;
    default:
        canvas_draw_circle(canvas, x + 6, y + 7, 4);
        canvas_draw_dot(canvas, x + 6, y + 7);
        break;
    }
}

static void mtools_draw_home(Canvas* canvas, MToolsApp* app) {
    static const char* const labels[] = {
        "Magic Check", "UID Changer", "BCC Recover", "About MTools"};
    const size_t total_items = COUNT_OF(labels);

    if(app->selected_tool >= total_items) {
        app->selected_tool = 0;
    }
    if(app->selected_tool < app->home_window_offset) {
        app->home_window_offset = app->selected_tool;
    } else if(app->selected_tool >= app->home_window_offset + MTOOLS_HOME_ITEMS_VISIBLE) {
        app->home_window_offset = app->selected_tool - MTOOLS_HOME_ITEMS_VISIBLE + 1;
    }

    canvas_set_font(canvas, FontPrimary);
    const char* title = "MTools";
    uint16_t title_width = canvas_string_width(canvas, title);
    uint16_t total_header_width = 10 + 4 + title_width;
    int16_t header_x = (128 - total_header_width) / 2;
    canvas_draw_icon(canvas, header_x, 2, &I_mtools_10x10);
    canvas_draw_str(canvas, header_x + 10 + 4, 11, title);

    for(size_t i = 0; i < MTOOLS_HOME_ITEMS_VISIBLE; ++i) {
        size_t item_idx = app->home_window_offset + i;
        if(item_idx >= total_items) break;

        int x = 2;
        int y = 15 + i * 16;
        if(app->selected_tool == item_idx) {
            canvas_draw_rbox(canvas, x, y, 119, 15, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        mtools_draw_home_icon(canvas, x + 5, y, item_idx);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, x + 21, y + 11, labels[item_idx]);
        canvas_set_color(canvas, ColorBlack);
    }

    if(total_items > MTOOLS_HOME_ITEMS_VISIBLE) {
        elements_scrollbar_pos(canvas, 125, 15, 47, app->selected_tool, total_items);
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
            canvas_draw_str(canvas, 7, 43, "MFC GEN1A / GEN2");
            canvas_draw_str(canvas, 7, 52, "MFC GEN3 / GEN4 / GDM");
            canvas_draw_str(canvas, 7, 62, "MFU GEN2 - DirectWrite");
        } else {
            canvas_draw_str(canvas, 4, 24, "Supported magic cards:");
            canvas_draw_str(canvas, 7, 34, "ISO15 GEN1 - Hidden UID");
            canvas_draw_str(canvas, 7, 43, "ISO15 GEN2 - Direct Block");
            canvas_draw_str(canvas, 7, 52, "ISO15 GEN3 - Activation");
            canvas_draw_str(canvas, 7, 62, "MFU BCC Recovery Tool");
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
            snprintf(
                line,
                sizeof(line),
                "%s",
                mtools_iso15_chip_name(app->magic_uid, app->magic_uid_len));
        } else if(app->magic_type2) {
            snprintf(
                line,
                sizeof(line),
                "%s",
                app->magic_type2_name ? app->magic_type2_name : "Ultralight / NTAG");
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
            snprintf(
                line,
                sizeof(line),
                "SAK: %02X ATQA: %02X%02X",
                app->magic_sak,
                app->magic_atqa[0],
                app->magic_atqa[1]);
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
        case 3:
            magic_name = "ISO15 GEN2";
            break;
        case 4:
            magic_name = "ISO15 GEN1";
            break;
        case 5:
            magic_name = "ISO15 GEN3";
            break;
        case 7:
            magic_name = "MFC GEN1A - UID";
            break;
        case 8:
            magic_name = "MFC GEN2 - CUID";
            break;
        case 9:
            magic_name = "MFC GEN3 - APDU";
            break;
        case 10:
            magic_name = "MFC GEN4 - UMC";
            break;
        case 11:
            magic_name = "MFC GDM - USCUID";
            break;
        case 12:
            magic_name = "MFU GEN2 - DirectWrite";
            break;
        case 13:
            magic_name = "GDM/USCUID + GEN1A";
            break;
        case 14:
            magic_name = "Possible ISO15 GEN1";
            break;
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

static void mtools_draw_bcc_recover(Canvas* canvas, MToolsApp* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 11, "BCC Recover");
    canvas_draw_line(canvas, 0, 14, 127, 14);
    canvas_set_font(canvas, FontSecondary);

    if(app->bcc_state == BccRecoverStatePrompt || app->bcc_state == BccRecoverStateReading) {
        mtools_scan_draw_flipper(canvas, 0, 17);
        mtools_scan_draw_fob(canvas, 5, 51);
        mtools_scan_draw_card(canvas, 35, 51);
        mtools_scan_draw_arrow(canvas, 26, 42 + app->magic_anim_phase % 3, false);
        canvas_draw_line(canvas, 62, 18, 62, 60);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 66, 35, "DIAGNOSE");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 66, 49, "Place Tag");
    } else if(app->bcc_state == BccRecoverStateWriting) {
        mtools_scan_draw_flipper(canvas, 0, 17);
        mtools_scan_draw_fob(canvas, 5, 51);
        mtools_scan_draw_card(canvas, 35, 51);
        mtools_scan_draw_arrow(canvas, 26, 42 + app->magic_anim_phase % 3, false);
        canvas_draw_line(canvas, 62, 18, 62, 60);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 66, 31, "RECOVER");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 66, 44, "Writing P0-2");
        canvas_draw_str(canvas, 66, 54, "Keep still");
    } else if(app->bcc_state == BccRecoverStateDiagnosed) {
        char line[32];
        char hex[16] = {0};
        for(size_t i = 0; i < 7; i++) {
            snprintf(hex + i * 2, sizeof(hex) - i * 2, "%02X", app->bcc_result.uid[i]);
        }
        snprintf(line, sizeof(line), "UID: %s", hex);
        canvas_draw_str(canvas, 4, 25, line);

        if(app->bcc_result.bcc0_valid) {
            snprintf(line, sizeof(line), "CL1 BCC: %02X (OK)", app->bcc_result.bcc0_actual);
        } else {
            snprintf(
                line,
                sizeof(line),
                "CL1: %02X (BAD -> %02X)",
                app->bcc_result.bcc0_actual,
                app->bcc_result.bcc0_expected);
        }
        canvas_draw_str(canvas, 4, 36, line);

        if(app->bcc_result.bcc1_valid) {
            snprintf(line, sizeof(line), "CL2 BCC: %02X (OK)", app->bcc_result.bcc1_actual);
        } else {
            snprintf(
                line,
                sizeof(line),
                "CL2: %02X (BAD -> %02X)",
                app->bcc_result.bcc1_actual,
                app->bcc_result.bcc1_expected);
        }
        canvas_draw_str(canvas, 4, 47, line);

        if(app->bcc_result.needs_recovery) {
            canvas_draw_rbox(canvas, 2, 51, 124, 12, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(
                canvas, 64, 61, AlignCenter, AlignBottom, "Press OK to Recover");
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, 61, "BCC is Valid. OK: Rescan");
        }
    } else if(app->bcc_state == BccRecoverStateSuccess) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 25, "Recovery Success!");
        canvas_set_font(canvas, FontSecondary);
        char hex[16] = {0};
        for(size_t i = 0; i < 7; i++) {
            snprintf(hex + i * 2, sizeof(hex) - i * 2, "%02X", app->bcc_result.uid[i]);
        }
        char line[32];
        snprintf(line, sizeof(line), "UID: %s", hex);
        canvas_draw_str(canvas, 4, 37, line);
        canvas_draw_str(
            canvas,
            4,
            48,
            app->bcc_result.verified ? "Verified by ISO14443-A" : "Written & Verified");
        canvas_draw_str(canvas, 4, 61, "Press OK to scan next");
    } else if(app->bcc_state == BccRecoverStateFailed) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 25, "Recovery Failed");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(
            canvas,
            4,
            37,
            app->bcc_result.error_message[0] ? app->bcc_result.error_message :
                                               "Write rejected by tag");
        canvas_draw_str(canvas, 4, 48, "Tag not DirectWrite/Locked");
        canvas_draw_str(canvas, 4, 61, "Press OK to retry");
    }
}

void mtools_ui_draw(Canvas* canvas, void* model) {
    MToolsApp* app = *(MToolsApp**)model;
    if(app->active_scene == MToolsSceneHome) {
        mtools_draw_home(canvas, app);
    } else if(app->active_scene == MToolsSceneAbout) {
        mtools_draw_about(canvas, app);
    } else if(app->active_scene == MToolsSceneBccRecover) {
        mtools_draw_bcc_recover(canvas, app);
    } else {
        mtools_draw_detail(canvas, app);
    }
}
